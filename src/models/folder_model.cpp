#include "models/folder_model.hpp"

#include "sorting/sort_service.hpp"

namespace qg {

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FolderModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case IdentityRole:
        return entry.identity;
    case PathRole:
        return entry.path;
    case NameRole:
        return entry.name;
    case KindRole:
        return static_cast<int>(entry.kind);
    case IsFolderRole:
        return entry.isFolder();
    case IsVideoRole:
        return entry.isVideo();
    case ThumbnailRole:
        return m_thumbnails.value(entry.identity);
    case ThumbnailStateRole:
        return static_cast<int>(m_thumbnailStates.value(entry.identity, entry.thumbnail));
    case DurationRole:
        return entry.durationMs.has_value() ? QVariant(*entry.durationMs) : QVariant();
    case SizeRole:
        return entry.size;
    case MtimeRole:
        return entry.mtimeNs;
    case FavoriteRole:
        return entry.favorite;
    case WatchedRole:
        return entry.watched;
    case ProgressRole:
        return entry.progress.has_value() ? QVariant(*entry.progress) : QVariant();
    case WidthRole:
        return entry.width.has_value() ? QVariant(*entry.width) : QVariant();
    case HeightRole:
        return entry.height.has_value() ? QVariant(*entry.height) : QVariant();
    default:
        return {};
    }
}

QHash<int, QByteArray> FolderModel::roleNames() const
{
    return {
        {IdentityRole, QByteArrayLiteral("identity")},
        {PathRole, QByteArrayLiteral("path")},
        {NameRole, QByteArrayLiteral("name")},
        {KindRole, QByteArrayLiteral("kind")},
        {IsFolderRole, QByteArrayLiteral("isFolder")},
        {IsVideoRole, QByteArrayLiteral("isVideo")},
        {ThumbnailRole, QByteArrayLiteral("thumbnail")},
        {ThumbnailStateRole, QByteArrayLiteral("thumbnailState")},
        {DurationRole, QByteArrayLiteral("duration")},
        {SizeRole, QByteArrayLiteral("size")},
        {MtimeRole, QByteArrayLiteral("mtime")},
        {FavoriteRole, QByteArrayLiteral("favorite")},
        {WatchedRole, QByteArrayLiteral("watched")},
        {ProgressRole, QByteArrayLiteral("progress")},
        {WidthRole, QByteArrayLiteral("width")},
        {HeightRole, QByteArrayLiteral("height")},
    };
}

void FolderModel::applySnapshot(const FolderSnapshot &snapshot)
{
    beginResetModel();
    m_entries = snapshot.entries;
    m_thumbnails.clear();
    m_thumbnailStates.clear();
    recount();
    endResetModel();
    emit countChanged();
}

void FolderModel::applyBatch(const FolderSnapshot &batch)
{
    if (batch.entries.isEmpty()) {
        return;
    }

    const int first = static_cast<int>(m_entries.size());
    const int last = first + static_cast<int>(batch.entries.size()) - 1;
    beginInsertRows(QModelIndex(), first, last);
    m_entries.append(batch.entries);
    for (const Entry &entry : batch.entries) {
        adjustCounts(entry, 1);
    }
    endInsertRows();
    emit countChanged();
}

void FolderModel::applyDiff(const FolderDiff &diff)
{
    // Removals, from the end so earlier indices stay valid.
    for (const QString &identity : diff.removedIdentities) {
        const int row = indexOfIdentity(identity);
        if (row < 0) {
            continue;
        }
        beginRemoveRows(QModelIndex(), row, row);
        adjustCounts(m_entries.at(row), -1);
        m_entries.removeAt(row);
        m_thumbnails.remove(identity);
        m_thumbnailStates.remove(identity);
        endRemoveRows();
        emit countChanged();
    }

    // In-place metadata changes (e.g. rename, content touched).
    for (const Entry &updated : diff.changed) {
        const int row = indexOfIdentity(updated.identity);
        if (row < 0) {
            continue;
        }
        Entry &entry = m_entries[row];
        entry.path = updated.path;
        entry.name = updated.name;
        entry.size = updated.size;
        entry.mtimeNs = updated.mtimeNs;

        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx,
                         {PathRole, NameRole, SizeRole, MtimeRole, ThumbnailRole,
                          ThumbnailStateRole});
    }

    // Additions append; ordering is the SortService's responsibility (P33).
    if (!diff.added.isEmpty()) {
        FolderSnapshot batch;
        batch.entries = diff.added;
        applyBatch(batch);
    }
}

void FolderModel::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    m_thumbnails.clear();
    m_thumbnailStates.clear();
    m_videoCount = 0;
    m_folderCount = 0;
    endResetModel();
    emit countChanged();
}

void FolderModel::updateThumbnail(const QString &identity,
                                  const QVariant &image,
                                  ThumbnailState state)
{
    const int row = indexOfIdentity(identity);
    if (row < 0) {
        return;
    }

    m_thumbnailStates.insert(identity, state);
    if (image.isValid()) {
        m_thumbnails.insert(identity, image);
    } else {
        m_thumbnails.remove(identity);
    }

    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ThumbnailRole, ThumbnailStateRole});
}

