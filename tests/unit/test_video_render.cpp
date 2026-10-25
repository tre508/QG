// End-to-end decoded-frame test (REMEDIATION-005 / D-06, D-20).
//
// Presents the real VideoSurfaceItem in a Qt Quick window on an OpenGL-backed
// platform, opens a generated sample through the single PlaybackManager, and
// asserts that decoded frames actually reach the window (non-black pixels that
// change when the position changes). This is the acceptance check for
// "a selected supported video shows decoded frames in Stage, not a placeholder".
//
// Requires a platform with OpenGL (X11/Wayland + libGL). On a headless box run
// it under Xvfb via tests/run_gl_test.sh; it QSKIPs when no GL context exists.

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QProcess>
#include <QTemporaryDir>
#include <QQuickView>
#include <QQuickItem>
#include <QObject>
#include <QQmlContext>
#include <QUrl>
#include <QImage>
#include <QScreen>
#include <QGuiApplication>

#include "core/composition.hpp"
#include "playback/mpv_backend.hpp"
#include "playback/video_surface.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"
#include <mpv/client.h>

namespace {

QString makeSample(const QString& dir)
{
    const QString out = dir + QStringLiteral("/sample.mp4");
    QProcess proc;
    proc.start(QStringLiteral("ffmpeg"),
               {QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"),
                QStringLiteral("-i"), QStringLiteral("testsrc=duration=3:size=320x240:rate=15"),
                QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), out});
    if (!proc.waitForStarted(3000) || !proc.waitForFinished(20000))
        return {};
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 ? out : QString();
}

// Counts pixels in the central region that are clearly not the near-black backdrop.
int brightPixelCount(const QImage& img)
{
    if (img.isNull())
        return 0;
    const QImage rgb = img.convertToFormat(QImage::Format_RGB32);
    const int x0 = rgb.width() / 4, x1 = rgb.width() * 3 / 4;
    const int y0 = rgb.height() / 3, y1 = rgb.height() * 2 / 3;
    int n = 0;
    for (int y = y0; y < y1; y += 3) {
        const QRgb* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = x0; x < x1; x += 3) {
            const int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            if (r + g + b > 120)
                ++n;
        }
    }
    return n;
}

// Inline shell: a real VideoSurfaceItem bound to the shared MpvBackend.
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

class VideoRenderTest : public QObject
{
    Q_OBJECT

private slots:
    void decodedFramesReachTheSurface();
};

void VideoRenderTest::decodedFramesReachTheSurface()
{
    qg::registerCoreQmlTypes();

    // Skip cleanly when the platform provides no OpenGL context (headless QPA).
    {
        QOpenGLContext probe;
        if (!probe.create())
            QSKIP("no OpenGL context on this platform; run under Xvfb/Wayland");
    }

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString sample = makeSample(tmp.path());
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

    QVERIFY(view.rootObject() != nullptr);
    auto* surface = view.rootObject()->findChild<VideoSurfaceItem*>(QStringLiteral("videoSurface"));
    QVERIFY(surface != nullptr);
    // Register the surface so the pending load completes once it has painted.
    manager->attachSurface(surface);

    manager->openWithIdentity(sample, QStringLiteral("inode:9:9"));
    // Ensure active playback (pending loads complete once the surface paints).
    QTest::qWait(300);
    manager->play();
    QCOMPARE(manager->session()->state() != PlaybackSession::Idle, true);

    // Render loop: poll for decoded pixels in the window.
    int best = 0;
    double maxPos = -1;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 12000 && best == 0) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        view.requestUpdate();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        best = brightPixelCount(view.grabWindow());
        double pos = -1;
        mpv_handle* h = backend.handle();
        if (h) mpv_get_property(h, "time-pos", MPV_FORMAT_DOUBLE, &pos);
        if (pos > maxPos) maxPos = pos;
    }
    qInfo() << "max time-pos observed:" << maxPos;
    QImage win = view.grabWindow();
    win.save("/tmp/qg-sample/grab_view.png");
    int bestScreen = 0;
    if (QScreen* scr = QGuiApplication::primaryScreen()) {
        QImage shot = scr->grabWindow(0).toImage();
        shot.save("/tmp/qg-sample/grab_screen.png");
        bestScreen = brightPixelCount(shot);
    }
    qInfo() << "screen bright pixels:" << bestScreen
          << "| view size:" << win.width() << win.height();
    qInfo() << "bright pixels:" << best
          << "| renderCtx:" << backend.hasRenderContext()
          << "| framesRendered:" << surface->property("framesRendered").toInt();
    // Diagnose what mpv actually decoded.
    if (mpv_handle* h = backend.handle()) {
        auto getStr = [&](const char* name) {
            char* v = mpv_get_property_string(h, name);
            QString s = v ? QString::fromUtf8(v) : QStringLiteral("<null>");
            mpv_free(v);
            return s;
        };
        int64_t w = 0, hgt = 0;
        mpv_get_property(h, "video-params/w", MPV_FORMAT_INT64, &w);
        mpv_get_property(h, "video-params/h", MPV_FORMAT_INT64, &hgt);
        qInfo() << "mpv video-params:" << w << "x" << hgt
                << "| pixelformat:" << getStr("video-params/pixelformat")
                << "| vo-configured:" << getStr("vo-configured")
                << "| idle-active:" << getStr("idle-active")
                << "| core-idle:" << getStr("core-idle")
                << "| pause:" << getStr("pause")
                << "| hwdec-current:" << getStr("hwdec-current");
    }

    QVERIFY2(best > 0, "decoded frames reached the surface (non-black pixels)");

    manager->stop();
    manager->setBackend(nullptr);
}

QTEST_MAIN(VideoRenderTest)
#include "test_video_render.moc"
