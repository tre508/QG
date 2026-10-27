// REMEDIATION-010 (D-08): ActionsController feedback and dialog support.
//
// The audit's complaint was that operation feedback was assumed rather than
// observed: `actionFinished(kind, true)` was emitted unconditionally and nothing
// ever reached the toast host. These cases pin the contract against the real
// filesystem and the real services:
//   * a rename reports the outcome FileOperationsService actually produced;
//   * a collision is a reported failure with a message, never a silent success;
//   * the collision/properties helpers read real filesystem data;
//   * Trash reports failure without claiming success, and a successful Trash +
//     Undo round-trips (isolated by redirecting XDG_DATA_HOME).
//
// AUD-003: the mutating entry points now run on the I/O pool
// (FileOperationsService is an asynchronous facade), so a result — and the
// filesystem effect that produced it — arrives after the event loop turns. Cases
// below wait for either the reported outcome (QTRY_COMPARE on the spy) or the
// effect (QTRY_VERIFY on the filesystem) instead of assuming inline completion.
// The async contract itself is pinned by `qg_file_ops_async`.

#include "controllers/actions_controller.hpp"
#include "filesystem/operations.hpp"
#include "filesystem/trash.hpp"
#include "models/folder_model.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QClipboard>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

namespace {

void writeFile(const QString &path, const QByteArray &data = QByteArray("payload"))
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(data);
    f.close();
}

} // namespace

class ActionFeedbackTest : public QObject {
    Q_OBJECT

private slots:
    void renameSuccessReportsServiceOutcome();
    void renameCollisionReportsFailureAndToasts();
    void renameHelpersReadTheRealFilesystem();
    void renameValidationUsesCanonicalRules();
    void uniqueBaseNameSuffixesOnlyWhenTaken();
    void fileInfoReportsRealStats();
    void trashFailureIsNeverAPhantomSuccess();
    void trashInfoFailureLeavesSourceInPlace();
    void trashRestoreRefusesOutsidePayload();
    void trashThenUndoRoundTrips();
    void copyPathPutsThePathOnTheClipboard();
    void clipboardCutPasteMovesFile();
    void clipboardCopyPasteCreatesDupe();
    void clipboardMoveVsCopyResolvesCorrectly();
    void permanentDeleteRemovesFile();
    void permanentDeleteMissingPathFails();
    void permanentDeleteMultiSelectPartialSummary();
    void permanentDeleteModelRemoveIsIdempotent();
};

void ActionFeedbackTest::renameSuccessReportsServiceOutcome()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath("a.mp4");
    writeFile(source);

    FileOperationsService ops;
    ActionsController actions;
    actions.setFileOperations(&ops);

    QSignalSpy finished(&actions, &ActionsController::actionFinished);
    QSignalSpy toasts(&actions, &ActionsController::toastRequested);

    actions.rename(source, "renamed");

    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toString(), QStringLiteral("rename"));
    QCOMPARE(finished.at(0).at(1).toBool(), true);
    QCOMPARE(toasts.count(), 0); // success is silent
    QVERIFY(QFileInfo::exists(dir.filePath("renamed.mp4"))); // extension preserved
    QVERIFY(!QFileInfo::exists(source));
}

void ActionFeedbackTest::renameCollisionReportsFailureAndToasts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath("a.mp4");
    writeFile(source);
    writeFile(dir.filePath("taken.mp4"), "other");

    FileOperationsService ops;
    ActionsController actions;
    actions.setFileOperations(&ops);

    QSignalSpy finished(&actions, &ActionsController::actionFinished);
    QSignalSpy toasts(&actions, &ActionsController::toastRequested);

    actions.rename(source, "taken");

    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(1).toBool(), false); // no phantom success
    QCOMPARE(toasts.count(), 1);
    const QString message = toasts.at(0).at(0).toString();
    QVERIFY(!message.isEmpty());
    // Both files survive: the collision changed nothing.
    QVERIFY(QFileInfo::exists(source));
    QCOMPARE(QFile(dir.filePath("taken.mp4")).size(), qint64(5));
}

