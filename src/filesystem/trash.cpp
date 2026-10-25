#include "trash.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QDateTime>
#include <QUrl>

TrashService::TrashService(QObject* parent) : QObject(parent) {}

QString TrashService::trashRoot() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/Trash");
}

TrashResult TrashService::trash(const QString& path) {
    // The spec-compliant implementation runs first: it writes the .trashinfo and
    // returns the trashed path, which is what Undo needs. `gio trash` moves the
    // file too but tells us nothing about where it went, and `gio trash --restore`
    // only accepts a trash:// URI served by the gvfs session daemon, so it cannot
    // be driven reliably from here (REMEDIATION-010 finding).
    TrashResult r = trashFallback(path);
    if (r.success) {
        emit trashed(r.originalPath, r.trashPath);
        return r;
    }
    if (QFileInfo::exists(path)) {
        // Cross-device (or unwritable home trash): let gio move it to the
        // volume's own .Trash-1000. Success here has no undo path — callers must
        // not offer an Undo affordance they cannot honour.
        const QStringList args{QStringLiteral("trash"), path};
        if (QProcess::execute(QStringLiteral("gio"), args) == 0) {
            TrashResult gio;
            gio.success = true;
            gio.originalPath = path;
            gio.canUndo = false;
            emit trashed(path, QString());
            return gio;
        }
    }
    return r;
}

bool TrashService::restore(const QString& trashPath, const QString& originalPath) {
    // Without the trashed path there is nothing to move back: the gio branch
    // reports no trash path, so callers do not offer Undo for it.
    if (trashPath.isEmpty()) {
        return false;
    }
    const bool ok = restoreFallback(trashPath, originalPath);
    if (ok) emit restored(originalPath);
    return ok;
}

TrashResult TrashService::trashFallback(const QString& path) {
    TrashResult r;
    r.originalPath = path;
    const QFileInfo fi(path);
    if (!fi.exists()) {
        r.error = QStringLiteral("Source does not exist");
        return r;
    }

    const QString filesDir = trashRoot() + QStringLiteral("/files");
    const QString infoDir = trashRoot() + QStringLiteral("/info");
    QDir().mkpath(filesDir);
    QDir().mkpath(infoDir);

    // Collision-suffixed name
    QString name = fi.fileName();
    QString target = filesDir + QDir::separator() + name;
    int suffix = 1;
    while (QFileInfo::exists(target)) {
        name = fi.completeBaseName() + QStringLiteral("_%1.").arg(suffix) + fi.suffix();
        target = filesDir + QDir::separator() + name;
        ++suffix;
    }

    if (!QFile::rename(path, target)) {
        r.error = QStringLiteral("Trash move failed");
        return r;
    }

    // Write .trashinfo
    const QString infoPath = infoDir + QDir::separator() + name + QStringLiteral(".trashinfo");
    const QString content = QStringLiteral("[Trash Info]\nPath=%1\nDeletionDate=%2\n")
                                .arg(QUrl::fromLocalFile(path).toEncoded(),
                                     QDateTime::currentDateTime().toString(Qt::ISODate));
    QFile infoFile(infoPath);
    if (infoFile.open(QIODevice::WriteOnly)) {
        infoFile.write(content.toUtf8());
        infoFile.close();
    }

    r.success = true;
    r.trashPath = target;
    r.canUndo = true;
    return r;
}

bool TrashService::restoreFallback(const QString& trashPath, const QString& originalPath) {
    QDir dir;
    if (dir.exists(originalPath)) return false; // collision: do not overwrite
    return QFile::rename(trashPath, originalPath);
}