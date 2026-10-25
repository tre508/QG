#include "actions_controller.hpp"
#include "filesystem/operations.hpp"
#include "filesystem/trash.hpp"

#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QMimeDatabase>
#include <QUrl>

ActionsController::ActionsController(QObject* parent) : QObject(parent) {}

void ActionsController::setFileOperations(FileOperationsService* ops)
{
    if (ops_ == ops) {
        return;
    }
    if (ops_) {
        ops_->disconnect(this);
    }
    ops_ = ops;
    if (ops_) {
        // The service reports the real outcome; surface it (never assume
        // success — DESIGN.md §11.1, "no phantom success").
        connect(ops_, &FileOperationsService::operationFinished, this,
                [this](const QString& kind, const OperationResult& result) {
                    emit actionFinished(kind, result.success);
                    if (!result.success) {
                        const QString reason = result.error.isEmpty()
                            ? QStringLiteral("The operation could not be completed.")
                            : result.error;
                        emit toastRequested(reason, QString(), QString());
                    }
                });
    }
}
void ActionsController::setTrashService(TrashService* trash) { trash_ = trash; }

void ActionsController::rename(const QString& path, const QString& newBaseName) {
    emit renameRequested(path, newBaseName);
    if (ops_) {
        // actionFinished/toastRequested come from operationFinished (above).
        ops_->rename(path, newBaseName);
    } else {
        emit actionFinished(QStringLiteral("rename"), false);
        emit toastRequested(QStringLiteral("Renaming is unavailable."), QString(), QString());
    }
}

void ActionsController::trash(const QStringList& paths) {
    emit trashRequested(paths);
    if (paths.isEmpty()) {
        return;
    }
    if (trash_ == nullptr) {
        emit actionFinished(QStringLiteral("trash"), false);
        emit toastRequested(QStringLiteral("Trash is unavailable."), QString(), QString());
        return;
    }

    int ok = 0;
    bool undoable = false;
    QString lastError;
    for (const QString& p : paths) {
        const TrashResult result = trash_->trash(p);
        if (result.success) {
            ++ok;
            // Only offer Undo when the trashed location is known; a fixed-affordance
            // that cannot restore would be phantom success.
            if (!result.trashPath.isEmpty()) {
                lastTrashPath_ = result.trashPath;
                lastTrashOriginalPath_ = result.originalPath;
                undoable = true;
            }
        } else {
            lastError = result.error;
        }
    }

    const bool success = ok == paths.size();
    emit actionFinished(QStringLiteral("trash"), success);
    if (success) {
        emit toastRequested(ok == 1
                                ? QStringLiteral("Moved to Trash")
                                : QStringLiteral("%1 items moved to Trash").arg(ok),
                            undoable ? QStringLiteral("trash") : QString(),
                            undoable ? lastTrashPath_ : QString());
    } else if (ok > 0) {
        // Bulk partial: say what happened instead of claiming success.
        emit toastRequested(QStringLiteral("%1 of %2 moved to Trash").arg(ok).arg(paths.size()),
                            undoable ? QStringLiteral("trash") : QString(),
                            undoable ? lastTrashPath_ : QString());
    } else {
        emit toastRequested(lastError.isEmpty()
                                ? QStringLiteral("Could not move to Trash")
                                : lastError,
                            QString(), QString());
    }
}

bool ActionsController::undoLastTrash()
{
    if (lastTrashOriginalPath_.isEmpty() || trash_ == nullptr) {
        return false;
    }
    const bool ok = trash_->restore(lastTrashPath_, lastTrashOriginalPath_);
    if (ok) {
        lastTrashPath_.clear();
        lastTrashOriginalPath_.clear();
        emit toastRequested(QStringLiteral("Restored"), QString(), QString());
    } else {
        emit toastRequested(QStringLiteral("Could not undo the move to Trash."),
                            QString(), QString());
    }
    return ok;
}

void ActionsController::trashOne(const QString& path) {
    trash({path});
}

void ActionsController::reveal(const QString& path) {
    if (ops_) {
        ops_->reveal(path); // outcome reported via operationFinished
    } else {
        emit actionFinished(QStringLiteral("reveal"), false);
    }
}

