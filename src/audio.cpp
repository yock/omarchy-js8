#include "audio.h"

#include "gfsk8modem.h"

#include <QDateTime>

#include <portaudio.h>

#include <algorithm>
#include <cmath>
#include <cstring>

static int paCallback(const void *input, void *, unsigned long frames,
                      const PaStreamCallbackTimeInfo *, PaStreamCallbackFlags, void *user);

namespace {

constexpr int CaptureRate = 48000;
constexpr int Decimate = CaptureRate / SampleRing::SampleRate;
constexpr int CaptureFrames = 512;

// 49-tap Kaiser-windowed lowpass for 48 kHz -> 12 kHz, cutoff 5.4 kHz.
// Copied from gfsk8-modem's js8rx example so the radio path matches the
// receiver that was used to validate the modem.
constexpr float Fir[49] = {
    -0.000693f, -0.000916f,  0.000000f,  0.001894f,  0.003226f,
     0.001834f, -0.003056f, -0.008550f, -0.008237f,  0.001713f,
     0.017358f,  0.026667f,  0.015536f, -0.020240f, -0.062744f,
    -0.089276f, -0.056437f,  0.066547f,  0.248069f,  0.398161f,
     0.447938f,  0.398161f,  0.248069f,  0.066547f, -0.056437f,
    -0.089276f, -0.062744f, -0.020240f,  0.015536f,  0.026667f,
     0.017358f,  0.001713f, -0.008237f, -0.008550f, -0.003056f,
     0.001834f,  0.003226f,  0.001894f,  0.000000f, -0.000916f,
    -0.000693f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,
     0.000000f,  0.000000f,  0.000000f,  0.000000f,
};

} // namespace

// PortAudio's callback needs C linkage and cannot be a private member with the
// opaque types, so it lives here and forwards to the engine.
static int paCallback(const void *input, void *, unsigned long frames,
                      const PaStreamCallbackTimeInfo *, PaStreamCallbackFlags, void *user)
{
    if (input && user)
        static_cast<AudioEngine *>(user)->onAudio(static_cast<const qint16 *>(input),
                                                  static_cast<int>(frames));
    return paContinue;
}

SampleRing::SampleRing()
    : m_data(Size, 0)
{
}

void SampleRing::reset()
{
    std::fill(m_data.begin(), m_data.end(), 0);
    m_write.store(0, std::memory_order_release);
}

void SampleRing::push(const qint16 *samples, int count)
{
    int position = m_write.load(std::memory_order_relaxed);
    for (int i = 0; i < count; ++i) {
        m_data[position] = samples[i];
        if (++position >= Size)
            position = 0;
    }
    m_write.store(position, std::memory_order_release);
}

void SampleRing::readLatest(qint16 *out, int count) const
{
    count = std::min(count, Size);
    int start = m_write.load(std::memory_order_acquire) - count;
    while (start < 0)
        start += Size;
    const int first = std::min(count, Size - start);
    std::memcpy(out, m_data.data() + start, static_cast<std::size_t>(first) * sizeof(qint16));
    if (first < count)
        std::memcpy(out + first, m_data.data(), static_cast<std::size_t>(count - first) * sizeof(qint16));
}

void SampleRing::snapshot(qint16 *out) const
{
    readLatest(out, Size);
}

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
{
    m_paReady = Pa_Initialize() == paNoError;
    refreshDevices();

    m_demoTimer.setInterval(25);
    connect(&m_demoTimer, &QTimer::timeout, this, &AudioEngine::demoFeed);
}

AudioEngine::~AudioEngine()
{
    stop();
    stopDemo();
    if (m_paReady)
        Pa_Terminate();
}

void AudioEngine::refreshDevices()
{
    m_devices.clear();
    m_names.clear();
    if (!m_paReady)
        return;

    const int defaultIndex = Pa_GetDefaultInputDevice();
    const PaDeviceInfo *defaultInfo = defaultIndex >= 0 ? Pa_GetDeviceInfo(defaultIndex) : nullptr;
    if (defaultInfo && defaultInfo->maxInputChannels > 0)
        m_devices.push_back({defaultIndex, QString::fromUtf8(defaultInfo->name),
                             defaultInfo->maxInputChannels});

    const int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info || info->maxInputChannels <= 0 || i == defaultIndex)
            continue;
        m_devices.push_back({i, QString::fromUtf8(info->name), info->maxInputChannels});
    }

    for (const auto &device : m_devices)
        m_names.push_back(device.name);
}

int AudioEngine::paIndexAt(int listIndex) const
{
    if (listIndex < 0 || listIndex >= m_devices.size())
        return -1;
    return m_devices.at(listIndex).paIndex;
}

int AudioEngine::listIndexForName(const QString &name) const
{
    for (int i = 0; i < m_devices.size(); ++i)
        if (m_devices.at(i).name == name)
            return i;
    return -1;
}

int AudioEngine::defaultListIndex() const
{
    return m_devices.isEmpty() ? -1 : 0;
}

QString AudioEngine::deviceNameAt(int listIndex) const
{
    return listIndex >= 0 && listIndex < m_devices.size() ? m_devices.at(listIndex).name : QString();
}

