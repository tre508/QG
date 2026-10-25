#pragma once

// Directory enumeration (DESIGN.md §17.2).
//
// Runs on the I/O worker pool only. It lists a directory, keeps child folders
// and supported videos, discards every other regular file, and publishes the
// result in incremental batches plus a final `FolderSnapshot`. Per-entry
// problems (broken symlink, permission denied) are recorded and never abort the
// scan. Cancellation is checked continuously.

#include "core/types.hpp"
#include "workers/task_token.hpp"

#include <QObject>
#include <QString>

namespace qg {

struct EnumeratorOptions {
    bool showHidden = false;
    int batchSize = 200;
};

class FilesystemEnumerator : public QObject {
    Q_OBJECT

public:
    explicit FilesystemEnumerator(QObject *parent = nullptr);

    // Synchronous enumeration. Emits `batchReady` for each batch (~batchSize
    // entries) and `finished` with the aggregate snapshot. Returns the same
    // aggregate snapshot. Intended to be invoked from the I/O pool; when the
    // owner lives on the GUI thread the signals are delivered queued.
    FolderSnapshot enumerate(const QString &path,
                             const TaskToken &token,
                             const EnumeratorOptions &options = {});

signals:
    // `entries` holds one batch; `complete` is false.
    void batchReady(const qg::FolderSnapshot &batch);
    // Full aggregate snapshot; `complete` reflects whether the scan finished.
    void finished(const qg::FolderSnapshot &snapshot);
};

} // namespace qg
