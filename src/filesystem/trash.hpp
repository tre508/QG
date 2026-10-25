#pragma once
#include <QObject>
#include <QString>

// TrashService (DESIGN.md §17.7): freedesktop-compliant Trash with undo.
// Prefers `gio trash`; else $XDG_DATA_HOME/Trash/files/<name> + .trashinfo.
// Undo record restores to the original path. Never permanently deletes
// without explicit confirmation.

struct TrashResult {
    bool success = false;
    QString originalPath;
    QString trashPath;
    QString error;
    // Undo: restore trashPath → originalPath
    bool canUndo = false;
};

class TrashService : public QObject {
    Q_OBJECT
public:
    explicit TrashService(QObject* parent = nullptr);

    Q_INVOKABLE TrashResult trash(const QString& path);
    Q_INVOKABLE bool restore(const QString& trashPath, const QString& originalPath);

    // Spec-compliant fallback (used when gio is unavailable or on removable media).
    static TrashResult trashFallback(const QString& path);
    static bool restoreFallback(const QString& trashPath, const QString& originalPath);

signals:
    void trashed(const QString& originalPath, const QString& trashPath);
    void restored(const QString& originalPath);

private:
    static QString trashRoot();
};