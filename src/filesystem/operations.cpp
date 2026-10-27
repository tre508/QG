#include "operations.hpp"

#include "workers/tasks.hpp"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QUrl>
#include <QDebug>

#include <memory>

bool copyDirRecursive(const QString& src, const QString& dst);

FileOperationsService::FileOperationsService(QObject* parent) : QObject(parent) {}

// Every mutating entry point is submitted to the bounded I/O pool and publishes
// its result back to this service on the GUI thread (DESIGN.md §17.6, §15.3,
// §22.1). The *Sync helpers stay the payload: they are pure I/O, safe on any
// thread, and are what the tests call directly.
//
// Contract: submitted from the GUI thread (the Q_INVOKABLE facade always is),
// `work` runs on a pool thread and never touches GUI state, and `deliver` —
// `emitFinished` — runs on the GUI thread only while `this` is still alive
// (submitTask's lifetime guard). Results are delivered in completion order, not
// request order, so a caller that keeps per-request state must correlate it with
// the result it receives.
//
// Progress and cancellation (DESIGN.md §8.1, §15.4 "chunked with cancellation
// checkpoints") are NOT implemented here: that needs a chunked copy and is the
// follow-up increment FIX-FO-02, not a silent omission.
void FileOperationsService::rename(const QString& path, const QString& newBaseName) {
    auto result = std::make_shared<OperationResult>();
    qg::submitTask(qg::WorkerPools::io(), qg::TaskPriority::Normal, nullptr, this,
                   [path, newBaseName, result]() {
                       *result = renameSync(path, newBaseName);
                   },
                   [this, result]() {
                       emitFinished(QStringLiteral("rename"), *result);
                   });
}

void FileOperationsService::move(const QStringList& paths, const QString& targetDir) {
    auto result = std::make_shared<OperationResult>();
    qg::submitTask(qg::WorkerPools::io(), qg::TaskPriority::Normal, nullptr, this,
                   [paths, targetDir, result]() {
                       *result = moveSync(paths, targetDir);
                   },
                   [this, result]() {
                       emitFinished(QStringLiteral("move"), *result);
                   });
}

void FileOperationsService::copy(const QStringList& paths, const QString& targetDir, bool) {
    auto result = std::make_shared<OperationResult>();
    qg::submitTask(qg::WorkerPools::io(), qg::TaskPriority::Normal, nullptr, this,
                   [paths, targetDir, result]() {
                       *result = copySync(paths, targetDir);
                   },
                   [this, result]() {
                       emitFinished(QStringLiteral("copy"), *result);
                   });
}

void FileOperationsService::deletePermanently(const QStringList& paths) {
    auto result = std::make_shared<OperationResult>();
    qg::submitTask(qg::WorkerPools::io(), qg::TaskPriority::Normal, nullptr, this,
                   [paths, result]() {
                       *result = deletePermanentlySync(paths);
                   },
                   [this, result]() {
                       emitFinished(QStringLiteral("delete"), *result);
                   });
}

// reveal()/openWith() stay synchronous on the GUI thread: they only spawn a
// short-lived launcher (`dbus-send`/`xdg-open`) and their detached/blocking
// launch semantics are the subject of the launch-helper work item, not this one.
void FileOperationsService::reveal(const QString& path) {
    OperationResult r;
    r.completedPath = path;
    // D-Bus org.freedesktop.FileManager1.ShowItems with xdg-open fallback
    const QStringList args{path};
    const QStringList dbusArgs{
        QStringLiteral("--session"), QStringLiteral("--dest=org.freedesktop.FileManager1"),
        QStringLiteral("--print-reply"), QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1.ShowItems"),
        QStringLiteral("array:string:").append(QUrl::fromLocalFile(path).toString()),
        QStringLiteral("string:")};
    if (QProcess::execute(QStringLiteral("dbus-send"), dbusArgs) == 0) {
        r.success = true;
    } else if (QProcess::execute(QStringLiteral("xdg-open"), QStringList{path}) == 0) {
        r.success = true;
    } else {
        r.error = QStringLiteral("No file manager available");
    }
    emitFinished(QStringLiteral("reveal"), r);
}

void FileOperationsService::openWith(const QString& path, const QString&) {
    OperationResult r;
    r.completedPath = path;
    r.success = QProcess::startDetached(QStringLiteral("xdg-open"), QStringList{path});
    if (!r.success)
        r.error = QStringLiteral("xdg-open failed");
    emitFinished(QStringLiteral("openWith"), r);
}

