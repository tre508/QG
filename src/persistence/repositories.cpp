#include "persistence/repositories.hpp"

#include <QDateTime>

namespace qg {
namespace {

qint64 nowSeconds()
{
    return QDateTime::currentSecsSinceEpoch();
}

QString sortKeyToken(SortKey key)
{
    switch (key) {
    case SortKey::Name:
        return QStringLiteral("name");
    case SortKey::Modified:
        return QStringLiteral("modified");
    case SortKey::Duration:
        return QStringLiteral("duration");
    case SortKey::Size:
        return QStringLiteral("size");
    case SortKey::Watched:
        return QStringLiteral("watched");
    case SortKey::Favorite:
        return QStringLiteral("favorite");
    }
    return QStringLiteral("name");
}

SortKey sortKeyFromToken(const QString &token)
{
    if (token == QStringLiteral("modified")) {
        return SortKey::Modified;
    }
    if (token == QStringLiteral("duration")) {
        return SortKey::Duration;
    }
    if (token == QStringLiteral("size")) {
        return SortKey::Size;
    }
    if (token == QStringLiteral("watched")) {
        return SortKey::Watched;
    }
    if (token == QStringLiteral("favorite")) {
        return SortKey::Favorite;
    }
    return SortKey::Name;
}

QString encodeSortSpec(const SortSpec &spec)
{
    return QStringLiteral("%1:%2:%3")
        .arg(sortKeyToken(spec.key),
             spec.direction == SortDirection::Descending ? QStringLiteral("desc")
                                                         : QStringLiteral("asc"))
        .arg(spec.foldersFirst ? 1 : 0);
}

SortSpec decodeSortSpec(const QString &value)
{
    SortSpec spec;
    const QStringList parts = value.split(QLatin1Char(':'));
    if (parts.size() == 3) {
        spec.key = sortKeyFromToken(parts.at(0));
        spec.direction = parts.at(1) == QStringLiteral("desc") ? SortDirection::Descending
                                                               : SortDirection::Ascending;
        spec.foldersFirst = parts.at(2) == QStringLiteral("1");
    }
    return spec;
}

} // namespace

// ---------------------------------------------------------------------------
// PathRepository
// ---------------------------------------------------------------------------

Result<void> PathRepository::upsert(const QString &path,
                                    const QString &identity,
                                    qint64 dev,
                                    qint64 ino,
                                    qint64 size,
                                    qint64 mtimeNs)
{
    Statement stmt(m_db,
                   QStringLiteral(
                       "INSERT INTO paths(path, identity, dev, ino, size, mtime_ns, seen_at) "
                       "VALUES(?, ?, ?, ?, ?, ?, ?) "
                       "ON CONFLICT(path) DO UPDATE SET identity=excluded.identity, "
                       "dev=excluded.dev, ino=excluded.ino, size=excluded.size, "
                       "mtime_ns=excluded.mtime_ns, seen_at=excluded.seen_at;"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), path);
    }
    stmt.bindText(1, path);
    stmt.bindText(2, identity);
    stmt.bindInt64(3, dev);
    stmt.bindInt64(4, ino);
    stmt.bindInt64(5, size);
    stmt.bindInt64(6, mtimeNs);
    stmt.bindInt64(7, nowSeconds());
    return stmt.exec();
}

QString PathRepository::identityForPath(const QString &path) const
{
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT identity FROM paths WHERE path = ? LIMIT 1;"));
    if (!stmt.isValid()) {
        return {};
    }
    stmt.bindText(1, path);
    if (stmt.step()) {
        return stmt.columnText(0);
    }
    return {};
}

bool PathRepository::pathInfo(const QString &path,
                              QString *identity,
                              qint64 *size,
                              qint64 *mtimeNs) const
{
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT identity, size, mtime_ns FROM paths "
                                  "WHERE path = ? LIMIT 1;"));
    if (!stmt.isValid()) {
        return false;
    }
    stmt.bindText(1, path);
    if (!stmt.step()) {
        return false;
    }
    if (identity != nullptr) {
        *identity = stmt.columnText(0);
    }
    if (size != nullptr) {
        *size = stmt.columnInt64(1);
    }
    if (mtimeNs != nullptr) {
        *mtimeNs = stmt.columnInt64(2);
    }
    return true;
}

