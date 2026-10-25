#include "filesystem/enumerator.hpp"

#include "core/identity.hpp"
#include "media/formats.hpp"

#include <QDir>

#include <cerrno>
#include <cstring>
#include <memory>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace qg {
namespace {

struct DirCloser {
    void operator()(DIR *dir) const
    {
        if (dir != nullptr) {
            ::closedir(dir);
        }
    }
};

using DirPtr = std::unique_ptr<DIR, DirCloser>;

qint64 mtimeNs(const struct stat &st)
{
#if defined(__linux__)
    return static_cast<qint64>(st.st_mtim.tv_sec) * 1000000000LL
           + static_cast<qint64>(st.st_mtim.tv_nsec);
#else
    return static_cast<qint64>(st.st_mtime) * 1000000000LL;
#endif
}

void appendError(FolderSnapshot &snapshot, const QString &path, const QString &message)
{
    EntryError error;
    error.path = path;
    error.message = message;
    error.systemError = errno;
    snapshot.errors.append(error);
}

} // namespace

FilesystemEnumerator::FilesystemEnumerator(QObject *parent)
    : QObject(parent)
{
    registerCoreTypes();
}

FolderSnapshot FilesystemEnumerator::enumerate(const QString &path,
                                               const TaskToken &token,
                                               const EnumeratorOptions &options)
{
    FolderSnapshot snapshot;
    snapshot.path = path;
    snapshot.generation = token.generation();
    snapshot.complete = false;

    DIR *raw = ::opendir(QFile::encodeName(path).constData());
    if (raw == nullptr) {
        appendError(snapshot, path, QStringLiteral("Cannot open directory"));
        emit finished(snapshot);
        return snapshot;
    }
    DirPtr dir(raw);

    const int fd = ::dirfd(dir.get());
    const int batchSize = options.batchSize > 0 ? options.batchSize : 200;
    QList<Entry> batch;
    bool cancelled = false;

    while (true) {
        if (token.isCancelled()) {
            cancelled = true;
            break;
        }

        errno = 0;
        struct dirent *entry = ::readdir(dir.get());
        if (entry == nullptr) {
            break;
        }

        const QString name = QString::fromUtf8(entry->d_name);
        if (name == QStringLiteral(".") || name == QStringLiteral("..")) {
            continue;
        }
        if (!options.showHidden && name.startsWith(QLatin1Char('.'))) {
            continue;
        }

        struct stat st {};
        bool needStat = false;

        switch (entry->d_type) {
        case DT_DIR:
            needStat = true;
            break;
        case DT_REG:
            if (!isSupportedVideoName(name)) {
                continue; // discard without a stat
            }
            needStat = true;
            break;
        case DT_LNK:
        case DT_UNKNOWN:
            needStat = true;
            break;
        default:
            continue; // fifo / socket / device: not media
        }

        if (!needStat) {
            continue;
        }

        const QString fullPath = QDir(path).filePath(name);

        if (::fstatat(fd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
            appendError(snapshot, fullPath, QStringLiteral("Cannot stat entry"));
            continue;
        }

        if (S_ISLNK(st.st_mode)) {
            if (::fstatat(fd, entry->d_name, &st, 0) != 0) {
                appendError(snapshot, fullPath, QStringLiteral("Broken symlink"));
                continue;
            }
        }

        const bool isDir = S_ISDIR(st.st_mode);
        const bool isReg = S_ISREG(st.st_mode);
        if (!isDir && !(isReg && isSupportedVideoName(name))) {
            continue; // unsupported target: discard
        }

        Entry value;
        value.path = fullPath;
        value.name = name;
        value.kind = isDir ? EntryKind::Folder : EntryKind::Video;
        value.size = isDir ? 0 : static_cast<qint64>(st.st_size);
        value.mtimeNs = mtimeNs(st);
        if (st.st_ino != 0) {
            value.identity = identityFromStat(static_cast<quint64>(st.st_dev),
                                              static_cast<quint64>(st.st_ino))
                                 .key;
        }

        batch.append(value);
        if (batch.size() >= batchSize) {
            FolderSnapshot partial;
            partial.path = path;
            partial.generation = snapshot.generation;
            partial.entries = batch;
            partial.complete = false;
            emit batchReady(partial);
            snapshot.entries.append(batch);
            batch.clear();
        }
    }

    if (!batch.isEmpty()) {
        FolderSnapshot partial;
        partial.path = path;
        partial.generation = snapshot.generation;
        partial.entries = batch;
        partial.complete = false;
        emit batchReady(partial);
        snapshot.entries.append(batch);
        batch.clear();
    }

    snapshot.complete = !cancelled;
    emit finished(snapshot);
    return snapshot;
}

} // namespace qg
