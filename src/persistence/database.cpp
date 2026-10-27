#include "persistence/database.hpp"

#include "persistence/schema_sql.hpp"

#include <QDateTime>
#include <QFile>

#include <sqlite3.h>

#include <vector>

namespace qg {
namespace {

// Numbered migrations. Each entry is idempotent and applied transactionally.
// Migration 1 is the full MVP schema compiled from schema.sql.
struct Migration {
    int version;
    const char *sql;
};

const std::vector<Migration> &migrations()
{
    static const std::vector<Migration> kMigrations = {
        {1, persistence::kSchemaSql},
    };
    return kMigrations;
}

Error sqliteError(sqlite3 *db, const QString &path, const QString &context)
{
    return Error::make(ErrorCode::IoError,
                       context + QStringLiteral(": ")
                           + QString::fromUtf8(db != nullptr ? sqlite3_errmsg(db) : "no handle"),
                       path,
                       db != nullptr ? sqlite3_errcode(db) : 0);
}

qint64 nowSeconds()
{
    return QDateTime::currentSecsSinceEpoch();
}

} // namespace

Database::~Database()
{
    close();
}

Result<void> Database::open(const QString &path)
{
    if (isOpen()) {
        close();
    }

    m_path = path;
    m_recovered = false;
    m_quarantinedPath.clear();

    const bool existed = QFile::exists(path);

    Result<void> opened = openHandle();
    if (!opened) {
        return opened;
    }

    // Integrity check on a pre-existing database. A failure preserves the old
    // file beside the new one (never deletes it) and starts fresh.
    if (existed) {
        const Result<bool> check = quickCheck();
        if (!check || !check.value()) {
            close();

            const QString quarantine =
                path + QStringLiteral(".corrupt-%1").arg(nowSeconds());
            if (QFile::exists(path)) {
                QFile::rename(path, quarantine);
            }
            QFile::remove(path + QStringLiteral("-wal"));
            QFile::remove(path + QStringLiteral("-shm"));
            m_recovered = true;
            m_quarantinedPath = quarantine;

            opened = openHandle();
            if (!opened) {
                return opened;
            }
        }
    }

    Result<void> result = applyPragmas();
    if (!result) {
        return result;
    }
    return runMigrations();
}

void Database::close()
{
    if (m_db != nullptr) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

Result<void> Database::openHandle()
{
    sqlite3 *db = nullptr;
    const int rc = sqlite3_open_v2(m_path.toUtf8().constData(),
                                   &db,
                                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                   nullptr);
    if (rc != SQLITE_OK) {
        const Error error = sqliteError(db, m_path, QStringLiteral("Cannot open database"));
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return error;
    }

    sqlite3_busy_timeout(db, 5000);
    m_db = db;
    return Result<void>::success();
}

Result<void> Database::applyPragmas()
{
    for (const char *pragma : {"PRAGMA journal_mode=WAL;",
                               "PRAGMA synchronous=NORMAL;",
                               "PRAGMA foreign_keys=ON;"}) {
        const Result<void> result = execute(QString::fromLatin1(pragma));
        if (!result) {
            return result;
        }
    }
    return Result<void>::success();
}

Result<bool> Database::quickCheck()
{
    if (m_db == nullptr) {
        return false;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "PRAGMA quick_check;", -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    bool ok = false;
    const int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const auto *text = sqlite3_column_text(stmt, 0);
        ok = text != nullptr
             && QString::fromUtf8(reinterpret_cast<const char *>(text)) == QStringLiteral("ok");
    }
    sqlite3_finalize(stmt);
    return ok;
}

int Database::readSchemaVersion()
{
    if (m_db == nullptr) {
        return 0;
    }

    Statement stmt(*this,
                   QStringLiteral("SELECT version FROM schema_version LIMIT 1"));
    if (!stmt.isValid() || !stmt.step()) {
        return 0;
    }
    return static_cast<int>(stmt.columnInt64(0));
}

Result<void> Database::runMigrations()
{
    const Result<void> ensured =
        execute(QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);"));
    if (!ensured) {
        return ensured;
    }

    const int current = readSchemaVersion();
    for (const Migration &migration : migrations()) {
        if (migration.version <= current) {
            continue;
        }

        if (execute(QStringLiteral("BEGIN IMMEDIATE;")) ) {
            // fallthrough
        } else {
            return Error::make(ErrorCode::OperationFailed,
                               QStringLiteral("Cannot begin migration transaction"),
                               m_path);
        }

        Result<void> applied = execute(QString::fromUtf8(migration.sql));
        if (applied) {
            applied = execute(
                QStringLiteral("INSERT OR REPLACE INTO schema_version(version) VALUES(%1);")
                    .arg(migration.version));
        }

        if (!applied) {
            execute(QStringLiteral("ROLLBACK;"));
            return applied;
        }

        const Result<void> committed = execute(QStringLiteral("COMMIT;"));
        if (!committed) {
            return committed;
        }
    }

    return Result<void>::success();
}

int Database::schemaVersion() const
{
    // const-cast is acceptable: reading uses the same thread-affine handle.
    return const_cast<Database *>(this)->readSchemaVersion();
}

Result<void> Database::execute(const QString &sql)
{
    if (m_db == nullptr) {
        return Error::make(ErrorCode::OperationFailed,
                           QStringLiteral("Database is not open"),
                           m_path);
    }

    char *errorMessage = nullptr;
    const int rc = sqlite3_exec(m_db, sql.toUtf8().constData(), nullptr, nullptr, &errorMessage);
    if (rc != SQLITE_OK) {
        const QString detail = errorMessage != nullptr
                                   ? QString::fromUtf8(errorMessage)
                                   : QString::fromUtf8(sqlite3_errmsg(m_db));
        sqlite3_free(errorMessage);
        return Error::make(ErrorCode::IoError, detail, m_path, rc);
    }
    return Result<void>::success();
}

// ---------------------------------------------------------------------------
// Statement
// ---------------------------------------------------------------------------

Statement::Statement(Database &db, const QString &sql)
{
    if (db.handle() == nullptr) {
        m_error = QStringLiteral("Database is not open");
        return;
    }
    const int rc = sqlite3_prepare_v2(db.handle(),
                                      sql.toUtf8().constData(),
                                      -1,
                                      &m_stmt,
                                      nullptr);
    if (rc != SQLITE_OK) {
        m_error = QString::fromUtf8(sqlite3_errmsg(db.handle()));
        m_stmt = nullptr;
    }
}

Statement::~Statement()
{
    if (m_stmt != nullptr) {
        sqlite3_finalize(m_stmt);
    }
}

void Statement::bindText(int index, const QString &value)
{
    if (m_stmt != nullptr) {
        const QByteArray utf8 = value.toUtf8();
        sqlite3_bind_text(m_stmt, index, utf8.constData(), utf8.size(), SQLITE_TRANSIENT);
    }
}

void Statement::bindInt64(int index, qint64 value)
{
    if (m_stmt != nullptr) {
        sqlite3_bind_int64(m_stmt, index, value);
    }
}

bool Statement::step()
{
    if (m_stmt == nullptr) {
        return false;
    }
    const int rc = sqlite3_step(m_stmt);
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    m_error = QString::fromUtf8(sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
    return false;
}

Result<void> Statement::exec()
{
    if (m_stmt == nullptr) {
        return Error::make(ErrorCode::OperationFailed,
                           QStringLiteral("Statement is not prepared"));
    }
    const int rc = sqlite3_step(m_stmt);
    if (rc == SQLITE_DONE || rc == SQLITE_ROW) {
        return Result<void>::success();
    }
    m_error = QString::fromUtf8(sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
    return Error::make(ErrorCode::OperationFailed, m_error, {}, rc);
}

bool Statement::columnIsNull(int index) const
{
    return m_stmt == nullptr || sqlite3_column_type(m_stmt, index) == SQLITE_NULL;
}

QString Statement::columnText(int index) const
{
    if (m_stmt == nullptr) {
        return {};
    }
    const auto *text = sqlite3_column_text(m_stmt, index);
    return text != nullptr ? QString::fromUtf8(reinterpret_cast<const char *>(text)) : QString();
}

qint64 Statement::columnInt64(int index) const
{
    if (m_stmt == nullptr) {
        return 0;
    }
    return static_cast<qint64>(sqlite3_column_int64(m_stmt, index));
}

} // namespace qg
