#pragma once

// Thumbnail pipeline integration (DESIGN.md §18.4, Prompt 27).
//
// Owns a `ThumbnailScheduler`, runs extraction on the CPU/media pool, writes the
// QG cache + DB rows, and applies validated results to the canonical model with
// minimal `dataChanged`. Validation before any mutation: the generation must
// still be current, the entry must still be in the model, and the source
// (mtime_ns, size) must be unchanged since the request was made.

#include "media/thumbnails.hpp"

#include <QObject>
#include <QString>

#include <functional>

namespace qg {

class Database;
class FolderModel;

class ThumbnailController : public QObject {
    Q_OBJECT

public:
    using Extractor = std::function<ThumbnailExtraction(const ThumbnailRequest &)>;

    explicit ThumbnailController(QObject *parent = nullptr);

    void setModel(FolderModel *model) { m_model = model; }
    void setDatabase(Database *database) { m_database = database; }
    void setCacheDirectory(const QString &qgCacheDir) { m_cacheDir = qgCacheDir; }
    void setFreedesktopCacheHome(const QString &home) { m_fdHome = home; }
    void setThumbnailSize(int width, int height);
    void setExtractor(Extractor extractor) { m_extractor = std::move(extractor); }

    // Synchronous processing for deterministic tests (no worker pool).
    void setImmediateProcessing(bool immediate) { m_immediate = immediate; }

    // New navigation: cancel outstanding work and invalidate in-flight results.
    void beginGeneration();
    void cancelAll();

    // Enqueue the visible row range (and its neighbours) from the model.
    // QML-facing: the ContentView reports its viewport range and Main.qml routes
    // it here. Rows already settled (Loading/Ready/Failed) are skipped, so
    // scrolling never re-extracts a finished thumbnail (DESIGN.md §18.4).
    Q_INVOKABLE void requestVisible(int firstIndex, int lastIndex);

    // Manual thumbnail retry for one item (DESIGN.md §18.5: "failed terminally;
    // skip until source changes or manual retry"). Clears the failed marker so
    // the card returns to the loading placeholder, then re-requests the row at
    // interactive priority. QML-facing (the card's Retry control).
    Q_INVOKABLE void retry(const QString &identity);

    int pendingCount() const { return m_scheduler.pendingCount(); }
    quint64 generation() const { return m_generation; }

    // --- Testing hooks ------------------------------------------------------
    void processAllForTesting();
    void injectResultForTesting(const ThumbnailRequest &request,
                                quint64 generation,
                                const ThumbnailExtraction &extraction);

signals:
    void thumbnailReady(const QString &identity);
    void thumbnailFailed(const QString &identity);

private:
    ThumbnailExtraction runExtraction(const ThumbnailRequest &request) const;
    bool validate(const ThumbnailRequest &request, quint64 generation) const;
    void applyResult(const ThumbnailRequest &request,
                     quint64 generation,
                     const ThumbnailExtraction &extraction);
    void pumpAsync();

    ThumbnailScheduler m_scheduler;
    FolderModel *m_model = nullptr;
    Database *m_database = nullptr;
    QString m_cacheDir;
    QString m_fdHome;
    Extractor m_extractor;
    int m_width = 320;
    int m_height = 180;
    quint64 m_generation = 0;
    bool m_immediate = false;
    bool m_processing = false;
};

} // namespace qg
