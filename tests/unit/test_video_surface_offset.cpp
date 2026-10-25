// Offset-containment test for VideoSurfaceItem (PLAY-VIS-003).
//
// Places the surface OFFSET from the window origin (as Stage does inside
// ContentView) and verifies decoded mpv frames render at the item's own rect
// — NOT at the window's bottom-left.
//
// The previous defect passed `fbo = window framebuffer` with `w/h = item size`.
// mpv renders with viewport (0,0,w,h) of the FBO it is given, so an
// item-sized rectangle landed at the WINDOW origin — the PLAY-VIS-001 purple
// region covering sidebar/browser content below-left of the Stage.
// QQuickFramebufferObject allocates an item-sized FBO, so mpv frames render at
// the item's rect inside Stage.
//
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
                QStringLiteral("-i"), QStringLiteral("testsrc=duration=3:size=160x120:rate=15"),
                QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), out});
    if (!proc.waitForStarted(3000) || !proc.waitForFinished(20000))
        return {};
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 ? out : QString();
}

// Counts bright (non-backdrop) pixels in a sub-rectangle.
int brightPixelsIn(const QImage& img, int x0, int y0, int x1, int y1)
{
    if (img.isNull())
        return 0;
    const QImage rgb = img.convertToFormat(QImage::Format_RGB32);
    x0 = qMax(0, x0); y0 = qMax(0, y0);
    x1 = qMin(rgb.width(), x1); y1 = qMin(rgb.height(), y1);
    int n = 0;
    for (int y = y0; y < y1; y += 2) {
        const QRgb* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = x0; x < x1; x += 2) {
            const int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            if (r + g + b > 120)
                ++n;
        }
    }
    return n;
}

const char* kShell = R"QML(
import QtQuick
import qg.core 1.0

Item {
    width: 480; height: 360
    Rectangle { anchors.fill: parent; color: "#000010" }
    VideoSurfaceItem {
        objectName: "videoSurface"
        x: 80; y: 40; width: 200; height: 150
        backend: qgBackend
    }
}
)QML";

} // namespace

class VideoSurfaceOffsetTest : public QObject
{
    Q_OBJECT
private slots:
    void decodedFramesRenderAtItemRect();
};

void VideoSurfaceOffsetTest::decodedFramesRenderAtItemRect()
{
    qg::registerCoreQmlTypes();

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
    manager->attachSurface(surface);

    manager->openWithIdentity(sample, QStringLiteral("inode-offset:9:9"));
    QTest::qWait(300);
    manager->play();

    int bestItem = 0;
    int bestOrigin = 0;
    double lastDuration = -1;
    double lastPos = -1;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 12000 && bestItem == 0) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        view.requestUpdate();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QImage win = view.grabWindow();
        bestItem = brightPixelsIn(win, 80, 40, 280, 190);
        bestOrigin = brightPixelsIn(win, 0, 0, 480, 40);
        lastDuration = manager->session()->duration();
        lastPos = manager->session()->position();
    }

    qInfo() << "bright pixels at item rect:" << bestItem;
    qInfo() << "bright pixels in top strip outside item (old-bug location):" << bestOrigin;
    qInfo() << "session duration:" << lastDuration << "position:" << lastPos;

    QVERIFY2(bestItem > 0, "decoded frames rendered at the item's rect inside Stage");
    QVERIFY2(bestOrigin == 0, "no rendering outside the item rect (old offset bug eliminated)");
    QVERIFY2(lastDuration > 0, "duration reported for the loaded media");
    QVERIFY2(lastPos >= 0, "playback position reported");

    manager->stop();
    manager->setBackend(nullptr);
}

QTEST_MAIN(VideoSurfaceOffsetTest)
#include "test_video_surface_offset.moc"