void AudioEngine::onAudio(const qint16 *input, int frames)
{
    frames = std::min(frames, CaptureFrames);
    float samples[CaptureFrames];
    for (int i = 0; i < frames; ++i) {
        const qint16 value = input[i * m_channels];
        samples[i] = value * (1.0f / 32768.0f);
        const float magnitude = std::fabs(samples[i]);
        float peak = m_peak.load(std::memory_order_relaxed);
        while (magnitude > peak && !m_peak.compare_exchange_weak(peak, magnitude)) {}
    }

    qint16 output[CaptureFrames / Decimate + 1];
    int outCount = 0;
    for (int i = 0; i < frames; i += Decimate) {
        float accumulator = 0.0f;
        for (int k = 0; k < NFir; ++k) {
            const int index = i - (NFir - 1) + k;
            const float sample = index < 0 ? m_firState[NFir - 1 + index] : samples[index];
            accumulator += Fir[k] * sample;
        }
        const float scaled = accumulator * 32767.0f;
        output[outCount++] = static_cast<qint16>(std::clamp(scaled, -32768.0f, 32767.0f));
    }

    for (int i = 0; i < NFir - 1; ++i) {
        const int source = frames - (NFir - 1) + i;
        m_firState[i] = source < 0 ? 0.0f : samples[source];
    }

    m_ring.push(output, outCount);
}

bool AudioEngine::start(int listIndex, QString *error)
{
    stop();
    stopDemo();
    if (!m_paReady) {
        if (error)
            *error = tr("PortAudio could not be initialised.");
        return false;
    }

    const int paIndex = paIndexAt(listIndex);
    const PaDeviceInfo *info = paIndex >= 0 ? Pa_GetDeviceInfo(paIndex) : nullptr;
    if (!info) {
        if (error)
            *error = tr("No audio input device.");
        return false;
    }

    PaStreamParameters input;
    input.device = paIndex;
    input.channelCount = 1;
    input.sampleFormat = paInt16;
    input.suggestedLatency = info->defaultLowInputLatency;
    input.hostApiSpecificStreamInfo = nullptr;

    PaError result = Pa_OpenStream(&m_stream, &input, nullptr, CaptureRate,
                                   CaptureFrames, paClipOff, paCallback, this);
    if (result != paNoError) {
        // Some interfaces expose only stereo inputs.
        input.channelCount = 2;
        result = Pa_OpenStream(&m_stream, &input, nullptr, CaptureRate,
                               CaptureFrames, paClipOff, paCallback, this);
    }

    if (result != paNoError) {
        m_stream = nullptr;
        if (error)
            *error = QString::fromUtf8(Pa_GetErrorText(result));
        return false;
    }

    m_channels = input.channelCount;
    std::fill(std::begin(m_firState), std::end(m_firState), 0.0f);
    m_ring.reset();
    m_peak.store(0.0f);

    result = Pa_StartStream(static_cast<PaStream *>(m_stream));
    if (result != paNoError) {
        Pa_CloseStream(static_cast<PaStream *>(m_stream));
        m_stream = nullptr;
        if (error)
            *error = QString::fromUtf8(Pa_GetErrorText(result));
        return false;
    }

    m_running = true;
    return true;
}

void AudioEngine::stop()
{
    if (m_stream) {
        auto *stream = static_cast<PaStream *>(m_stream);
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        m_stream = nullptr;
    }
    m_running = false;
    m_peak.store(0.0f);
}

bool AudioEngine::startDemo()
{
    stop();
    stopDemo();
    m_ring.reset();
    m_peak.store(0.0f);
    if (m_demoSignals.empty())
        buildDemoSignals();

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 offset = (1 - (now % 15) + 15) % 15;
    m_demoBoundary = now + offset;
    m_demoPlaying = false;
    m_demoPosition = 0;
    m_demoTimer.start();
    return true;
}

void AudioEngine::stopDemo()
{
    m_demoTimer.stop();
    m_demoPlaying = false;
    m_demoPosition = 0;
}

void AudioEngine::buildDemoSignals()
{
    struct Demo {
        const char *text;
        double frequency;
    };
    static const Demo demos[] = {
        {"CQ CQ CQ", 800.0},
        {"HELLO FROM JS8", 1500.0},
        {"WATERFALL READY", 2300.0},
    };

    for (const auto &demo : demos) {
        const auto frames = gfsk8::pack("N0CALL", "FN31PR", demo.text, gfsk8::Submode::Normal);
        if (frames.empty())
            continue;
        const auto pcm = gfsk8::modulate(gfsk8::Submode::Normal, frames.front().frameType,
                                         frames.front().payload, demo.frequency);
        std::vector<qint16> samples;
        samples.reserve(pcm.size());
        for (float sample : pcm)
            samples.push_back(static_cast<qint16>(std::clamp(sample, -1.0f, 1.0f) * 24000.0f));
        m_demoSignals.push_back(std::move(samples));
    }
}

void AudioEngine::demoFeed()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (!m_demoPlaying && now >= m_demoBoundary && !m_demoSignals.empty()) {
        m_demoPlaying = true;
        m_demoPosition = 0;
        m_demoIndex = (m_demoIndex + 1) % static_cast<int>(m_demoSignals.size());
        m_demoBoundary += 15;
    }

    constexpr int count = 300; // 25 ms of 12 kHz audio
    qint16 samples[count];
    float peak = 0.0f;
    static const std::vector<qint16> emptySignal;
    const std::vector<qint16> &signal = m_demoSignals.empty()
        ? emptySignal
        : m_demoSignals.at(m_demoIndex);

    for (int i = 0; i < count; ++i) {
        float value = m_noise(m_random);
        if (m_demoPlaying) {
            if (m_demoPosition < signal.size())
                value += signal[m_demoPosition++] * (0.30f / 24000.0f);
            else
                m_demoPlaying = false;
        }
        value = std::clamp(value, -1.0f, 1.0f);
        peak = std::max(peak, std::fabs(value));
        samples[i] = static_cast<qint16>(value * 32767.0f);
    }

    m_ring.push(samples, count);
    float current = m_peak.load(std::memory_order_relaxed);
    while (peak > current && !m_peak.compare_exchange_weak(current, peak)) {}
}