void FolderModel::updateProbe(const QString &identity,
                              const std::optional<qint64> &durationMs,
                              const std::optional<int> &width,
                              const std::optional<int> &height)
{
    const int row = indexOfIdentity(identity);
    if (row < 0) {
        return;
    }

    Entry &entry = m_entries[row];
    entry.durationMs = durationMs;
    entry.width = width;
    entry.height = height;

    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {DurationRole, WidthRole, HeightRole});
}

void FolderModel::updateState(const QString &identity,
                              const std::optional<bool> &favorite,
                              const std::optional<bool> &watched,
                              const std::optional<double> &progress)
{
    const int row = indexOfIdentity(identity);
    if (row < 0) {
        return;
    }

    Entry &entry = m_entries[row];
    if (favorite.has_value()) {
        entry.favorite = *favorite;
    }
    if (watched.has_value()) {
        entry.watched = *watched;
    }
    if (progress.has_value()) {
        entry.progress = progress;
    }

    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {FavoriteRole, WatchedRole, ProgressRole});
}

void FolderModel::updateFavorite(const QString &identity, bool value)
{
    const int row = indexOfIdentity(identity);
    if (row < 0) {
        return;
    }
    m_entries[row].favorite = value;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {FavoriteRole});
}

void FolderModel::updateWatched(const QString &identity, bool value)
{
    const int row = indexOfIdentity(identity);
    if (row < 0) {
        return;
    }
    m_entries[row].watched = value;
    m_entries[row].progress = value ? std::optional<double>(1.0) : std::nullopt;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {WatchedRole, ProgressRole});
}

void FolderModel::updateProgress(const QString &identity, const std::optional<double> &progress)
{
    const int row = indexOfIdentity(identity);
    if (row < 0) {
        return;
    }
    m_entries[row].progress = progress;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ProgressRole});
}

void FolderModel::updatePathAndName(const QString &identity,
                                    const QString &newPath,
                                    const QString &newName)
{
    const int row = indexOfIdentity(identity);
    if (row < 0) {
        return;
    }
    m_entries[row].path = newPath;
    m_entries[row].name = newName;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {PathRole, NameRole});
}

bool FolderModel::removeByIdentity(const QString &identity)
{
    const int row = indexOfIdentity(identity);
    if (row < 0) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row);
    adjustCounts(m_entries.at(row), -1);
    m_entries.removeAt(row);
    m_thumbnails.remove(identity);
    m_thumbnailStates.remove(identity);
    endRemoveRows();
    emit countChanged();
    return true;
}

int FolderModel::indexOfIdentity(const QString &identity) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).identity == identity) {
            return i;
        }
    }
    return -1;
}

const Entry *FolderModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return nullptr;
    }
    return &m_entries.at(row);
}

QList<Entry> FolderModel::entries() const
{
    return m_entries;
}

void FolderModel::reorder(const SortSpec &spec)
{
    m_entries = SortService::apply(m_entries, spec);
    // Ordering changed only; contents, thumbnails and counts are unchanged.
    emit layoutChanged();
}

QString FolderModel::pathAt(int row) const
{
    const Entry *entry = entryAt(row);
    return entry != nullptr ? entry->path : QString();
}

QString FolderModel::nameAt(int row) const
{
    const Entry *entry = entryAt(row);
    return entry != nullptr ? entry->name : QString();
}

bool FolderModel::isFolderAt(int row) const
{
    const Entry *entry = entryAt(row);
    return entry != nullptr && entry->isFolder();
}

bool FolderModel::favoriteAt(int row) const
{
    const Entry *e = entryAt(row);
    return e != nullptr && e->favorite;
}

bool FolderModel::watchedAt(int row) const
{
    const Entry *e = entryAt(row);
    return e != nullptr && e->watched;
}

qint64 FolderModel::sizeAt(int row) const
{
    const Entry *entry = entryAt(row);
    return entry != nullptr ? entry->size : 0;
}

QString FolderModel::identityAt(int row) const
{
    const Entry *entry = entryAt(row);
    return entry != nullptr ? entry->identity : QString();
}

ThumbnailState FolderModel::thumbnailStateAt(int row) const
{
    const Entry *entry = entryAt(row);
    if (entry == nullptr) {
        return ThumbnailState::None;
    }
    // Live state set by updateThumbnail; falls back to the enumerated value.
    return m_thumbnailStates.value(entry->identity, entry->thumbnail);
}

void FolderModel::adjustCounts(const Entry &entry, int delta)
{
    if (entry.isFolder()) {
        m_folderCount += delta;
    } else {
        m_videoCount += delta;
    }
}

void FolderModel::recount()
{
    m_videoCount = 0;
    m_folderCount = 0;
    for (const Entry &entry : m_entries) {
        adjustCounts(entry, 1);
    }
}

QList<Entry> FolderModel::entriesMatching(const std::function<bool(const Entry &)> &predicate) const
{
    QList<Entry> result;
    for (const Entry &entry : m_entries) {
        if (predicate(entry)) {
            result.append(entry);
        }
    }
    return result;
}

} // namespace qg
