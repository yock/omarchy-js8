#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <atomic>
#include <random>
#include <vector>

struct AudioDevice {
    int paIndex = -1;
    QString name;
    int channels = 0;
};

// A 60-second ring of 12 kHz mono audio. The PortAudio callback writes, while
// the decoder thread and the FFT timer read snapshots; the write cursor is the
// only shared state, so the audio thread never waits on a mutex.
class SampleRing {
public:
    static constexpr int SampleRate = 12000;
    static constexpr int Size = 60 * SampleRate;

    SampleRing();

    void reset();
    void push(const qint16 *samples, int count);
    int writePosition() const { return m_write.load(std::memory_order_acquire); }

    void readLatest(qint16 *out, int count) const;
    void snapshot(qint16 *out) const;

private:
    std::vector<qint16> m_data;
    std::atomic<int> m_write{0};
};

// Captures the radio interface's audio and decimates it to the modem's 12 kHz.
// Also provides a self-contained demo signal so the waterfall and decoder can
// be exercised without a radio attached.
class AudioEngine : public QObject {
    Q_OBJECT

public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    QStringList deviceNames() const { return m_names; }
    int deviceCount() const { return static_cast<int>(m_devices.size()); }
    int paIndexAt(int listIndex) const;
    int listIndexForName(const QString &name) const;
    int defaultListIndex() const;
    QString deviceNameAt(int listIndex) const;
    void refreshDevices();

    bool start(int listIndex, QString *error);
    void stop();
    bool running() const { return m_running; }

    bool startDemo();
    void stopDemo();
    bool demo() const { return m_demoTimer.isActive(); }

    const SampleRing &ring() const { return m_ring; }
    float takePeak() { return m_peak.exchange(0.0f, std::memory_order_relaxed); }

    // Called by PortAudio's callback (the free function in audio.cpp).
    void onAudio(const qint16 *input, int frames);

private:
    void demoFeed();
    void buildDemoSignals();

    SampleRing m_ring;
    QStringList m_names;
    QVector<AudioDevice> m_devices;
    bool m_paReady = false;
    bool m_running = false;
    void *m_stream = nullptr;
    int m_channels = 1;
    std::atomic<float> m_peak{0.0f};

    // 4:1 FIR decimator, from js8rx (GPL-3.0, same project family as the modem).
    static constexpr int NFir = 49;
    float m_firState[NFir - 1] = {};

    QTimer m_demoTimer;
    std::vector<std::vector<qint16>> m_demoSignals;
    int m_demoIndex = 0;
    std::size_t m_demoPosition = 0;
    bool m_demoPlaying = false;
    qint64 m_demoBoundary = 0;
    std::mt19937 m_random{0x5A17};
    std::normal_distribution<float> m_noise{0.0f, 0.004f};
};
