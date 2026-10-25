#pragma once

// Intention-revealing SQLite repositories (DESIGN.md §21.1, §21.3).
//
// Callers never see SQL. Each repository wraps one `Database` connection and
// uses short, single-row statements. Repositories are GUI-thread-agnostic; the
// caller decides which thread/connection owns them.

#include "core/errors.hpp"
#include "core/types.hpp"
#include "persistence/database.hpp"

#include <QList>
#include <QString>

namespace qg {

// Known paths <-> stable identities (rename-safe state).
class PathRepository {
public:
    explicit PathRepository(Database &db) : m_db(db) {}

    Result<void> upsert(const QString &path,
                        const QString &identity,
                        qint64 dev,
                        qint64 ino,
                        qint64 size,
                        qint64 mtimeNs);

    // Empty when the path is unknown.
    QString identityForPath(const QString &path) const;

    // Full stored record for a path (used to validate cached probe/thumbnail
    // results against the current source). Returns false when unknown.
    bool pathInfo(const QString &path,
                  QString *identity,
                  qint64 *size,
                  qint64 *mtimeNs) const;

    // Reverse lookup: the most recent path recorded for an identity.
    QString pathForIdentity(const QString &identity) const;

    // Rename-safe mapping update in a single transaction (media_state, keyed by
    // identity, is untouched).
    Result<void> renamePath(const QString &oldPath,
                            const QString &newPath,
                            const QString &identity);

    // Reconcile a fresh enumeration: records/refreshes path<->identity and
    // seen_at for every entry. Vanished identities are retained (never deleted
    // here) so a reconnect restores state.
    Result<void> reconcileSnapshot(const QList<Entry> &entries);

    // Remove path mappings not seen within the grace period (retention cleanup).
    Result<int> pruneVanished(qint64 graceSeconds);

private:
    Database &m_db;
};

// Application metadata keyed by identity.
class MediaStateRepository {
public:
    explicit MediaStateRepository(Database &db) : m_db(db) {}

    bool favorite(const QString &identity) const;
    bool watched(const QString &identity) const;
    qint64 positionMs(const QString &identity) const;

    Result<void> setFavorite(const QString &identity, bool value);
    Result<void> setWatched(const QString &identity, bool value);
    Result<void> recordPosition(const QString &identity, qint64 positionMs);
    Result<void> setProbeResult(const QString &identity,
                                qint64 durationMs,
                                int width,
                                int height);

    // True and populated when a duration has been probed for this identity.
    bool probeResult(const QString &identity,
                     qint64 *durationMs,
                     int *width,
                     int *height) const;

private:
    Result<void> ensureRow(const QString &identity);

    Database &m_db;
};

// Thumbnail references and failure markers.
class ThumbnailRepository {
public:
    explicit ThumbnailRepository(Database &db) : m_db(db) {}

    struct Record {
        QString cachePath;
        QString state; // "ok" | "failed"
        int failCount = 0;
        qint64 sourceMtimeNs = 0;
        qint64 sourceSize = 0;
    };

    Result<void> upsertOk(const QString &identity,
                          qint64 sourceMtimeNs,
                          qint64 sourceSize,
                          const QString &cachePath);
    Result<void> upsertFailed(const QString &identity,
                              qint64 sourceMtimeNs,
                              qint64 sourceSize);
    bool lookup(const QString &identity, Record *out) const;

private:
    Database &m_db;
};

// Recent locations (paths, capped by the application).
class RecentRepository {
public:
    explicit RecentRepository(Database &db) : m_db(db) {}

    Result<void> remember(const QString &path);
    QList<QString> recent(int limit) const;

private:
    Database &m_db;
};

class BookmarkRepository {
public:
    explicit BookmarkRepository(Database &db) : m_db(db) {}

    Result<void> add(const QString &path, const QString &label);
    Result<void> remove(const QString &path);
    QList<QString> paths() const;

private:
    Database &m_db;
};

// Free-form key/value settings, including per-folder sort specs.
class SettingsRepository {
public:
    explicit SettingsRepository(Database &db) : m_db(db) {}

    Result<void> setValue(const QString &key, const QString &value);
    QString value(const QString &key) const;

    Result<void> setSortSpec(const QString &folderPath, const SortSpec &spec);
    SortSpec sortSpec(const QString &folderPath) const;

    // Per-folder view density (§5.4): default Standard for unseen folders.
    Result<void> setDensity(const QString &folderPath, ViewDensity density);
    ViewDensity density(const QString &folderPath) const;

private:
    Database &m_db;
};

} // namespace qg
