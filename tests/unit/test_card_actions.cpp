// Card action wiring tests (REMEDIATION-009 / D-04, D-25).
//
// The card signals must reach the composed controllers and the state must be
// real: favorite/watched write SQLite and are visible immediately in the model,
// and survive a "restart" (fresh model + fresh controller on the same database).
// Rename reaches the filesystem; the thumbnail retry re-extracts for one item.

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include "controllers/actions_controller.hpp"
#include "controllers/state_controller.hpp"
#include "controllers/thumbnail_controller.hpp"
#include "filesystem/operations.hpp"
#include "media/thumbnails.hpp"
#include "models/folder_model.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"

namespace {

const QString kIdentity = QStringLiteral("inode:5:202");

qg::Entry makeVideo(const QString& path, qint64 size = 4096, qint64 mtimeNs = 1700000000000000000LL)
{
    qg::Entry e;
    e.identity = kIdentity;
    e.path = path;
    e.name = QFileInfo(path).fileName();
    e.kind = qg::EntryKind::Video;
    e.size = size;
    e.mtimeNs = mtimeNs;
    e.durationMs = 60000;
    return e;
}

void fill(qg::FolderModel& model, const qg::Entry& entry)
{
    qg::FolderSnapshot snap;
    snap.path = QFileInfo(entry.path).absolutePath();
    snap.entries = {entry};
    snap.complete = true;
    model.applySnapshot(snap);
}

QString dbPath(const QTemporaryDir& dir)
{
    return QDir(dir.path()).filePath(QStringLiteral("qg.db"));
}

} // namespace

class CardActionsTest : public QObject
{
    Q_OBJECT

private slots:
    void favoriteAndWatchedPersistAcrossRestart();
    void watchedSetsAndClearsProgress();
    void renameReachesTheFileSystem();
    void thumbnailRetryReExtracts();
};

void CardActionsTest::favoriteAndWatchedPersistAcrossRestart()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("clip.mp4"));
    QFile(path).open(QIODevice::WriteOnly);

    qg::Database db;
    QVERIFY(db.open(dbPath(dir)));
    qg::MediaStateRepository media(db);

    // --- First run: toggle through the controller (what Main.qml calls) ------
    {
        qg::FolderModel model;
        fill(model, makeVideo(path));
        qg::StateController state;
        state.setDatabase(&db);
        state.setModel(&model);
        state.loadIntoModel();

        QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::FavoriteRole).toBool(), false);
        QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::WatchedRole).toBool(), false);

        QCOMPARE(state.toggleFavorite(kIdentity), true);
        QCOMPARE(state.toggleWatched(kIdentity), true);

        // Visible immediately: the model roles the card binds to.
        QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::FavoriteRole).toBool(), true);
        QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::WatchedRole).toBool(), true);
    }

    // --- Persisted: the database holds the state ----------------------------
    QCOMPARE(media.favorite(kIdentity), true);
    QCOMPARE(media.watched(kIdentity), true);

    // --- Restart: a fresh model + controller on the same database -----------
    {
        qg::FolderModel model;
        fill(model, makeVideo(path));
        qg::StateController state;
        state.setDatabase(&db);
        state.setModel(&model);
        state.loadIntoModel();   // what App does on startup

        QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::FavoriteRole).toBool(), true);
        QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::WatchedRole).toBool(), true);

        // Toggling back clears both the model and the database.
        QCOMPARE(state.toggleFavorite(kIdentity), false);
        QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::FavoriteRole).toBool(), false);
    }
    QCOMPARE(media.favorite(kIdentity), false);
}

void CardActionsTest::watchedSetsAndClearsProgress()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("clip.mp4"));
    QFile(path).open(QIODevice::WriteOnly);

    qg::Database db;
    QVERIFY(db.open(dbPath(dir)));

    qg::FolderModel model;
    fill(model, makeVideo(path));
    qg::StateController state;
    state.setDatabase(&db);
    state.setModel(&model);

    // Marking watched sets progress to 100% (DESIGN.md §19.6).
    state.setWatched(kIdentity, true);
    QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::WatchedRole).toBool(), true);
    QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::ProgressRole).toDouble(), 1.0);

    // Marking unwatched clears both.
    state.setWatched(kIdentity, false);
    QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::WatchedRole).toBool(), false);
    QVERIFY(!model.data(model.index(0, 0), qg::FolderModel::ProgressRole).isValid());
    QCOMPARE(qg::MediaStateRepository(db).watched(kIdentity), false);
}

void CardActionsTest::renameReachesTheFileSystem()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString original = dir.filePath(QStringLiteral("clip.mp4"));
    {
        QFile f(original);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("data");
    }

    FileOperationsService ops;
    ActionsController actions;
    actions.setFileOperations(&ops);

    QSignalSpy requested(&actions, &ActionsController::renameRequested);
    actions.rename(original, QStringLiteral("renamed"));

    QCOMPARE(requested.count(), 1);
    QCOMPARE(requested.at(0).at(0).toString(), original);
    QCOMPARE(requested.at(0).at(1).toString(), QStringLiteral("renamed"));

    // The rename actually happened, with the extension preserved.
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("renamed.mp4"))));
    QVERIFY(!QFile::exists(original));
}

void CardActionsTest::thumbnailRetryReExtracts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("clip.mp4"));
    QFile(path).open(QIODevice::WriteOnly);

    qg::FolderModel model;
    fill(model, makeVideo(path));

    qg::ThumbnailController controller;
    controller.setModel(&model);
    controller.setImmediateProcessing(true);

    int calls = 0;
    controller.setExtractor([&calls](const qg::ThumbnailRequest&) {
        ++calls;
        qg::ThumbnailExtraction result;
        if (calls >= 2) {
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(Qt::blue);
            result.image = image;
        }
        return result; // first call fails (null image)
    });

    QSignalSpy failedSpy(&controller, &qg::ThumbnailController::thumbnailFailed);
    QSignalSpy readySpy(&controller, &qg::ThumbnailController::thumbnailReady);

    // First attempt fails → the card shows the failure marker.
    controller.requestVisible(0, 0);
    QCOMPARE(calls, 1);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(qg::ThumbnailState::Failed));

    // Retry (the card's Retry control) clears the marker and re-extracts.
    controller.retry(kIdentity);
    QCOMPARE(calls, 2);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), kIdentity);
    QCOMPARE(model.data(model.index(0, 0), qg::FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(qg::ThumbnailState::Ready));
    QVERIFY(model.data(model.index(0, 0), qg::FolderModel::ThumbnailRole).isValid());

    // Unknown identity is a safe no-op.
    const int before = calls;
    controller.retry(QStringLiteral("ghost"));
    QCOMPARE(calls, before);
}

QTEST_MAIN(CardActionsTest)
#include "test_card_actions.moc"
