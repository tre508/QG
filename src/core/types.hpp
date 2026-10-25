#pragma once

// QG core domain value types (DESIGN.md §15.4, §16, Appendix B).
//
// These are lightweight, copyable values shared by services, workers and
// models. They hold no UI state, no ownership and perform no I/O. A single
// `Entry` row type discriminates folders from videos via `EntryKind`, keeping
// model rows small for large directories (DESIGN.md §22.4). `MediaEntry` and
// `FolderEntry` are represented by `Entry` with the matching kind.

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace qg {

enum class EntryKind {
    Folder,
    Video,
};

enum class ViewDensity {
    Gallery,
    Standard,
    Compact,
};

enum class SortKey {
    Name,
    Modified,
    Duration,
    Size,
    Watched,
    Favorite,
};

enum class SortDirection {
    Ascending,
    Descending,
};

// Visual/thumbnail lifecycle for a single entry (DESIGN.md §6.2).
enum class ThumbnailState {
    None,
    Loading,
    Ready,
    Failed,
    Unreadable,
};

// First-class per-folder sort order (DESIGN.md §5.7, §20.2).
struct SortSpec {
    SortKey key = SortKey::Name;
    SortDirection direction = SortDirection::Ascending;
    bool foldersFirst = true;

    friend bool operator==(const SortSpec &, const SortSpec &) = default;
};

// One canonical folder/media row. `identity` is the stable media identity key
// (see identity.hpp); `path` is always the real filesystem path.
struct Entry {
    QString identity;
    QString path;
    QString name;
    EntryKind kind = EntryKind::Video;

    qint64 size = 0;    // bytes (0 for folders)
    qint64 mtimeNs = 0; // modification time, nanoseconds since the epoch

    // Optional probed/derived fields; unset means "unknown", not zero.
    std::optional<qint64> durationMs;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> rotation;

    // Application state overlaid on the real file (persisted by identity).
    bool favorite = false;
    bool watched = false;
    std::optional<double> progress; // 0.0..1.0 partial-watch fraction

    ThumbnailState thumbnail = ThumbnailState::None;

    bool isFolder() const noexcept { return kind == EntryKind::Folder; }
    bool isVideo() const noexcept { return kind == EntryKind::Video; }
};

// A non-fatal per-entry problem found while enumerating (e.g. broken symlink,
// permission denied). Enumeration records these and continues.
struct EntryError {
    QString path;
    QString message;
    int systemError = 0;
};

// Immutable result of enumerating one directory (DESIGN.md Appendix B).
struct FolderSnapshot {
    QString path;
    std::uint64_t generation = 0;
    QList<Entry> entries;
    QList<EntryError> errors;
    bool complete = false;
};

// Identity-based difference between a model's current rows and a fresh
// enumeration, used to apply minimal updates on external change (§17.5).
struct FolderDiff {
    QStringList removedIdentities;
    QList<Entry> added;
    QList<Entry> changed; // same identity, differing metadata (path/name/size/mtime)
};

// Register the value types with the Qt metatype system so they can cross
// thread boundaries via queued signals. Safe to call more than once.
inline void registerCoreTypes()
{
    qRegisterMetaType<Entry>("qg::Entry");
    qRegisterMetaType<QList<Entry>>("QList<qg::Entry>");
    qRegisterMetaType<EntryError>("qg::EntryError");
    qRegisterMetaType<FolderSnapshot>("qg::FolderSnapshot");
    qRegisterMetaType<FolderDiff>("qg::FolderDiff");
    qRegisterMetaType<SortSpec>("qg::SortSpec");
    qRegisterMetaType<EntryKind>("qg::EntryKind");
    qRegisterMetaType<ViewDensity>("qg::ViewDensity");
    qRegisterMetaType<SortKey>("qg::SortKey");
    qRegisterMetaType<SortDirection>("qg::SortDirection");
    qRegisterMetaType<ThumbnailState>("qg::ThumbnailState");
}

} // namespace qg

Q_DECLARE_METATYPE(qg::Entry)
Q_DECLARE_METATYPE(QList<qg::Entry>)
Q_DECLARE_METATYPE(qg::EntryError)
Q_DECLARE_METATYPE(qg::FolderSnapshot)
Q_DECLARE_METATYPE(qg::FolderDiff)
Q_DECLARE_METATYPE(qg::SortSpec)
Q_DECLARE_METATYPE(qg::EntryKind)
Q_DECLARE_METATYPE(qg::ViewDensity)
Q_DECLARE_METATYPE(qg::SortKey)
Q_DECLARE_METATYPE(qg::SortDirection)
Q_DECLARE_METATYPE(qg::ThumbnailState)
