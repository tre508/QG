#pragma once

// RecursiveSearchService (DESIGN.md §20.4, SEARCH-08): a bounded, cancellable,
// batched filename walk of a directory and its descendants. Runs on the shared
// I/O pool (WorkerPools::io()) — never on the GUI thread — and NEVER blocks the
// in-folder SearchProxy/typing path (§7.1).
//
// It reuses the pure, I/O-free SearchService to match filenames per directory;
// the filesystem walk lives here, not in SearchService. Results carry the
// containing path (they have no FolderModel row, so modelRow/mapToSource
// semantics do not apply).

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>

namespace qg { class TaskToken; }

struct RecursiveSearchResult {
    QString filename;
    QString containingPath;  // the directory that contains this entry
    QString identity;        // best-effort identity (empty when unresolved)
    bool isFolder = false;

    // Metadata fields (populated from database/filesystem)
    std::optional<qint64> durationMs;
    std::optional<qint64> sizeBytes;
    std::optional<qint64> mtimeNs;
    bool favorite = false;
    bool watched = false;
    std::optional<double> progress;
    std::optional<int> width;
    std::optional<int> height;
    int thumbnailState = 0;  // 0 None, 1 Loading, 2 Ready, 3 Failed, 4 Unreadable
};
Q_DECLARE_METATYPE(RecursiveSearchResult)

struct RecursiveSearchConfig {
    int maxDepth = 8;         // stop descending beyond this depth
    int resultCap = 2000;     // mirror SearchProxy::kRowCap
    int batchSize = 100;      // results per incremental batch
};

class RecursiveSearchService : public QObject {
    Q_OBJECT
public:
    explicit RecursiveSearchService(QObject* parent = nullptr);
    ~RecursiveSearchService() override;

    // Begin a walk from `rootDir`. Batches + finished arrive on this object's
    // thread via queued delivery. A subsequent call cancels the previous walk.
    // Returns the walk's generation (use it to drop stale batches).
    //
    // The walk runs on the I/O pool and holds a lifetime guard for this object,
    // so destroying the service while a walk is in flight drops the remaining
    // batches instead of letting a worker touch freed memory.
    std::uint64_t start(const QString& query, const QString& rootDir,
                        const RecursiveSearchConfig& cfg = RecursiveSearchConfig());

    // Cancel the in-flight walk (checked between directories); later batches
    // and `finished` are dropped by generation.
    void cancel();

signals:
    void batchReady(const QVector<RecursiveSearchResult>& batch, std::uint64_t generation);
    void finished(std::uint64_t generation, bool cancelled);

private:
    // Worker-thread body. `guard` and `token` are copies made on the GUI thread
    // in start(): the walk never reads this object's members, so it cannot race
    // with cancel() and cannot touch a destroyed service.
    void walk(const QPointer<RecursiveSearchService>& guard,
              const std::shared_ptr<qg::TaskToken>& token,
              const QString& query, const QString& rootDir,
              const RecursiveSearchConfig& cfg, std::uint64_t generation);

    std::shared_ptr<qg::TaskToken> m_token;
};