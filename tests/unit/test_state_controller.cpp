// StateController tests (Prompt 30).

#include <QtTest/QtTest>

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "controllers/state_controller.hpp"
#include "models/folder_model.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"

using namespace qg;

namespace {

Entry makeVideo(const QString &identity, qint64 durationMs)
{
    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = QStringLiteral("clip.mp4");
    entry.path = QStringLiteral("/v/clip.mp4");
    entry.identity = identity;
    entry.size = 100;
    entry.mtimeNs = 200;
    entry.durationMs = durationMs;
    return entry;
}

} // namespace

class StateControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void favoriteTogglePersists();
    void watchedSetsAndClearsProgress();
    void recordPositionUpdatesProgress();
    void loadIntoModelAppliesPersistedState();
    void renamePathUpdatesMapping();
};

void StateControllerTest::favoriteTogglePersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = QDir(dir.path()).filePath(QStringLiteral("qg.db"));
    Database db;
    QVERIFY(db.open(dbPath));

    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("id"), 10000)};
    model.applySnapshot(snapshot);

    StateController controller;
    controller.setDatabase(&db);
    controller.setModel(&model);

    QSignalSpy changed(&controller, &StateController::favoriteChanged);
    QVERIFY(controller.toggleFavorite(QStringLiteral("id")));
    QVERIFY(model.data(model.index(0, 0), FolderModel::FavoriteRole).toBool());
    QCOMPARE(changed.count(), 1);

    {
        MediaStateRepository repository(db);
        QVERIFY(repository.favorite(QStringLiteral("id")));
    }

    // Reopen: persisted.
    db.close();
    QVERIFY(db.open(dbPath));
    {
        MediaStateRepository repository(db);
        QVERIFY(repository.favorite(QStringLiteral("id")));
    }
}

void StateControllerTest::watchedSetsAndClearsProgress()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dir.path()).filePath(QStringLiteral("qg.db"))));

    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("id"), 10000)};
    model.applySnapshot(snapshot);

    StateController controller;
    controller.setDatabase(&db);
    controller.setModel(&model);

    controller.setWatched(QStringLiteral("id"), true);
    QVERIFY(model.data(model.index(0, 0), FolderModel::WatchedRole).toBool());
    QCOMPARE(model.data(model.index(0, 0), FolderModel::ProgressRole).toDouble(), 1.0);

    controller.setWatched(QStringLiteral("id"), false);
    QVERIFY(!model.data(model.index(0, 0), FolderModel::WatchedRole).toBool());
    QVERIFY(!model.data(model.index(0, 0), FolderModel::ProgressRole).isValid());
}

void StateControllerTest::recordPositionUpdatesProgress()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dir.path()).filePath(QStringLiteral("qg.db"))));

    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("id"), 10000)};
    model.applySnapshot(snapshot);

    StateController controller;
    controller.setDatabase(&db);
    controller.setModel(&model);

    controller.recordPosition(QStringLiteral("id"), 5000);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::ProgressRole).toDouble(), 0.5);

    MediaStateRepository repository(db);
    QCOMPARE(repository.positionMs(QStringLiteral("id")), 5000);
}

void StateControllerTest::loadIntoModelAppliesPersistedState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dir.path()).filePath(QStringLiteral("qg.db"))));
    {
        MediaStateRepository repository(db);
        QVERIFY(repository.setFavorite(QStringLiteral("id"), true));
        QVERIFY(repository.setWatched(QStringLiteral("id"), true));
    }

    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("id"), 10000)};
    model.applySnapshot(snapshot);

    StateController controller;
    controller.setDatabase(&db);
    controller.setModel(&model);
    controller.loadIntoModel();

    QVERIFY(model.data(model.index(0, 0), FolderModel::FavoriteRole).toBool());
    QVERIFY(model.data(model.index(0, 0), FolderModel::WatchedRole).toBool());
}

void StateControllerTest::renamePathUpdatesMapping()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dir.path()).filePath(QStringLiteral("qg.db"))));

    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("id"), 10000)};
    model.applySnapshot(snapshot);

    StateController controller;
    controller.setDatabase(&db);
    controller.setModel(&model);

    // Set up initial path mapping
    PathRepository paths(db);
    QVERIFY(paths.upsert(QStringLiteral("/v/old.mp4"), QStringLiteral("id"), 1, 2, 100, 200));

    // Call renamePath
    controller.renamePath(QStringLiteral("/v/old.mp4"), QStringLiteral("/v/new.mp4"), QStringLiteral("id"));

    // Verify the mapping was updated
    QCOMPARE(paths.identityForPath(QStringLiteral("/v/new.mp4")), QStringLiteral("id"));
    QVERIFY(paths.identityForPath(QStringLiteral("/v/old.mp4")).isEmpty());
    // media_state (keyed by identity) should be untouched
    MediaStateRepository media(db);
    QVERIFY(!media.favorite(QStringLiteral("id"))); // default false
    QVERIFY(!media.watched(QStringLiteral("id")));

    // No-op when database is null or identity is empty
    StateController controller2;
    controller2.setDatabase(nullptr);
    controller2.setModel(&model);
    controller2.renamePath(QStringLiteral("/a/b.mp4"), QStringLiteral("/a/c.mp4"), QStringLiteral("id")); // should not crash
    controller2.renamePath(QStringLiteral("/a/b.mp4"), QStringLiteral("/a/c.mp4"), QStringLiteral("")); // should not crash
}

QTEST_GUILESS_MAIN(StateControllerTest)
#include "test_state_controller.moc"
