// Reel / ordered-context integration test (REMEDIATION-007 / D-07, D-23, D-24).
//
// Drives the real PlaybackController against a real FolderModel and the single
// PlaybackManager (backed by a real mpv handle — no render context needed for a
// deferred load), asserting the acceptance criteria:
//   1. Prev/Next follow the displayed media order and the Reel matches/selects
//      the same entries.
//   2. Model changes (filter, remove, rename) cannot leave an invalid index or
//      crash playback.

#include <QtTest/QtTest>

#include "models/folder_model.hpp"
#include "models/reel_model.hpp"
#include "playback/mpv_backend.hpp"
#include "playback/playback_context.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"
#include "controllers/playback_controller.hpp"

namespace {

qg::Entry video(int ino, const QString& name, bool favorite = false, bool watched = false)
{
    qg::Entry e;
    e.identity = QStringLiteral("inode:1:%1").arg(ino);
    e.path = QStringLiteral("/videos/") + name;
    e.name = name;
    e.kind = qg::EntryKind::Video;
    e.size = 1000 * ino;
    e.durationMs = 60000;
    e.favorite = favorite;
    e.watched = watched;
    return e;
}

qg::Entry folder(const QString& name)
{
    qg::Entry e;
    e.identity = QStringLiteral("inode:2:%1").arg(name);
    e.path = QStringLiteral("/videos/") + name;
    e.name = name;
    e.kind = qg::EntryKind::Folder;
    return e;
}

void fill(qg::FolderModel& model, const QList<qg::Entry>& entries)
{
    qg::FolderSnapshot snap;
    snap.path = QStringLiteral("/videos");
    snap.entries = entries;
    snap.complete = true;
    model.applySnapshot(snap);
}

} // namespace

class ReelWiringTest : public QObject
{
    Q_OBJECT

private slots:
    void prevNextFollowDisplayedOrderWithReelSync();
    void filterConstrainsPrevNextAndReel();
    void removalNeverLeavesAStaleIndexOrCrashes();
    void renameUpdatesReelMinimally();

private:
    // identities used by the fixture (a.mp4 .. d.mp4)
    static QString idA() { return QStringLiteral("inode:1:101"); }
    static QString idB() { return QStringLiteral("inode:1:102"); }
    static QString idC() { return QStringLiteral("inode:1:103"); }
    static QString idD() { return QStringLiteral("inode:1:104"); }
};

