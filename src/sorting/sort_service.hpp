#pragma once

// Deterministic, stable, folders-first ordering (DESIGN.md §5.7, §20.2).
//
// `SortService::apply` is the single source of ordering truth. It is a pure
// static function over `Entry` rows (no QObject, no I/O) and can therefore run
// on any thread; the caller owns the thread and applies the result to the
// canonical `FolderModel`. QML must never re-sort a copy (DEC-014).

#include "core/types.hpp"

#include <QList>

namespace qg {

class SortService
{
public:
    // Returns the rows in deterministic total order:
    //   folders-first (when enabled) → primary key → filename (case-insensitive,
    //   then case-sensitive) → absolute path.
    // Direction applies to the primary key only; tie-breaks stay ascending, so
    // the result is stable and repeatable across runs and filesystems.
    // Duration/Size unknowns sort last ascending and first descending (§20.2).
    static QList<Entry> apply(const QList<Entry> &rows, const SortSpec &spec);
};

} // namespace qg