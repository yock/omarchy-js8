#pragma once

#include <QThread>

#include <atomic>

class SampleRing;

// Decodes the last JS8 Normal period once per UTC 15-second cycle. It lives on
// its own thread because the modem's search takes long enough to stutter the
// waterfall if it ran on the GUI thread.
class DecoderWorker : public QThread {
    Q_OBJECT

public:
    explicit DecoderWorker(const SampleRing *ring, QObject *parent = nullptr);
    ~DecoderWorker() override;

    void stop();

signals:
    void decoded(const QString &text, int snrDb, int frequencyHz, double dtSeconds,
                 const QString &submode, bool lowConfidence);

protected:
    void run() override;

private:
    const SampleRing *m_ring;
    std::atomic<bool> m_stop{false};
};
