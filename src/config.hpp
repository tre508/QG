#pragma once

// QG configuration: XDG base-directory resolution and derived application
// locations (DESIGN.md 15.1, 16, 23.1).
//
// The filesystem is authoritative for media; this type only locates QG's own
// data/cache/state/config directories. Resolution is a pure function of the
// process environment so it can be unit-tested with XDG_* overrides.

#include <QString>

namespace qg {

// The four XDG base directories QG cares about, already resolved to absolute
// paths (with spec defaults applied when the environment does not override).
struct XdgBaseDirs {
    QString dataHome;   // $XDG_DATA_HOME   or $HOME/.local/share
    QString cacheHome;  // $XDG_CACHE_HOME  or $HOME/.cache
    QString stateHome;  // $XDG_STATE_HOME  or $HOME/.local/state
    QString configHome; // $XDG_CONFIG_HOME or $HOME/.config
};

class Config {
public:
    Config() = default;
    explicit Config(XdgBaseDirs dirs, QString appName = QStringLiteral("qg"));

    // Reads XDG_* from the process environment. A set-but-relative XDG_* value
    // is ignored per the XDG Base Directory specification and the spec default
    // is used instead.
    static XdgBaseDirs resolveXdgBaseDirs();

    // Convenience: resolveXdgBaseDirs() wrapped in a Config.
    static Config fromEnvironment(QString appName = QStringLiteral("qg"));

    const QString &appName() const { return m_appName; }
    const XdgBaseDirs &baseDirs() const { return m_dirs; }

    QString dataDir() const;       // <dataHome>/<app>
    QString cacheDir() const;      // <cacheHome>/<app>
    QString stateDir() const;      // <stateHome>/<app>
    QString configDir() const;     // <configHome>/<app>
    QString databasePath() const;  // <dataDir>/qg.db
    QString logFilePath() const;   // <stateDir>/qg.log

private:
    XdgBaseDirs m_dirs;
    QString m_appName{QStringLiteral("qg")};
};

} // namespace qg
