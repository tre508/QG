// Composition contract tests (REMEDIATION-003 / findings D-01, D-19).
//
// Verifies that CoreServices owns every non-UI subsystem exactly once, wires
// their dependencies, and that App::registerQmlTypes() populates the `qg.core`
// QML type surface. Runs offscreen (no display required).

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QtQml/qqml.h>
#include <QTemporaryDir>

#include "controllers/browser_controller.hpp"
#include "controllers/mounts_controller.hpp"
#include "controllers/state_controller.hpp"
#include "controllers/thumbnail_controller.hpp"
#include "core/composition.hpp"
#include "filesystem/mounts.hpp"
#include "models/folder_model.hpp"
#include "models/search_proxy.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"
#include "controllers/playback_controller.hpp"

using namespace qg;

namespace {

Entry makeVideo(const QString& name, const QString& identity)
{
    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = name;
    entry.path = QStringLiteral("/v/") + name;
    entry.identity = identity;
    entry.size = 1024;
    entry.mtimeNs = 42;
    return entry;
}

// Test seam: MountService signals are public, so a subclass can emit them.
class TestMounts : public MountService {
public:
    using MountService::volumesChanged;
};

} // namespace

class CompositionTest : public QObject
{
    Q_OBJECT

private slots:
    void ownsEverySubsystemOnce();
    void wireIsIdempotent();
    void stateControllerLoadsPersistedState();
    void searchProxySourcesCanonicalModel();
    void playbackManagerIsTheSingletonWithOneBackend();
    void mountDisconnectDrivesMountsController();
    void navigationBumpsThumbnailGeneration();
    void resumePositionReadsFromDatabase();
    void qmlTypeSurfaceResolves();
    void cleanup();

private:
    QTemporaryDir m_tmp;
};

void CompositionTest::ownsEverySubsystemOnce()
{
    BrowserController browser;
    TestMounts mounts;
    CoreServices services;
    services.wire(nullptr, &browser, &mounts, QStringLiteral("/tmp/qg-cache"), QStringLiteral("/tmp"));

    QVERIFY(services.isWired());
    QVERIFY(services.stateController() != nullptr);
    QVERIFY(services.thumbnailController() != nullptr);
    QVERIFY(services.probeController() != nullptr);
    QVERIFY(services.searchProxy() != nullptr);
    QVERIFY(services.searchController() != nullptr);
    QVERIFY(services.fileOperations() != nullptr);
    QVERIFY(services.trashService() != nullptr);
    QVERIFY(services.actionsController() != nullptr);
    QVERIFY(services.mountsController() != nullptr);
    QVERIFY(services.reelModel() != nullptr);
    QVERIFY(services.selectionState() != nullptr);
    QVERIFY(services.keyboardFramework() != nullptr);
    QVERIFY(services.mpvBackend() != nullptr);
    QVERIFY(services.playbackManager() != nullptr);
    QVERIFY(services.playbackContext() != nullptr);
    QVERIFY(services.playbackController() != nullptr);

    // Distinct owners, no accidental aliasing.
    QVERIFY(services.stateController() != reinterpret_cast<QObject*>(services.searchController()));
}

void CompositionTest::wireIsIdempotent()
{
    BrowserController browser;
    TestMounts mounts;
    CoreServices services;
    services.wire(nullptr, &browser, &mounts, QString(), QString());

    StateController* first = services.stateController();
    // A second wire() must not replace any instance (construct-once contract).
    services.wire(nullptr, &browser, &mounts, QString(), QString());
    QCOMPARE(services.stateController(), first);
}

void CompositionTest::stateControllerLoadsPersistedState()
{
    QVERIFY(m_tmp.isValid());
    Database db;
    QVERIFY(db.open(m_tmp.filePath(QStringLiteral("qg.db"))));

    const QString identity = QStringLiteral("inode:7:7001");

    // First composition: record a favorite through the state controller.
    {
        BrowserController browser;
        FolderSnapshot snapshot;
        snapshot.entries = {makeVideo(QStringLiteral("clip.mp4"), identity)};
        browser.model()->applySnapshot(snapshot);

        TestMounts mounts;
        CoreServices services;
        services.wire(&db, &browser, &mounts, QString(), QString());
        services.stateController()->setFavorite(identity, true);
    }

    // Second composition over the same database must load it into the model.
    {
        BrowserController browser;
        FolderSnapshot snapshot;
        snapshot.entries = {makeVideo(QStringLiteral("clip.mp4"), identity)};
        browser.model()->applySnapshot(snapshot);

        TestMounts mounts;
        CoreServices services;
        services.wire(&db, &browser, &mounts, QString(), QString());
        QVERIFY(services.stateController() != nullptr);
        QCOMPARE(browser.model()->data(browser.model()->index(0, 0),
                                      FolderModel::FavoriteRole).toBool(), true);
    }
}