void ActionFeedbackTest::renameHelpersReadTheRealFilesystem()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString clip = dir.filePath("clip.mp4");
    writeFile(clip);

    ActionsController actions;

    // Extension is preserved exactly as FileOperationsService::renameSync does it.
    QCOMPARE(actions.targetPathForRename(clip, "holiday"), dir.filePath("holiday.mp4"));
    QCOMPARE(actions.targetPathForRename(clip, "holiday.mp4"), dir.filePath("holiday.mp4"));
    // A suffix-less name stays suffix-less.
    const QString noext = dir.filePath("README");
    writeFile(noext);
    QCOMPARE(actions.targetPathForRename(noext, "NOTES"), dir.filePath("NOTES"));

    QVERIFY(actions.pathExists(clip));
    QVERIFY(!actions.pathExists(dir.filePath("missing.mp4")));
    QVERIFY(!actions.pathExists(QString()));
}

void ActionFeedbackTest::renameValidationUsesCanonicalRules()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString clip = dir.filePath(QStringLiteral("clip.MKV"));
    writeFile(clip);

    ActionsController actions;
    const QStringList siblings{QStringLiteral("clip.MKV"), QStringLiteral("Taken.mkv")};

    QVariantMap verdict = actions.validateRename(clip, QStringLiteral("holiday"), siblings);
    QVERIFY(verdict.value(QStringLiteral("ok")).toBool());
    QCOMPARE(verdict.value(QStringLiteral("finalName")).toString(), QStringLiteral("holiday.MKV"));

    verdict = actions.validateRename(clip, QStringLiteral("clip.mkv"), siblings);
    QVERIFY(verdict.value(QStringLiteral("ok")).toBool());
    QCOMPARE(verdict.value(QStringLiteral("finalName")).toString(), QStringLiteral("clip.mkv"));

    verdict = actions.validateRename(clip, QStringLiteral("bad/name"), siblings);
    QVERIFY(!verdict.value(QStringLiteral("ok")).toBool());
    QCOMPARE(verdict.value(QStringLiteral("message")).toString(),
             QStringLiteral("Name cannot contain '/' or NUL"));

    verdict = actions.validateRename(clip, QStringLiteral("taken"), siblings);
    QVERIFY(!verdict.value(QStringLiteral("ok")).toBool());
    QCOMPARE(verdict.value(QStringLiteral("message")).toString(),
             QStringLiteral("A file with that name already exists"));

    verdict = actions.validateRename(dir.filePath(QStringLiteral("gone.mkv")),
                                     QStringLiteral("new"), siblings);
    QVERIFY(!verdict.value(QStringLiteral("ok")).toBool());
    QCOMPARE(verdict.value(QStringLiteral("message")).toString(),
             QStringLiteral("The file no longer exists"));
}

void ActionFeedbackTest::uniqueBaseNameSuffixesOnlyWhenTaken()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString clip = dir.filePath("clip.mp4");
    writeFile(clip);

    ActionsController actions;
    // Free target → the plain "(2)" candidate.
    QCOMPARE(actions.uniqueBaseName(clip, "clip"), QStringLiteral("clip (2)"));
    QCOMPARE(actions.uniqueBaseName(clip, "clip.mp4"), QStringLiteral("clip (2)"));

    // Taken targets are skipped.
    writeFile(dir.filePath("clip (2).mp4"));
    QCOMPARE(actions.uniqueBaseName(clip, "clip"), QStringLiteral("clip (3)"));
}