void ActionsController::openWith(const QString& path) {
    if (ops_) {
        ops_->openWith(path, QString());
    } else {
        emit actionFinished(QStringLiteral("openWith"), false);
    }
}

void ActionsController::properties(const QString& path) {
    if (ops_) {
        ops_->showProperties(path);
    } else {
        emit actionFinished(QStringLiteral("properties"), false);
    }
}

void ActionsController::copy(const QStringList& paths, const QString& targetDir) {
    if (ops_) ops_->copy(paths, targetDir);
}

void ActionsController::move(const QStringList& paths, const QString& targetDir) {
    if (ops_) ops_->move(paths, targetDir);
}

void ActionsController::deletePermanently(const QStringList& paths) {
    if (ops_) {
        // Outcome reported via operationFinished → actionFinished (the same path
        // rename/trash/move/copy use) — no parallel reporting.
        ops_->deletePermanently(paths);
    } else {
        emit actionFinished(QStringLiteral("delete"), false);
        emit toastRequested(QStringLiteral("Permanent delete is unavailable."),
                            QString(), QString());
    }
}

QStringList ActionsController::menuItemsForSelection(int count) const {    // Verbs first, destructive last; collection disabled "coming soon".
    QStringList items{
        QStringLiteral("Play"),
        QStringLiteral("Favorite"),
        QStringLiteral("Mark Watched"),
        QStringLiteral("Rename"),
        QStringLiteral("Cut"),
        QStringLiteral("Copy"),
        QStringLiteral("Reveal"),
        QStringLiteral("Open With"),
        QStringLiteral("Add to Collection (coming soon)"),
        QStringLiteral("Properties"),
        QStringLiteral("Move to Trash"),
        QStringLiteral("Delete Permanently"),
    };
    if (count > 1) {
        // Multi-select: bulk-capable verbs + N items title
        items.prepend(QStringLiteral("%1 items").arg(count));
    }
    return items;
}

void ActionsController::cut(const QStringList& paths) {
    clipboardPaths_ = paths;
    clipboardCut_ = true;
    writeClipboardMimeData(paths, true);
}

void ActionsController::copyToClipboard(const QStringList& paths) {
    clipboardPaths_ = paths;
    clipboardCut_ = false;
    writeClipboardMimeData(paths, false);
}

void ActionsController::paste(const QString& targetDir) {
    // System clipboard is the source of truth for cross-app paste
    // (Nautilus -> QG). Fall back to QG's own internal clipboard state
    // for QG -> QG paste when the system clipboard has no file URLs.
    QStringList paths = readClipboardMimeData();
    bool cut = clipboardCut_;
    if (paths.isEmpty()) {
        paths = clipboardPaths_;
        cut = clipboardCut_;
    }
    if (paths.isEmpty()) return;
    if (cut) {
        move(paths, targetDir);
        clipboardPaths_.clear();
        clipboardCut_ = false;
    } else {
        copy(paths, targetDir);
    }
}

void ActionsController::writeClipboardMimeData(const QStringList& paths, bool cut) {
    QClipboard* cb = QGuiApplication::clipboard();
    if (!cb || paths.isEmpty()) return;
    QMimeData* mime = new QMimeData();
    QList<QUrl> urls;
    for (const QString& p : paths) {
        if (!p.isEmpty())
            urls.append(QUrl::fromLocalFile(p));
    }
    mime->setUrls(urls);  // text/uri-list — standards-compatible, Nautilus-readable
    // GNOME/Nautilus cut/copy intent marker (first line: "copy" or "cut").
    QByteArray gnomeData;
    gnomeData.append(cut ? "cut\n" : "copy\n");
    for (const QString& p : paths) {
        if (!p.isEmpty())
            gnomeData.append("file://" + p.toUtf8() + "\n");
    }
    mime->setData("x-special/gnome-copied-files", gnomeData);
    cb->setMimeData(mime);  // QClipboard takes ownership of mime
}

QStringList ActionsController::readClipboardMimeData() const {
    QClipboard* cb = QGuiApplication::clipboard();
    if (!cb) return {};
    const QMimeData* mime = cb->mimeData();
    if (!mime || !mime->hasUrls()) return {};
    QStringList paths;
    for (const QUrl& url : mime->urls()) {
        if (url.isLocalFile() && !url.toLocalFile().isEmpty())
            paths.append(url.toLocalFile());
    }
    return paths;
}

