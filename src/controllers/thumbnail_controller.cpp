#include "controllers/thumbnail_controller.hpp"

#include "models/folder_model.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"
#include "workers/priority_pool.hpp"
#include "workers/tasks.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QVariant>

#include <memory>

namespace qg {

namespace {

// Stores a decoded frame where QML can load it and returns the local path.
//
// Thumbnails are delivered to the view as a file URL because `Image.source` is a
// QUrl. The QG cache is the durable home (DESIGN §18.2); when no cache directory
// is configured (tests, embedders) a temp cache keeps delivery working instead of
// silently producing a blank card.
QString publishThumbnail(const QString &cacheDir, const ThumbnailKey &key, const QImage &image)
{
    if (image.isNull()) {
        return QString();
    }

    if (!cacheDir.isEmpty()) {
        const QString path = qgThumbnailPath(cacheDir, key);
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (image.save(path, "PNG")) {
            return path;
        }
    }

    const QString fallbackRoot =
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("qg-thumb-cache"));
    const QString path = qgThumbnailPath(fallbackRoot, key);
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (image.save(path, "PNG")) {
        return path;
    }

    return QString();
}

} // namespace

ThumbnailController::ThumbnailController(QObject *parent)
    : QObject(parent)
{
}

void ThumbnailController::setThumbnailSize(int width, int height)
{
    m_width = width > 0 ? width : m_width;
    m_height = height > 0 ? height : m_height;
}

void ThumbnailController::beginGeneration()
{
    ++m_generation;
    m_scheduler.clear();
}

void ThumbnailController::cancelAll()
{
    beginGeneration();
}

void ThumbnailController::requestVisible(int firstIndex, int lastIndex)
{
    if (m_model == nullptr || m_model->rowCount() == 0) {
        return;
    }

    const int count = m_model->rowCount();
    const int first = qMax(0, firstIndex);
    const int last = qMin(count - 1, lastIndex);
    if (last < first) {
        return;
    }

    // Prioritize visible range; neighbours within a viewport become Normal.
    m_scheduler.setViewport(first, last, count);

    // Request the visible window plus one viewport of prefetch on each side.
    const int span = last - first + 1;
    const int from = qMax(0, first - span);
    const int to = qMin(count - 1, last + span);
    for (int index = from; index <= to; ++index) {
        const Entry *entry = m_model->entryAt(index);
        if (entry == nullptr || entry->identity.isEmpty()) {
            continue;
        }
        // Folder rows never carry a thumbnail: requesting one wastes a decode
        // slot on the media pool and records a bogus failed row for a directory.
        if (entry->isFolder()) {
            continue;
        }
        // Skip rows whose thumbnail is already settled: re-requesting on every
        // scroll event would otherwise re-extract finished images (DESIGN §18.4).
        if (m_model->thumbnailStateAt(index) != ThumbnailState::None) {
            continue;
        }
        ThumbnailRequest request;
        request.index = index;
        request.identity = entry->identity;
        request.path = entry->path;
        request.mtimeNs = entry->mtimeNs;
        request.size = entry->size;
        request.width = m_width;
        request.height = m_height;
        m_scheduler.request(index, request);
    }

    if (m_immediate) {
        processAllForTesting();
    } else {
        pumpAsync();
    }
}

void ThumbnailController::retry(const QString &identity)
{
    if (m_model == nullptr || identity.isEmpty()) {
        return;
    }
    const int row = m_model->indexOfIdentity(identity);
    if (row < 0) {
        return;
    }
    const Entry *entry = m_model->entryAt(row);
    if (entry == nullptr || entry->path.isEmpty()) {
        return;
    }

    // Reset the visible failure marker so the placeholder shows immediately, then
    // re-request the row (a manual retry bypasses the terminal-failure backoff).
    m_model->updateThumbnail(identity, QVariant(), ThumbnailState::Loading);

    ThumbnailRequest request;
    request.index = row;
    request.identity = identity;
    request.path = entry->path;
    request.mtimeNs = entry->mtimeNs;
    request.size = entry->size;
    request.width = m_width;
    request.height = m_height;
    // A hand retry is the user's escape hatch from a terminal failure (§18.5),
    // so the cache decision must not refuse it.
    request.manualRetry = true;
    m_scheduler.request(row, request);

    if (m_immediate) {
        processAllForTesting();
    } else {
        pumpAsync();
    }
}

bool ThumbnailController::validate(const ThumbnailRequest &request, quint64 generation) const
{
    if (generation != m_generation) {
        return false; // user navigated away
    }
    if (m_model == nullptr) {
        return false;
    }
    const int row = m_model->indexOfIdentity(request.identity);
    if (row < 0) {
        return false; // entry no longer present
    }
    const Entry *entry = m_model->entryAt(row);
    if (entry == nullptr || entry->mtimeNs != request.mtimeNs || entry->size != request.size) {
        return false; // source changed while the task ran
    }
    return true;
}

