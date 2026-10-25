// XDG base-directory resolution tests (Prompt 02).
//
// Config::resolveXdgBaseDirs() is a pure function of the process environment,
// so these tests override XDG_* / HOME with temporary directories and assert
// the resolved locations, including the "relative XDG values are ignored" rule
// from the XDG Base Directory specification.

#include <QtTest/QtTest>

#include <QDir>
#include <QTemporaryDir>

#include "config.hpp"

class XdgPathsTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void honorsEnvironmentOverrides();
    void ignoresRelativeXdgValues();
    void derivesAppDirectories();

private:
    struct SavedEnv {
        QByteArray name;
        QByteArray value;
        bool wasSet = false;
    };

    void saveEnv(const char *name)
    {
        SavedEnv saved;
        saved.name = QByteArray(name);
        saved.wasSet = qEnvironmentVariableIsSet(name);
        if (saved.wasSet) {
            saved.value = qgetenv(name);
        }
        m_saved.append(saved);
    }

    void restoreEnv()
    {
        for (const SavedEnv &saved : std::as_const(m_saved)) {
            if (saved.wasSet) {
                qputenv(saved.name.constData(), saved.value);
            } else {
                qunsetenv(saved.name.constData());
            }
        }
        m_saved.clear();
    }

    QList<SavedEnv> m_saved;
};

void XdgPathsTest::init()
{
    for (const char *name : {"HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME",
                             "XDG_CONFIG_HOME"}) {
        saveEnv(name);
    }
}

void XdgPathsTest::cleanup()
{
    restoreEnv();
}

void XdgPathsTest::honorsEnvironmentOverrides()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QDir root(tmp.path());
    qputenv("XDG_DATA_HOME", root.filePath("data").toUtf8());
    qputenv("XDG_CACHE_HOME", root.filePath("cache").toUtf8());
    qputenv("XDG_STATE_HOME", root.filePath("state").toUtf8());
    qputenv("XDG_CONFIG_HOME", root.filePath("config").toUtf8());

    const qg::Config config = qg::Config::fromEnvironment();

    QCOMPARE(config.dataDir(), root.filePath("data/qg"));
    QCOMPARE(config.cacheDir(), root.filePath("cache/qg"));
    QCOMPARE(config.stateDir(), root.filePath("state/qg"));
    QCOMPARE(config.configDir(), root.filePath("config/qg"));
}

void XdgPathsTest::ignoresRelativeXdgValues()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QDir home(tmp.path());

    qputenv("HOME", home.absolutePath().toUtf8());
    // Relative XDG values must be ignored per the spec.
    qputenv("XDG_DATA_HOME", QByteArrayLiteral("relative/data"));

    const qg::Config config = qg::Config::fromEnvironment();

    QCOMPARE(config.dataDir(), home.filePath(".local/share/qg"));
    QCOMPARE(config.cacheDir(), home.filePath(".cache/qg"));
    QCOMPARE(config.stateDir(), home.filePath(".local/state/qg"));
    QCOMPARE(config.configDir(), home.filePath(".config/qg"));
}

void XdgPathsTest::derivesAppDirectories()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QDir root(tmp.path());

    qputenv("XDG_DATA_HOME", root.filePath("data").toUtf8());
    qputenv("XDG_STATE_HOME", root.filePath("state").toUtf8());

    const qg::Config config = qg::Config::fromEnvironment();

    QCOMPARE(config.databasePath(), root.filePath("data/qg/qg.db"));
    QCOMPARE(config.logFilePath(), root.filePath("state/qg/qg.log"));
}

QTEST_APPLESS_MAIN(XdgPathsTest)
#include "test_xdg_paths.moc"
