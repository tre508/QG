#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QStringList>
#include <functional>

// FileOperationsService (DESIGN.md §11, §17.6): real filesystem operations.
// Rename preserves extension + records old→new mapping; move/copy chunked with
// cross-device fallback; reveal via org.freedesktop.FileManager1.ShowItems;
// Open With via GIO/GDesktopAppInfo. Never permanently deletes by default.

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

    // Async, worker-safe entry points (results via queued signals).
    Q_INVOKABLE void rename(const QString& path, const QString& newBaseName);
    Q_INVOKABLE void move(const QStringList& paths, const QString& targetDir);
    Q_INVOKABLE void copy(const QStringList& paths, const QString& targetDir, bool cancellable = true);
    Q_INVOKABLE void deletePermanently(const QStringList& paths);
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