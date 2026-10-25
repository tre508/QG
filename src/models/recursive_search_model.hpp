#pragma once

// RecursiveSearchModel (DESIGN.md §20.4, SEARCH-08): a flat result list for
// recursive ("Subfolders") hits. These rows have no qg::FolderModel row, so the
// search proxy's modelRow/mapToSource semantics do NOT apply here. Exposes the
// minimal roles the Manifest delegate needs (name / path / isFolder); the
// delegate guards the metadata columns it reads on other models.

#include <QAbstractListModel>
#include <QString>
#include <QVector>

#include "search/recursive_search_service.hpp"

class RecursiveSearchModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IsFolderRole,
        DurationRole,
        SizeRole,
        MtimeRole,
        FavoriteRole,
        WatchedRole,
        ProgressRole,
        WidthRole,
        HeightRole,
        ThumbnailRole,
        ThumbnailStateRole,
        IdentityRole,
    };

    explicit RecursiveSearchModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replace the result set (model reset); tracking the query is presentation
    // bookkeeping so summaries can count recursive hits.
    void setResults(const QVector<RecursiveSearchResult>& results, const QString& query);
    // Append an incremental batch (streaming walker) with a targeted rowsInserted.
    void appendResults(const QVector<RecursiveSearchResult>& batch);
    void setQuery(const QString& query);
    void clear();

    QString query() const { return m_query; }

private:
    QVector<RecursiveSearchResult> m_results;
    QString m_query;
};