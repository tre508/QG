// Resume policy + stored-position tests (REMEDIATION-008 / D-26).
//
// Verifies the resume decision surface: the stored position is read from a real
// database through the injected loader (never a session-local guess), the
// eligibility window is exactly DESIGN.md §19.6 (> 5s, < duration − 10s), the
// present-but-filtered cases behave, and each policy (Always/Ask/Never) decides
// as specified. The actual seek on a real stream is verified by
// `qg_resume_playback` (GL).

#include <QtTest/QtTest>

#include <QDir>
#include <QTemporaryDir>

#include "models/folder_model.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"
#include "playback/mpv_backend.hpp"
#include "playback/playback_manager.hpp"

namespace {

const QString kIdentity = QStringLiteral("inode:801:3039");

QString dbPath(const QTemporaryDir& dir)
{
    return QDir(dir.path()).filePath(QStringLiteral("qg.db"));
}

} // namespace

class ResumePolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void storedPositionSurvivesRelaunch();
    void noLoaderMeansNoResume();
    void eligibilityWindowMatchesDesign();
    void unknownDurationUsesFloorOnly();
    void watchedNeverResumes();
    void policyAlwaysAskNever();
    void invalidPolicyFallsBackToAlways();

private:
    // Inject a database-backed loader, exactly as the composition root does.
    static void wireLoader(PlaybackManager* manager, qg::Database* db)
    {
        manager->setPositionLoader([db](const QString& identity) -> qint64 {
            if (db == nullptr || identity.isEmpty())
                return 0;
            return qg::MediaStateRepository(*db).positionMs(identity);
        });
    }
};

void ResumePolicyTest::init()
{
    // The manager is a process singleton: reset the mutable policy between cases.
    PlaybackManager::instance()->setResumePolicy(0);
    PlaybackManager::instance()->setPositionLoader({});
    PlaybackManager::instance()->setBackend(nullptr);
}

void ResumePolicyTest::cleanup()
{
    PlaybackManager::instance()->setPositionLoader({});
    PlaybackManager::instance()->setResumePolicy(0);
}

void ResumePolicyTest::storedPositionSurvivesRelaunch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dbPath(dir);

    // First run: record a position (as the 5s tick / stop flush would).
    {
        qg::Database db;
        QVERIFY(db.open(path));
        qg::MediaStateRepository media(db);
        QVERIFY(media.recordPosition(kIdentity, 42000));
    }

    // Relaunch: a fresh connection reads the same value.
    qg::Database db;
    QVERIFY(db.open(path));
    QCOMPARE(qg::MediaStateRepository(db).positionMs(kIdentity), qint64(42000));

    // And the manager surfaces it as seconds through the injected loader.
    PlaybackManager* manager = PlaybackManager::instance();
    wireLoader(manager, &db);
    QCOMPARE(manager->persistedPositionFor(kIdentity), 42.0);
    QCOMPARE(manager->persistedPositionFor(QStringLiteral("unknown")), 0.0);
    QCOMPARE(manager->persistedPositionFor(QString()), 0.0);
}

void ResumePolicyTest::noLoaderMeansNoResume()
{
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setPositionLoader({});
    // No stored state → no fabricated position, no resume.
    QCOMPARE(manager->persistedPositionFor(kIdentity), 0.0);
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 0.0);
}

void ResumePolicyTest::eligibilityWindowMatchesDesign()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qg::Database db;
    QVERIFY(db.open(dbPath(dir)));
    PlaybackManager* manager = PlaybackManager::instance();
    wireLoader(manager, &db);
    manager->setResumePolicy(0); // Always

    qg::MediaStateRepository media(db);

    // Exactly 5s is NOT eligible (must be > 5s).
    QVERIFY(media.recordPosition(kIdentity, 5000));
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 0.0);

    // Just over 5s, well inside the tail guard → resumes.
    QVERIFY(media.recordPosition(kIdentity, 5500));
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 5.5);

    // duration − 10s boundary is NOT eligible (strict less-than).
    QVERIFY(media.recordPosition(kIdentity, 50000));
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 0.0);

    // Just inside the tail guard → resumes.
    QVERIFY(media.recordPosition(kIdentity, 49900));
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 49.9);

    // No stored position at all.
    QVERIFY(media.recordPosition(kIdentity, 0));
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 0.0);
}

void ResumePolicyTest::unknownDurationUsesFloorOnly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qg::Database db;
    QVERIFY(db.open(dbPath(dir)));
    PlaybackManager* manager = PlaybackManager::instance();
    wireLoader(manager, &db);
    manager->setResumePolicy(0);

    qg::MediaStateRepository media(db);
    QVERIFY(media.recordPosition(kIdentity, 30000));

    // Unknown/negative duration: the tail guard cannot apply, only the > 5s
    // floor (recorded decision).
    QCOMPARE(manager->resumeStartFor(kIdentity, -1.0, false), 30.0);
    QCOMPARE(manager->resumeStartFor(kIdentity, 0.0, false), 30.0);
}

void ResumePolicyTest::watchedNeverResumes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qg::Database db;
    QVERIFY(db.open(dbPath(dir)));
    PlaybackManager* manager = PlaybackManager::instance();
    wireLoader(manager, &db);
    manager->setResumePolicy(0);

    qg::MediaStateRepository media(db);
    QVERIFY(media.recordPosition(kIdentity, 30000));

    // A finished item starts from the beginning, whatever the policy.
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, true), 0.0);
    QCOMPARE(manager->resumeStartFor(kIdentity, -1.0, true), 0.0);
}

void ResumePolicyTest::policyAlwaysAskNever()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qg::Database db;
    QVERIFY(db.open(dbPath(dir)));
    PlaybackManager* manager = PlaybackManager::instance();
    wireLoader(manager, &db);
    qg::MediaStateRepository media(db);
    QVERIFY(media.recordPosition(kIdentity, 30000));

    // Always (default): resume.
    QCOMPARE(manager->resumePolicy(), 0);
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 30.0);

    // Never: never resume.
    manager->setResumePolicy(2);
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 0.0);

    // Ask: no auto-resume, but a real prompt hook carrying the stored position.
    manager->setResumePolicy(1);
    QSignalSpy promptSpy(manager, &PlaybackManager::resumePromptRequested);
    QCOMPARE(manager->resumeStartFor(kIdentity, 60.0, false), 0.0);
    QCOMPARE(promptSpy.count(), 1);
    QCOMPARE(promptSpy.at(0).at(0).toString(), kIdentity);
    QCOMPARE(promptSpy.at(0).at(1).toDouble(), 30.0);

    // Accepting the prompt asks the live session to seek (no backend here, so it
    // is a safe no-op; the real seek is covered by qg_resume_playback).
    manager->applyResume(kIdentity, 30.0);
    QCOMPARE(manager->session()->position(), 0.0); // unchanged without a session
}

void ResumePolicyTest::invalidPolicyFallsBackToAlways()
{
    PlaybackManager* manager = PlaybackManager::instance();
    manager->setResumePolicy(99);
    QCOMPARE(manager->resumePolicy(), 0);
    manager->setResumePolicy(-1);
    QCOMPARE(manager->resumePolicy(), 0);
    manager->setResumePolicy(2);
    QCOMPARE(manager->resumePolicy(), 2);
}

QTEST_MAIN(ResumePolicyTest)
#include "test_resume_policy.moc"
