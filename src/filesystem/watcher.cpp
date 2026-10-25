#include "filesystem/watcher.hpp"

#include <QDir>
#include <QFileInfo>
#include <QHash>

namespace qg {

DirectoryWatcher::DirectoryWatcher(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, &DirectoryWatcher::onDebounceElapsed);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &DirectoryWatcher::onDirectoryChanged);
}

void DirectoryWatcher::setPath(const QString &directory)
{
    m_directory = QDir::cleanPath(directory);
    reArm();
}

void DirectoryWatcher::clear()
{
    const QStringList watched = m_watcher.directories();
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }
    m_directory.clear();
}

void DirectoryWatcher::reArm()
{
    if (m_directory.isEmpty()) {
        return;
    }

    const QStringList watched = m_watcher.directories();
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }

    m_watcher.addPath(m_directory);

    // Watch the parent too: replacing the directory itself invalidates a watch
    // on the directory path, and a change in the parent is our cue to re-arm.
    const QString parent = QFileInfo(m_directory).absolutePath();
    if (!parent.isEmpty() && parent != m_directory) {
        m_watcher.addPath(parent);
    }
}

void DirectoryWatcher::onDirectoryChanged()
{
    m_debounce.start();
}

void DirectoryWatcher::onDebounceElapsed()
{
    reArm();
    if (!m_directory.isEmpty()) {
        emit changed(m_directory);
    }
}

void DirectoryWatcher::forceRefresh()
{
    if (!m_directory.isEmpty()) {
        emit refreshRequested(m_directory);
    }
}

FolderDiff computeFolderDiff(const QList<Entry> &current, const QList<Entry> &next)
{
    FolderDiff diff;

    QHash<QString, Entry> currentByIdentity;
    for (const Entry &entry : current) {
        if (!entry.identity.isEmpty()) {
            currentByIdentity.insert(entry.identity, entry);
        }
    }

    QHash<QString, Entry> nextByIdentity;
    for (const Entry &entry : next) {
        if (!entry.identity.isEmpty()) {
            nextByIdentity.insert(entry.identity, entry);
        }
    }

    for (const Entry &entry : current) {
        if (!entry.identity.isEmpty() && !nextByIdentity.contains(entry.identity)) {
            diff.removedIdentities.append(entry.identity);
        }
    }

    for (const Entry &entry : next) {
        if (entry.identity.isEmpty()) {
            diff.added.append(entry);
            continue;
        }
        const auto it = currentByIdentity.constFind(entry.identity);
        if (it == currentByIdentity.constEnd()) {
            diff.added.append(entry);
            continue;
        }
        const Entry &previous = it.value();
        if (previous.path != entry.path || previous.name != entry.name
            || previous.size != entry.size || previous.mtimeNs != entry.mtimeNs) {
            diff.changed.append(entry);
        }
    }

    return diff;
}

} // namespace qg