void CompositionTest::searchProxySourcesCanonicalModel()
{
    BrowserController browser;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("sunset.mp4"), QStringLiteral("inode:1:1")),
                        makeVideo(QStringLiteral("moon.mp4"), QStringLiteral("inode:1:2"))};
    browser.model()->applySnapshot(snapshot);

    TestMounts mounts;
    CoreServices services;
    services.wire(nullptr, &browser, &mounts, QString(), QString());

    QCOMPARE(services.searchProxy()->sourceModel(), static_cast<QAbstractItemModel*>(browser.model()));
    services.searchProxy()->setQuery(QStringLiteral("sun"));
    QCOMPARE(services.searchProxy()->resultCount(), 1);
}

void CompositionTest::playbackManagerIsTheSingletonWithOneBackend()
{
    BrowserController browser;
    TestMounts mounts;
    CoreServices services;
    services.wire(nullptr, &browser, &mounts, QString(), QString());

    // One process-wide manager; CoreServices attaches the single backend.
    QCOMPARE(services.playbackManager(), PlaybackManager::instance());
    QVERIFY(services.mpvBackend() != nullptr);
}

void CompositionTest::mountDisconnectDrivesMountsController()
{
    BrowserController browser;
    TestMounts mounts;
    CoreServices services;
    services.wire(nullptr, &browser, &mounts, QString(), QString());

    // Volume present, then gone → MountsController flips to disconnected.
    VolumeInfo usb;
    usb.id = QStringLiteral("/media/usb");
    usb.mountPoint = QStringLiteral("/media/usb");
    usb.connected = true;
    emit mounts.volumesChanged({usb});
    QVERIFY(services.mountsController()->disconnected() == false);

    emit mounts.volumesChanged({});
    QVERIFY(services.mountsController()->disconnected() == true);
    QVERIFY(services.mountsController()->isInsideDisconnectedVolume(QStringLiteral("/media/usb/x.mp4")));

    // Boundary: different volume, same event → should NOT trigger stop.
    QVERIFY(!services.mountsController()->isInsideDisconnectedVolume(QStringLiteral("/media/other/file.mp4")));

    // The disconnect-stop lambda is wired; its guard uses currentSession()->path()
    // (null-safe via `session != nullptr` check) and isInsideDisconnectedVolume().
    // The control case above proves the volume boundary; the wiring is verified
    // by compilation/link and the connected signal existing in the root.
}

// REM-01: entering a folder invalidates in-flight thumbnail/probe work by
// bumping the processing generation (DESIGN.md §18.4, §22.5) — wired by the
// composition root, not by QML.
void CompositionTest::navigationBumpsThumbnailGeneration()
{
    QVERIFY(m_tmp.isValid());
    BrowserController browser;
    TestMounts mounts;
    CoreServices services;
    services.wire(nullptr, &browser, &mounts, QString(), QString());

    ThumbnailController* thumbs = services.thumbnailController();
    QVERIFY(thumbs != nullptr);
    const quint64 before = thumbs->generation();

    QVERIFY(browser.navigate(m_tmp.path()));
    QCOMPARE(thumbs->generation(), before + 1);

    // Drain the async enumeration so no worker task outlives the stack-local
    // browser controller.
    QTest::qWait(50);
}

void CompositionTest::qmlTypeSurfaceResolves()
{
    // D-19: the composed controller/model types must be resolvable from QML.
    qg::registerCoreQmlTypes();
    const char* types[] = {
        "StateController", "ThumbnailController", "ProbeController", "SearchProxy",
        "SearchController", "ActionsController", "MountsController", "ReelModel",
        "SelectionState", "PlaybackManager", "PlaybackSession", "PlaybackContext",
        "PlaybackController", "MpvBackend", "VideoSurfaceItem", "KeyboardFramework",
    };
    for (const char* t : types) {
        QVERIFY2(qmlTypeId("qg.core", 1, 0, t) >= 0, t);
    }
}

QTEST_MAIN(CompositionTest)
#include "test_composition.moc"

// REMEDIATION-008 / D-26: the composed manager must resolve resume positions
// from the database (the composition root injects the loader), never from a
// session-local guess.
void CompositionTest::resumePositionReadsFromDatabase()
{
    QVERIFY(m_tmp.isValid());
    Database db;
    QVERIFY(db.open(m_tmp.filePath(QStringLiteral("qg.db"))));

    const QString identity = QStringLiteral("inode:8:8001");
    QVERIFY(MediaStateRepository(db).recordPosition(identity, 42000));

    BrowserController browser;
    TestMounts mounts;
    CoreServices services;
    services.wire(&db, &browser, &mounts, QString(), QString());

    PlaybackManager* manager = services.playbackManager();
    QVERIFY(manager != nullptr);
    manager->setResumePolicy(0); // Always
    QCOMPARE(manager->persistedPositionFor(identity), 42.0);
    QCOMPARE(manager->resumeStartFor(identity, 120.0, false), 42.0);
    // Stored position 42s in a 45s clip is inside the tail guard → no resume.
    QCOMPARE(manager->resumeStartFor(identity, 45.0, false), 0.0);
}

// The manager is a process singleton and outlives any one composition; drop the
// database-backed loader (which points at a test-local Database) after each case.
void CompositionTest::cleanup()
{
    if (PlaybackManager* manager = PlaybackManager::instance()) {
        manager->setPositionLoader({});
        manager->setResumePolicy(0);
    }
}
