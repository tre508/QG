// SearchService contract tests (Prompt 39 / REMEDIATION-001).
// Covers substring matching, case-insensitivity, prefix/word-boundary boosts,
// matched ranges, folders-first ranking, and empty-query behavior.

#include <QtTest/QtTest>
#include "search/search_service.hpp"

using namespace qg;

namespace {
QVector<QPair<QString, bool>> entries()
{
    return {
        {"sunset_over_sea.mp4", false},
        {"SUNSET_DOCUMENTARY.mkv", false},
        {"beach_sunset_timelapse.webm", false},
        {"sun_trip", true}, // folder: contains "sun", not "sunset"
        {"sunrise_clip.mp4", false},
        {"notes.txt.txt", false},
    };
}
} // namespace

class SearchServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void substringMatchIsCaseInsensitive();
    void emptyQueryReturnsNothing();
    void prefixAndWordBoundaryBoostRanking();
    void foldersFirstRanking();
    void matchRangesComputed();
};

void SearchServiceTest::substringMatchIsCaseInsensitive()
{
    SearchService service;
    const auto results = service.search(SearchSpec{QStringLiteral("SUNSET")}, entries());
    QCOMPARE(results.size(), 3); // 3 videos; sun_trip does not contain "sunset"
    for (const SearchResult& r : results) {
        QVERIFY(r.filename.toLower().contains(QStringLiteral("sunset")));
    }
}

void SearchServiceTest::emptyQueryReturnsNothing()
{
    SearchService service;
    const auto results = service.search(SearchSpec{QString()}, entries());
    QCOMPARE(results.size(), 0);
}

void SearchServiceTest::prefixAndWordBoundaryBoostRanking()
{
    SearchService service;
    // "sunset_over" matches the prefix of "sunset_over_sea.mp4" and a word
    // boundary after the underscore in "beach_sunset_timelapse.webm".
    const auto results = service.search(SearchSpec{QStringLiteral("sunset")}, entries());

    int prefixRow = -1, wordRow = -1, plainRow = -1;
    for (const SearchResult& r : results) {
        if (r.filename == QStringLiteral("sunset_over_sea.mp4"))
            prefixRow = r.rank;
        else if (r.filename == QStringLiteral("beach_sunset_timelapse.webm"))
            wordRow = r.rank;
        else if (r.filename == QStringLiteral("sunrise_clip.mp4"))
            plainRow = r.rank;
    }
    // Prefix boost (base 1 + 10) dominates word-boundary (base 1 + 5),
    // which dominates a plain mid-name match (base 1).
    QVERIFY2(prefixRow > wordRow, "prefix boost above word-boundary boost");
    QVERIFY2(wordRow > plainRow, "word-boundary boost above plain match");
}

void SearchServiceTest::foldersFirstRanking()
{
    SearchService service;
    const auto results = service.search(SearchSpec{QStringLiteral("sun")}, entries());
    QVERIFY(results.size() >= 2);
    // "summer_trip" is a folder and must sort above every video match.
    QCOMPARE(results.first().isFolder, true);
    QCOMPARE(results.first().filename, QStringLiteral("sun_trip"));
}

void SearchServiceTest::matchRangesComputed()
{
    SearchService service;
    const auto results = service.search(SearchSpec{QStringLiteral("sunset")}, entries());
    QVERIFY(results.size() >= 1);
    for (const SearchResult& r : results) {
        QVERIFY(r.matchRanges.size() >= 1);
        for (const MatchRange& range : r.matchRanges) {
            QVERIFY(range.length == QStringLiteral("sunset").size());
            QVERIFY(r.filename.mid(range.start, range.length)
                        .compare(QStringLiteral("sunset"), Qt::CaseInsensitive) == 0);
        }
    }
}

#include "test_search_service.moc"
QTEST_MAIN(SearchServiceTest)