// Transport state-machine end-to-end test (REMEDIATION-006 / D-21, D-22).
//
// Verifies that the transport's live state actually tracks mpv: a load moves
// the session Loading → Playing with a real duration and seekable=true,
// position advances, pause/play and an absolute seek take effect, EOF flips the
// session to Ended, an external `stop` command returns it to Idle, and a bad
// file produces the Error state with a message.
//
// This needs a real decoded stream, so it requires an OpenGL-capable display
// (vo=libmpv ends the file instantly without a render context). Run it under
// tests/run_gl_test.sh, which supplies Xvfb when headless; it QSKIPs cleanly
// when no GL context exists.

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QProcess>
#include <QTemporaryDir>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QUrl>
#include <QElapsedTimer>
#include <functional>

#include "core/composition.hpp"
#include "playback/mpv_backend.hpp"
#include "playback/video_surface.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"
#include <mpv/client.h>

namespace {

QString makeSample(const QString& dir, int seconds = 2)
{
    const QString out = dir + QStringLiteral("/clip.mp4");
    QProcess proc;
    proc.start(QStringLiteral("ffmpeg"),
               {QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"),
                QStringLiteral("-i"),
                QStringLiteral("testsrc=duration=%1:size=160x120:rate=15").arg(seconds),
                QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), out});
    if (!proc.waitForStarted(3000) || !proc.waitForFinished(20000))
        return {};
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 ? out : QString();
}

const char* kShell = R"QML(
import QtQuick
import qg.core 1.0

Item {
    width: 480; height: 360
    Rectangle { anchors.fill: parent; color: "black" }
    VideoSurfaceItem {
        objectName: "videoSurface"
        anchors.fill: parent
        backend: qgBackend
    }
}
)QML";

} // namespace

class TransportStateTest : public QObject
{
    Q_OBJECT

private slots:
    void transportTracksLivePlaybackState();

private:
    static void pump(QQuickView& view, int ms)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            view.requestUpdate();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    }

    static bool waitFor(std::function<bool()> predicate, QQuickView& view, int timeoutMs)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < timeoutMs) {
            if (predicate())
                return true;
            pump(view, 50);
        }
        return predicate();
    }
};

void TransportStateTest::transportTracksLivePlaybackState()
{
    qg::registerCoreQmlTypes();

    {
        QOpenGLContext probe;
        if (!probe.create())
            QSKIP("no OpenGL context on this platform; run under Xvfb/Wayland");
    }

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString sample = makeSample(tmp.path(), 2);
    if (sample.isEmpty())
        QSKIP("ffmpeg fixture unavailable");

    MpvBackend backend;
    QVERIFY(backend.handle() != nullptr);
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setBackend(&backend);

    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("qgBackend"), &backend);
    view.setSource(QUrl(QStringLiteral("data:text/plain,")
                        + QUrl::toPercentEncoding(QString::fromUtf8(kShell))));
    view.setResizeMode(QQuickView::SizeViewToRootObject);
    view.resize(480, 360);
    view.show();
    QVERIFY2(view.status() == QQuickView::Ready, "shell loaded");
    auto* surface = view.rootObject()->findChild<VideoSurfaceItem*>(QStringLiteral("videoSurface"));
    QVERIFY(surface != nullptr);
    manager->attachSurface(surface);

    PlaybackSession* session = manager->session();
    QVERIFY(session != nullptr);

    // --- Load → Loading then Playing with real duration + seekable ----------
    manager->openWithIdentity(sample, QStringLiteral("inode:9:11"));
    QVERIFY(waitFor([&] {
                return session->state() == PlaybackSession::Playing
                       && session->duration() > 0;
            }, view, 15000));
    QVERIFY2(session->duration() > 1.0, "duration comes from mpv");
    QVERIFY2(session->seekable(), "a local file is seekable");

    // --- Position advances --------------------------------------------------
    const double p1 = session->position();
    pump(view, 900);
    const double p2 = session->position();
    QVERIFY2(p2 > p1, qPrintable(QStringLiteral("position advanced %1 -> %2").arg(p1).arg(p2)));

    // --- Pause / play round-trip -------------------------------------------
    manager->pause();
    QCOMPARE(session->state(), PlaybackSession::Paused);
    manager->play();
    QCOMPARE(session->state(), PlaybackSession::Playing);

    // --- Absolute seek ------------------------------------------------------
    manager->seekAbsolute(1.0);
    QVERIFY(waitFor([&] { return session->position() > 0.6; }, view, 4000));

    // --- EOF → Ended (keep-open leaves the frame) ---------------------------
    QVERIFY2(waitFor([&] { return session->state() == PlaybackSession::Ended; },
                     view, 8000),
             "session reached Ended at EOF");

    // --- Explicit stop → Idle ----------------------------------------------
    manager->stop();
    QCOMPARE(session->state(), PlaybackSession::Idle);

    // --- External stop (raw mpv command) → Idle -----------------------------
    manager->openWithIdentity(sample, QStringLiteral("inode:9:11"));
    QVERIFY(waitFor([&] { return session->state() == PlaybackSession::Playing; },
                    view, 15000));
    {
        const char* stopCmd[] = {"stop", nullptr};
        mpv_command(backend.handle(), stopCmd);
    }
    QVERIFY2(waitFor([&] { return session->state() == PlaybackSession::Idle; },
                     view, 5000),
             "external stop returns the session to Idle");

    // --- Failed open → Error with a message --------------------------------
    manager->openWithIdentity(tmp.path() + QStringLiteral("/does-not-exist.mp4"),
                              QStringLiteral("inode:0:0"));
    QVERIFY2(waitFor([&] { return session->state() == PlaybackSession::Error; },
                     view, 8000),
             "a bad file yields the Error state");
    QVERIFY2(!session->errorMessage().isEmpty(), "an error message is surfaced");

    manager->setBackend(nullptr);
}

QTEST_MAIN(TransportStateTest)
#include "test_transport_state.moc"
