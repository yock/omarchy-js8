#include "decoderworker.h"

#include "DecodedText.h"
#include "audio.h"
#include "gfsk8modem.h"

#include <QDateTime>
#include <QHash>

#include <algorithm>
#include <span>
#include <vector>

namespace {

constexpr int SnapshotSize = 60 * SampleRing::SampleRate;
constexpr int PeriodSeconds = 15;

QString submodeName(int submode)
{
    switch (submode) {
    case 0: return QStringLiteral("JS8 Normal");
    case 1: return QStringLiteral("JS8 Fast");
    case 2: return QStringLiteral("JS8 Turbo");
    case 4: return QStringLiteral("JS8 Slow");
    case 8: return QStringLiteral("JS8 Ultra");
    default: return QStringLiteral("JS8");
    }
}

int utcCode()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    return now.time().hour() * 10000 + now.time().minute() * 100 + now.time().second();
}

} // namespace

DecoderWorker::DecoderWorker(const SampleRing *ring, QObject *parent)
    : QThread(parent)
    , m_ring(ring)
{
    setObjectName(QStringLiteral("js8-decoder"));
}

DecoderWorker::~DecoderWorker()
{
    stop();
}

void DecoderWorker::stop()
{
    m_stop.store(true, std::memory_order_release);
    if (!wait(30000))
        requestInterruption();
}

void DecoderWorker::run()
{
    m_stop.store(false, std::memory_order_release);
    gfsk8::Decoder decoder(1 << 0); // Normal only, for a responsive bootstrap.
    std::vector<qint16> snapshot(SnapshotSize);
    QHash<QString, qint64> seen;

    while (!m_stop.load(std::memory_order_acquire)) {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const qint64 offset = (1 - (now % PeriodSeconds) + PeriodSeconds) % PeriodSeconds;
        qint64 target = now + offset;

        while (!m_stop.load(std::memory_order_acquire)
               && QDateTime::currentSecsSinceEpoch() < target)
            msleep(100);
        if (m_stop.load(std::memory_order_acquire))
            break;

        m_ring->snapshot(snapshot.data());
        const int utc = utcCode();
        decoder.decode(std::span<const qint16>(snapshot.data(), snapshot.size()), utc,
                       [&](const gfsk8::Decoded &frame) {
            DecodedText text(frame.message, frame.frameType, frame.submode);
            QString message = QString::fromStdString(text.message()).trimmed();
            if (message.isEmpty())
                message = QString::fromStdString(frame.message).trimmed();

            // The decoder can report the same frame twice in adjoining
            // periods; keep one entry per signal for a few seconds.
            const QString key = message + QLatin1Char('|')
                + QString::number(static_cast<int>(frame.frequencyHz / 50));
            const qint64 stamp = QDateTime::currentSecsSinceEpoch();
            if (seen.contains(key) && stamp - seen.value(key) < 20)
                return;
            seen.insert(key, stamp);

            emit decoded(message, frame.snrDb, static_cast<int>(frame.frequencyHz + 0.5),
                         frame.dtSeconds, submodeName(frame.submode), text.isLowConfidence());
        });
    }
}