void FileOperationsService::showProperties(const QString& path) {
    OperationResult r;
    r.completedPath = path;
    r.success = true;
    emitFinished(QStringLiteral("properties"), r);
}

void FileOperationsService::emitFinished(const QString& kind, const OperationResult& r) {
    emit operationFinished(kind, r);
}

OperationResult FileOperationsService::renameSync(const QString& path, const QString& newBaseName) {
    OperationResult r;
    r.completedPath = path;
    const QFileInfo fi(path);
    if (!fi.exists()) {
        r.error = QStringLiteral("Source does not exist");
        return r;
    }

    // Preserve extension by default
    QString newName = newBaseName;
    if (!fi.suffix().isEmpty() && !newName.endsWith('.' + fi.suffix()))
        newName += '.' + fi.suffix();

    const QString target = fi.absolutePath() + QDir::separator() + newName;
    if (target == path) {
        r.success = true;
        r.completedPath = path;
        return r;
    }
    if (QFileInfo::exists(target)) {
        r.error = QStringLiteral("A file named \"%1\" already exists").arg(newName);
        return r;
    }
    if (QFile::rename(path, target)) {
        r.success = true;
        r.completedPath = target;
    } else {
        r.error = QStringLiteral("Rename failed");
    }
    return r;
}

OperationResult FileOperationsService::moveSync(const QStringList& paths, const QString& targetDir) {
    OperationResult r;
    r.completedPaths.clear();
    QDir target(targetDir);
    if (!target.exists()) {
        r.error = QStringLiteral("Target folder does not exist");
        return r;
    }
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        const QString dest = targetDir + QDir::separator() + fi.fileName();
        if (QFileInfo::exists(dest)) {
            r.completedPaths.append(p); // skip collision, record as not-moved
            continue;
        }
        if (QFile::rename(p, dest)) {
            r.completedPaths.append(dest);
        } else {
            r.error = QStringLiteral("Move failed for %1").arg(fi.fileName());
            break;
        }
    }
    r.success = r.error.isEmpty();
    return r;
}

OperationResult FileOperationsService::copySync(const QStringList& paths, const QString& targetDir) {
    OperationResult r;
    r.completedPaths.clear();
    QDir target(targetDir);
    if (!target.exists()) {
        r.error = QStringLiteral("Target folder does not exist");
        return r;
    }
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        const QString dest = targetDir + QDir::separator() + fi.fileName();
        if (QFileInfo::exists(dest)) {
            continue; // collision: skip
        }
        if (fi.isDir()) {
            if (copyDirRecursive(p, dest)) r.completedPaths.append(dest);
            else { r.error = QStringLiteral("Copy failed for %1").arg(fi.fileName()); break; }
        } else if (QFile::copy(p, dest)) {
            r.completedPaths.append(dest);
        } else {
            r.error = QStringLiteral("Copy failed for %1").arg(fi.fileName());
            break;
        }
    }
    r.success = r.error.isEmpty();
    return r;
}

bool copyDirRecursive(const QString& src, const QString& dst) {
    QDir source(src);
    if (!source.exists()) return false;
    QDir dest(dst);
    if (!dest.mkpath(".")) return false;
    const auto entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& e : entries) {
        const QString target = dst + QDir::separator() + e.fileName();
        if (e.isDir()) {
            if (!copyDirRecursive(e.absoluteFilePath(), target)) return false;
        } else if (!QFile::copy(e.absoluteFilePath(), target)) {
            return false;
        }
    }
    return true;
}

OperationResult FileOperationsService::deletePermanentlySync(const QStringList& paths) {
    OperationResult r;
    r.completedPaths.clear();
    if (paths.isEmpty()) {
        r.error = QStringLiteral("Nothing to delete");
        return r;
    }
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        if (!fi.exists()) {
            // A missing path is an explicit failure, never a phantom success.
            if (r.error.isEmpty()) {
                r.error = QStringLiteral("%1 does not exist").arg(fi.fileName().isEmpty() ? p : fi.fileName());
            }
            continue;
        }
        const bool ok = fi.isDir() ? QDir(p).removeRecursively() : QFile::remove(p);
        if (ok) {
            r.completedPaths.append(p);
        } else if (r.error.isEmpty()) {
            r.error = QStringLiteral("Delete failed for %1").arg(fi.fileName());
        }
    }
    r.success = r.error.isEmpty() && r.completedPaths.size() == paths.size();
    return r;
}