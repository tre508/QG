#include "search_service.hpp"
#include <algorithm>

SearchService::SearchService() = default;

QVector<SearchResult> SearchService::search(
    const SearchSpec& spec,
    const QVector<QPair<QString, bool>>& entries) const
{
    QVector<SearchResult> results;
    if (spec.query.isEmpty()) return results;

    const QString lcQuery = spec.query.toLower();

    for (int i = 0; i < entries.size(); ++i) {
        const auto& [filename, isFolder] = entries[i];
        const QString lcFilename = filename.toLower();
        if (!lcFilename.contains(lcQuery)) continue;

        SearchResult r;
        r.modelRow = i;
        r.filename = filename;
        r.isFolder = isFolder;
        r.rank = computeRank(lcFilename, lcQuery);
        r.matchRanges = findMatchRanges(lcFilename, lcQuery);
        results.append(r);
    }

    // Sort: folders-first, then descending rank (higher = better match)
    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        if (a.isFolder != b.isFolder) return a.isFolder > b.isFolder;
        if (a.rank != b.rank) return a.rank > b.rank;
        return a.filename < b.filename;
    });

    return results;
}

int SearchService::computeRank(QStringView filename, QStringView query) const {
    // Base: match present = 1 point.
    // Prefix boost: if filename starts with query → +10
    // Word-boundary boost: if query matches at a word start (after space/underscore/hyphen/dot) → +5 per occurrence
    int rank = 1;

    if (filename.startsWith(query, Qt::CaseInsensitive))
        rank += 10;

    // Word boundary: scan for query occurring after a non-alphanumeric boundary
    char prev = '\0';
    int pos = 0;
    while (pos < filename.size()) {
        const int idx = filename.indexOf(query, pos, Qt::CaseInsensitive);
        if (idx < 0) break;
        if (idx == 0 || !filename[idx - 1].isLetterOrNumber())
            rank += 5;
        pos = idx + 1;
    }

    return rank;
}

QVector<MatchRange> SearchService::findMatchRanges(QStringView filename, QStringView query) const {
    QVector<MatchRange> ranges;
    int pos = 0;
    while (pos < filename.size()) {
        const int idx = filename.indexOf(query, pos, Qt::CaseInsensitive);
        if (idx < 0) break;
        MatchRange r;
        r.start = idx;
        r.length = query.size();
        ranges.append(r);
        pos = idx + 1;
    }
    return ranges;
}