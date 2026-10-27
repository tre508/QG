// AUD-001 seek policy tests (DESIGN.md playback §3.1/§3.1.4/§3.1.5/§9).
//
// The seek *policy* — target normalisation, central seekability, rapid-request
// consolidation and the optimistic indicator's reconciliation with observed
// positions — is verified here without a render context or real media, by
// injecting the seek dispatch (setSeekDispatcher). The end-to-end path on a real
// decoded stream stays with `qg_transport_state` / `qg_resume_playback` (GL).

#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QVariant>

#include <cmath>
#include <limits>
#include <memory>

#include "playback/mpv_backend.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"

namespace {

// Records what the manager actually handed to the backend, in order.
struct SeekRecorder
{
    QList<double> targets;
    QList<QString> modes;

    int calls() const { return static_cast<int>(targets.size()); }
    double lastTarget() const { return targets.isEmpty() ? -1.0 : targets.last(); }
    QString lastMode() const { return modes.isEmpty() ? QString() : modes.last(); }
};

constexpr double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();

} // namespace

class SeekPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void normalizeSeekTargetHandlesEveryInvalidInput();
    void absoluteSeekClampsToTheKnownDuration();
    void infiniteTargetIsRefusedWithoutADuration();
    void nonFiniteRelativeOffsetIsRefused();

    void noBackendIsASilentNoOp();
    void nonSeekableMediaRejectWithoutTouchingTheBackend();

    void rapidSeeksConsolidateToTheLatestTarget();
    void relativeSeeksAccumulateOnTheIntendedPosition();
    void relativeSeekClampsAtZero();
    void rejectionDropsThePendingBurst();

    void optimisticPositionIgnoresStaleObservations();
    void observedPositionTakesOverOnceTheSeekLands();
    void observedPositionTakesOverAfterTheSettleDeadline();

private:
    // A backend with a live mpv handle; `seekable` models the observed property.
    void attachBackend(bool seekable, SeekRecorder &recorder);
    // Simulate mpv reporting a new `time-pos` (the backend's public signal).
    void observeTimePos(double seconds);

    PlaybackManager *m_manager = nullptr;
    std::unique_ptr<MpvBackend> m_backend;
};

void SeekPolicyTest::init()
{
    m_manager = PlaybackManager::instance();
    QVERIFY(m_manager != nullptr);

    m_backend.reset();
    m_manager->setBackend(nullptr);
    m_manager->setSeekDispatcher(PlaybackManager::SeekDispatcher()); // released with the recorder
    m_manager->setResumePolicy(0);
    m_manager->setPositionLoader({});

    PlaybackSession *session = m_manager->session();
    QVERIFY(session != nullptr);
    session->setIdentity(QString());
    session->setPath(QString());
    session->setPosition(0);
    session->setDuration(0);
    session->setSeekable(false);
    session->setState(PlaybackSession::Idle);
}

void SeekPolicyTest::cleanup()
{
    // Drop the injected dispatch first: it borrows the test-local recorder.
    m_manager->setSeekDispatcher(PlaybackManager::SeekDispatcher());
    m_manager->setBackend(nullptr);
    m_backend.reset();
}

void SeekPolicyTest::attachBackend(bool seekable, SeekRecorder &recorder)
{
    m_backend = std::make_unique<MpvBackend>();
    QVERIFY(m_backend->handle() != nullptr);

    m_manager->setBackend(m_backend.get());
    // mpv observes the properties it was asked about and delivers their initial
    // values (seekable=false, idle-active=true, ...) as soon as the handle is
    // created. Let that first batch land before modelling the state under test,
    // so a later observation cannot silently flip it mid-test.
    QTest::qWait(50);
    m_manager->session()->setSeekable(seekable);
    m_manager->setSeekDispatcher(
        [&recorder](double target, const QString &mode) {
            recorder.targets.append(target);
            recorder.modes.append(mode);
            return 0;
        });
}

void SeekPolicyTest::observeTimePos(double seconds)
{
    const bool deliv = QMetaObject::invokeMethod(m_backend.get(), "propertyChanged",
                                                 Qt::DirectConnection,
                                                 Q_ARG(QString, QStringLiteral("time-pos")),
                                                 Q_ARG(QVariant, QVariant(seconds)));
    QVERIFY2(deliv, "the backend's time-pos observation reached the manager");
}