void ActionFeedbackTest::fileInfoReportsRealStats()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString clip = dir.filePath("clip.mp4");
    writeFile(clip, QByteArray(2048, 'z'));

    ActionsController actions;
    const QVariantMap info = actions.fileInfo(clip);

    QCOMPARE(info.value("path").toString(), clip);
    QCOMPARE(info.value("name").toString(), QStringLiteral("clip.mp4"));
    QCOMPARE(info.value("exists").toBool(), true);
    QCOMPARE(info.value("sizeBytes").toLongLong(), qint64(2048));
    QVERIFY(info.value("mtimeNs").toLongLong() > 0);
    QCOMPARE(info.value("permissions").toString().length(), 9);
    QVERIFY(!info.value("owner").toString().isEmpty());
    QVERIFY(!info.value("mimeType").toString().isEmpty());
    QCOMPARE(info.value("isDir").toBool(), false);

    // A missing path reports the truth instead of inventing fields.
    const QVariantMap missing = actions.fileInfo(dir.filePath("nope.mp4"));
    QCOMPARE(missing.value("exists").toBool(), false);
    QVERIFY(!missing.contains("sizeBytes"));
}

void ActionFeedbackTest::trashFailureIsNeverAPhantomSuccess()
{
    ActionsController actions;
    TrashService trash;
    actions.setTrashService(&trash);

    QSignalSpy finished(&actions, &ActionsController::actionFinished);
    QSignalSpy toasts(&actions, &ActionsController::toastRequested);

    actions.trashOne(QDir::tempPath() + QStringLiteral("/qg-does-not-exist-12345.mp4"));

    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toString(), QStringLiteral("trash"));
    QCOMPARE(finished.at(0).at(1).toBool(), false);
    QCOMPARE(toasts.count(), 1);
    QVERIFY(!toasts.at(0).at(0).toString().isEmpty());
    QCOMPARE(actions.canUndoTrash(), false);
    QCOMPARE(actions.undoLastTrash(), false); // nothing to undo
}

void ActionFeedbackTest::trashInfoFailureLeavesSourceInPlace()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataHome = dir.filePath("xdg");
    const QString trashDir = dataHome + QStringLiteral("/Trash");
    QVERIFY(QDir().mkpath(trashDir + QStringLiteral("/files")));
    qputenv("XDG_DATA_HOME", dataHome.toUtf8());
    qputenv("HOME", dir.path().toUtf8());

    const QString clip = dir.filePath("clip.mp4");
    writeFile(clip);
    const QString infoPath = trashDir + QStringLiteral("/info/clip.mp4.trashinfo");

    // Force the info-file open to fail even when tests run with elevated
    // permissions: a regular file blocks creation of Trash/info/*.trashinfo.
    writeFile(trashDir + QStringLiteral("/info"), QByteArrayLiteral("blocker"));
    TrashResult result = TrashService::trashFallback(clip);
    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(QFileInfo::exists(clip));
    QVERIFY(!QFileInfo::exists(trashDir + QStringLiteral("/files/clip.mp4")));

    QVERIFY(QFile::remove(trashDir + QStringLiteral("/info")));
    QVERIFY(QDir().mkpath(trashDir + QStringLiteral("/info")));
    writeFile(infoPath, QByteArrayLiteral("occupied"));
    result = TrashService::trashFallback(clip);
    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(QFileInfo::exists(clip));
    QVERIFY(!QFileInfo::exists(trashDir + QStringLiteral("/files/clip.mp4")));
    QCOMPARE(QFile(infoPath).size(), qint64(8));
}

void ActionFeedbackTest::trashRestoreRefusesOutsidePayload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataHome = dir.filePath("xdg");
    QVERIFY(QDir().mkpath(dataHome));
    qputenv("XDG_DATA_HOME", dataHome.toUtf8());
    qputenv("HOME", dir.path().toUtf8());

    const QString outside = dir.filePath("outside.mp4");
    writeFile(outside);
    QVERIFY(!TrashService::restoreFallback(outside, dir.filePath("restored.mp4")));
    QVERIFY(QFileInfo::exists(outside));
    QVERIFY(!QFileInfo::exists(dir.filePath("restored.mp4")));
}

