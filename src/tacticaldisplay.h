#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickFramebufferObject>
#include <QString>
#include <QVariantList>
#include <QVector>

// The whole visible console: a vector status strip, spectrum trace, waterfall
// frame, decode ticker, and help overlay drawn with raylib's rlgl renderer.
// Qt still owns the window and the Omarchy lifecycle around it.
class TacticalDisplay : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(QObject *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY accentChanged)
    Q_PROPERTY(QString background READ background WRITE setBackground NOTIFY backgroundChanged)
    Q_PROPERTY(bool helpVisible READ helpVisible WRITE setHelpVisible NOTIFY helpVisibleChanged)
    Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(bool error READ error WRITE setError NOTIFY errorChanged)
    Q_PROPERTY(bool monitoring READ monitoring WRITE setMonitoring NOTIFY monitoringChanged)
    Q_PROPERTY(bool demo READ demo WRITE setDemo NOTIFY demoChanged)
    Q_PROPERTY(double level READ level WRITE setLevel NOTIFY levelChanged)
    Q_PROPERTY(QString callsign READ callsign WRITE setCallsign NOTIFY callsignChanged)
    Q_PROPERTY(QString grid READ grid WRITE setGrid NOTIFY gridChanged)
    Q_PROPERTY(double dialMhz READ dialMhz WRITE setDialMhz NOTIFY dialMhzChanged)
    Q_PROPERTY(QString utcClock READ utcClock WRITE setUtcClock NOTIFY utcClockChanged)
    Q_PROPERTY(int secondsToPeriod READ secondsToPeriod WRITE setSecondsToPeriod NOTIFY secondsToPeriodChanged)
    Q_PROPERTY(double periodProgress READ periodProgress WRITE setPeriodProgress NOTIFY periodProgressChanged)
    Q_PROPERTY(QString inputDeviceName READ inputDeviceName WRITE setInputDeviceName NOTIFY inputDeviceNameChanged)
    Q_PROPERTY(QVariantList ticker READ ticker WRITE setTicker NOTIFY tickerChanged)

public:
    explicit TacticalDisplay(QQuickItem *parent = nullptr);

    Renderer *createRenderer() const override;

    QObject *source() const { return m_source; }
    void setSource(QObject *source);

    QString accent() const { return m_accent; }
    void setAccent(const QString &accent);

    QString background() const { return m_background; }
    void setBackground(const QString &background);

    bool helpVisible() const { return m_helpVisible; }
    void setHelpVisible(bool visible);

    QString status() const { return m_status; }
    void setStatus(const QString &status);

    bool error() const { return m_error; }
    void setError(bool error);

    bool monitoring() const { return m_monitoring; }
    void setMonitoring(bool monitoring);

    bool demo() const { return m_demo; }
    void setDemo(bool demo);

    double level() const { return m_level; }
    void setLevel(double level);

    QString callsign() const { return m_callsign; }
    void setCallsign(const QString &callsign);

    QString grid() const { return m_grid; }
    void setGrid(const QString &grid);

    double dialMhz() const { return m_dialMhz; }
    void setDialMhz(double dialMhz);

    QString utcClock() const { return m_utcClock; }
    void setUtcClock(const QString &utcClock);

    int secondsToPeriod() const { return m_secondsToPeriod; }
    void setSecondsToPeriod(int seconds);

    double periodProgress() const { return m_periodProgress; }
    void setPeriodProgress(double progress);

    QString inputDeviceName() const { return m_inputDeviceName; }
    void setInputDeviceName(const QString &name);

    QVariantList ticker() const { return m_ticker; }
    void setTicker(const QVariantList &ticker);

public slots:
    void appendRow(const QByteArray &row);

signals:
    void sourceChanged();
    void accentChanged();
    void backgroundChanged();
    void helpVisibleChanged();
    void statusChanged();
    void errorChanged();
    void monitoringChanged();
    void demoChanged();
    void levelChanged();
    void callsignChanged();
    void gridChanged();
    void dialMhzChanged();
    void utcClockChanged();
    void secondsToPeriodChanged();
    void periodProgressChanged();
    void inputDeviceNameChanged();
    void tickerChanged();

private:
    friend class TacticalRenderer;

    static constexpr int Rows = 256;

    void buildFontAtlas();

    QObject *m_source = nullptr;
    QMetaObject::Connection m_connection;
    QString m_accent = QStringLiteral("#FFD60A");
    QString m_background = QStringLiteral("#05070b");
    bool m_helpVisible = false;
    QString m_status;
    bool m_error = false;
    bool m_monitoring = false;
    bool m_demo = false;
    double m_level = 0.0;
    QString m_callsign;
    QString m_grid;
    double m_dialMhz = 7.078;
    QString m_utcClock;
    int m_secondsToPeriod = 0;
    double m_periodProgress = 0.0;
    QString m_inputDeviceName;
    QVariantList m_ticker;

    QImage m_fontImage;
    int m_fontCellWidth = 0;
    int m_fontCellHeight = 0;
    int m_fontAscent = 0;
    int m_fontCapHeight = 0;
    int m_fontPixelSize = 64;
    int m_fontColumns = 16;
    int m_fontFirst = 32;
    int m_fontLast = 255;
    quint64 m_fontGeneration = 0;
    QString m_fontFamily;

    mutable QMutex m_mutex;
    QVector<QByteArray> m_rows;
    QByteArray m_latestSpectrum;
    quint64 m_generation = 0;
};