void SeekPolicyTest::normalizeSeekTargetHandlesEveryInvalidInput()
{
    const double duration = 100.0;

    QCOMPARE(PlaybackManager::normalizeSeekTarget(3.5, duration), 3.5);
    QCOMPARE(PlaybackManager::normalizeSeekTarget(0.0, duration), 0.0);
    // NaN and -inf name no position: the historic `position > 0 ? position : 0`
    // clamp landed on 0 for both, and that stays the documented rule.
    QCOMPARE(PlaybackManager::normalizeSeekTarget(kNaN, duration), 0.0);
    QCOMPARE(PlaybackManager::normalizeSeekTarget(-kInf, duration), 0.0);
    QCOMPARE(PlaybackManager::normalizeSeekTarget(-25.0, duration), 0.0);
    // +inf means "end of media" when a duration is known.
    QCOMPARE(PlaybackManager::normalizeSeekTarget(kInf, duration), duration);
    // Beyond the known duration is clamped; an unknown duration clamps nothing.
    QCOMPARE(PlaybackManager::normalizeSeekTarget(duration + 50.0, duration), duration);
    QCOMPARE(PlaybackManager::normalizeSeekTarget(1e9, 0.0), 1e9);
    QCOMPARE(PlaybackManager::normalizeSeekTarget(kInf, 0.0), 0.0);
}

void SeekPolicyTest::absoluteSeekClampsToTheKnownDuration()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);
    m_manager->session()->setDuration(120.0);

    m_manager->seekAbsolute(500.0);
    QCOMPARE(recorder.calls(), 1);
    QCOMPARE(recorder.lastTarget(), 120.0); // never past the end
    QCOMPARE(m_manager->session()->position(), 120.0);

    // A second request outside the burst window is issued on its own.
    QTest::qWait(PlaybackManager::kSeekCoalesceMs * 3);
    m_manager->seekAbsolute(-9.0);
    QCOMPARE(recorder.calls(), 2);
    QCOMPARE(recorder.lastTarget(), 0.0); // negative targets clamp to zero
}

void SeekPolicyTest::infiniteTargetIsRefusedWithoutADuration()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);
    QCOMPARE(m_manager->session()->duration(), 0.0);

    QSignalSpy rejected(m_manager, &PlaybackManager::seekRejected);

    m_manager->seekAbsolute(kInf);
    QCOMPARE(rejected.count(), 1); // "the end" is not expressible without a duration
    QCOMPARE(recorder.calls(), 0);

    // With a duration it becomes a real request for the end of the media.
    m_manager->session()->setDuration(60.0);
    m_manager->seekAbsolute(kInf);
    QCOMPARE(recorder.calls(), 1);
    QCOMPARE(recorder.lastTarget(), 60.0);
}

void SeekPolicyTest::nonFiniteRelativeOffsetIsRefused()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);
    QSignalSpy rejected(m_manager, &PlaybackManager::seekRejected);

    m_manager->seek(kNaN);
    m_manager->seek(kInf);
    m_manager->seek(-kInf);

    QCOMPARE(rejected.count(), 3);
    QCOMPARE(recorder.calls(), 0);
    QCOMPARE(m_manager->session()->position(), 0.0); // restored, not corrupted
}

void SeekPolicyTest::noBackendIsASilentNoOp()
{
    SeekRecorder recorder;
    m_manager->setSeekDispatcher([&recorder](double target, const QString &) {
        recorder.targets.append(target);
        return 0;
    });
    QSignalSpy rejected(m_manager, &PlaybackManager::seekRejected);

    m_manager->seek(10.0);
    m_manager->seekAbsolute(42.0);

    // Nothing is playing: no command and no "not seekable" flash on the UI.
    QCOMPARE(rejected.count(), 0);
    QCOMPARE(recorder.calls(), 0);
}

void SeekPolicyTest::nonSeekableMediaRejectWithoutTouchingTheBackend()
{
    SeekRecorder recorder;
    attachBackend(false, recorder);
    m_manager->session()->setPosition(7.0);

    QSignalSpy rejected(m_manager, &PlaybackManager::seekRejected);

    m_manager->seek(10.0);
    m_manager->seekAbsolute(30.0);

    QCOMPARE(rejected.count(), 2);
    QCOMPARE(recorder.calls(), 0); // no doomed command reached mpv
    // actualPosition() could not read a position (nothing is loaded), so the last
    // known session position stands — the rejected target is never published.
    QCOMPARE(m_manager->session()->position(), 7.0);
}

