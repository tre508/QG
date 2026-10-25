#include "reel_model.hpp"

ReelModel::ReelModel(QObject* parent) : QAbstractListModel(parent) {}

int ReelModel::rowCount(const QModelIndex&) const { return entries_.size(); }

QVariant ReelModel::data(const QModelIndex& idx, int role) const
{
    if (idx.row() < 0 || idx.row() >= entries_.size())
        return {};
    const ReelEntry& e = entries_.at(idx.row());
    switch (role) {
    case IndexRole: return e.index;
    case IdentityRole: return e.identity;
    case NameRole: return e.name;
    case PathRole: return e.path;
    case DurationMsRole: return e.durationMs >= 0 ? QVariant(e.durationMs) : QVariant();
    case SizeRole: return e.sizeBytes;
    case FavoriteRole: return e.favorite;
    case WatchedRole: return e.watched;
    case ProgressRole: return e.progress >= 0 ? QVariant(e.progress) : QVariant();
    case IsCurrentRole: return idx.row() == currentIdx_;
    default: return {};
    }
}

QHash<int, QByteArray> ReelModel::roleNames() const
{
    return {
        {IndexRole, QByteArrayLiteral("reelIndex")},
        {IdentityRole, QByteArrayLiteral("reelIdentity")},
        {NameRole, QByteArrayLiteral("reelName")},
        {PathRole, QByteArrayLiteral("reelPath")},
        {DurationMsRole, QByteArrayLiteral("reelDurationMs")},
        {SizeRole, QByteArrayLiteral("reelSize")},
        {FavoriteRole, QByteArrayLiteral("reelFavorite")},
        {WatchedRole, QByteArrayLiteral("reelWatched")},
        {ProgressRole, QByteArrayLiteral("reelProgress")},
        {IsCurrentRole, QByteArrayLiteral("reelIsCurrent")},
    };
}

void ReelModel::setEntries(const QVector<ReelEntry>& entries)
{
    beginResetModel();
    entries_ = entries;
    // Re-derive display positions and clamp the highlight into range.
    for (int i = 0; i < entries_.size(); ++i)
        entries_[i].index = i + 1;
    if (currentIdx_ >= entries_.size())
        currentIdx_ = entries_.isEmpty() ? -1 : entries_.size() - 1;
    endResetModel();
    emit countChanged();
    emit currentIndexChanged();
}

void ReelModel::setCurrentIndex(int idx)
{
    const int clamped = (idx >= 0 && idx < entries_.size()) ? idx : -1;
    if (currentIdx_ == clamped)
        return;
    const int old = currentIdx_;
    currentIdx_ = clamped;
    if (old >= 0 && old < entries_.size())
        emit dataChanged(index(old), index(old), {IsCurrentRole});
    if (currentIdx_ >= 0 && currentIdx_ < entries_.size())
        emit dataChanged(index(currentIdx_), index(currentIdx_), {IsCurrentRole});
    emit currentIndexChanged();
}

bool ReelModel::setCurrentByIdentity(const QString& identity)
{
    const int row = indexOfIdentity(identity);
    if (row < 0)
        return false;
    setCurrentIndex(row);
    return true;
}

int ReelModel::currentIndex() const { return currentIdx_; }

QString ReelModel::identityAt(int row) const
{
    if (row < 0 || row >= entries_.size())
        return QString();
    return entries_.at(row).identity;
}

int ReelModel::indexOfIdentity(const QString& identity) const
{
    if (identity.isEmpty())
        return -1;
    for (int i = 0; i < entries_.size(); ++i) {
        if (entries_.at(i).identity == identity)
            return i;
    }
    return -1;
}

bool ReelModel::updateEntry(const ReelEntry& entry)
{
    const int row = indexOfIdentity(entry.identity);
    if (row < 0)
        return false;
    ReelEntry& current = entries_[row];
    current.index = row + 1;            // keep the display position canonical
    current.name = entry.name;
    current.path = entry.path;
    current.durationMs = entry.durationMs;
    current.sizeBytes = entry.sizeBytes;
    current.favorite = entry.favorite;
    current.watched = entry.watched;
    current.progress = entry.progress;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx,
                     {IndexRole, NameRole, PathRole, DurationMsRole, SizeRole,
                      FavoriteRole, WatchedRole, ProgressRole});
    return true;
}

bool ReelModel::removeByIdentity(const QString& identity)
{
    const int row = indexOfIdentity(identity);
    if (row < 0)
        return false;

    beginRemoveRows(QModelIndex(), row, row);
    entries_.removeAt(row);
    endRemoveRows();

    // The current item may have shifted or been removed.
    if (currentIdx_ == row)
        currentIdx_ = -1;
    else if (currentIdx_ > row)
        --currentIdx_;

    renumberFrom(row);
    emit countChanged();
    emit currentIndexChanged();
    return true;
}

void ReelModel::renumberFrom(int startRow)
{
    if (startRow < 0 || startRow >= entries_.size())
        return;
    for (int i = startRow; i < entries_.size(); ++i)
        entries_[i].index = i + 1;
    emit dataChanged(index(startRow), index(entries_.size() - 1), {IndexRole});
}

QString ReelModel::summary() const
{
    if (entries_.isEmpty() || currentIdx_ < 0)
        return QStringLiteral("0 / %1").arg(entries_.size());
    return QStringLiteral("%1 / %2").arg(currentIdx_ + 1).arg(entries_.size());
}
