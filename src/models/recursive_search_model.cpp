#include "models/recursive_search_model.hpp"

RecursiveSearchModel::RecursiveSearchModel(QObject* parent) : QAbstractListModel(parent) {}

int RecursiveSearchModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_results.size();
}

QVariant RecursiveSearchModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size()) {
        return {};
    }
    const RecursiveSearchResult& r = m_results.at(index.row());
    switch (role) {
    case NameRole:             return r.filename;
    case PathRole:             return r.containingPath;
    case IsFolderRole:         return r.isFolder;
    case DurationRole:         return r.durationMs.has_value() ? QVariant(*r.durationMs) : QVariant();
    case SizeRole:             return r.sizeBytes.has_value() ? QVariant(*r.sizeBytes) : QVariant();
    case MtimeRole:            return r.mtimeNs.has_value() ? QVariant(*r.mtimeNs) : QVariant();
    case FavoriteRole:         return r.favorite;
    case WatchedRole:          return r.watched;
    case ProgressRole:         return r.progress.has_value() ? QVariant(*r.progress) : QVariant();
    case WidthRole:            return r.width.has_value() ? QVariant(*r.width) : QVariant();
    case HeightRole:           return r.height.has_value() ? QVariant(*r.height) : QVariant();
    case ThumbnailRole:        return QVariant(); // Thumbnails not loaded in recursive search yet
    case ThumbnailStateRole:   return r.thumbnailState;
    case IdentityRole:         return r.identity;
    default:                   return {};
    }
}

QHash<int, QByteArray> RecursiveSearchModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {IsFolderRole, "isFolder"},
        {DurationRole, "duration"},
        {SizeRole, "size"},
        {MtimeRole, "mtime"},
        {FavoriteRole, "favorite"},
        {WatchedRole, "watched"},
        {ProgressRole, "progress"},
        {WidthRole, "width"},
        {HeightRole, "height"},
        {ThumbnailRole, "thumbnail"},
        {ThumbnailStateRole, "thumbnailState"},
        {IdentityRole, "identity"},
    };
}

void RecursiveSearchModel::setResults(const QVector<RecursiveSearchResult>& results,
                                      const QString& query)
{
    beginResetModel();
    m_results = results;
    m_query = query;
    endResetModel();
}

void RecursiveSearchModel::appendResults(const QVector<RecursiveSearchResult>& batch)
{
    if (batch.isEmpty()) {
        return;
    }
    beginInsertRows(QModelIndex(), m_results.size(), m_results.size() + batch.size() - 1);
    m_results.append(batch);
    endInsertRows();
}

void RecursiveSearchModel::setQuery(const QString& query)
{
    if (m_query != query) {
        m_query = query;
    }
}

void RecursiveSearchModel::clear()
{
    beginResetModel();
    m_results.clear();
    m_query.clear();
    endResetModel();
}