QString PathRepository::pathForIdentity(const QString &identity) const
{
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT path FROM paths WHERE identity = ? "
                                  "ORDER BY seen_at DESC, rowid DESC LIMIT 1;"));
    if (!stmt.isValid()) {
        return {};
    }
    stmt.bindText(1, identity);
    return stmt.step() ? stmt.columnText(0) : QString();
}

Result<void> PathRepository::renamePath(const QString &oldPath,
                                        const QString &newPath,
                                        const QString &identity)
{
    QString oldIdentity = identity;
    qint64 size = 0;
    qint64 mtimeNs = 0;
    if (oldIdentity.isEmpty()) {
        pathInfo(oldPath, &oldIdentity, &size, &mtimeNs);
    } else {
        pathInfo(oldPath, nullptr, &size, &mtimeNs);
    }

    const Result<void> begun = m_db.execute(QStringLiteral("BEGIN IMMEDIATE;"));
    if (!begun) {
        return begun;
    }

    {
        Statement remove(m_db, QStringLiteral("DELETE FROM paths WHERE path = ?;"));
        if (!remove.isValid()) {
            m_db.execute(QStringLiteral("ROLLBACK;"));
            return Error::make(ErrorCode::OperationFailed, remove.error(), oldPath);
        }
        remove.bindText(1, oldPath);
        const Result<void> removed = remove.exec();
        if (!removed) {
            m_db.execute(QStringLiteral("ROLLBACK;"));
            return removed;
        }
    }

    {
        Statement insert(
            m_db,
            QStringLiteral("INSERT INTO paths(path, identity, dev, ino, size, mtime_ns, seen_at) "
                           "VALUES(?, ?, 0, 0, ?, ?, ?) "
                           "ON CONFLICT(path) DO UPDATE SET identity=excluded.identity, "
                           "size=excluded.size, mtime_ns=excluded.mtime_ns, seen_at=excluded.seen_at;"));
        if (!insert.isValid()) {
            m_db.execute(QStringLiteral("ROLLBACK;"));
            return Error::make(ErrorCode::OperationFailed, insert.error(), newPath);
        }
        insert.bindText(1, newPath);
        insert.bindText(2, oldIdentity);
        insert.bindInt64(3, size);
        insert.bindInt64(4, mtimeNs);
        insert.bindInt64(5, nowSeconds());
        const Result<void> inserted = insert.exec();
        if (!inserted) {
            m_db.execute(QStringLiteral("ROLLBACK;"));
            return inserted;
        }
    }

    return m_db.execute(QStringLiteral("COMMIT;"));
}

Result<void> PathRepository::reconcileSnapshot(const QList<Entry> &entries)
{
    const Result<void> begun = m_db.execute(QStringLiteral("BEGIN IMMEDIATE;"));
    if (!begun) {
        return begun;
    }

    for (const Entry &entry : entries) {
        if (entry.identity.isEmpty()) {
            continue;
        }
        Result<void> upserted =
            upsert(entry.path, entry.identity, 0, 0, entry.size, entry.mtimeNs);
        if (!upserted) {
            m_db.execute(QStringLiteral("ROLLBACK;"));
            return upserted;
        }
    }

    return m_db.execute(QStringLiteral("COMMIT;"));
}

Result<int> PathRepository::pruneVanished(qint64 graceSeconds)
{
    const qint64 cutoff = nowSeconds() - graceSeconds;

    qint64 count = 0;
    {
        Statement countStmt(const_cast<Database &>(m_db),
                            QStringLiteral("SELECT COUNT(*) FROM paths WHERE seen_at < ?;"));
        if (countStmt.isValid()) {
            countStmt.bindInt64(1, cutoff);
            if (countStmt.step()) {
                count = countStmt.columnInt64(0);
            }
        }
    }

    Statement remove(m_db, QStringLiteral("DELETE FROM paths WHERE seen_at < ?;"));
    if (!remove.isValid()) {
        return Error::make(ErrorCode::OperationFailed, remove.error());
    }
    remove.bindInt64(1, cutoff);
    const Result<void> removed = remove.exec();
    if (!removed) {
        return removed.error();
    }
    return static_cast<int>(count);
}

