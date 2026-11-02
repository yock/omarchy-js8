#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include "audio.h"
#include "messagemodel.h"
#include "settings.h"
#include "spectrum.h"

class DecoderWorker;

// The app's state and work: owns the audio source, the modem thread, the
// waterfall's spectrum rows, and the persisted station settings.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *messages READ messages CONSTANT)
    Q_PROPERTY(QStringList inputDevices READ inputDevices NOTIFY inputDevicesChanged)
    Q_PROPERTY(int inputDeviceIndex READ inputDeviceIndex NOTIFY inputDeviceChanged)
    Q_PROPERTY(bool monitoring READ monitoring NOTIFY monitoringChanged)
    Q_PROPERTY(bool demo READ demo NOTIFY demoChanged)
    Q_PROPERTY(bool error READ error NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(double level READ level NOTIFY levelChanged)
    Q_PROPERTY(QString callsign READ callsign WRITE setCallsign NOTIFY callsignChanged)
    Q_PROPERTY(QString grid READ grid WRITE setGrid NOTIFY gridChanged)
    Q_PROPERTY(double dialMhz READ dialMhz WRITE setDialMhz NOTIFY dialMhzChanged)
    Q_PROPERTY(QString utcClock READ utcClock NOTIFY clockChanged)
    Q_PROPERTY(double periodProgress READ periodProgress NOTIFY clockChanged)
    Q_PROPERTY(int secondsToPeriod READ secondsToPeriod NOTIFY clockChanged)
    Q_PROPERTY(QString lastDecode READ lastDecode NOTIFY lastDecodeChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    QAbstractItemModel *messages() { return &m_messages; }
    QStringList inputDevices() const { return m_inputDevices; }
    int inputDeviceIndex() const { return m_inputDeviceIndex; }
    bool monitoring() const { return m_monitoring; }
    bool demo() const { return m_demo; }
    bool error() const { return m_error; }
    QString status() const;
    double level() const { return m_level; }
    QString callsign() const { return m_callsign; }
    QString grid() const { return m_grid; }
    double dialMhz() const { return m_dialMhz; }
    QString utcClock() const { return m_utcClock; }
    double periodProgress() const { return m_periodProgress; }
    int secondsToPeriod() const { return m_secondsToPeriod; }
    QString lastDecode() const { return m_lastDecode; }

    Q_INVOKABLE void toggleMonitor();
    Q_INVOKABLE void setDemo(bool on);
    Q_INVOKABLE void selectInputDevice(int index);
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void clearMessages();
    Q_INVOKABLE void saveSettings();

    void setCallsign(const QString &value);
    void setGrid(const QString &value);
    void setDialMhz(double value);

signals:
    void spectrumRow(const QByteArray &row);
    void inputDevicesChanged();
    void inputDeviceChanged();
    void monitoringChanged();
    void demoChanged();
    void statusChanged();
    void levelChanged();
    void callsignChanged();
    void gridChanged();
    void dialMhzChanged();
    void clockChanged();
    void lastDecodeChanged();

private:
    void startCapture();
    void startDemoSource();
    void startWorker();
    void stopReceiver();
    // Kept as a state so the footer re-translates when the language changes;
    // only the detail (a device name or an error) is stored as text.
    enum class Status { Stopped, NoDevice, Listening, Demo, InputError };

    void scheduleSave();
    void tickClock();
    void tickLevel();
    void tickSpectrum();
    void onDecoded(const QString &text, int snrDb, int frequencyHz, double dtSeconds,
                   const QString &submode, bool lowConfidence);
    void setStatus(Status status, const QString &detail = {}, bool error = false);
    void logDecode(const DecodeEntry &entry);

    AudioEngine m_audio;
    Settings m_settings;
    MessageModel m_messages;
    Spectrum m_spectrum;
    DecoderWorker *m_decoder = nullptr;

    QTimer m_spectrumTimer;
    QTimer m_clockTimer;
    QTimer m_levelTimer;
    QTimer m_saveTimer;

    QStringList m_inputDevices;
    int m_inputDeviceIndex = -1;
    bool m_monitoring = false;
    bool m_demo = false;
    bool m_error = false;
    Status m_status = Status::Stopped;
    QString m_statusDetail;
    double m_level = 0.0;
    QString m_callsign;
    QString m_grid;
    double m_dialMhz = 7.078;
    QString m_utcClock;
    double m_periodProgress = 0.0;
    int m_secondsToPeriod = 0;
    QString m_lastDecode;
    int m_lastSpectrumPosition = -1;
};
