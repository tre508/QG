#pragma once
#include <QPair>
#include <QString>
#include <QVector>
#include <QStringView>
#include "core/types.hpp"

// Matched position range within a filename.
struct MatchRange {
    int start = 0;
    int length = 0;
};
Q_DECLARE_METATYPE(MatchRange)
Q_DECLARE_METATYPE(QVector<MatchRange>)

// Single search result entry.
struct SearchResult {
    int modelRow = -1;
    QString filename;
    int rank = 0;                    // higher = better match
    QVector<MatchRange> matchRanges; // positions of query substring hits
    bool isFolder = false;
};

// Search spec — transient query + scope hint.
struct SearchSpec {
    QString query;
    bool recursive = false;          // reserved for later; not implemented in MVC
};

class SearchService {
public:
    SearchService();

    // Search over a flat list of (filename, isFolder) entries. No disk I/O.
    // Returns results sorted by rank (descending), folders-first within ties.
    QVector<SearchResult> search(const SearchSpec& spec,
                                 const QVector<QPair<QString, bool>>& entries) const;

private:
    int computeRank(QStringView filename, QStringView query) const;
    QVector<MatchRange> findMatchRanges(QStringView filename, QStringView query) const;
};