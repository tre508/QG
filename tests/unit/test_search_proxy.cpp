// SearchProxy contract tests (Prompt 40 / REMEDIATION-001).
// Covers filter updates on query change, match-range role correctness,
// the 2,000-row cap, and reset on empty query.

#include <QtTest/QtTest>
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

void fillModel(FolderModel* model, int videoCount, const QStringList& names = {})
{
    FolderSnapshot snapshot;
    for (int i = 0; i < videoCount; ++i) {
        const QString n = i < names.size() ? names.at(i)
                                           : QStringLiteral("clip_%1.mp4").arg(i);
        snapshot.entries.append(makeVideo(n, QStringLiteral("inode:1:%1").arg(i + 1)));
    }
    model->applySnapshot(snapshot);
}

} // namespace

class SearchProxyTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyQueryShowsEverything();
    void queryFiltersRows();
    void queryChangeRefreshesResults();
    void matchRangeRoleExposed();
    void rowCapEnforced();
};

void SearchProxyTest::emptyQueryShowsEverything()
{
    FolderModel model;
    fillModel(&model, 10);
    SearchProxy proxy;
    proxy.setSourceModel(&model);
    proxy.setQuery(QString());
    QCOMPARE(proxy.rowCount(), 10);
}

void SearchProxyTest::queryFiltersRows()
{
    FolderModel model;
    fillModel(&model, 5, {QStringLiteral("alpha.mp4"),
                                      QStringLiteral("beta.mp4"),
                                      QStringLiteral("gamma.mp4"),
                                      QStringLiteral("delta.mp4"),
                                      QStringLiteral("alphabet.mp4")});
    SearchProxy proxy;
    proxy.setSourceModel(&model);
    proxy.setQuery(QStringLiteral("alpha"));
    QCOMPARE(proxy.rowCount(), 2); // alpha.mp4 + alphabet.mp4
    proxy.setQuery(QStringLiteral("zzz"));
    QCOMPARE(proxy.rowCount(), 0);
}

void SearchProxyTest::queryChangeRefreshesResults()
{
    FolderModel model;
    fillModel(&model, 4, {QStringLiteral("sun.mp4"),
                                      QStringLiteral("moon.mp4"),
                                      QStringLiteral("sunset.mp4"),
                                      QStringLiteral("star.mp4")});
    SearchProxy proxy;
    proxy.setSourceModel(&model);
    proxy.setQuery(QStringLiteral("sun"));
    QCOMPARE(proxy.rowCount(), 2);
    proxy.setQuery(QStringLiteral("moon"));
    QCOMPARE(proxy.rowCount(), 1);
    proxy.setQuery(QString());
    QCOMPARE(proxy.rowCount(), 4);
}

void SearchProxyTest::matchRangeRoleExposed()
{
    FolderModel model;
    fillModel(&model, 2, {QStringLiteral("beach_sunset.mp4"),
                                      QStringLiteral("other.mp4")});
    SearchProxy proxy;
    proxy.setSourceModel(&model);
    proxy.setQuery(QStringLiteral("sunset"));

    bool found = false;
    for (int row = 0; row < proxy.rowCount(); ++row) {
        const QModelIndex idx = proxy.index(row, 0);
        const QVariant v = proxy.data(idx, SearchProxy::MatchRangesRole);
        QVERIFY(v.isValid());
        const QVector<MatchRange> ranges = v.value<QVector<MatchRange>>();
        if (proxy.data(idx, SearchProxy::EntryFileNameRole).toString() ==
            QStringLiteral("beach_sunset.mp4")) {
            QCOMPARE(ranges.size(), 1);
            QCOMPARE(ranges.first().start, 6);
            QCOMPARE(ranges.first().length, 6);
            found = true;
        }
    }
    QVERIFY(found);
}

void SearchProxyTest::rowCapEnforced()
{
    FolderModel model;
    fillModel(&model, SearchProxy::kRowCap + 5);
    SearchProxy proxy;
    proxy.setSourceModel(&model);
    // Row cap applies to the filtered result set; with an empty query the cap
    // is intentionally bypassed (filterAcceptsRow early-returns for empty).
    proxy.setQuery(QStringLiteral("clip"));
    QVERIFY(proxy.rowCount() <= SearchProxy::kRowCap);
    // Note: cap text is only claimed once results reach the cap.
    if (proxy.rowCount() >= SearchProxy::kRowCap)
        QVERIFY(proxy.showingNote().contains(QStringLiteral("2,000")));
}

#include "test_search_proxy.moc"
QTEST_MAIN(SearchProxyTest)