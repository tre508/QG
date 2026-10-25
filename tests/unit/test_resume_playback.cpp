// Resume end-to-end test (REMEDIATION-008 / D-26).
//
// Plays a real 60s stream through the real PlaybackController with a real
// database-backed position, and asserts:
//   * an eligible clip resumes near the stored position as soon as it loads —
//     the seek cannot be lost to the load race because mpv applies `start=` when
//     the file loads;
//   * an ineligible policy (Never) starts from the beginning;
//   * the position is written to the database on pause (DB-backed, not a
//     session-local guess) and survives a "relaunch" (fresh connection);
//   * the 5s tick is throttled and the final flush is not redundant.
//
// The composition wiring (positionTick → MediaStateRepository::recordPosition) is
// reproduced here deliberately, so the assertions read the real database.
//
// Needs a decoded stream (vo=libmpv), so it runs under tests/run_gl_test.sh and
// QSKIPs when no GL context exists.

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
#include "controllers/playback_controller.hpp"
#include "models/folder_model.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"
#include "playback/mpv_backend.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"
#include "playback/video_surface.hpp"

namespace {

const QString kIdentity = QStringLiteral("inode:9:99");
constexpr qint64 kSeedMs = 30000;   // the position a previous run left behind

QString makeSample(const QString& dir)
{
    // 60s but tiny (64x48 @ 5fps) so the fixture is fast to produce and seek.
    const QString out = dir + QStringLiteral("/long.mp4");
    QProcess proc;
    proc.start(QStringLiteral("ffmpeg"),
               {QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"),
                QStringLiteral("-i"), QStringLiteral("testsrc=duration=60:size=64x48:rate=5"),
                QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), out});
    if (!proc.waitForStarted(3000) || !proc.waitForFinished(60000))
        return {};
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 ? out : QString();
}

void fillModel(qg::FolderModel& model, const QString& path)
{
    qg::Entry e;
    e.identity = kIdentity;
    e.path = path;
    e.name = QStringLiteral("long.mp4");
    e.kind = qg::EntryKind::Video;
    e.size = 4096;
    e.durationMs = 60000;   // probed: 60s → resume window (5s, 50s)
    e.watched = false;

    qg::FolderSnapshot snap;
    snap.path = path.section(QLatin1Char('/'), 0, -2);
    snap.entries = {e};
    snap.complete = true;
    model.applySnapshot(snap);
}

const char* kShell = R"QML(
import QtQuick
import qg.core 1.0

Item {
    width: 320; height: 240
    Rectangle { anchors.fill: parent; color: "black" }
    VideoSurfaceItem {
        objectName: "videoSurface"
        anchors.fill: parent
        backend: qgBackend
    }
}
)QML";

} // namespace

class ResumePlaybackTest : public QObject
{
    Q_OBJECT

private slots:
    void eligibleClipResumesAndPersistsAcrossRelaunch();

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

    static bool waitFor(const std::function<bool()>& predicate, QQuickView& view, int timeoutMs)
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

void ResumePlaybackTest::eligibleClipResumesAndPersistsAcrossRelaunch()
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
    const QString dbFile = tmp.path() + QStringLiteral("/qg.db");

    qg::Database db;
    QVERIFY(db.open(dbFile));
    qg::MediaStateRepository media(db);

    // --- A previous run left the clip at 30s (database-backed) --------------
    QVERIFY(media.recordPosition(kIdentity, kSeedMs));

    MpvBackend backend;
    QVERIFY(backend.handle() != nullptr);
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setBackend(&backend);

    // The composition root's wiring: the throttled ticks write to the database.
    QObject::connect(manager, &PlaybackManager::positionTick, this,
                     [&media](const QString& identity, double seconds) {
                         media.recordPosition(identity, static_cast<qint64>(seconds * 1000.0));
                     });
    manager->setPositionLoader([&media](const QString& identity) -> qint64 {
        return identity.isEmpty() ? 0 : media.positionMs(identity);
    });

    qg::FolderModel model;
    fillModel(model, sample);

    PlaybackController controller;
    controller.setPlaybackManager(manager);
    controller.setFolderModel(&model);

    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("qgBackend"), &backend);
    view.setSource(QUrl(QStringLiteral("data:text/plain,")
                        + QUrl::toPercentEncoding(QString::fromUtf8(kShell))));
    view.setResizeMode(QQuickView::SizeViewToRootObject);
    view.resize(320, 240);
    view.show();
    QVERIFY2(view.status() == QQuickView::Ready, "shell loaded");
    auto* surface = view.rootObject()->findChild<VideoSurfaceItem*>(QStringLiteral("videoSurface"));
    QVERIFY(surface != nullptr);
    manager->attachSurface(surface);

    // --- Eligible + Always → resumes near 30s -------------------------------
    manager->setResumePolicy(0); // Always (default)
    QCOMPARE(manager->persistedPositionFor(kIdentity), 30.0);

    controller.play(sample, kIdentity);
    QVERIFY2(waitFor([&] {
                 return manager->session()->state() == PlaybackSession::Playing
                        && manager->session()->position() > 25.0;
             }, view, 15000),
             "resumed near the stored 30s as soon as the file loaded");

    // Keeps playing forward from there (the seek was not lost to the load).
    const double p1 = manager->session()->position();
    pump(view, 800);
    QVERIFY2(manager->session()->position() > p1, "playback continues forward after resume");

    // --- Throttle: periodic writes are capped, the flush is not redundant ---
    QSignalSpy tickSpy(manager, &PlaybackManager::positionTick);
    const int count0 = tickSpy.count();
    for (int i = 0; i < 5; ++i)
        manager->persistPosition();              // 5 rapid periodic calls
    const int count1 = tickSpy.count();
    QVERIFY2(count1 - count0 <= 1, "periodic ticks are throttled to ~1 per 5s");

    pump(view, 700);                              // the position advances
    manager->flushPosition();                     // final write
    QVERIFY2(tickSpy.count() > count1, "the final flush writes the new position");
    QVERIFY2(media.positionMs(kIdentity) > kSeedMs,
             "the database now holds a position advanced past the seed");

    // --- Pause writes the live position to the real database ----------------
    pump(view, 500);
    manager->pause();
    const qint64 stored = media.positionMs(kIdentity);
    QVERIFY2(stored > kSeedMs + 500,
             qPrintable(QStringLiteral("DB position after pause: %1").arg(stored)));

    // --- Relaunch: a fresh connection reads the durably written position ----
    {
        qg::Database reopened;
        QVERIFY(reopened.open(dbFile));
        QCOMPARE(qg::MediaStateRepository(reopened).positionMs(kIdentity), stored);
    }

    // --- Policy Never → starts from the beginning despite the stored position -
    QVERIFY(manager->persistedPositionFor(kIdentity) > 25.0); // stored…
    manager->setResumePolicy(2);                              // …but Never
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 0.0);

    controller.play(sample, kIdentity);
    QVERIFY(waitFor([&] { return manager->session()->state() == PlaybackSession::Playing; },
                    view, 15000));
    pump(view, 400);
    QVERIFY2(manager->session()->position() < 5.0, "Never starts from the beginning");

    manager->stop();
    manager->setBackend(nullptr);
}

QTEST_MAIN(ResumePlaybackTest)
#include "test_resume_playback.moc"
