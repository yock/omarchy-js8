#include "backend.h"

#include "decoderworker.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace {

constexpr int PeriodSeconds = 15;
constexpr int SpectrumHop = 2048; // 50% overlap at 4096 samples.

} // namespace

Backend::Backend(QObject *parent)
    : QObject(parent)
    , m_settings()
    , m_decoder(new DecoderWorker(&m_audio.ring(), this))
{
    m_settings.load();
    m_callsign = m_settings.callsign;
    m_grid = m_settings.grid;
    m_dialMhz = m_settings.dialMhz;

    m_inputDevices = m_audio.deviceNames();
    m_inputDeviceIndex = m_audio.listIndexForName(m_settings.inputDevice);
    if (m_inputDeviceIndex < 0)
        m_inputDeviceIndex = m_audio.defaultListIndex();

    connect(m_decoder, &DecoderWorker::decoded, this, &Backend::onDecoded);

    m_spectrumTimer.setInterval(40);
    connect(&m_spectrumTimer, &QTimer::timeout, this, &Backend::tickSpectrum);

    m_clockTimer.setInterval(200);
    connect(&m_clockTimer, &QTimer::timeout, this, &Backend::tickClock);
    m_clockTimer.start();

    m_levelTimer.setInterval(50);
    connect(&m_levelTimer, &QTimer::timeout, this, &Backend::tickLevel);
    m_levelTimer.start();

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &Backend::saveSettings);

    tickClock();

    if (m_settings.monitor) {
        if (m_inputDeviceIndex >= 0)
            startCapture();
        else
            setStatus(Status::NoDevice, {}, true);
    } else {
        setStatus(Status::Stopped);
    }
}

Backend::~Backend()
{
    m_saveTimer.stop();
    saveSettings();

    m_spectrumTimer.stop();
    m_clockTimer.stop();
    m_levelTimer.stop();
    if (m_decoder)
        m_decoder->stop();
    m_audio.stop();
    m_audio.stopDemo();
}

QString Backend::status() const
{
    switch (m_status) {
    case Status::Stopped:
        return tr("Receiver stopped");
    case Status::NoDevice:
        return tr("No audio input device found. Connect a radio interface, or press D for the demo signal.");
    case Status::Listening:
        return tr("Listening on %1").arg(m_statusDetail);
    case Status::Demo:
        return tr("Demo signal running — no radio input");
    case Status::InputError:
        return tr("Could not open the audio input: %1").arg(m_statusDetail);
    }
    return {};
}

void Backend::setStatus(Status status, const QString &detail, bool error)
{
    if (status == m_status && detail == m_statusDetail && error == m_error)
        return;
    m_status = status;
    m_statusDetail = detail;
    m_error = error;
    emit statusChanged();
}

void Backend::startWorker()
{
    if (m_decoder && !m_decoder->isRunning())
        m_decoder->start();
    m_lastSpectrumPosition = -1;
    m_spectrumTimer.start();
}

void Backend::startCapture()
{
    if (m_inputDeviceIndex < 0) {
        setStatus(Status::NoDevice, {}, true);
        m_monitoring = false;
        emit monitoringChanged();
        return;
    }

    QString error;
    if (!m_audio.start(m_inputDeviceIndex, &error)) {
        m_spectrumTimer.stop();
        if (m_decoder)
            m_decoder->stop();
        m_monitoring = false;
        m_demo = false;
        setStatus(Status::InputError, error, true);
        emit monitoringChanged();
        emit demoChanged();
        return;
    }

    m_monitoring = true;
    m_demo = false;
    startWorker();
    setStatus(Status::Listening, m_audio.deviceNameAt(m_inputDeviceIndex));
    emit monitoringChanged();
    emit demoChanged();
    scheduleSave();
}

void Backend::startDemoSource()
{
    m_audio.startDemo();
    m_monitoring = true;
    m_demo = true;
    startWorker();
    setStatus(Status::Demo);
    emit monitoringChanged();
    emit demoChanged();
}

void Backend::stopReceiver()
{
    m_spectrumTimer.stop();
    if (m_decoder)
        m_decoder->stop();
    m_audio.stop();
    m_audio.stopDemo();
    m_monitoring = false;
    m_demo = false;
    m_level = 0.0;
    m_lastSpectrumPosition = -1;
    setStatus(Status::Stopped);
    emit monitoringChanged();
    emit demoChanged();
    emit levelChanged();
    scheduleSave();
}

void Backend::toggleMonitor()
{
    if (m_demo) {
        stopReceiver();
        return;
    }
    if (m_monitoring)
        stopReceiver();
    else
        startCapture();
}

void Backend::setDemo(bool on)
{
    if (on == m_demo)
        return;
    if (on) {
        m_spectrumTimer.stop();
        if (m_decoder)
            m_decoder->stop();
        m_audio.stop();
        startDemoSource();
    } else {
        stopReceiver();
    }
}

