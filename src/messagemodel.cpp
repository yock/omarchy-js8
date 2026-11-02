#include "messagemodel.h"

MessageModel::MessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const DecodeEntry &entry = m_entries.at(index.row());
    switch (role) {
    case TimeRole: return entry.time;
    case SnrRole: return entry.snrDb;
    case FrequencyRole: return entry.frequencyHz;
    case MessageRole: return entry.message;
    case SubmodeRole: return entry.submode;
    case LowConfidenceRole: return entry.lowConfidence;
    case DemoRole: return entry.demo;
    default: return {};
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const
{
    return {
        {TimeRole, "time"},
        {SnrRole, "snr"},
        {FrequencyRole, "frequency"},
        {MessageRole, "message"},
        {SubmodeRole, "submode"},
        {LowConfidenceRole, "lowConfidence"},
        {DemoRole, "demo"},
    };
}

void MessageModel::add(const DecodeEntry &entry)
{
    // Newest first, like the waterfall's newest row at the top.
    beginInsertRows({}, 0, 0);
    m_entries.prepend(entry);
    endInsertRows();

    while (m_entries.size() > 200) {
        beginRemoveRows({}, m_entries.size() - 1, m_entries.size() - 1);
        m_entries.removeLast();
        endRemoveRows();
    }
    emit countChanged();
}

void MessageModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
}
