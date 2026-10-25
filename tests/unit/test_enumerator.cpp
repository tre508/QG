// Filesystem enumerator tests (Prompt 09).
//
// Uses temporary directories only. Verifies filtering, per-entry errors,
// hidden-file handling, incremental batching, and cancellation.

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <unistd.h>

#include "filesystem/enumerator.hpp"

using namespace qg;

namespace {

void writeFile(const QString &path, const QByteArray &data = QByteArrayLiteral("x"))
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
    }
}

int countKind(const FolderSnapshot &snapshot, EntryKind kind)
{
    int count = 0;
    for (const Entry &entry : snapshot.entries) {
        if (entry.kind == kind) {
            ++count;
        }
    }
    return count;
}

} // namespace

class EnumeratorTest : public QObject
{
    Q_OBJECT

private slots:
    void filtersToFoldersAndVideos();
    void folderOnlyPlusUnsupportedReturnsFolder();
    void brokenSymlinkRecordsErrorWithoutAborting();
    void hiddenFilesRespectOption();
    void batchesLargeDirectory();
    void cancellationReturnsPartial();
};

void EnumeratorTest::filtersToFoldersAndVideos()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());

    QVERIFY(root.mkdir(QStringLiteral("sub")));
    writeFile(root.filePath(QStringLiteral("clip.mp4")));
    writeFile(root.filePath(QStringLiteral("notes.txt")));
    writeFile(root.filePath(QStringLiteral("photo.jpg")));
    writeFile(root.filePath(QStringLiteral("noext")));

    FilesystemEnumerator enumerator;
    TaskToken token;
    const FolderSnapshot snapshot = enumerator.enumerate(dir.path(), token);

    QVERIFY(snapshot.complete);
    QCOMPARE(snapshot.entries.size(), 2);
    QCOMPARE(countKind(snapshot, EntryKind::Folder), 1);
    QCOMPARE(countKind(snapshot, EntryKind::Video), 1);
    QVERIFY(snapshot.errors.isEmpty());
}

void EnumeratorTest::folderOnlyPlusUnsupportedReturnsFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    QVERIFY(root.mkdir(QStringLiteral("only_sub")));
    writeFile(root.filePath(QStringLiteral("a.txt")));
    writeFile(root.filePath(QStringLiteral("b.jpg")));

    FilesystemEnumerator enumerator;
    TaskToken token;
    const FolderSnapshot snapshot = enumerator.enumerate(dir.path(), token);

    QCOMPARE(snapshot.entries.size(), 1);
    QCOMPARE(snapshot.entries.first().name, QStringLiteral("only_sub"));
    QCOMPARE(snapshot.entries.first().kind, EntryKind::Folder);
}

void EnumeratorTest::brokenSymlinkRecordsErrorWithoutAborting()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    const QString link = root.filePath(QStringLiteral("dangling.mp4"));
    QVERIFY(::symlink("does-not-exist.mp4", QFile::encodeName(link).constData()) == 0);
    writeFile(root.filePath(QStringLiteral("real.mp4")));

    FilesystemEnumerator enumerator;
    TaskToken token;
    const FolderSnapshot snapshot = enumerator.enumerate(dir.path(), token);

    QVERIFY(snapshot.complete);
    QCOMPARE(snapshot.entries.size(), 1);
    QCOMPARE(snapshot.entries.first().name, QStringLiteral("real.mp4"));
    QCOMPARE(snapshot.errors.size(), 1);
    QCOMPARE(snapshot.errors.first().path, link);
}

void EnumeratorTest::hiddenFilesRespectOption()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    writeFile(root.filePath(QStringLiteral(".hidden.mp4")));
    QVERIFY(root.mkdir(QStringLiteral(".hiddenDir")));
    writeFile(root.filePath(QStringLiteral("visible.mp4")));

    FilesystemEnumerator enumerator;
    TaskToken token;

    const FolderSnapshot defaults = enumerator.enumerate(dir.path(), token);
    QCOMPARE(defaults.entries.size(), 1);
    QCOMPARE(defaults.entries.first().name, QStringLiteral("visible.mp4"));

    EnumeratorOptions showHidden;
    showHidden.showHidden = true;
    const FolderSnapshot all = enumerator.enumerate(dir.path(), token, showHidden);
    QCOMPARE(all.entries.size(), 3);
}

void EnumeratorTest::batchesLargeDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    constexpr int kCount = 5000;
    for (int i = 0; i < kCount; ++i) {
        writeFile(root.filePath(QStringLiteral("clip_%1.mp4").arg(i, 5, 10, QLatin1Char('0'))));
    }

    FilesystemEnumerator enumerator;
    TaskToken token;
    EnumeratorOptions options;
    options.batchSize = 200;

    int batches = 0;
    connect(&enumerator, &FilesystemEnumerator::batchReady, this,
            [&batches](const FolderSnapshot &) { ++batches; });

    const FolderSnapshot snapshot = enumerator.enumerate(dir.path(), token, options);

    QVERIFY(snapshot.complete);
    QCOMPARE(snapshot.entries.size(), kCount);
    QCOMPARE(batches, (kCount + 199) / 200);
}

void EnumeratorTest::cancellationReturnsPartial()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    for (int i = 0; i < 100; ++i) {
        writeFile(root.filePath(QStringLiteral("clip_%1.mp4").arg(i)));
    }

    FilesystemEnumerator enumerator;
    TaskToken token;
    token.cancel();

    const FolderSnapshot snapshot = enumerator.enumerate(dir.path(), token);
    QVERIFY(!snapshot.complete);
    QVERIFY(snapshot.entries.size() < 100);
}

QTEST_GUILESS_MAIN(EnumeratorTest)
#include "test_enumerator.moc"