void Backend::selectInputDevice(int index)
{
    if (index < 0 || index >= m_inputDevices.size() || index == m_inputDeviceIndex)
        return;

    m_inputDeviceIndex = index;
    m_settings.inputDevice = m_audio.deviceNameAt(index);
    emit inputDeviceChanged();
    scheduleSave();

    if (m_monitoring && !m_demo) {
        m_audio.stop();
        m_spectrumTimer.stop();
        if (m_decoder)
            m_decoder->stop();
        startCapture();
    }
}

void Backend::refreshDevices()
{
    const QString currentName = m_audio.deviceNameAt(m_inputDeviceIndex);
    m_audio.refreshDevices();
    m_inputDevices = m_audio.deviceNames();

    int next = m_audio.listIndexForName(currentName);
    if (next < 0)
        next = m_audio.listIndexForName(m_settings.inputDevice);
    if (next < 0)
        next = m_audio.defaultListIndex();

    const bool changed = next != m_inputDeviceIndex;
    m_inputDeviceIndex = next;
    emit inputDevicesChanged();
    if (changed)
        emit inputDeviceChanged();

    if (m_monitoring && !m_demo) {
        if (m_inputDeviceIndex < 0) {
            stopReceiver();
            setStatus(Status::NoDevice, {}, true);
        }
    }
}

void Backend::clearMessages()
{
    m_messages.clear();
    m_lastDecode.clear();
    emit lastDecodeChanged();
}

void Backend::setCallsign(const QString &value)
{
    const QString trimmed = value.trimmed().toUpper();
    if (trimmed == m_callsign)
        return;
    m_callsign = trimmed;
    emit callsignChanged();
    scheduleSave();
}

void Backend::setGrid(const QString &value)
{
    const QString trimmed = value.trimmed().toUpper();
    if (trimmed == m_grid)
        return;
    m_grid = trimmed;
    emit gridChanged();
    scheduleSave();
}

void Backend::setDialMhz(double value)
{
    if (value < 0.1 || value > 6000.0 || std::abs(value - m_dialMhz) < 0.0005)
        return;
    m_dialMhz = value;
    emit dialMhzChanged();
    scheduleSave();
}

void Backend::scheduleSave()
{
    m_saveTimer.start();
}

void Backend::saveSettings()
{
    m_settings.callsign = m_callsign;
    m_settings.grid = m_grid;
    m_settings.dialMhz = m_dialMhz;
    m_settings.monitor = m_monitoring && !m_demo;
    if (m_inputDeviceIndex >= 0)
        m_settings.inputDevice = m_audio.deviceNameAt(m_inputDeviceIndex);
    m_settings.save();
}

void Backend::tickClock()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    m_utcClock = now.toString(QStringLiteral("HH:mm:ss"));

    const int seconds = now.time().hour() * 3600 + now.time().minute() * 60 + now.time().second();
    m_periodProgress = (seconds % PeriodSeconds) / static_cast<double>(PeriodSeconds);
    m_secondsToPeriod = (1 - seconds % PeriodSeconds + PeriodSeconds) % PeriodSeconds;
    emit clockChanged();
}

void Backend::tickLevel()
{
    const double peak = std::min(1.0, m_audio.takePeak() * 1.8);
    m_level = std::max(peak, m_level * 0.78);
    if (m_level < 0.004)
        m_level = 0.0;
    emit levelChanged();
}

void Backend::tickSpectrum()
{
    const int position = m_audio.ring().writePosition();
    if (m_lastSpectrumPosition < 0) {
        m_lastSpectrumPosition = position;
        return;
    }

    int delta = position - m_lastSpectrumPosition;
    if (delta < 0)
        delta += SampleRing::Size;
    if (delta < SpectrumHop)
        return;

    qint16 window[Spectrum::FftSize];
    m_audio.ring().readLatest(window, Spectrum::FftSize);
    emit spectrumRow(m_spectrum.row(window, Spectrum::FftSize));
    m_lastSpectrumPosition = position;
}

void Backend::onDecoded(const QString &text, int snrDb, int frequencyHz, double dtSeconds,
                        const QString &submode, bool lowConfidence)
{
    DecodeEntry entry;
    entry.time = QDateTime::currentDateTimeUtc().toString(QStringLiteral("HH:mm:ss"));
    entry.snrDb = snrDb;
    entry.frequencyHz = frequencyHz;
    entry.dtSeconds = dtSeconds;
    entry.message = text;
    entry.submode = submode;
    entry.lowConfidence = lowConfidence;
    entry.demo = m_demo;

    m_messages.add(entry);
    logDecode(entry);

    m_lastDecode = text;
    emit lastDecodeChanged();
}

void Backend::logDecode(const DecodeEntry &entry)
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(directory))
        return;

    QFile log(directory + "/decodes.log");
    if (!log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    // One line per decode, easy to grep and to feed into other station tools.
    QTextStream out(&log);
    out << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << '\t'
        << entry.frequencyHz << "Hz\t" << entry.snrDb << "dB\t" << entry.submode << '\t'
        << entry.message << '\n';
}