void ActionFeedbackTest::trashThenUndoRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Keep the fixture and the trash on one filesystem so the move cannot fail
    // for a cross-device reason, and keep the user's real trash untouched.
    const QString dataHome = dir.filePath("xdg");
    QVERIFY(QDir().mkpath(dataHome));
    qputenv("XDG_DATA_HOME", dataHome.toUtf8());
    qputenv("HOME", dir.path().toUtf8());

    const QString clip = dir.filePath("clip.mp4");
    writeFile(clip);

    ActionsController actions;
    TrashService trash;
    actions.setTrashService(&trash);

    QSignalSpy toasts(&actions, &ActionsController::toastRequested);
    QSignalSpy finished(&actions, &ActionsController::actionFinished);

    actions.trashOne(clip);
    QCOMPARE(finished.at(0).at(1).toBool(), true);
    QVERIFY(!QFileInfo::exists(clip));
    QCOMPARE(actions.canUndoTrash(), true);

    // The toast carries the Undo affordance (the QML toast host reads this).
    QCOMPARE(toasts.count(), 1);
    QCOMPARE(toasts.at(0).at(1).toString(), QStringLiteral("trash"));

    QVERIFY(actions.undoLastTrash());
    QVERIFY(QFileInfo::exists(clip)); // restored in place
    QCOMPARE(actions.canUndoTrash(), false);
    // The restore announced itself (the Undo was not silent).
    QCOMPARE(toasts.count(), 2);
    QVERIFY(toasts.at(1).at(0).toString().contains(QStringLiteral("Restored")));
}

void ActionFeedbackTest::copyPathPutsThePathOnTheClipboard()
{
    if (QGuiApplication::clipboard() == nullptr) {
        QSKIP("no clipboard on this platform");
    }
    ActionsController actions;
    const QString path = QStringLiteral("/tmp/some folder/clip.mp4");
    actions.copyPath(path);
    QCOMPARE(QGuiApplication::clipboard()->text(), path);
}

void ActionFeedbackTest::clipboardCutPasteMovesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath("original.mp4");
    writeFile(source);

    FileOperationsService ops;
    ActionsController actions;
    actions.setFileOperations(&ops);

    // Cut then paste creates a move.
    actions.cut({source});
    QVERIFY(actions.clipboardIsCut());
    const QString targetDir = dir.filePath("target");
    QDir().mkpath(targetDir);

    QSignalSpy finishedSpy(&actions, &ActionsController::actionFinished);
    actions.paste(targetDir);
    QCOMPARE(actions.clipboardPaths().size(), 0); // cleared after paste
    QCOMPARE(actions.clipboardIsCut(), false);
    QTRY_COMPARE(finishedSpy.count(), 1); // typed as move (cut + paste)
    QCOMPARE(finishedSpy.at(0).at(0).toString(), QStringLiteral("move"));
    QVERIFY(QFileInfo::exists(targetDir + QDir::separator() + "original.mp4"));
    QVERIFY(!QFileInfo::exists(source));
}

void ActionFeedbackTest::clipboardCopyPasteCreatesDupe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath("original.mp4");
    writeFile(source);

    FileOperationsService ops;
    ActionsController actions;
    actions.setFileOperations(&ops);

    actions.copyToClipboard({source});
    QVERIFY(!actions.clipboardIsCut());
    const QString targetDir = dir.filePath("target");
    QDir().mkpath(targetDir);

    actions.paste(targetDir);
    QVERIFY(QFileInfo::exists(source)); // source retained
    QTRY_VERIFY(QFileInfo::exists(targetDir + QDir::separator() + "original.mp4")); // dupe
}

void ActionFeedbackTest::clipboardMoveVsCopyResolvesCorrectly()
{
    // The clipboard state (cut=true → move; cut=false → copy) determines the
    // operation, not any drag modifier. This verifies the contract.
    ActionsController actions;
    actions.cut({"/tmp/a.mp4"});
    QVERIFY(actions.clipboardIsCut());
    actions.copyToClipboard({"/tmp/b.mp4"});
    QVERIFY(!actions.clipboardIsCut());
}

