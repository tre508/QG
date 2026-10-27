#pragma once

// RAII SQLite access for QG (DESIGN.md §21.1, §21.5).
//
// One `Database` is one connection and is thread-affine: open a connection per
// thread, never share a connection across threads. The primary connection is
// opened in WAL mode so GUI-thread reads never block on worker writes.
//
// `Database` owns the handle and applies PRAGMAs, numbered migrations and a
// startup `PRAGMA quick_check`. A database that fails the integrity check is
// never destroyed: it is preserved beside the live file as
// `<path>.corrupt-<timestamp>` and a fresh database is created.

#include "core/errors.hpp"

#include <QString>

#include <cstdint>

struct sqlite3;
struct sqlite3_stmt;

namespace qg {

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    Database(Database &&) = delete;
    Database &operator=(Database &&) = delete;

    // Open (creating if needed) `path`; apply PRAGMAs, run migrations, and
    // verify integrity. On corruption the old file is quarantined and a fresh
    // database is created (the old file is never deleted).
    Result<void> open(const QString &path);
    void close();

    bool isOpen() const noexcept { return m_db != nullptr; }
    sqlite3 *handle() const noexcept { return m_db; }
    const QString &path() const noexcept { return m_path; }

    // True when the previous on-disk database was quarantined on this open.
    bool recoveredCorruptDatabase() const noexcept { return m_recovered; }
    const QString &quarantinedPath() const noexcept { return m_quarantinedPath; }

    // Current applied schema version (0 when uninitialised).
    int schemaVersion() const;

    // Execute a statement with no result rows. Internal to persistence.
    Result<void> execute(const QString &sql);

private:
    Result<void> openHandle();
    Result<void> applyPragmas();
    Result<void> runMigrations();
    Result<bool> quickCheck();
    int readSchemaVersion();

    sqlite3 *m_db = nullptr;
    QString m_path;
    bool m_recovered = false;
    QString m_quarantinedPath;
};

// Small RAII prepared-statement wrapper used by the repositories. It never
// throws; callers check validity via isValid()/step() results.
class Statement {
public:
    Statement(Database &db, const QString &sql);
    ~Statement();

    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;

    bool isValid() const noexcept { return m_stmt != nullptr; }

    void bindText(int index, const QString &value);
    void bindInt64(int index, qint64 value);

    // Advances one row; returns true when a row is available.
    bool step();
    // Execute a single INSERT/UPDATE/DELETE and report SQLite errors.
    Result<void> exec();

    bool columnIsNull(int index) const;
    QString columnText(int index) const;
    qint64 columnInt64(int index) const;

    const QString &error() const noexcept { return m_error; }

private:
    sqlite3_stmt *m_stmt = nullptr;
    QString m_error;
};

} // namespace qg