void SeekPolicyTest::rapidSeeksConsolidateToTheLatestTarget()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);
    m_manager->session()->setDuration(600.0);

    // A burst: the first request goes out immediately (no added latency), the rest
    // are consolidated to the newest target (DESIGN.md playback §3.1.4).
    m_manager->seekAbsolute(10.0);
    m_manager->seekAbsolute(20.0);
    m_manager->seekAbsolute(30.0);

    QCOMPARE(recorder.calls(), 1);
    QCOMPARE(recorder.lastTarget(), 10.0);
    QCOMPARE(recorder.lastMode(), QStringLiteral("absolute+exact"));
    QCOMPARE(m_manager->session()->position(), 30.0); // §3.1.2: indicator is live

    QTRY_COMPARE_WITH_TIMEOUT(recorder.calls(), 2, PlaybackManager::kSeekCoalesceMs * 8);
    QCOMPARE(recorder.lastTarget(), 30.0);            // newest request wins

    // The window closes with nothing pending: no further commands accumulate.
    QTest::qWait(PlaybackManager::kSeekCoalesceMs * 4);
    QCOMPARE(recorder.calls(), 2);
}

void SeekPolicyTest::relativeSeeksAccumulateOnTheIntendedPosition()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);
    m_manager->session()->setDuration(600.0);
    m_manager->session()->setPosition(10.0); // what mpv last observed

    m_manager->seek(10.0);
    m_manager->seek(10.0);
    m_manager->seek(10.0);

    QCOMPARE(recorder.calls(), 1);
    QCOMPARE(recorder.lastTarget(), 20.0);

    QTRY_COMPARE_WITH_TIMEOUT(recorder.calls(), 2, PlaybackManager::kSeekCoalesceMs * 8);
    // Every increment is preserved: the target builds on the intended position
    // (40), never on the stale observed one (which would give 20).
    QCOMPARE(recorder.lastTarget(), 40.0);
    QCOMPARE(m_manager->session()->position(), 40.0);
}

void SeekPolicyTest::relativeSeekClampsAtZero()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);
    m_manager->session()->setDuration(600.0);
    m_manager->session()->setPosition(4.0);

    m_manager->seek(-10.0);

    QCOMPARE(recorder.calls(), 1);
    QCOMPARE(recorder.lastTarget(), 0.0);
    QCOMPARE(m_manager->session()->position(), 0.0);
}

void SeekPolicyTest::rejectionDropsThePendingBurst()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);

    m_manager->seekAbsolute(5.0); // issued: the window is now open
    m_manager->seekAbsolute(6.0); // deferred, waiting for the window

    // The stream turns out to be unseekable mid-burst.
    m_manager->session()->setSeekable(false);
    QSignalSpy rejected(m_manager, &PlaybackManager::seekRejected);
    m_manager->seekAbsolute(7.0);
    QCOMPARE(rejected.count(), 1);

    QTest::qWait(PlaybackManager::kSeekCoalesceMs * 4);
    QCOMPARE(recorder.calls(), 1); // the deferred 6.0 never reached the backend
}

void SeekPolicyTest::optimisticPositionIgnoresStaleObservations()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);

    m_manager->seekAbsolute(300.0);
    QCOMPARE(m_manager->session()->position(), 300.0);

    // An observation from before the seek lands must not drag the indicator back
    // (DESIGN.md playback §3.1.2: it updates immediately).
    observeTimePos(10.01);
    QCOMPARE(m_manager->session()->position(), 300.0);
    observeTimePos(120.0);
    QCOMPARE(m_manager->session()->position(), 300.0);
}

void SeekPolicyTest::observedPositionTakesOverOnceTheSeekLands()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);

    m_manager->seekAbsolute(300.0);
    observeTimePos(300.02); // landed within tolerance
    QCOMPARE(m_manager->session()->position(), 300.02);

    // Intent released: ordinary playback observations reach the indicator again
    // (§3.1.5, "the final displayed position is synchronized with the actual").
    observeTimePos(11.0);
    QCOMPARE(m_manager->session()->position(), 11.0);
}

void SeekPolicyTest::observedPositionTakesOverAfterTheSettleDeadline()
{
    SeekRecorder recorder;
    attachBackend(true, recorder);

    m_manager->seekAbsolute(300.0);
    // mpv never reports a position within tolerance (an inexactly seekable stream,
    // or a seek that fails): the optimistic value must still expire.
    QTest::qWait(PlaybackManager::kSeekSettleMs + 250);
    observeTimePos(11.0);

    QCOMPARE(m_manager->session()->position(), 11.0);
}

QTEST_MAIN(SeekPolicyTest)
#include "test_seek_policy.moc"