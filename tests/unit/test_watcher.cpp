// Directory watcher + identity diff tests (Prompt 12).

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "filesystem/enumerator.hpp"
#include "filesystem/watcher.hpp"
#include "models/folder_model.hpp"

using namespace qg;

namespace {

void writeFile(const QString &path, const QByteArray &data = QByteArrayLiteral("x"))
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
    }
}

QList<Entry> enumerateNow(const QString &dir, TaskToken *token)
{
    FilesystemEnumerator enumerator;
    return enumerator.enumerate(dir, *token).entries;
}

} // namespace

class WatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void diffReportsRemovedChangedAdded();
    void watcherDebouncesChange();
    void modelDiffPreservesUnaffectedRows();
};

void WatcherTest::diffReportsRemovedChangedAdded()
{
    Entry a;
    a.identity = QStringLiteral("a");
    a.name = QStringLiteral("a.mp4");
    a.path = QStringLiteral("/d/a.mp4");
    a.kind = EntryKind::Video;
    a.size = 10;
    a.mtimeNs = 100;

    Entry b = a;
    b.identity = QStringLiteral("b");
    b.name = QStringLiteral("b.mp4");
    b.path = QStringLiteral("/d/b.mp4");

    Entry aChanged = a; // same identity, new size
    aChanged.size = 99;

    Entry c = a;
    c.identity = QStringLiteral("c");
    c.name = QStringLiteral("c.mp4");
    c.path = QStringLiteral("/d/c.mp4");

    const FolderDiff diff = computeFolderDiff({a, b}, {aChanged, c});

    QCOMPARE(diff.removedIdentities, QStringList{QStringLiteral("b")});
    QCOMPARE(diff.added.size(), 1);
    QCOMPARE(diff.added.first().identity, QStringLiteral("c"));
    QCOMPARE(diff.changed.size(), 1);
    QCOMPARE(diff.changed.first().identity, QStringLiteral("a"));
}

void WatcherTest::watcherDebouncesChange()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DirectoryWatcher watcher;
    watcher.setPath(dir.path());
    watcher.setDebounceInterval(50);

    QSignalSpy changed(&watcher, &DirectoryWatcher::changed);
    QVERIFY(changed.isValid());

    writeFile(QDir(dir.path()).filePath(QStringLiteral("new.mp4")));

    QTRY_VERIFY_WITH_TIMEOUT(changed.count() >= 1, 3000);
    QCOMPARE(changed.first().at(0).toString(), QDir::cleanPath(dir.path()));
}

void WatcherTest::modelDiffPreservesUnaffectedRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    writeFile(root.filePath(QStringLiteral("keep.mp4")));
    writeFile(root.filePath(QStringLiteral("gone.mp4")));

    TaskToken token;
    FolderModel model;
    FolderSnapshot initial;
    initial.entries = enumerateNow(dir.path(), &token);
    model.applySnapshot(initial);
    QCOMPARE(model.rowCount(), 2);

    // External change: remove one file, add another.
    QVERIFY(QFile::remove(root.filePath(QStringLiteral("gone.mp4"))));
    writeFile(root.filePath(QStringLiteral("added.mp4")));

    const QList<Entry> next = enumerateNow(dir.path(), &token);
    const FolderDiff diff = computeFolderDiff(model.entries(), next);

    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    model.applyDiff(diff);

    QCOMPARE(removed.count(), 1);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(model.rowCount(), 2);

    // The unaffected file is still present (order of the original row is kept).
    bool keptPresent = false;
    for (const Entry &entry : model.entries()) {
        if (entry.name == QStringLiteral("keep.mp4")) {
            keptPresent = true;
        }
    }
    QVERIFY(keptPresent);
}

QTEST_GUILESS_MAIN(WatcherTest)
#include "test_watcher.moc"
