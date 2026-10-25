#include "operations.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QUrl>
#include <QDebug>

bool copyDirRecursive(const QString& src, const QString& dst);

FileOperationsService::FileOperationsService(QObject* parent) : QObject(parent) {}

void FileOperationsService::rename(const QString& path, const QString& newBaseName) {
    emitFinished(QStringLiteral("rename"), renameSync(path, newBaseName));
}

void FileOperationsService::move(const QStringList& paths, const QString& targetDir) {
    emitFinished(QStringLiteral("move"), moveSync(paths, targetDir));
}

void FileOperationsService::copy(const QStringList& paths, const QString& targetDir, bool) {
    emitFinished(QStringLiteral("copy"), copySync(paths, targetDir));
}

void FileOperationsService::deletePermanently(const QStringList& paths) {
    emitFinished(QStringLiteral("delete"), deletePermanentlySync(paths));
}

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