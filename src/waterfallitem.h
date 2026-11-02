#pragma once

#include <QMutex>
#include <QObject>
#include <QQuickFramebufferObject>
#include <QString>
#include <QVector>

// The waterfall surface. QML owns the layout; the pixels are drawn by raylib's
// rlgl renderer into Qt's framebuffer object, so raylib does not need its own
// window or GLFW context.
class WaterfallItem : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(QObject *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY accentChanged)
    Q_PROPERTY(QString background READ background WRITE setBackground NOTIFY backgroundChanged)

public:
    explicit WaterfallItem(QQuickItem *parent = nullptr);

    Renderer *createRenderer() const override;

    QObject *source() const { return m_source; }
    void setSource(QObject *source);

    QString accent() const { return m_accent; }
    void setAccent(const QString &accent);

    QString background() const { return m_background; }
    void setBackground(const QString &background);

public slots:
    void appendRow(const QByteArray &row);

signals:
    void sourceChanged();
    void accentChanged();
    void backgroundChanged();

private:
    friend class WaterfallRenderer;

    // About 44 seconds of history at the ~5.9 rows/s produced by the FFT hop.
    static constexpr int Rows = 256;

    QObject *m_source = nullptr;
    QMetaObject::Connection m_connection;
    QString m_accent = QStringLiteral("#FFD60A");
    QString m_background = QStringLiteral("#0b0b0e");

    mutable QMutex m_mutex;
    QVector<QByteArray> m_rows;
    quint64 m_generation = 0;
};