void ThumbnailController::applyResult(const ThumbnailRequest &request,
                                      quint64 generation,
                                      const ThumbnailExtraction &extraction)
{
    if (!validate(request, generation)) {
        return;
    }

    if (extraction.ok()) {
        m_scheduler.storeDecoded(request.identity, extraction.image);

        // Publish the decoded frame as a file and expose it as a URL. QML's
        // `Image.source` is a QUrl: handing it a QImage (as this used to) yields
        // an empty source, so every "ready" row rendered a blank well (VIS-010).
        ThumbnailKey key;
        key.identity = request.identity;
        key.mtimeNs = request.mtimeNs;
        key.size = request.size;
        // A QG-cache hit *is* the file for this key, so re-encoding it would pay a
        // synchronous PNG encode per served row for no new pixels (AUD-008 /
        // FIX-TH-01). A freedesktop hit is a different source and is still written
        // through to QG's cache, so the delivered file always matches what was
        // served. A vanished file falls back to publishing, which also repairs the
        // row's `cache_path` below.
        const QString existing = qgThumbnailPath(m_cacheDir, key);
        const bool alreadyPublished =
            extraction.cacheOrigin == ThumbnailExtraction::CacheOrigin::QgCache
            && QFileInfo::exists(existing);
        const QString published = alreadyPublished
                                      ? existing
                                      : publishThumbnail(m_cacheDir, key, extraction.image);

        if (published.isEmpty()) {
            // Extraction produced pixels we could not store; report it honestly
            // as a failed thumbnail (retryable) instead of a blank that claims
            // success.
            qWarning("thumbnail: could not publish image for %s",
                     qUtf8Printable(request.path));
            if (m_database != nullptr) {
                ThumbnailRepository repository(*m_database);
                repository.upsertFailed(request.identity, request.mtimeNs, request.size);
            }
            m_model->updateThumbnail(request.identity, QVariant(), ThumbnailState::Failed);
            emit thumbnailFailed(request.identity);
            return;
        }

        if (m_database != nullptr) {
            ThumbnailRepository repository(*m_database);
            repository.upsertOk(request.identity, request.mtimeNs, request.size, published);
        }

        m_model->updateThumbnail(request.identity,
                                 QVariant(QUrl::fromLocalFile(published)),
                                 ThumbnailState::Ready);
        emit thumbnailReady(request.identity);
    } else {
        if (m_database != nullptr) {
            ThumbnailRepository repository(*m_database);
            repository.upsertFailed(request.identity, request.mtimeNs, request.size);
        }
        m_model->updateThumbnail(request.identity, QVariant(), ThumbnailState::Failed);
        emit thumbnailFailed(request.identity);
    }
}

ThumbnailExtraction ThumbnailController::runExtraction(const ThumbnailRequest &request) const
{
    // Runs on the media pool; never touches the database (thread-affine).

    // 1. QG's own cache: the GUI thread already resolved the row through the
    //    §18.5 policy and handed over the persisted PNG path, so this reads the
    //    image without touching the decoder (§18.2 "Serve" precedes "Extract").
    if (!request.cachedPngPath.isEmpty()) {
        const QImage cached(request.cachedPngPath);
        if (!cached.isNull()) {
            ThumbnailExtraction hit;
            hit.image = cached;
            hit.sourceMtimeNs = request.mtimeNs;
            hit.sourceSize = request.size;
            hit.cacheOrigin = ThumbnailExtraction::CacheOrigin::QgCache;
            return hit;
        }
        // The row points at a PNG that vanished (cache cleaned, path moved):
        // fall through and regenerate rather than failing.
    }

    // 2. Native freedesktop thumbnail: instant and free (§18.2).
    if (!m_fdHome.isEmpty()) {
        const FreedesktopThumbnail native =
            findFreedesktopThumbnail(m_fdHome, request.path, request.mtimeNs);
        if (native.valid) {
            ThumbnailExtraction result;
            result.image = native.image;
            result.sourceMtimeNs = request.mtimeNs;
            result.sourceSize = request.size;
            result.cacheOrigin = ThumbnailExtraction::CacheOrigin::FreedesktopCache;
            return result;
        }
    }

    // 3. Extract (§18.3): injected extractor first, then the in-process decoder.
    if (m_extractor) {
        return m_extractor(request);
    }

    return extractThumbnail(request.path, request.width, request.height);
}

