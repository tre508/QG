#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QStringList>
#include <functional>

// FileOperationsService (DESIGN.md §11, §17.6): real filesystem operations.
//
// The Q_INVOKABLE entry points are an asynchronous facade: rename/move/copy/
// deletePermanently submit the work to the bounded I/O pool and deliver the
// result to this object on the GUI thread, so no directory walk, copy or delete
// ever runs on the GUI thread (DESIGN.md §17.6, §15.3, §22.1). Results arrive in
// completion order, after the event loop turns.
//
// Rename preserves extension + records old→new mapping. Reveal via
// org.freedesktop.FileManager1.ShowItems with an xdg-open fallback; Open With via
// xdg-open. Never permanently deletes by default.
//
// The *Sync helpers are the payload of that facade: pure I/O, safe to call from
// any thread, and used directly by tests. Cross-device fallback (§17.6) and
// chunked copy with progress + cancellation checkpoints (§8.1, §15.4) are NOT
// implemented — they are the follow-up increments FIX-FO-01/FIX-FO-02, not
// silent omissions.

struct OperationResult {
    bool success = false;
    QString message;
    QString completedPath;        // primary resulting path
    QStringList completedPaths;   // subset completed for bulk/cancelled ops
    QString error;
};

struct RenameSpec {
    QString path;
    QString newBaseName;   // without extension
};

class FileOperationsService : public QObject {
    Q_OBJECT
public:
    explicit FileOperationsService(QObject* parent = nullptr);

    // Async entry points (results via queued signals on the GUI thread).
    Q_INVOKABLE void rename(const QString& path, const QString& newBaseName);
    Q_INVOKABLE void move(const QStringList& paths, const QString& targetDir);
    Q_INVOKABLE void copy(const QStringList& paths, const QString& targetDir, bool cancellable = true);
    Q_INVOKABLE void deletePermanently(const QStringList& paths);
    // Launcher entry points: short-lived, still called on the GUI thread.
    Q_INVOKABLE void reveal(const QString& path);
    Q_INVOKABLE void openWith(const QString& path, const QString& appId);
    Q_INVOKABLE void showProperties(const QString& path);

    // Synchronous building blocks (used by tests and the async wrappers).
    static OperationResult renameSync(const QString& path, const QString& newBaseName);
    static OperationResult moveSync(const QStringList& paths, const QString& targetDir);
    static OperationResult copySync(const QStringList& paths, const QString& targetDir);
    static OperationResult deletePermanentlySync(const QStringList& paths);

signals:
    void operationFinished(const QString& kind, const OperationResult& result);

private:
    void emitFinished(const QString& kind, const OperationResult& r);
};