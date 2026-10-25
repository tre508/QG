#pragma once
#include <QSortFilterProxyModel>
#include <QString>
#include "search/search_service.hpp"

namespace qg {
class FolderModel;
}

// SearchProxy: QSortFilterProxyModel wrapping FolderModel, driven by a
// SearchService instance. Exposes match-range roles for the Manifest delegate.
class SearchProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    enum Role {
        MatchRangesRole = Qt::UserRole + 200, // QVector<MatchRange>
        EntryFileNameRole,                     // for ManifestRow display
        EntryDurationRole,
        EntrySizeRole,
        EntryModifiedRole,
    };
    Q_ENUM(Role)

    explicit SearchProxy(QObject* parent = nullptr);

    Q_INVOKABLE void setQuery(const QString& query);
    Q_INVOKABLE QString query() const;
    // Live result count (bindable from QML: `searchProxy.resultCount`).
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)
    int resultCount() const;
    Q_INVOKABLE QString showingNote() const; // "showing first 2,000" when capped

    QVariant data(const QModelIndex& proxyIndex, int role) const override;

    static constexpr int kRowCap = 2000;

signals:
    void resultCountChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString query_;
    SearchService searchService_;
};