void ActionFeedbackTest::permanentDeleteRemovesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath("doomed.mp4");
    writeFile(source);
    QVERIFY(QFileInfo::exists(source));

    FileOperationsService ops;
    ActionsController actions;
    actions.setFileOperations(&ops);

    QSignalSpy finished(&actions, &ActionsController::actionFinished);
    QSignalSpy toasts(&actions, &ActionsController::toastRequested);

    actions.deletePermanently({source});

    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toString(), QStringLiteral("delete"));
    QCOMPARE(finished.at(0).at(1).toBool(), true);
    QCOMPARE(toasts.count(), 0); // success is not a failure toast
    QVERIFY(!QFileInfo::exists(source)); // file really gone, not just the signal
}

void ActionFeedbackTest::permanentDeleteMissingPathFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ghost = dir.filePath("never-existed.mp4");
    const QString survivor = dir.filePath("survivor.mp4");
    writeFile(survivor);

    FileOperationsService ops;
    ActionsController actions;
    actions.setFileOperations(&ops);

    QSignalSpy finished(&actions, &ActionsController::actionFinished);
    QSignalSpy toasts(&actions, &ActionsController::toastRequested);

    actions.deletePermanently({ghost});

    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(1).toBool(), false); // missing is a failure, not success
    QCOMPARE(toasts.count(), 1);                     // exactly one toast
    QVERIFY(QFileInfo::exists(survivor));            // nothing else touched
}

void ActionFeedbackTest::permanentDeleteMultiSelectPartialSummary()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString a = dir.filePath("a.mp4");
    const QString b = dir.filePath("b.mp4");
    const QString ghost = dir.filePath("gone.mp4");
    writeFile(a);
    writeFile(b);

    FileOperationsService ops;
    ActionsController actions;
    actions.setFileOperations(&ops);

    // (a) full set: both gone, one success.
    {
        QSignalSpy finished(&actions, &ActionsController::actionFinished);
        actions.deletePermanently({a, b});
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(1).toBool(), true);
        QVERIFY(!QFileInfo::exists(a));
        QVERIFY(!QFileInfo::exists(b));
    }

    // (b) mixed set: one present, one missing → partial, survivor intact.
    const QString c = dir.filePath("c.mp4");
    writeFile(c);
    {
        QSignalSpy finished(&actions, &ActionsController::actionFinished);
        actions.deletePermanently({c, ghost});
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(1).toBool(), false); // partial → overall failure
        QVERIFY(!QFileInfo::exists(c));                 // present one removed
    }
}

void ActionFeedbackTest::permanentDeleteModelRemoveIsIdempotent()
{
    qg::FolderModel model;
    qg::FolderSnapshot snap;
    qg::Entry e1;
    e1.kind = qg::EntryKind::Video;
    e1.identity = QStringLiteral("inode:9:9001");
    e1.path = QStringLiteral("/v/a.mp4");
    e1.name = QStringLiteral("a.mp4");
    qg::Entry e2;
    e2.kind = qg::EntryKind::Video;
    e2.identity = QStringLiteral("inode:9:9002");
    e2.path = QStringLiteral("/v/b.mp4");
    e2.name = QStringLiteral("b.mp4");
    snap.entries = {e1, e2};
    model.applySnapshot(snap);
    QCOMPARE(model.count(), 2);

    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QVERIFY(model.removeByIdentity(QStringLiteral("inode:9:9001")));
    QCOMPARE(removed.count(), 1);          // exactly one targeted rowsRemoved
    QCOMPARE(model.count(), 1);

    QVERIFY(!model.removeByIdentity(QStringLiteral("inode:9:9001"))); // idempotent no-op
    QCOMPARE(removed.count(), 1);          // no second rowsRemoved
    QCOMPARE(model.count(), 1);
}

QTEST_MAIN(ActionFeedbackTest)
#include "test_action_feedback.moc"
