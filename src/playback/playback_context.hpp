#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include "core/types.hpp"

namespace qg {
class FolderModel;
}

// PlaybackContext: the ordered, videos-only list of the containing folder that
// Previous/Next traverse and that the Reel displays (DESIGN.md §10.2, §19.4,
// §20.1, §23.8). It is derived from the canonical `FolderModel` row order — the
// SAME source the grid shows — so the two can never disagree. Folders are never
// media; the active playback filter (All / Favorites / Unwatched) constrains the
// list. The current item is located by identity, never by path.

struct ContextEntry {
    QString identity;
    QString path;
    QString name;
    int modelRow = -1; // row in the canonical FolderModel
};

class PlaybackContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(int filter READ filterValue NOTIFY filterChanged)

public:
    enum Filter { All, Favorites, Unwatched };
    Q_ENUM(Filter)

    explicit PlaybackContext(QObject* parent = nullptr);

    // Build the ordered videos-only list from the model (already in the active
    // sort order), keep only rows passing `filter`, and locate `currentIdentity`.
    // A missing/empty identity leaves currentIndex() == -1 (no throw, no crash).
    Q_INVOKABLE void build(const qg::FolderModel* model,
                           const QString& currentIdentity,
                           Filter filter = All);

    Q_INVOKABLE QVector<ContextEntry> entries() const;

    int currentIndex() const;
    int totalCount() const;
    int filterValue() const { return static_cast<int>(filter_); }

    Q_INVOKABLE ContextEntry entryAt(int index) const;
    Q_INVOKABLE ContextEntry currentEntry() const;

    // Position of `identity` in the ordered list, or -1 when absent.
    Q_INVOKABLE int indexOfIdentity(const QString& identity) const;
    // Move the cursor to `identity`. Returns false (and leaves the cursor
    // unchanged) when the identity is not part of the current context.
    Q_INVOKABLE bool setCurrentByIdentity(const QString& identity);

    // Step by `delta` (±1). Returns false at either end and emits endReached()
    // (DESIGN.md §19.4, §23.8: stop at the ends, no wrap in MVP). With no current
    // item (index -1, e.g. the playing item was filtered out), +1 starts at the
    // first entry while -1 is still a boundary.
    Q_INVOKABLE bool step(int delta);
    Q_INVOKABLE bool canStep(int delta) const;
    bool canGoPrevious() const { return canStep(-1); }
    bool canGoNext() const { return canStep(1); }

    Q_INVOKABLE Filter filter() const { return filter_; }
    Q_INVOKABLE void setFilter(Filter f);

    // True when `entry` passes the given playback filter.
    static bool matchesFilter(const qg::Entry& entry, Filter filter);

signals:
    void currentIndexChanged();
    void totalCountChanged();
    void filterChanged();
    void endReached();

private:
    QVector<ContextEntry> entries_;
    int currentIndex_ = -1;
    Filter filter_ = All;
};
