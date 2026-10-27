#pragma once

// QG media identity (DESIGN.md §17.8, §21.4).
//
// Identity keeps application state (favorite, watched, position, costs) attached
// to a file across renames within a filesystem. The primary key is the inode
// `(st_dev, st_ino)`. Where a filesystem does not provide a stable inode, a
// lazily computed fingerprint `(size, mtime_ns, first-64KiB hash)` is used, and
// only as a last resort is a path-only key produced (degradation is explicit,
// never silent).

#include "core/errors.hpp"

#include <QByteArray>
#include <QString>

#include <cstdint>

namespace qg {

enum class IdentityKind {
    Inode,       // stable (st_dev, st_ino)
    Fingerprint, // (size, mtime_ns, first-64KiB hash)
    PathOnly,    // degraded: no stable identity available
};

struct FileIdentity {
    QString key;
    IdentityKind kind = IdentityKind::Inode;

    bool isValid() const noexcept { return !key.isEmpty(); }

    friend bool operator==(const FileIdentity &, const FileIdentity &) = default;
};

// Deterministic string encodings (used as SQLite keys).
QString encodeInodeIdentity(std::uint64_t device, std::uint64_t inode);
QString encodeFingerprintIdentity(qint64 size, qint64 mtimeNs, const QByteArray &hash);
QString encodePathIdentity(const QString &absolutePath);

// Identity from already-collected `stat` data. Callers must only use this when
// `inode` is non-zero; otherwise use fingerprintIdentity().
FileIdentity identityFromStat(std::uint64_t device,
                              std::uint64_t inode);

// Lazily computed fingerprint fallback. Reads at most 64 KiB.
Result<FileIdentity> fingerprintIdentity(const QString &path, qint64 size, qint64 mtimeNs);

// Full ladder for already-collected stat data (§17.8): inode when the
// filesystem provides one, otherwise (size, mtime_ns, 64 KiB) fingerprint,
// otherwise path-only. Must run on a worker thread (the fingerprint reads up
// to 64 KiB from the file).
Result<FileIdentity> identityForStat(const QString &path,
                                     std::uint64_t device,
                                     std::uint64_t inode,
                                     qint64 size,
                                     qint64 mtimeNs);

// Full resolution for a path: stat -> inode, else fingerprint, else path-only.
Result<FileIdentity> identityFromPath(const QString &path);

} // namespace qg
