#pragma once

// Canonical ordered entry model (DESIGN.md §14.2, §15.2, §20.1).
//
// `FolderModel` holds folders + videos in a single, ordered list and is the one
// authoritative entry source: the ContentView, the Reel and the PlaybackContext
// all derive from it. Rows are compact value structs; heavy per-row data
// (decoded thumbnails, probe results) is attached lazily by targeted role
// updates. Workers must never mutate the model directly — results are applied on
// the GUI thread only.

#include "core/types.hpp"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QVariant>

#include <functional>

namespace qg {

class FolderModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int videoCount READ videoCount NOTIFY countChanged)
    Q_PROPERTY(int folderCount READ folderCount NOTIFY countChanged)
    // The directory these rows belong to. Consumers need it to distinguish
    // "this folder changed" from "a different folder is now loaded" — without
    // it, navigation looks identical to the playing file disappearing
    // (AUD-007 / FIX-NAV-01).
    Q_PROPERTY(QString folderPath READ folderPath NOTIFY folderPathChanged)

public:
    enum Roles {
        IdentityRole = Qt::UserRole + 1,
        PathRole,
        NameRole,
        KindRole,
        IsFolderRole,
        IsVideoRole,
        ThumbnailRole,
        ThumbnailStateRole,
        DurationRole,
        SizeRole,
        MtimeRole,
        FavoriteRole,
        WatchedRole,
        ProgressRole,
        WidthRole,
        HeightRole,
    };
    Q_ENUM(Roles)

    explicit FolderModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replace all rows (model reset).
    void applySnapshot(const FolderSnapshot &snapshot);

    // Append one enumeration batch (minimal rowsInserted).
    void applyBatch(const FolderSnapshot &batch);

    // Apply an identity-based diff (external change): minimal rowsRemoved /
    // dataChanged / rowsInserted, preserving unaffected rows and ordering.
    void applyDiff(const FolderDiff &diff);

    void clear();

    // Targeted role updates (identity-keyed). No-op when unknown.
    void updateThumbnail(const QString &identity, const QVariant &image, ThumbnailState state);
    void updateProbe(const QString &identity,
                     const std::optional<qint64> &durationMs,
                     const std::optional<int> &width,
                     const std::optional<int> &height);
    void updateState(const QString &identity,
                     const std::optional<bool> &favorite,
                     const std::optional<bool> &watched,
                     const std::optional<double> &progress);

    // Targeted state mutators (identity-keyed, GUI thread only).
    void updateFavorite(const QString &identity, bool value);
    void updateWatched(const QString &identity, bool value); // sets/clears progress
    void updateProgress(const QString &identity, const std::optional<double> &progress);

    // Update path and name for an identity (rename); emits dataChanged for PathRole/NameRole.
    void updatePathAndName(const QString &identity, const QString &newPath, const QString &newName);

    bool removeByIdentity(const QString &identity);

    // Reverse lookup for identity-keyed callers (bulk verbs, the status strip's
    // aggregate size). Q_INVOKABLE because QML resolves a selected identity to a
    // row through it (AUD-004: without it the byte total silently stayed 0).
    Q_INVOKABLE int indexOfIdentity(const QString &identity) const;
    const Entry *entryAt(int row) const;
    QList<Entry> entries() const;
    int count() const { return static_cast<int>(m_entries.size()); }
    // Directory the current rows were enumerated from (empty before the first
    // snapshot). Not canonicalised — it is `QDir::cleanPath()`ed by the browser
    // before enumeration, and the playback layer compares it as-is.
    QString folderPath() const { return m_folderPath; }
    int videoCount() const noexcept { return m_videoCount; }
    int folderCount() const noexcept { return m_folderCount; }

    // Re-apply the canonical order (DESIGN.md §20.1): sorts the rows via
    // SortService and emits layoutChanged. Contents are unchanged.
    void reorder(const SortSpec &spec);

    // QML-facing row access (used by the view for keyboard activation).
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE bool isFolderAt(int row) const;
    // Persistent QG flags for the Properties surface (DESIGN §11.1). Identity-
    // keyed state, exposed read-only; false for out-of-range rows.
    Q_INVOKABLE bool favoriteAt(int row) const;
    Q_INVOKABLE bool watchedAt(int row) const;
    Q_INVOKABLE qint64 sizeAt(int row) const;
    // Canonical (st_dev, st_ino) identity fingerprint string of the row.
    // Empty for out-of-range rows. Never derived from the path (DESIGN §17.5).
    Q_INVOKABLE QString identityAt(int row) const;

    // Effective thumbnail lifecycle state of the row (Ready/Failed/Loading/None).
    // The thumbnail controller consults it to avoid re-requesting a row that is
    // already settled, so scrolling does not re-extract finished thumbnails.
    ThumbnailState thumbnailStateAt(int row) const;
    // Row byte size (0 when unknown) — used by the status strip's selection
    // aggregate; presentation reads it without owning selection state.

    // Projection hook for the Reel / PlaybackContext (videos only, in order).
    QList<Entry> entriesMatching(const std::function<bool(const Entry &)> &predicate) const;

signals:
    void countChanged();
    void folderPathChanged();

private:
    void recount();
    void adjustCounts(const Entry &entry, int delta);

    QString m_folderPath;
    QList<Entry> m_entries;
    QHash<QString, QVariant> m_thumbnails;
    QHash<QString, ThumbnailState> m_thumbnailStates;
    int m_videoCount = 0;
    int m_folderCount = 0;
};

} // namespace qg
