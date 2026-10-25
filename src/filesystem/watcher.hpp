#pragma once

// Directory watching and identity-based diffing (DESIGN.md §17.5).
//
// `DirectoryWatcher` watches the current directory (and its parent, so it can
// re-arm when the directory is replaced), debounces bursts to a single
// `changed(path)` after 300 ms, and exposes an immediate refresh request.
// `computeFolderDiff` turns a fresh enumeration into the minimal set of
// identity-based updates, so the view applies `rowsInserted`/`rowsRemoved`/
// `dataChanged` instead of a full reload.

#include "core/types.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

namespace qg {

class DirectoryWatcher : public QObject {
    Q_OBJECT

public:
    explicit DirectoryWatcher(QObject *parent = nullptr);

    void setPath(const QString &directory);
    void clear();
    QString path() const { return m_directory; }

    void setDebounceInterval(int msec) { m_debounce.setInterval(msec); }

public slots:
    void forceRefresh();

signals:
    void changed(const QString &directory);
    void refreshRequested(const QString &directory);

private:
    void onDirectoryChanged();
    void onDebounceElapsed();
    void reArm();

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_directory;
};

// Difference from `current` rows to `next` enumeration, keyed by identity.
FolderDiff computeFolderDiff(const QList<Entry> &current, const QList<Entry> &next);

} // namespace qg
