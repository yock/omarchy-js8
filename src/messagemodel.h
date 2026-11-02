#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

struct DecodeEntry {
    QString time;
    int snrDb = 0;
    int frequencyHz = 0;
    double dtSeconds = 0.0;
    QString message;
    QString submode;
    bool lowConfidence = false;
    bool demo = false;
};

class MessageModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TimeRole = Qt::UserRole + 1,
        SnrRole,
        FrequencyRole,
        MessageRole,
        SubmodeRole,
        LowConfidenceRole,
        DemoRole,
    };

    explicit MessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_entries.size()); }
    void add(const DecodeEntry &entry);
    void clear();

signals:
    void countChanged();

private:
    QVector<DecodeEntry> m_entries;
};
