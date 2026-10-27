#pragma once

// Thumbnail cache model and freedesktop cache reader (DESIGN.md §18.2).
//
// A thumbnail is keyed by identity + source mtime_ns + size (+ thumbnailer
// version), so any source change invalidates it automatically. Before doing any
// work QG looks in the freedesktop thumbnail cache and, when a valid entry
// exists (matching Thumb::URI and Thumb::MTime), reuses it. Decoding happens on
// the CPU/media pool, never on the GUI thread.

#include "core/errors.hpp"

#include <QHash>
#include <QImage>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>

namespace qg {

// QG's own thumbnail cache key (DESIGN.md §18.2).
struct ThumbnailKey {
    QString identity;
    qint64 mtimeNs = 0;
    qint64 size = 0;
    int version = 1;

    friend bool operator==(const ThumbnailKey &, const ThumbnailKey &) = default;
};

// Deterministic string encoding of the key (used for QG cache filenames).
QString encodeThumbnailKey(const ThumbnailKey &key);

// QG cache file path: `<qgCacheDir>/thumbs/<md5(key)>.png`.
QString qgThumbnailPath(const QString &qgCacheDir, const ThumbnailKey &key);

// --- Freedesktop thumbnail cache -------------------------------------------

// The `file://` URI used as the freedesktop cache key input.
QString freedesktopUri(const QString &sourcePath);

// md5 hex of the URI (freedesktop cache filename stem).
QString freedesktopCacheKey(const QString &uri);

// Path of the freedesktop thumbnail for `sourcePath` in the size bucket
// (128 -> normal, 256 -> large, 512 -> x-large).
QString freedesktopThumbnailPath(const QString &cacheHome, const QString &sourcePath, int bucket);

struct FreedesktopThumbnail {
    bool valid = false;
    QString path;
    QImage image;
};

// Looks up a validated freedesktop thumbnail for the source. Validation checks
// `Thumb::URI` and `Thumb::MTime` (seconds) against the source.
FreedesktopThumbnail findFreedesktopThumbnail(const QString &cacheHome,
                                              const QString &sourcePath,
                                              qint64 sourceMtimeNs,
                                              const QList<int> &buckets = {128, 256, 512});

// --- In-process extraction (DESIGN.md §18.3) --------------------------------

struct ThumbnailExtraction {
    QImage image;
    qint64 sourceMtimeNs = 0;
    qint64 sourceSize = 0;
    Error error;

    // Where the pixels came from. `QgCache` means they were read back from QG's own
    // cache file for this key — that file *is* the final artefact and must not be
    // re-encoded (AUD-008 / FIX-TH-01; re-publishing a cache hit costs a synchronous
    // PNG encode on the GUI thread per row, FIX-TH-02). `FreedesktopCache` means the
    // native cache served them, which is a different source: QG's own cache still has
    // to be written through with what was served.
    enum class CacheOrigin { Decoded, QgCache, FreedesktopCache };
    CacheOrigin cacheOrigin = CacheOrigin::Decoded;

    bool fromCache() const noexcept { return cacheOrigin != CacheOrigin::Decoded; }
    bool ok() const noexcept { return !image.isNull(); }
};

// Decodes a representative frame with FFmpeg libraries and scales it to
// `targetWidth` x `targetHeight` (RGBA). `posterMs` < 0 seeks to ~10% of the
// media. Runs on the CPU/media pool; never uses the playback instance. Does not
// enforce the 16:9 aspect — callers pass the exact thumbnail size.
ThumbnailExtraction extractThumbnail(const QString &path,
                                     int targetWidth,
                                     int targetHeight,
                                     qint64 posterMs = -1);

// --- Visible-first scheduler + decoded-image LRU (DESIGN.md §18.3, §22) ------

struct ThumbnailRequest {
    int index = -1; // row index (set by the scheduler)
    QString identity;
    QString path;
    qint64 mtimeNs = 0;
    qint64 size = 0;
    int width = 320;
    int height = 180;
    // Set by the card's manual Retry control: it bypasses the terminal-failure
    // backoff (§18.5) instead of being refused by it.
    bool manualRetry = false;
    // Set by the controller's GUI-thread cache consultation (never by the
    // scheduler) when a valid QG cache entry exists: `runExtraction` reads this
    // PNG on the media pool instead of decoding, so the GUI thread performs no
    // file I/O (§22.1).
    QString cachedPngPath;
};

// Pure scheduling policy: visible rows are requested first; queued work for rows
// that scroll far away is demoted and eventually dropped; decoded images live in
// a bounded LRU. Kept free of threading so it is deterministic to test; the
// service layer drives it from the priority pool.
class ThumbnailScheduler {
public:
    enum class Priority { Interactive = 0, Normal = 1, Background = 2 };

    void request(int index, const ThumbnailRequest &request);
    // Recompute priorities from the visible range. Rows within the visible
    // range are Interactive, within one viewport Normal, within `horizon`
    // Background, and beyond it dropped.
    void setViewport(int firstVisible, int lastVisible, int totalCount, int horizon = 60);

    void clear();
    bool hasPending() const { return !m_pending.isEmpty(); }
    int pendingCount() const { return static_cast<int>(m_pending.size()); }
    int priorityOf(int index) const; // -1 when not queued
    std::optional<ThumbnailRequest> takeNext();

    void setDecodedCapacity(int count);
    int decodedCapacity() const { return m_capacity; }
    void storeDecoded(const QString &identity, const QImage &image);
    bool hasDecoded(const QString &identity) const;
    QImage decoded(const QString &identity) const;
    int decodedCount() const { return static_cast<int>(m_lru.size()); }

private:
    struct Entry {
        ThumbnailRequest request;
        Priority priority = Priority::Normal;
        quint64 sequence = 0;
    };

    void recomputePriorities(int horizon);

    QMap<int, Entry> m_pending;
    quint64 m_sequence = 0;
    int m_viewFirst = -1;
    int m_viewLast = -1;

    QHash<QString, QImage> m_decoded;
    QStringList m_lru; // most-recent first
    int m_capacity = 64;
};

// --- Failure markers, retry/backoff, source invalidation (DESIGN.md §18.5) ---

// Snapshot of the persisted `thumbnails` row for a source.
struct ThumbnailCacheState {
    bool present = false;
    QString state; // "ok" | "failed"
    int failCount = 0;
    qint64 sourceMtimeNs = 0;
    qint64 sourceSize = 0;
};

enum class ThumbnailAction {
    Generate, // no cache entry: extract
    Serve,    // current, valid cache entry
    Retry,    // failed but retryable (bounded backoff or source changed/manual)
    Terminal, // failed terminally; skip until source changes or manual retry
};

// Bounded retry budget for a failed thumbnail (§18.5: "up to 2 retries, then the
// marker is terminal until the source changes"). Named once so the controller's
// call site and the policy's default cannot drift apart.
inline constexpr int kMaxThumbnailFailRetries = 2;

// Pure policy: bounded retries (up to `maxRetries` after the first failure),
// invalidation when the source (mtime_ns, size) changes, and explicit manual
// retry. A failed marker is terminal otherwise.
ThumbnailAction decideThumbnail(const ThumbnailCacheState &cache,
                                qint64 sourceMtimeNs,
                                qint64 sourceSize,
                                int maxRetries = kMaxThumbnailFailRetries,
                                bool manualRetry = false);

} // namespace qg
