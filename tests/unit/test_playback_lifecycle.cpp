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
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"

namespace {

// Generates a tiny playable fixture with ffmpeg; empty path if unavailable.
QString makeSample(const QString& dir)
{
    const QString out = dir + QStringLiteral("/sample.mp4");
    QProcess proc;
    proc.start(QStringLiteral("ffmpeg"),
               {QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"),
                QStringLiteral("-i"), QStringLiteral("testsrc=duration=1:size=160x120:rate=10"),
                QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), out});
    if (!proc.waitForStarted(3000) || !proc.waitForFinished(15000))
        return {};
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 ? out : QString();
}

} // namespace

class PlaybackLifecycleTest : public QObject
{
    Q_OBJECT

private slots:
    void singleHandleIsReused();
    void openStopReopenKeepsOneHandle();
    void controllerPlayDrivesSession();
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

QTEST_MAIN(PlaybackLifecycleTest)
#include "test_playback_lifecycle.moc"