// ---------------------------------------------------------------------------
// MediaStateRepository
// ---------------------------------------------------------------------------

Result<void> MediaStateRepository::ensureRow(const QString &identity)
{
    Statement stmt(m_db,
                   QStringLiteral("INSERT OR IGNORE INTO media_state(identity, updated_at) "
                                  "VALUES(?, ?);"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), identity);
    }
    stmt.bindText(1, identity);
    stmt.bindInt64(2, nowSeconds());
    return stmt.exec();
}

bool MediaStateRepository::favorite(const QString &identity) const
{
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT favorite FROM media_state WHERE identity = ? LIMIT 1;"));
    if (!stmt.isValid()) {
        return false;
    }
    stmt.bindText(1, identity);
    return stmt.step() && stmt.columnInt64(0) != 0;
}

bool MediaStateRepository::watched(const QString &identity) const
{
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT watched FROM media_state WHERE identity = ? LIMIT 1;"));
    if (!stmt.isValid()) {
        return false;
    }
    stmt.bindText(1, identity);
    return stmt.step() && stmt.columnInt64(0) != 0;
}

qint64 MediaStateRepository::positionMs(const QString &identity) const
{
    Statement stmt(
        const_cast<Database &>(m_db),
        QStringLiteral("SELECT last_position_ms FROM media_state WHERE identity = ? LIMIT 1;"));
    if (!stmt.isValid()) {
        return 0;
    }
    stmt.bindText(1, identity);
    return stmt.step() ? stmt.columnInt64(0) : 0;
}

Result<void> MediaStateRepository::setFavorite(const QString &identity, bool value)
{
    Result<void> ensured = ensureRow(identity);
    if (!ensured) {
        return ensured;
    }
    Statement stmt(m_db,
                   QStringLiteral("UPDATE media_state SET favorite = ?, updated_at = ? "
                                  "WHERE identity = ?;"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), identity);
    }
    stmt.bindInt64(1, value ? 1 : 0);
    stmt.bindInt64(2, nowSeconds());
    stmt.bindText(3, identity);
    return stmt.exec();
}

Result<void> MediaStateRepository::setWatched(const QString &identity, bool value)
{
    Result<void> ensured = ensureRow(identity);
    if (!ensured) {
        return ensured;
    }
    Statement stmt(m_db,
                   QStringLiteral("UPDATE media_state SET watched = ?, updated_at = ? "
                                  "WHERE identity = ?;"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), identity);
    }
    stmt.bindInt64(1, value ? 1 : 0);
    stmt.bindInt64(2, nowSeconds());
    stmt.bindText(3, identity);
    return stmt.exec();
}

Result<void> MediaStateRepository::recordPosition(const QString &identity, qint64 positionMs)
{
    Result<void> ensured = ensureRow(identity);
    if (!ensured) {
        return ensured;
    }
    Statement stmt(m_db,
                   QStringLiteral("UPDATE media_state SET last_position_ms = ?, updated_at = ? "
                                  "WHERE identity = ?;"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), identity);
    }
    stmt.bindInt64(1, positionMs);
    stmt.bindInt64(2, nowSeconds());
    stmt.bindText(3, identity);
    return stmt.exec();
}

Result<void> MediaStateRepository::setProbeResult(const QString &identity,
                                                  qint64 durationMs,
                                                  int width,
                                                  int height)
{
    Result<void> ensured = ensureRow(identity);
    if (!ensured) {
        return ensured;
    }
    Statement stmt(m_db,
                   QStringLiteral("UPDATE media_state SET duration_ms = ?, width = ?, height = ?, "
                                  "updated_at = ? WHERE identity = ?;"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), identity);
    }
    stmt.bindInt64(1, durationMs);
    stmt.bindInt64(2, width);
    stmt.bindInt64(3, height);
    stmt.bindInt64(4, nowSeconds());
    stmt.bindText(5, identity);
    return stmt.exec();
}