void ReelWiringTest::prevNextFollowDisplayedOrderWithReelSync()
{
    qg::FolderModel model;
    fill(model, {folder(QStringLiteral("sub")),
                 video(101, QStringLiteral("a.mp4")),
                 video(102, QStringLiteral("b.mp4"), true),
                 video(103, QStringLiteral("c.mp4")),
                 video(104, QStringLiteral("d.mp4"), true)});

    MpvBackend backend;
    QVERIFY(backend.handle() != nullptr);
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setBackend(&backend);

    PlaybackContext ctx;
    ReelModel reel;
    PlaybackController controller;
    controller.setPlaybackManager(manager);
    controller.setPlaybackContext(&ctx);
    controller.setFolderModel(&model);
    controller.setReelModel(&reel);

    QSignalSpy endSpy(&controller, &PlaybackController::endOfFolder);

    // Open b.mp4 (the second video; folder excluded).
    controller.play(model.pathAt(2), idB());
    QCOMPARE(ctx.totalCount(), 4);
    QCOMPARE(ctx.currentIndex(), 1);
    QCOMPARE(controller.positionLabel(), QStringLiteral("2 / 4"));
    QCOMPARE(controller.canGoPrevious(), true);
    QCOMPARE(controller.canGoNext(), true);

    // The Reel mirrors the same ordered list and highlight.
    QCOMPARE(reel.count(), 4);
    QCOMPARE(reel.currentIndex(), 1);
    QCOMPARE(reel.data(reel.index(1, 0), ReelModel::NameRole).toString(), QStringLiteral("b.mp4"));
    QCOMPARE(reel.data(reel.index(1, 0), ReelModel::IsCurrentRole).toBool(), true);
    QCOMPARE(reel.data(reel.index(1, 0), ReelModel::DurationMsRole).toLongLong(), 60000);
    QCOMPARE(reel.data(reel.index(0, 0), ReelModel::NameRole).toString(), QStringLiteral("a.mp4"));
    QVERIFY(reel.identityAt(2) == idC());

    // Next → c.mp4, Reel re-anchors to the same entry.
    mpv_handle* handleBefore = backend.handle();
    controller.next();
    QCOMPARE(manager->session()->identity(), idC());
    QCOMPARE(ctx.currentIndex(), 2);
    QCOMPARE(reel.currentIndex(), 2);
    QCOMPARE(reel.data(reel.index(2, 0), ReelModel::IsCurrentRole).toBool(), true);
    QCOMPARE(controller.positionLabel(), QStringLiteral("3 / 4"));
    // Media switching reuses the single handle (DESIGN.md §19.4, PLAY-009).
    QCOMPARE(backend.handle(), handleBefore);

    // Next → d.mp4 (last).
    controller.next();
    QCOMPARE(manager->session()->identity(), idD());
    QCOMPARE(ctx.currentIndex(), 3);
    QCOMPARE(controller.canGoNext(), false);

    // Next at the end: no move, "end of folder" signalled.
    controller.next();
    QCOMPARE(ctx.currentIndex(), 3);
    QCOMPARE(endSpy.count(), 1);

    // Previous walks back in order.
    controller.previous();
    QCOMPARE(manager->session()->identity(), idC());
    QCOMPARE(ctx.currentIndex(), 2);

    // Selecting a Reel row plays exactly that entry (click a row).
    controller.selectReelRow(0);
    QCOMPARE(manager->session()->identity(), idA());
    QCOMPARE(ctx.currentIndex(), 0);
    QCOMPARE(reel.currentIndex(), 0);

    // Previous at the start is a boundary.
    controller.previous();
    QCOMPARE(endSpy.count(), 2);
    QCOMPARE(ctx.currentIndex(), 0);

    manager->setBackend(nullptr);
}

void ReelWiringTest::filterConstrainsPrevNextAndReel()
{
    qg::FolderModel model;
    fill(model, {video(101, QStringLiteral("a.mp4")),
                 video(102, QStringLiteral("b.mp4"), true),
                 video(103, QStringLiteral("c.mp4")),
                 video(104, QStringLiteral("d.mp4"), true)});

    MpvBackend backend;
    QVERIFY(backend.handle() != nullptr);
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setBackend(&backend);

    PlaybackContext ctx;
    ReelModel reel;
    PlaybackController controller;
    controller.setPlaybackManager(manager);
    controller.setPlaybackContext(&ctx);
    controller.setFolderModel(&model);
    controller.setReelModel(&reel);

    controller.play(model.pathAt(0), idA()); // a.mp4 — not a favorite
    QCOMPARE(ctx.totalCount(), 4);

    // Favorites filter: the list and prev/next shrink to b.mp4 + d.mp4.
    controller.setReelFilter(1);
    QCOMPARE(controller.reelFilter(), 1);
    QCOMPARE(ctx.totalCount(), 2);
    QCOMPARE(reel.count(), 2);
    QCOMPARE(reel.data(reel.index(0, 0), ReelModel::NameRole).toString(), QStringLiteral("b.mp4"));
    // a.mp4 is filtered out: no anchor, so Previous is a boundary and Next starts
    // the filtered list.
    QCOMPARE(ctx.currentIndex(), -1);
    QCOMPARE(reel.currentIndex(), -1);
    QCOMPARE(controller.canGoPrevious(), false);
    QCOMPARE(controller.canGoNext(), true);

    controller.next();
    QCOMPARE(manager->session()->identity(), idB());

    // Unwatched filter: everything (none marked watched) — same two favourites plus a/c.
    controller.setReelFilter(2);
    QCOMPARE(ctx.totalCount(), 4);

    // Back to All: the running item re-anchors by identity.
    controller.setReelFilter(0);
    QCOMPARE(ctx.totalCount(), 4);
    QCOMPARE(ctx.currentIndex(), ctx.indexOfIdentity(idB()));
    QCOMPARE(reel.currentIndex(), ctx.currentIndex());
    QCOMPARE(controller.positionLabel(), QStringLiteral("2 / 4"));

    manager->setBackend(nullptr);
}

