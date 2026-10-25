#include "search/recursive_search_service.hpp"

#include "core/identity.hpp"
#include "search/search_service.hpp"
#include "workers/tasks.hpp"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>

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

    // The walk runs on the I/O pool. `work` posts incremental batches back to
    // this object's thread via queued invocations; no `deliver` is needed because
    // the walk itself signals `finished` at the end.
    qg::submitTask(qg::WorkerPools::io(), qg::TaskPriority::Background, m_token, this,
        [this, query, rootDir, cfg, generation]() {
            walk(query, rootDir, cfg, generation);
        },
        []() {});
    return generation;
}

void RecursiveSearchService::walk(const QString& query, const QString& rootDir,
                                  const RecursiveSearchConfig& cfg, std::uint64_t generation)
{
    const SearchService matcher;
    const SearchSpec spec{query, true};

    QVector<RecursiveSearchResult> batch;
    int found = 0;

    struct Node { QString path; int depth; };
    QVector<Node> queue;
    queue.reserve(64);
    queue.append({rootDir, 0});

    int head = 0;
    while (head < queue.size()) {
        if (m_token && m_token->isCancelled()) {
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
                QMetaObject::invokeMethod(this,
                    [this, b = batch, generation]() { emit batchReady(b, generation); },
                    Qt::QueuedConnection);
                batch.clear();
            }
        }

        for (const QString& s : subdirs) {
            queue.append({s, cur.depth + 1});
        }
    }

    if (!batch.isEmpty()) {
        QMetaObject::invokeMethod(this,
            [this, b = batch, generation]() { emit batchReady(b, generation); },
            Qt::QueuedConnection);
    }

    const bool cancelled = m_token && m_token->isCancelled();
    QMetaObject::invokeMethod(this,
        [this, generation, cancelled]() { emit finished(generation, cancelled); },
        Qt::QueuedConnection);
}