ThumbnailController::CacheDecision ThumbnailController::consultQgCache(
    const ThumbnailRequest &request, QString *cachePath) const
{
    // GUI thread only (see the header). This is the database half of the
    // "Serve → Extract" decision of §18.2 — without it the PNG + DB cache was
    // write-only: every navigation paid FFmpeg for every visible row, and a file
    // carrying a failure marker was retried on every visit (AUD-008 / FIX-TH-01).
    if (cachePath != nullptr) {
        cachePath->clear();
    }
    if (m_database == nullptr || request.identity.isEmpty()) {
        return CacheDecision::Extract;
    }

    ThumbnailRepository repository(*m_database);
    ThumbnailRepository::Record record;
    if (!repository.lookup(request.identity, &record)) {
        return CacheDecision::Extract; // no row: extract (§18.3)
    }

    ThumbnailCacheState cache;
    cache.present = true;
    cache.state = record.state;
    cache.failCount = record.failCount;
    cache.sourceMtimeNs = record.sourceMtimeNs;
    cache.sourceSize = record.sourceSize;

    // The policy compares the persisted source stamp with the *current*
    // (mtime_ns, size): a touched/replaced file regenerates, a failed file
    // retries within its bounded budget, and an exhausted marker is terminal
    // until the source changes or the user retries by hand (§18.5).
    const ThumbnailAction action = decideThumbnail(
        cache, request.mtimeNs, request.size, kMaxThumbnailFailRetries, request.manualRetry);

    if (action == ThumbnailAction::Serve && !record.cachePath.isEmpty()) {
        if (cachePath != nullptr) {
            *cachePath = record.cachePath;
        }
        return CacheDecision::Serve;
    }

    if (action == ThumbnailAction::Terminal) {
        return CacheDecision::Terminal;
    }

    // Generate / Retry: a decode is genuinely needed.
    return CacheDecision::Extract;
}

ThumbnailExtraction ThumbnailController::terminalResult(const ThumbnailRequest &request) const
{
    ThumbnailExtraction failed;
    failed.sourceMtimeNs = request.mtimeNs;
    failed.sourceSize = request.size;
    failed.error = Error::make(ErrorCode::OperationFailed,
                               QStringLiteral("Thumbnail generation failed previously"),
                               request.path);
    return failed;
}

void ThumbnailController::processAllForTesting()
{
    while (m_scheduler.hasPending()) {
        const auto request = m_scheduler.takeNext();
        if (!request) {
            break;
        }
        // Same order as pumpAsync (GUI thread → cache decision → decode), so
        // immediate mode exercises the production path.
        QString cachedPngPath;
        const CacheDecision decision = consultQgCache(*request, &cachedPngPath);
        if (decision == CacheDecision::Terminal) {
            applyResult(*request, m_generation, terminalResult(*request));
            continue;
        }
        ThumbnailRequest dispatched = *request;
        if (decision == CacheDecision::Serve) {
            dispatched.cachedPngPath = cachedPngPath;
        }
        const ThumbnailExtraction extraction = runExtraction(dispatched);
        applyResult(dispatched, m_generation, extraction);
    }
}

void ThumbnailController::injectResultForTesting(const ThumbnailRequest &request,
                                                 quint64 generation,
                                                 const ThumbnailExtraction &extraction)
{
    applyResult(request, generation, extraction);
}

void ThumbnailController::pumpAsync()
{
    // `m_resolving` guards the cache-served path below: it applies results
    // synchronously, and a re-entrant call from a signal it emits (a binding that
    // re-reports its viewport) must not start a second pump — that would break the
    // one-in-flight-request invariant (DESIGN §18.4, §22.2).
    if (m_processing || m_resolving) {
        return;
    }
    if (!m_scheduler.hasPending()) {
        return;
    }

    // QG's own cache is consulted here, on the GUI thread, before any pool work
    // is dispatched: a cache hit or a terminal failure completes the request
    // without a decode (§18.2 "Serve" precedes "Extract"). Loop rather than
    // recurse so a long run of served rows cannot grow the stack.
    m_resolving = true;
    while (m_scheduler.hasPending()) {
        const auto request = m_scheduler.takeNext();
        if (!request) {
            m_resolving = false;
            return;
        }

        const quint64 generation = m_generation;
        QString cachedPngPath;
        const CacheDecision decision = consultQgCache(*request, &cachedPngPath);
        if (decision == CacheDecision::Terminal) {
            // Retry budget exhausted: the amber placeholder is re-raised without
            // a decode, and no pool work is queued (§18.5).
            applyResult(*request, generation, terminalResult(*request));
            continue;
        }

        // Real work: hand the request to the media pool. A cached PNG is read
        // there, not here (§22.1).
        m_processing = true;
        ThumbnailRequest captured = *request;
        if (decision == CacheDecision::Serve) {
            captured.cachedPngPath = cachedPngPath;
        }
        auto extraction = std::make_shared<ThumbnailExtraction>();

        submitTask(WorkerPools::cpuMedia(),
                   TaskPriority::Normal,
                   nullptr,
                   this,
                   [this, captured, extraction]() { *extraction = runExtraction(captured); },
                   [this, captured, generation, extraction]() {
                       applyResult(captured, generation, *extraction);
                       m_processing = false;
                       pumpAsync(); // continue with the next pending request
                   });
        m_resolving = false;
        return;
    }
    m_resolving = false;
}

} // namespace qg