void ReelWiringTest::removalNeverLeavesAStaleIndexOrCrashes()
{
    qg::FolderModel model;
    fill(model, {video(101, QStringLiteral("a.mp4")),
                 video(102, QStringLiteral("b.mp4")),
                 video(103, QStringLiteral("c.mp4"))});

    MpvBackend backend;
    QVERIFY(backend.handle() != nullptr);
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setBackend(&backend);

    PlaybackContext ctx;
    ReelModel reel;
    PlaybackController controller;
    controller.setPlaybackManager(manager);
    controller.setPlaybackContext(&ctx);
    controller.setFolderModel(&model);
    controller.setReelModel(&reel);

    controller.play(model.pathAt(1), idB());
    QCOMPARE(ctx.currentIndex(), 1);

    // Remove a NON-current video: the list shrinks, the cursor follows the same
    // identity, nothing crashes.
    QVERIFY(model.removeByIdentity(idA()));
    QCOMPARE(ctx.totalCount(), 2);
    QCOMPARE(reel.count(), 2);
    QCOMPARE(ctx.currentIndex(), 0);
    QCOMPARE(reel.currentIndex(), 0);
    QCOMPARE(reel.data(reel.index(0, 0), ReelModel::NameRole).toString(), QStringLiteral("b.mp4"));

    // Remove the CURRENT (playing) video: playback must stop, no stale index.
    QSignalSpy endedSpy(&controller, &PlaybackController::playbackEnded);
    QVERIFY(model.removeByIdentity(idB()));
    QCOMPARE(ctx.totalCount(), 1);
    QCOMPARE(ctx.currentIndex(), -1);
    QCOMPARE(reel.currentIndex(), -1);
    QCOMPARE(manager->session()->state(), PlaybackSession::Idle);
    QCOMPARE(endedSpy.count(), 1);

    // A full navigation-style reset is also safe.
    fill(model, {video(201, QStringLiteral("z.mp4"))});
    QCOMPARE(ctx.totalCount(), 1);
    QCOMPARE(ctx.currentIndex(), -1);
    QCOMPARE(reel.currentIndex(), -1);

    manager->setBackend(nullptr);
}

void ReelWiringTest::renameUpdatesReelMinimally()
{
    qg::FolderModel model;
    fill(model, {video(101, QStringLiteral("a.mp4")),
                 video(102, QStringLiteral("b.mp4")),
                 video(103, QStringLiteral("c.mp4"))});

    MpvBackend backend;
    QVERIFY(backend.handle() != nullptr);
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setBackend(&backend);

    PlaybackContext ctx;
    ReelModel reel;
    PlaybackController controller;
    controller.setPlaybackManager(manager);
    controller.setPlaybackContext(&ctx);
    controller.setFolderModel(&model);
    controller.setReelModel(&reel);

    controller.play(model.pathAt(0), idA());
    QCOMPARE(reel.data(reel.index(0, 0), ReelModel::NameRole).toString(), QStringLiteral("a.mp4"));

    // Rename a.mp4 in place (same identity, new name) — the diff path.
    qg::FolderDiff diff;
    qg::Entry renamed = video(101, QStringLiteral("renamed.mp4"));
    diff.changed.append(renamed);
    model.applyDiff(diff);

    QCOMPARE(ctx.totalCount(), 3);   // still one context entry per video
    QCOMPARE(reel.count(), 3);
    QCOMPARE(reel.data(reel.index(0, 0), ReelModel::NameRole).toString(),
             QStringLiteral("renamed.mp4"));
    QCOMPARE(reel.data(reel.index(0, 0), ReelModel::IdentityRole).toString(), idA());
    // The playing identity is unchanged by a rename.
    QCOMPARE(manager->session()->identity(), idA());

    manager->setBackend(nullptr);
}

QTEST_MAIN(ReelWiringTest)
#include "test_reel_wiring.moc"
