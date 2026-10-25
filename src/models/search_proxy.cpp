#include "search_proxy.hpp"
#include "models/folder_model.hpp"
#include <QAbstractItemModel>
#include <QVariant>

SearchProxy::SearchProxy(QObject* parent) : QSortFilterProxyModel(parent) {}

void SearchProxy::setQuery(const QString& query) {
    if (query_ != query) {
        query_ = query;
        invalidateFilter();
        // Force the filter to evaluate so the emitted count is accurate
        // immediately (QSortFilterProxyModel filters lazily otherwise).
        (void)rowCount();
        emit resultCountChanged();
    }
}

QString SearchProxy::query() const { return query_; }
int SearchProxy::resultCount() const { return rowCount(); }

QString SearchProxy::showingNote() const {
    const int n = rowCount();
    if (n >= kRowCap)
        return QStringLiteral("showing first 2,000");
    return QStringLiteral("%1 results").arg(n);
}

QVariant SearchProxy::data(const QModelIndex& proxyIndex, int role) const {
    if (!proxyIndex.isValid() || !sourceModel())
        return {};

    const QModelIndex src = mapToSource(proxyIndex);

    switch (role) {
    case MatchRangesRole: {
        if (query_.isEmpty())
            return QVariant::fromValue(QVector<MatchRange>{});
        const QString filename = sourceModel()->data(src, qg::FolderModel::NameRole).toString();
        const QVector<SearchResult> hits = searchService_.search(
            SearchSpec{query_, false}, {{filename, false}});
        return QVariant::fromValue(hits.isEmpty() ? QVector<MatchRange>{}
                                                  : hits.first().matchRanges);
    }
    case EntryFileNameRole:
        return sourceModel()->data(src, qg::FolderModel::NameRole);
    case EntryDurationRole:
        return sourceModel()->data(src, qg::FolderModel::DurationRole);
    case EntrySizeRole:
        return sourceModel()->data(src, qg::FolderModel::SizeRole);
    case EntryModifiedRole:
        return sourceModel()->data(src, qg::FolderModel::MtimeRole);
    default:
        return QSortFilterProxyModel::data(proxyIndex, role);
    }
}

bool SearchProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (query_.isEmpty()) return true;
    if (sourceRow >= kRowCap) return false; // cap enforced

    // Delegate to the source model's filename role
    QAbstractItemModel* src = sourceModel();
    if (!src) return false;

    const auto idx = src->index(sourceRow, 0, sourceParent);
    const QString filename = src->data(idx, qg::FolderModel::NameRole).toString();
    const bool isFolder = src->data(idx, qg::FolderModel::IsFolderRole).toBool();

    const QVector<QPair<QString, bool>> single = {{filename, isFolder}};
    return !searchService_.search(SearchSpec{query_, false}, single).isEmpty();
}