bool MediaStateRepository::probeResult(const QString &identity,
                                       qint64 *durationMs,
                                       int *width,
                                       int *height) const
{
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT duration_ms, width, height FROM media_state "
                                  "WHERE identity = ? LIMIT 1;"));
    if (!stmt.isValid()) {
        return false;
    }
    stmt.bindText(1, identity);
    if (!stmt.step() || stmt.columnIsNull(0)) {
        return false;
    }
    if (durationMs != nullptr) {
        *durationMs = stmt.columnInt64(0);
    }
    if (width != nullptr) {
        *width = static_cast<int>(stmt.columnInt64(1));
    }
    if (height != nullptr) {
        *height = static_cast<int>(stmt.columnInt64(2));
    }
    return true;
}

// ---------------------------------------------------------------------------
// ThumbnailRepository
// ---------------------------------------------------------------------------

Result<void> ThumbnailRepository::upsertOk(const QString &identity,
                                           qint64 sourceMtimeNs,
                                           qint64 sourceSize,
                                           const QString &cachePath)
{
    Statement stmt(m_db,
                   QStringLiteral("INSERT OR REPLACE INTO thumbnails(identity, source_mtime_ns, "
                                  "source_size, cache_path, thumb_ms, state, fail_count, "
                                  "generated_at) VALUES(?, ?, ?, ?, NULL, 'ok', 0, ?);"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), identity);
    }
    stmt.bindText(1, identity);
    stmt.bindInt64(2, sourceMtimeNs);
    stmt.bindInt64(3, sourceSize);
    stmt.bindText(4, cachePath);
    stmt.bindInt64(5, nowSeconds());
    return stmt.exec();
}

Result<void> ThumbnailRepository::upsertFailed(const QString &identity,
                                               qint64 sourceMtimeNs,
                                               qint64 sourceSize)
{
    Statement stmt(m_db,
                   QStringLiteral(
                       "INSERT INTO thumbnails(identity, source_mtime_ns, source_size, "
                       "cache_path, state, fail_count, generated_at) "
                       "VALUES(?, ?, ?, NULL, 'failed', 1, ?) "
                       "ON CONFLICT(identity) DO UPDATE SET "
                       "source_mtime_ns=excluded.source_mtime_ns, source_size=excluded.source_size, "
                       "cache_path=NULL, state='failed', "
                       "fail_count=thumbnails.fail_count+1, generated_at=excluded.generated_at;"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), identity);
    }
    stmt.bindText(1, identity);
    stmt.bindInt64(2, sourceMtimeNs);
    stmt.bindInt64(3, sourceSize);
    stmt.bindInt64(4, nowSeconds());
    return stmt.exec();
}

bool ThumbnailRepository::lookup(const QString &identity, Record *out) const
{
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT cache_path, state, fail_count, source_mtime_ns, "
                                  "source_size FROM thumbnails WHERE identity = ? LIMIT 1;"));
    if (!stmt.isValid()) {
        return false;
    }
    stmt.bindText(1, identity);
    if (!stmt.step()) {
        return false;
    }
    if (out != nullptr) {
        out->cachePath = stmt.columnIsNull(0) ? QString() : stmt.columnText(0);
        out->state = stmt.columnText(1);
        out->failCount = static_cast<int>(stmt.columnInt64(2));
        out->sourceMtimeNs = stmt.columnInt64(3);
        out->sourceSize = stmt.columnInt64(4);
    }
    return true;
}

// ---------------------------------------------------------------------------
// RecentRepository
// ---------------------------------------------------------------------------

Result<void> RecentRepository::remember(const QString &path)
{
    Statement stmt(m_db,
                   QStringLiteral("INSERT OR REPLACE INTO recent_locations(path, visited_at) "
                                  "VALUES(?, ?);"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), path);
    }
    stmt.bindText(1, path);
    stmt.bindInt64(2, nowSeconds());
    return stmt.exec();
}

