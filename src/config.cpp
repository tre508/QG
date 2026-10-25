#include "config.hpp"

#include <QDir>

namespace qg {
namespace {

// Resolve $HOME without QStandardPaths caching, so tests that override HOME
// observe the change immediately.
QString homePath()
{
    const QString home = qEnvironmentVariable("HOME");
    return home.isEmpty() ? QDir::homePath() : home;
}

// Return the value of `name` only when it is set and absolute; otherwise fall
// back to `homeRelative` under $HOME (XDG Base Directory specification).
QString xdgDir(const char *name, const QString &homeRelative)
{
    const QString value = qEnvironmentVariable(name);
    if (!value.isEmpty() && QDir::isAbsolutePath(value)) {
        return QDir::cleanPath(value);
    }
    return QDir::cleanPath(QDir(homePath()).filePath(homeRelative));
}

} // namespace

Config::Config(XdgBaseDirs dirs, QString appName)
    : m_dirs(std::move(dirs)), m_appName(std::move(appName))
{
    if (m_appName.isEmpty()) {
        m_appName = QStringLiteral("qg");
    }
}

XdgBaseDirs Config::resolveXdgBaseDirs()
{
    XdgBaseDirs dirs;
    dirs.dataHome = xdgDir("XDG_DATA_HOME", QStringLiteral(".local/share"));
    dirs.cacheHome = xdgDir("XDG_CACHE_HOME", QStringLiteral(".cache"));
    dirs.stateHome = xdgDir("XDG_STATE_HOME", QStringLiteral(".local/state"));
    dirs.configHome = xdgDir("XDG_CONFIG_HOME", QStringLiteral(".config"));
    return dirs;
}

Config Config::fromEnvironment(QString appName)
{
    return Config(resolveXdgBaseDirs(), std::move(appName));
}

QString Config::dataDir() const
{
    return QDir(m_dirs.dataHome).filePath(m_appName);
}

QString Config::cacheDir() const
{
    return QDir(m_dirs.cacheHome).filePath(m_appName);
}

QString Config::stateDir() const
{
    return QDir(m_dirs.stateHome).filePath(m_appName);
}

QString Config::configDir() const
{
    return QDir(m_dirs.configHome).filePath(m_appName);
}

QString Config::databasePath() const
{
    return QDir(dataDir()).filePath(QStringLiteral("qg.db"));
}

QString Config::logFilePath() const
{
    return QDir(stateDir()).filePath(QStringLiteral("qg.log"));
}

} // namespace qg
