// VideoSurfaceItem renderer-lifecycle test (REMEDIATION-005 / D-06, D-20).
//
// Verifies the surface's ownership contract without requiring a live render
// pass: default state, backend setter + change signal, videoActive false
// without a render context, and safe construct/destroy. Actual decoded-frame
// display requires a real OpenGL + libmpv render pass and is verified manually
// (see GUIDE_STATE REMEDIATION-005 reproduction procedure).

#include <QtTest/QtTest>
#include <QGuiApplication>

#include "playback/mpv_backend.hpp"
#include "playback/video_surface.hpp"

class VideoSurfaceTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultStateIsInactive();
    void settingBackendEmitsChange();
    void clearsBackendWithoutCrash();
    void constructDestroyRecreateIsSafe();
};

void VideoSurfaceTest::defaultStateIsInactive()
{
    VideoSurfaceItem surface;
    QCOMPARE(surface.backend(), nullptr);
    QCOMPARE(surface.videoActive(), false);
}

void VideoSurfaceTest::settingBackendEmitsChange()
{
    VideoSurfaceItem surface;
    QSignalSpy spy(&surface, &VideoSurfaceItem::backendChanged);

    MpvBackend backend;
    surface.setBackend(&backend);
    QCOMPARE(surface.backend(), &backend);
    QCOMPARE(spy.count(), 1);

    // Re-setting the same backend must not re-emit.
    surface.setBackend(&backend);
    QCOMPARE(spy.count(), 1);

    // Detach before the backend goes out of scope.
    surface.setBackend(nullptr);
}

void VideoSurfaceTest::clearsBackendWithoutCrash()
{
    VideoSurfaceItem surface;
    MpvBackend backend;
    surface.setBackend(&backend);
    surface.setBackend(nullptr);
    QCOMPARE(surface.backend(), nullptr);
    QCOMPARE(surface.videoActive(), false);
}

void VideoSurfaceTest::constructDestroyRecreateIsSafe()
{
    for (int i = 0; i < 3; ++i) {
        auto* surface = new VideoSurfaceItem;
        MpvBackend backend;
        surface->setBackend(&backend);
        surface->setBackend(nullptr);
        delete surface;
    }
    QVERIFY(true);
}

QTEST_MAIN(VideoSurfaceTest)
#include "test_video_surface.moc"