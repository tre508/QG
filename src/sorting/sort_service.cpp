#include "sorting/sort_service.hpp"

#include <QCollator>
#include <QString>

#include <algorithm>
#include <optional>

namespace qg {
namespace {

int compareInt64(qint64 a, qint64 b)
{
    return (a < b) ? -1 : (a > b ? 1 : 0);
}

int compareBool(bool a, bool b)
{
    return compareInt64(a ? 1 : 0, b ? 1 : 0);
}

// Unknown durations sort after any known duration when ascending; the caller's
// direction reversal then places them first when descending.
int compareDuration(const Entry &a, const Entry &b)
{
    const bool knownA = a.durationMs.has_value();
    const bool knownB = b.durationMs.has_value();
    if (knownA && knownB) {
        return compareInt64(*a.durationMs, *b.durationMs);
    }
    if (knownA) {
        return -1; // known before unknown (ascending)
    }
    if (knownB) {
        return 1;
    }
    return 0;
}

int comparePrimary(const Entry &a, const Entry &b, const QCollator &collator, SortKey key)
{
    switch (key) {
    case SortKey::Name:
        return collator.compare(a.name, b.name);
    case SortKey::Modified:
        return compareInt64(a.mtimeNs, b.mtimeNs);
    case SortKey::Duration:
        return compareDuration(a, b);
    case SortKey::Size:
        return compareInt64(a.size, b.size);
    case SortKey::Watched:
        return compareBool(a.watched, b.watched);
    case SortKey::Favorite:
        return compareBool(a.favorite, b.favorite);
    }
    return 0;
}

struct EntryLess {
    const QCollator &collator;
    const SortSpec &spec;

    bool operator()(const Entry &a, const Entry &b) const
    {
        // Folders group first when enabled, independent of direction.
        if (spec.foldersFirst && a.isFolder() != b.isFolder()) {
            return a.isFolder();
        }

        const int primary = comparePrimary(a, b, collator, spec.key);
        if (primary != 0) {
            return spec.direction == SortDirection::Ascending ? primary < 0 : primary > 0;
        }

        // Deterministic tie-breaks (always ascending): name case-insensitive,
        // then case-sensitive, then absolute path.
        const int caseInsensitive = collator.compare(a.name, b.name);
        if (caseInsensitive != 0) {
            return caseInsensitive < 0;
        }
        const int caseSensitive = QString::compare(a.name, b.name, Qt::CaseSensitive);
        if (caseSensitive != 0) {
            return caseSensitive < 0;
        }
        return a.path < b.path;
    }
};

} // namespace

QList<Entry> SortService::apply(const QList<Entry> &rows, const SortSpec &spec)
{
    QList<Entry> ordered = rows;

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setIgnorePunctuation(false);

    std::stable_sort(ordered.begin(), ordered.end(), EntryLess{collator, spec});
    return ordered;
}

} // namespace qg