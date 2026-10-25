// BrowserController tests (Prompt 13).

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "controllers/browser_controller.hpp"

using namespace qg;

namespace {

void writeFile(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("x");
    }
}

} // namespace

class BrowserControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void navigatePopulatesModel();
    void backAndForwardHistory();
    void navigateTruncatesForwardStack();
    void invalidPathDoesNotNavigate();
    void parentGoesUp();
    void accessDeniedShowsOverlay(); // REM-24: real EACCES fixture
    void rapidNavigationCancelsStaleResults();
};

void BrowserControllerTest::navigatePopulatesModel()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    QVERIFY(root.mkdir(QStringLiteral("sub")));
    writeFile(root.filePath(QStringLiteral("clip.mp4")));
    writeFile(root.filePath(QStringLiteral("notes.txt")));

    BrowserController controller;
    QVERIFY(controller.navigate(dir.path()));
    QCOMPARE(controller.currentPath(), QDir::cleanPath(dir.path()));

    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    // Folder + one supported video; unsupported notes.txt is filtered.
    QCOMPARE(controller.model()->rowCount(), 2);
}

void BrowserControllerTest::backAndForwardHistory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    QVERIFY(root.mkdir(QStringLiteral("sub")));

    BrowserController controller;
    QVERIFY(controller.navigate(dir.path()));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(!controller.canGoBack());

    QVERIFY(controller.navigate(root.filePath(QStringLiteral("sub"))));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.canGoBack());
    QVERIFY(!controller.canGoForward());

    controller.back();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.currentPath(), QDir::cleanPath(dir.path()));
    QVERIFY(controller.canGoForward());

    controller.forward();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.currentPath(), QDir::cleanPath(root.filePath(QStringLiteral("sub"))));
}

void BrowserControllerTest::navigateTruncatesForwardStack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    QVERIFY(root.mkdir(QStringLiteral("a")));
    QVERIFY(root.mkdir(QStringLiteral("b")));
    QVERIFY(root.mkdir(QStringLiteral("c")));

    BrowserController controller;
    QVERIFY(controller.navigate(root.filePath(QStringLiteral("a"))));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.navigate(root.filePath(QStringLiteral("b"))));
    QTRY_VERIFY(!controller.busy());
    controller.back();
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.canGoForward());

    // A new navigation to a different folder must drop the forward stack.
    QVERIFY(controller.navigate(root.filePath(QStringLiteral("c"))));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(!controller.canGoForward());
}

void BrowserControllerTest::invalidPathDoesNotNavigate()
{
    QDir existing(QDir::tempPath());
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(QDir(dir.path()).filePath(QStringLiteral("clip.mp4")));

    BrowserController controller;
    QVERIFY(controller.navigate(dir.path()));
    QTRY_VERIFY(!controller.busy());
    const QString before = controller.currentPath();

    QSignalSpy errors(&controller, &BrowserController::errorOccurred);
    QVERIFY(!controller.navigate(QStringLiteral("/definitely/not/here")));
    QCOMPARE(errors.count(), 1);
    QCOMPARE(controller.currentPath(), before);
}

void BrowserControllerTest::parentGoesUp()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());
    QVERIFY(root.mkdir(QStringLiteral("sub")));

    BrowserController controller;
    QVERIFY(controller.navigate(root.filePath(QStringLiteral("sub"))));
    QTRY_VERIFY(!controller.busy());

    controller.parent();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.currentPath(), QDir::cleanPath(dir.path()));
}

void BrowserControllerTest::accessDeniedShowsOverlay()
{
    // REM-24: Real EACCES fixture — create a directory with no permissions (chmod 000)
    // Must run in a QTemporaryDir under /tmp (tmpfs) where chmod works reliably.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir root(dir.path());

    // Create a subdirectory and remove all permissions
    QVERIFY(root.mkdir(QStringLiteral("noaccess")));
    const QString noaccessPath = root.filePath(QStringLiteral("noaccess"));

    // chmod 000 - remove all permissions
    QFile::setPermissions(noaccessPath, QFileDevice::Permissions(0));

    // Verify permissions are actually 000
    QFileInfo info(noaccessPath);
    QVERIFY(!(info.permissions() & QFileDevice::ReadOwner));
    QVERIFY(!(info.permissions() & QFileDevice::WriteOwner));
    QVERIFY(!(info.permissions() & QFileDevice::ExeOwner));

    BrowserController controller;
    QSignalSpy errors(&controller, &BrowserController::errorOccurred);

    // Navigate to the no-access directory - should fail with permission error
    QVERIFY(!controller.navigate(noaccessPath));

    // The errorOccurred signal should fire with a permission-denied message
    QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 5000);
    const QString errorMessage = errors.takeFirst().at(1).toString();
    QVERIFY(errorMessage.toLower().contains("permission denied") || errorMessage.toLower().contains("denied"));

    // Cleanup: MUST restore permissions before QTemporaryDir destructor runs
    // (otherwise the temp dir cleanup will fail because it can't remove the directory)
    QFile::setPermissions(noaccessPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

void BrowserControllerTest::rapidNavigationCancelsStaleResults()
{
    // Create 20 nested folders with video files
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir root(dir.path());
    
    QStringList folderPaths;
    QString current = dir.path();
    for (int i = 0; i < 20; ++i) {
        QString folderName = QStringLiteral("folder_%1").arg(i, 2, 10, QLatin1Char('0'));
        QVERIFY(QDir(current).mkdir(QStringLiteral("sub")));
        current = QDir(current).filePath(QStringLiteral("sub"));
        folderPaths.append(current);
        // Add a video file in each folder
        writeFile(QDir(current).filePath(QStringLiteral("video_%1.mp4").arg(i)));
    }
    
    BrowserController controller;
    
    // Rapidly navigate through all 20 folders without waiting for completion
    for (int i = 0; i < 20; ++i) {
        controller.navigate(folderPaths[i]);
        // Don't wait for completion - trigger next navigation immediately
        if (i < 19) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
    }
    
    // Wait for the final navigation to complete
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
    
    // Verify only the final folder's content is displayed
    QCOMPARE(controller.currentPath(), QDir::cleanPath(folderPaths.last()));
    QCOMPARE(controller.model()->rowCount(), 1); // 1 video file (no subfolder in final folder)
    
    // Verify no stale results from earlier navigations
    const auto model = controller.model();
    for (int i = 0; i < model->rowCount(); ++i) {
        const auto entry = model->entryAt(i);
        QVERIFY(entry);
        // All entries should belong to the final folder
        QVERIFY(entry->path.startsWith(folderPaths.last()));
    }
}

QTEST_GUILESS_MAIN(BrowserControllerTest)
#include "test_browser_controller.moc"