QStringList ActionsController::clipboardPaths() const { return clipboardPaths_; }
bool ActionsController::clipboardIsCut() const { return clipboardCut_; }

namespace {

// POSIX-style "rw-r--r--" (the Properties dialog shows what the OS says).
QString permissionsString(const QFileInfo& fi)
{
    const QFile::Permissions p = fi.permissions();
    auto bit = [p](QFile::Permission perm) { return p.testFlag(perm) ? QLatin1Char('r') : QLatin1Char('-'); };
    QString out;
    out += bit(QFile::ReadOwner);
    out += p.testFlag(QFile::WriteOwner) ? QLatin1Char('w') : QLatin1Char('-');
    out += p.testFlag(QFile::ExeOwner) ? QLatin1Char('x') : QLatin1Char('-');
    out += p.testFlag(QFile::ReadGroup) ? QLatin1Char('r') : QLatin1Char('-');
    out += p.testFlag(QFile::WriteGroup) ? QLatin1Char('w') : QLatin1Char('-');
    out += p.testFlag(QFile::ExeGroup) ? QLatin1Char('x') : QLatin1Char('-');
    out += p.testFlag(QFile::ReadOther) ? QLatin1Char('r') : QLatin1Char('-');
    out += p.testFlag(QFile::WriteOther) ? QLatin1Char('w') : QLatin1Char('-');
    out += p.testFlag(QFile::ExeOther) ? QLatin1Char('x') : QLatin1Char('-');
    return out;
}

} // namespace

// Real filesystem stats for the Properties dialog (nothing is invented; a
// missing file simply reports exists == false).
QVariantMap ActionsController::fileInfo(const QString& path) const
{
    QVariantMap info;
    const QFileInfo fi(path);
    info.insert(QStringLiteral("path"), path);
    info.insert(QStringLiteral("name"), fi.fileName());
    info.insert(QStringLiteral("exists"), fi.exists());
    if (!fi.exists()) {
        return info;
    }
    info.insert(QStringLiteral("sizeBytes"), fi.size());
    info.insert(QStringLiteral("mtimeNs"), fi.lastModified().toMSecsSinceEpoch() * 1000000LL);
    info.insert(QStringLiteral("permissions"), permissionsString(fi));
    info.insert(QStringLiteral("owner"), fi.owner());
    info.insert(QStringLiteral("isDir"), fi.isDir());
    info.insert(QStringLiteral("mimeType"),
                QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchDefault).name());
    return info;
}

// The target renameSync would use (extension preserved) — the conflict dialog
// and the collision check both read this, so the UI cannot drift from what the
// service does.
QString ActionsController::targetPathForRename(const QString& path,
                                               const QString& newBaseName) const
{
    const QFileInfo fi(path);
    QString newName = newBaseName;
    const QString suffix = fi.suffix();
    if (!suffix.isEmpty() && !newName.endsWith(QLatin1Char('.') + suffix)) {
        newName += QLatin1Char('.') + suffix;
    }
    return fi.absolutePath() + QDir::separator() + newName;
}

bool ActionsController::pathExists(const QString& path) const
{
    return !path.isEmpty() && QFileInfo::exists(path);
}

QString ActionsController::uniqueBaseName(const QString& path, const QString& newBaseName) const
{
    const QFileInfo fi(path);
    const QString suffix = fi.suffix();
    QString base = newBaseName;
    if (!suffix.isEmpty() && base.endsWith(QLatin1Char('.') + suffix)) {
        base.chop(suffix.size() + 1);
    }

    const QString dir = fi.absolutePath();
    for (int i = 2; i < 1000; ++i) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(base).arg(i);
        QString full = dir + QDir::separator() + candidate;
        if (!suffix.isEmpty()) {
            full += QLatin1Char('.') + suffix;
        }
        if (!QFileInfo::exists(full)) {
            return candidate;
        }
    }
    return base;
}

void ActionsController::copyPath(const QString& path) const
{
    if (QGuiApplication::clipboard() != nullptr && !path.isEmpty()) {
        QGuiApplication::clipboard()->setText(path);
    }
}
