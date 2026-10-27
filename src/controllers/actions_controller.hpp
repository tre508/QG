#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class FileOperationsService;
class TrashService;

// ActionsController (DESIGN.md §11.1, §11.2): orchestrates file actions
// (Rename, Trash, Favorite, Watched, Move, Copy, Reveal, Open With,
// Properties, Collection) and dispatches to services. Destructive actions
// never live on hover controls.

class ActionsController : public QObject {
    Q_OBJECT
public:
    explicit ActionsController(QObject* parent = nullptr);

    void setFileOperations(FileOperationsService* ops);
    void setTrashService(TrashService* trash);

    Q_INVOKABLE void rename(const QString& path, const QString& newBaseName);
    Q_INVOKABLE void trash(const QStringList& paths);
    Q_INVOKABLE void trashOne(const QString& path);
    void toastForFailure(const QString& kind, const QString& message);
    Q_INVOKABLE void reveal(const QString& path);
    Q_INVOKABLE void openWith(const QString& path);
    Q_INVOKABLE void properties(const QString& path);
    Q_INVOKABLE void copy(const QStringList& paths, const QString& targetDir);
    Q_INVOKABLE void move(const QStringList& paths, const QString& targetDir);
    Q_INVOKABLE void deletePermanently(const QStringList& paths);

    // Clipboard cut/copy/paste (P63, DESIGN.md §11.1)
    Q_INVOKABLE void cut(const QStringList& paths);
    Q_INVOKABLE void copyToClipboard(const QStringList& paths);
    Q_INVOKABLE void paste(const QString& targetDir);
    Q_INVOKABLE QStringList clipboardPaths() const;
    Q_INVOKABLE bool clipboardIsCut() const;

    Q_INVOKABLE QStringList menuItemsForSelection(int count) const;

    // --- Dialog / feedback support (REMEDIATION-010 / D-08) ------------------
    // Real file stats for the Properties dialog (raw values; the presentation
    // formats them). Never fabricated: unknown fields are empty.
    Q_INVOKABLE QVariantMap fileInfo(const QString& path) const;
    // Live rename validation for the inline editor (§7.2/§11.3). Returns
    // { ok: bool, message: QString, finalName: QString } and uses the same
    // canonical validator as the file-operation service.
    Q_INVOKABLE QVariantMap validateRename(const QString& path,
                                           const QString& newBaseName,
                                           const QStringList& siblingNames) const;
    // Extension-preserving rename target (what renameSync will use).
    Q_INVOKABLE QString targetPathForRename(const QString& path,
                                            const QString& newBaseName) const;
    Q_INVOKABLE bool pathExists(const QString& path) const;
    // "clip (2).mp4"-style base name when `newBaseName` is taken (Keep both).
    Q_INVOKABLE QString uniqueBaseName(const QString& path,
                                       const QString& newBaseName) const;
    // Put `path` on the system clipboard (the access-denied overlay action).
    Q_INVOKABLE void copyPath(const QString& path) const;

    // Undo the most recent Trash (DESIGN.md §3.6: the toast's Undo affordance).
    // Uses the undo record from that trash call; false when there is nothing to
    // undo or the restore failed.
    Q_INVOKABLE bool undoLastTrash();
    Q_INVOKABLE bool canUndoTrash() const { return !lastTrashOriginalPath_.isEmpty(); }

signals:
    void renameRequested(const QString& path, const QString& newBaseName);
    void trashRequested(const QStringList& paths);
    void toastRequested(const QString& message, const QString& undoKind, const QString& undoPath);
    void actionFinished(const QString& kind, bool success);

private:
    FileOperationsService* ops_ = nullptr;
    TrashService* trash_ = nullptr;
    QStringList clipboardPaths_;
    bool clipboardCut_ = false;
    // Undo record for the last Trash (freedesktop restore needs both paths).
    QString lastTrashPath_;
    QString lastTrashOriginalPath_;

    // System clipboard integration for cross-app file copy/paste (FIX-DND-007).
    void writeClipboardMimeData(const QStringList& paths, bool cut);
    QStringList readClipboardMimeData() const;
};