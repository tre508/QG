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
        const QString published = publishThumbnail(m_cacheDir, key, extraction.image);

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
    // Cache first: a freedesktop hit must never pay for a decode, even when an
    // extractor has been injected (DESIGN §18.2 "Serve" precedes "Extract").
    if (!m_fdHome.isEmpty()) {
        const FreedesktopThumbnail native =
            findFreedesktopThumbnail(m_fdHome, request.path, request.mtimeNs);
        if (native.valid) {
            ThumbnailExtraction result;
            result.image = native.image;
            result.sourceMtimeNs = request.mtimeNs;
            result.sourceSize = request.size;
            return result;
        }
    }

    if (m_extractor) {
        return m_extractor(request);
    }

    return extractThumbnail(request.path, request.width, request.height);
}

void ThumbnailController::processAllForTesting()
{
    while (m_scheduler.hasPending()) {
        const auto request = m_scheduler.takeNext();
        if (!request) {
            break;
        }
        const ThumbnailExtraction extraction = runExtraction(*request);
        applyResult(*request, m_generation, extraction);
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
    if (m_processing) {
        return;
    }
    if (!m_scheduler.hasPending()) {
        return;
    }

    const auto request = m_scheduler.takeNext();
    if (!request) {
        return;
    }

    m_processing = true;
    const ThumbnailRequest captured = *request;
    const quint64 generation = m_generation;
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
}

} // namespace qg
