#include "search/recursive_search_service.hpp"

#include "core/identity.hpp"
#include "search/search_service.hpp"
#include "workers/tasks.hpp"

#include <QDir>
#include <QFileInfo>

RecursiveSearchService::RecursiveSearchService(QObject* parent) : QObject(parent) {}

RecursiveSearchService::~RecursiveSearchService()
{
    cancel();
}

void RecursiveSearchService::cancel()
{
    if (m_token) {
        m_token->cancel();
        m_token.reset();
    }
}

std::uint64_t RecursiveSearchService::start(const QString& query, const QString& rootDir,
                                            const RecursiveSearchConfig& cfg)
{
    cancel();
    if (query.isEmpty() || rootDir.isEmpty()) {
        return 0;
    }

    m_token = std::make_shared<qg::TaskToken>();
    const auto generation = m_token->generation();

    // Everything the walk needs is captured by value here, on the GUI thread:
    // the token (so the worker cannot race with cancel() over m_token), and a
    // lifetime guard for this object (so a worker can hand batches back without
    // ever dereferencing a service that may already have been destroyed).
    const QPointer<RecursiveSearchService> guard(this);
    const std::shared_ptr<qg::TaskToken> token = m_token;

    // The walk runs on the I/O pool. `work` posts incremental batches back to
    // the GUI thread via queued invocations; no `deliver` is needed because the
    // walk itself signals `finished` at the end.
    qg::submitTask(qg::WorkerPools::io(), qg::TaskPriority::Background, m_token, this,
        [this, guard, token, query, rootDir, cfg, generation]() {
            walk(guard, token, query, rootDir, cfg, generation);
        },
        []() {});
    return generation;
}

void RecursiveSearchService::walk(const QPointer<RecursiveSearchService>& guard,
                                  const std::shared_ptr<qg::TaskToken>& token,
                                  const QString& query, const QString& rootDir,
                                  const RecursiveSearchConfig& cfg, std::uint64_t generation)
{
    const SearchService matcher;
    const SearchSpec spec{query, true};

    QVector<RecursiveSearchResult> batch;
    int found = 0;

    // Post a batch through the guarded dispatcher, never through `this`: if the
    // service died while the walk was running, the payload is dropped instead of
    // being handed to freed memory.
    const auto postBatch = [this, guard, generation](const QVector<RecursiveSearchResult>& b) {
        qg::postToGui(guard, [this, b, generation]() { emit batchReady(b, generation); });
    };

    struct Node { QString path; int depth; };
    QVector<Node> queue;
    queue.reserve(64);
    queue.append({rootDir, 0});

    int head = 0;
    while (head < queue.size()) {
        if (token && token->isCancelled()) {
            break;
        }
        if (found >= cfg.resultCap) {
            break;
        }

        const Node cur = queue[head++];
        const QDir dir(cur.path);
        if (!dir.exists() || !dir.isReadable()) {
            continue; // unreadable subdirectory: skipped, never aborts the walk
        }

        const QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

        QVector<QPair<QString, bool>> names;
        QVector<QString> subdirs;
        names.reserve(entries.size());
        for (const QFileInfo& fi : entries) {
            if (fi.isDir()) {
                if (cur.depth + 1 <= cfg.maxDepth) {
                    subdirs.append(fi.absoluteFilePath());
                }
                names.append({fi.fileName(), true});
            } else {
                names.append({fi.fileName(), false});
            }
        }

        const QVector<SearchResult> hits = matcher.search(spec, names);
        for (const SearchResult& h : hits) {
            if (found >= cfg.resultCap) {
                break;
            }
            RecursiveSearchResult r;
            r.filename = h.filename;
            r.containingPath = cur.path;
            r.isFolder = h.isFolder;

            // Populate filesystem metadata
            const QString fullPath = cur.path + QLatin1Char('/') + h.filename;
            const QFileInfo fi(fullPath);
            if (fi.exists()) {
                r.sizeBytes = fi.size();
                const QDateTime dt = fi.lastModified();
                r.mtimeNs = dt.toSecsSinceEpoch() * 1000000000LL
                          + (dt.toMSecsSinceEpoch() % 1000) * 1000000LL;
                const auto id = qg::identityFromPath(fullPath);
                r.identity = id ? id.value().key : QString();
            }

            batch.append(r);
            ++found;

            if (batch.size() >= cfg.batchSize) {
                postBatch(batch);
                batch.clear();
            }
        }

        for (const QString& s : subdirs) {
            queue.append({s, cur.depth + 1});
        }
    }

    if (!batch.isEmpty()) {
        postBatch(batch);
    }

    const bool cancelled = token && token->isCancelled();
    qg::postToGui(guard, [this, generation, cancelled]() { emit finished(generation, cancelled); });
}