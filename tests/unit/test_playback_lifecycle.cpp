// Playback one-handle + open/close/reopen test (REMEDIATION-005 / D-06, D-20).
//
// Verifies the single-handle policy and the open/stop/reopen session lifecycle
// without requiring a live OpenGL render pass (mpv loads/decodes with
// vo=libmpv; frame presentation is verified manually — see GUIDE_STATE).

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QProcess>
#include <QTemporaryDir>

#include "playback/mpv_backend.hpp"
#include "controllers/playback_controller.hpp"
#include "models/folder_model.hpp"
#include "playback/playback_context.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"

namespace {

// Generates a tiny playable fixture with ffmpeg; empty path if unavailable.
QString makeSample(const QString& dir,
                   const QString& name = QStringLiteral("sample.mp4"))
{
    const QString out = dir + QLatin1Char('/') + name;
    QProcess proc;
    proc.start(QStringLiteral("ffmpeg"),
               {QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"),
                QStringLiteral("-i"), QStringLiteral("testsrc=duration=1:size=160x120:rate=10"),
                QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), out});
    if (!proc.waitForStarted(3000) || !proc.waitForFinished(15000))
        return {};
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 ? out : QString();
}

qg::Entry videoEntry(int ino, const QString& path, const QString& name)
{
    qg::Entry e;
    e.identity = QStringLiteral("inode:5:%1").arg(ino);
    e.path = path;
    e.name = name;
    e.kind = qg::EntryKind::Video;
    e.size = 4096;
    e.durationMs = 60000; // long enough that resume never lands at the end
    return e;
}

} // namespace

class PlaybackLifecycleTest : public QObject
{
    Q_OBJECT

private slots:
    void singleHandleIsReused();
    void openStopReopenKeepsOneHandle();
    void controllerPlayDrivesSession();
    void cinemaSurvivesAMediaChange();
};

void PlaybackLifecycleTest::singleHandleIsReused()
{
    MpvBackend backend;
    mpv_handle* first = backend.handle();
    QVERIFY(first != nullptr);
    QCOMPARE(backend.handle(), first); // lazy, created once
    QVERIFY(backend.initialized());
}

void PlaybackLifecycleTest::openStopReopenKeepsOneHandle()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString sample = makeSample(tmp.path());
    if (sample.isEmpty())
        QSKIP("ffmpeg fixture unavailable");

    MpvBackend backend;
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setBackend(&backend);
    QCOMPARE(manager->backend(), &backend);

    mpv_handle* handle = backend.handle();
    QVERIFY(handle != nullptr);

    // Open → session reflects the media.
    manager->openWithIdentity(sample, QStringLiteral("inode:1:1"));
    PlaybackSession* session = manager->session();
    QVERIFY(session != nullptr);
    QCOMPARE(session->path(), sample);
    QCOMPARE(session->identity(), QStringLiteral("inode:1:1"));
    QVERIFY(session->state() != PlaybackSession::Idle);

    // Stop → idle, then reopen → still the same single handle.
    manager->stop();
    QCOMPARE(session->state(), PlaybackSession::Idle);
    manager->openWithIdentity(sample, QStringLiteral("inode:1:1"));
    QCOMPARE(backend.handle(), handle); // no second handle
    manager->stop();

    // Detach before the backend is destroyed.
    manager->setBackend(nullptr);
}

// End-to-end user path: the activated card calls PlaybackController::play()
// (Main.qml onActivatedItem), which must open a session on the single backend.
void PlaybackLifecycleTest::controllerPlayDrivesSession()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString sample = makeSample(tmp.path());
    if (sample.isEmpty())
        QSKIP("ffmpeg fixture unavailable");

    MpvBackend backend;
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setBackend(&backend);
    mpv_handle* handle = backend.handle();
    QVERIFY(handle != nullptr);

    PlaybackController controller;
    controller.setPlaybackManager(manager);
    QSignalSpy sessionSpy(&controller, &PlaybackController::currentSessionChanged);
    controller.play(sample, QStringLiteral("inode:7:7"));

    PlaybackSession* session = controller.currentSession();
    QVERIFY(session != nullptr);
    QCOMPARE(session->path(), sample);
    QCOMPARE(session->identity(), QStringLiteral("inode:7:7"));
    QVERIFY(session->state() != PlaybackSession::Idle);
    QVERIFY(sessionSpy.count() >= 1);

    controller.stop();
    QCOMPARE(backend.handle(), handle); // still exactly one handle
    manager->setBackend(nullptr);
}

// DESIGN.md §4.6 + §4.5: Cinema is a display state of the ONE playback session.
// Loading the next item keeps the user in Cinema (they must not be dropped back
// to the grid between media items); only the explicit exit/close transitions
// change the state. This is the invariant the Esc wiring used to violate by
// routing fullscreen exit to an unconditional stop.
void PlaybackLifecycleTest::cinemaSurvivesAMediaChange()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString first = makeSample(tmp.path(), QStringLiteral("first.mp4"));
    const QString second = makeSample(tmp.path(), QStringLiteral("second.mp4"));
    if (first.isEmpty() || second.isEmpty())
        QSKIP("ffmpeg fixture unavailable");

    MpvBackend backend;
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setBackend(&backend);

    // The ordered context the session traverses: two videos, so next() has
    // somewhere to go.
    qg::FolderModel model;
    qg::FolderSnapshot snapshot;
    snapshot.path = tmp.path();
    snapshot.complete = true;
    snapshot.entries = {videoEntry(1, first, QStringLiteral("first.mp4")),
                        videoEntry(2, second, QStringLiteral("second.mp4"))};
    model.applySnapshot(snapshot);

    PlaybackContext context;
    PlaybackController controller;
    controller.setPlaybackManager(manager);
    controller.setPlaybackContext(&context);
    controller.setFolderModel(&model);

    controller.play(first, QStringLiteral("inode:5:1"));
    PlaybackSession* session = controller.currentSession();
    QVERIFY(session != nullptr);

    controller.enterCinema();
    QVERIFY(controller.cinemaActive());

    // Next: the new video loads into the SAME session and Cinema is preserved.
    controller.next();
    QVERIFY(controller.cinemaActive());                       // §4.6
    QCOMPARE(controller.currentSession(), session);           // no second player
    QCOMPARE(controller.currentSession()->path(), second);    // new media loaded

    // Return to Stage (§4.5): leaves Cinema, playback untouched.
    controller.exitCinema();
    QVERIFY(!controller.cinemaActive());
    QVERIFY(controller.currentSession()->state() != PlaybackSession::Idle);

    // Explicit Close (§4.5): stops playback and leaves Cinema.
    controller.enterCinema();
    QVERIFY(controller.cinemaActive());
    controller.stop();
    QVERIFY(!controller.cinemaActive());

    manager->setBackend(nullptr);
}

QTEST_MAIN(PlaybackLifecycleTest)
#include "test_playback_lifecycle.moc"