QList<QString> RecentRepository::recent(int limit) const
{
    QList<QString> result;
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT path FROM recent_locations "
                                  "ORDER BY visited_at DESC, rowid DESC LIMIT ?;"));
    if (!stmt.isValid()) {
        return result;
    }
    stmt.bindInt64(1, limit);
    while (stmt.step()) {
        result.append(stmt.columnText(0));
    }
    return result;
}

// ---------------------------------------------------------------------------
// BookmarkRepository
// ---------------------------------------------------------------------------

Result<void> BookmarkRepository::add(const QString &path, const QString &label)
{
    Statement stmt(m_db,
                   QStringLiteral(
                       "INSERT INTO bookmarks(path, label, position, kind) "
                       "VALUES(?, ?, COALESCE((SELECT MAX(position) + 1 FROM bookmarks), 0), 'folder') "
                       "ON CONFLICT(path) DO UPDATE SET label=excluded.label;"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), path);
    }
    stmt.bindText(1, path);
    stmt.bindText(2, label);
    return stmt.exec();
}

Result<void> BookmarkRepository::remove(const QString &path)
{
    Statement stmt(m_db, QStringLiteral("DELETE FROM bookmarks WHERE path = ?;"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), path);
    }
    stmt.bindText(1, path);
    return stmt.exec();
}

QList<QString> BookmarkRepository::paths() const
{
    QList<QString> result;
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT path FROM bookmarks ORDER BY position;"));
    if (!stmt.isValid()) {
        return result;
    }
    while (stmt.step()) {
        result.append(stmt.columnText(0));
    }
    return result;
}

// ---------------------------------------------------------------------------
// SettingsRepository
// ---------------------------------------------------------------------------

Result<void> SettingsRepository::setValue(const QString &key, const QString &value)
{
    Statement stmt(m_db,
                   QStringLiteral("INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?);"));
    if (!stmt.isValid()) {
        return Error::make(ErrorCode::OperationFailed, stmt.error(), key);
    }
    stmt.bindText(1, key);
    stmt.bindText(2, value);
    return stmt.exec();
}

QString SettingsRepository::value(const QString &key) const
{
    Statement stmt(const_cast<Database &>(m_db),
                   QStringLiteral("SELECT value FROM settings WHERE key = ? LIMIT 1;"));
    if (!stmt.isValid()) {
        return {};
    }
    stmt.bindText(1, key);
    return stmt.step() ? stmt.columnText(0) : QString();
}

Result<void> SettingsRepository::setSortSpec(const QString &folderPath, const SortSpec &spec)
{
    return setValue(QStringLiteral("sort:") + folderPath, encodeSortSpec(spec));
}

SortSpec SettingsRepository::sortSpec(const QString &folderPath) const
{
    const QString encoded = value(QStringLiteral("sort:") + folderPath);
    return encoded.isEmpty() ? SortSpec{} : decodeSortSpec(encoded);
}

namespace {

QString densityToken(ViewDensity density)
{
    switch (density) {
    case ViewDensity::Gallery:
        return QStringLiteral("gallery");
    case ViewDensity::Compact:
        return QStringLiteral("compact");
    case ViewDensity::Standard:
        break;
    }
    return QStringLiteral("standard");
}

ViewDensity densityFromToken(const QString &token)
{
    if (token == QStringLiteral("gallery")) {
        return ViewDensity::Gallery;
    }
    if (token == QStringLiteral("compact")) {
        return ViewDensity::Compact;
    }
    return ViewDensity::Standard;
}

} // namespace

Result<void> SettingsRepository::setDensity(const QString &folderPath, ViewDensity density)
{
    return setValue(QStringLiteral("density:") + folderPath, densityToken(density));
}

ViewDensity SettingsRepository::density(const QString &folderPath) const
{
    const QString encoded = value(QStringLiteral("density:") + folderPath);
    return encoded.isEmpty() ? ViewDensity::Standard : densityFromToken(encoded);
}

} // namespace qg
