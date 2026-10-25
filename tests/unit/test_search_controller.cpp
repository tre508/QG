// SearchController contract tests (Prompt 41 / REMEDIATION-001).
// Covers debounced query updates, clear/restore, scope chip, and summary.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "controllers/search_controller.hpp"
#include "models/folder_model.hpp"
#include "models/search_proxy.hpp"

using namespace qg;

namespace {

Entry makeVideo(const QString& name, const QString& identity)
{
    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = name;
    entry.path = QStringLiteral("/v/") + name;
    entry.identity = identity;
    entry.size = 1024;
    entry.mtimeNs = 42;
    return entry;
}

} // namespace

class SearchControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void debouncedQueryUpdatesResults();
    void clearRestoresFullModel();
    void summaryIncludesCountAndQuery();
    void scopeIsThisFolder();
    void liveCountAgreesWithSummary();
    void rapidTypingCoalescesToFinalQuery();
};

void SearchControllerTest::debouncedQueryUpdatesResults()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("sunset.mp4"), QStringLiteral("inode:1:1")),
                        makeVideo(QStringLiteral("moon.mp4"), QStringLiteral("inode:1:2"))};
    model.applySnapshot(snapshot);

    SearchProxy proxy;
    proxy.setSourceModel(&model);

    SearchController controller;
    controller.setProxy(&proxy);
    controller.setFolderName(QStringLiteral("Summer trip"));
    controller.setSortLabel(QStringLiteral("Name \u2191"));

    QSignalSpy activeSpy(&controller, &SearchController::activeChanged);
    QSignalSpy resultsSpy(&controller, &SearchController::resultsChanged);

    controller.setQuery(QStringLiteral("sun"));
    QVERIFY(proxy.rowCount() == 0 || proxy.rowCount() == 2); // debounce pending

    // The 80 ms debounce must flush within a short wait.
    QTRY_VERIFY_WITH_TIMEOUT(activeSpy.count() >= 1, 500);
    QTRY_VERIFY_WITH_TIMEOUT(resultsSpy.count() >= 1, 500);
    QCOMPARE(proxy.rowCount(), 1);
    QVERIFY(activeSpy.takeFirst().at(0).toBool());
}

void SearchControllerTest::clearRestoresFullModel()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("inode:1:1")),
                        makeVideo(QStringLiteral("b.mp4"), QStringLiteral("inode:1:2"))};
    model.applySnapshot(snapshot);

    SearchProxy proxy;
    proxy.setSourceModel(&model);
    SearchController controller;
    controller.setProxy(&proxy);

    controller.setQuery(QStringLiteral("a"));
    QTRY_VERIFY_WITH_TIMEOUT(proxy.rowCount() == 1, 500);

    controller.clear();
    QCOMPARE(proxy.rowCount(), 2);
    QCOMPARE(proxy.query(), QString());
}

void SearchControllerTest::summaryIncludesCountAndQuery()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("sunset.mp4"), QStringLiteral("inode:1:1")),
                        makeVideo(QStringLiteral("moon.mp4"), QStringLiteral("inode:1:2"))};
    model.applySnapshot(snapshot);

    SearchProxy proxy;
    proxy.setSourceModel(&model);
    SearchController controller;
    controller.setProxy(&proxy);
    controller.setFolderName(QStringLiteral("Summer trip"));
    controller.setSortLabel(QStringLiteral("Name \u2191"));

    controller.setQuery(QStringLiteral("sun"));
    QTRY_VERIFY_WITH_TIMEOUT(proxy.rowCount() == 1, 500);
    const QString s = controller.summary();
    QVERIFY(s.contains(QStringLiteral("1 results for \"sun\"")));
    QVERIFY(s.contains(QStringLiteral("Summer trip")));
    QVERIFY(s.contains(QStringLiteral("Name \u2191")));
}

void SearchControllerTest::scopeIsThisFolder()
{
    SearchController controller;
    QCOMPARE(controller.scope(), QStringLiteral("This folder"));
}

void SearchControllerTest::liveCountAgreesWithSummary()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("sunset.mp4"), QStringLiteral("inode:1:1")),
                        makeVideo(QStringLiteral("sunrise.mp4"), QStringLiteral("inode:1:2")),
                        makeVideo(QStringLiteral("moon.mp4"), QStringLiteral("inode:1:3"))};
    model.applySnapshot(snapshot);

    SearchProxy proxy;
    proxy.setSourceModel(&model);
    SearchController controller;
    controller.setProxy(&proxy);

    // The proxy's bindable resultCount must equal the count the summary claims.
    controller.setQuery(QStringLiteral("sun"));
    QTRY_VERIFY_WITH_TIMEOUT(proxy.rowCount() == 2, 500);
    QCOMPARE(proxy.resultCount(), 2);
    QVERIFY(controller.summary().startsWith(QStringLiteral("2 results for \"sun\"")));
}

void SearchControllerTest::rapidTypingCoalescesToFinalQuery()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("sunset.mp4"), QStringLiteral("inode:1:1")),
                        makeVideo(QStringLiteral("moon.mp4"), QStringLiteral("inode:1:2"))};
    model.applySnapshot(snapshot);

    SearchProxy proxy;
    proxy.setSourceModel(&model);
    SearchController controller;
    controller.setProxy(&proxy);

    QSignalSpy resultsSpy(&controller, &SearchController::resultsChanged);

    // Rapid keystrokes within the debounce window: only the final query applies.
    controller.setQuery(QStringLiteral("s"));
    controller.setQuery(QStringLiteral("su"));
    controller.setQuery(QStringLiteral("sun"));

    QTRY_VERIFY_WITH_TIMEOUT(proxy.query() == QStringLiteral("sun"), 500);
    QCOMPARE(controller.query(), QStringLiteral("sun"));
    QCOMPARE(proxy.resultCount(), 1);
    // One debounced flush, not one per keystroke.
    QCOMPARE(resultsSpy.count(), 1);
}

#include "test_search_controller.moc"
QTEST_MAIN(SearchControllerTest)