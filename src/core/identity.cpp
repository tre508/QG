#include "core/identity.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>

#include <cerrno>
#include <sys/stat.h>

namespace qg {
namespace {

// Read at most the first 64 KiB for the fingerprint fallback (DESIGN.md §17.8).
constexpr qint64 kFingerprintBytes = 64 * 1024;

qint64 mtimeNsFromStat(const struct stat &st)
{
#if defined(__linux__)
    return static_cast<qint64>(st.st_mtim.tv_sec) * 1000000000LL
           + static_cast<qint64>(st.st_mtim.tv_nsec);
#else
    return static_cast<qint64>(st.st_mtime) * 1000000000LL;
#endif
}

} // namespace

QString encodeInodeIdentity(std::uint64_t device, std::uint64_t inode)
{
    return QStringLiteral("inode:%1:%2")
        .arg(device, 0, 16)
        .arg(inode, 0, 16);
}

QString encodeFingerprintIdentity(qint64 size, qint64 mtimeNs, const QByteArray &hash)
{
    return QStringLiteral("fp:%1:%2:%3")
        .arg(size)
        .arg(mtimeNs)
        .arg(QString::fromLatin1(hash.toHex()));
}

QString encodePathIdentity(const QString &absolutePath)
{
    return QStringLiteral("path:") + QDir::cleanPath(absolutePath);
}

FileIdentity identityFromStat(std::uint64_t device, std::uint64_t inode)
{
    return FileIdentity{encodeInodeIdentity(device, inode), IdentityKind::Inode};
}

Result<FileIdentity> fingerprintIdentity(const QString &path, qint64 size, qint64 mtimeNs)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return Error::make(ErrorCode::AccessDenied,
                           QStringLiteral("Cannot read file for fingerprint"),
                           path,
                           errno);
    }

    const QByteArray head = file.read(kFingerprintBytes);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(head);

    return FileIdentity{encodeFingerprintIdentity(size, mtimeNs, hash.result()),
                        IdentityKind::Fingerprint};
}

Result<FileIdentity> identityForStat(const QString &path,
                                     std::uint64_t device,
                                     std::uint64_t inode,
                                     qint64 size,
                                     qint64 mtimeNs)
{
    if (inode != 0) {
        return identityFromStat(device, inode);
    }
    // Network/FUSE filesystems with unstable inodes: fingerprint fallback (§17.8).
    const Result<FileIdentity> fingerprint = fingerprintIdentity(path, size, mtimeNs);
    if (fingerprint) {
        return fingerprint;
    }
    // Last resort: path-only identity (explicit degradation, never empty).
    return FileIdentity{encodePathIdentity(path), IdentityKind::PathOnly};
}

Result<FileIdentity> identityFromPath(const QString &path)
{
    struct stat st {};
    const QByteArray encoded = QFile::encodeName(path);
    if (::stat(encoded.constData(), &st) != 0) {
        return Error::make(ErrorCode::NotFound,
                           QStringLiteral("Cannot stat file"),
                           path,
                           errno);
    }

    if (st.st_ino != 0) {
        return identityFromStat(static_cast<std::uint64_t>(st.st_dev),
                                static_cast<std::uint64_t>(st.st_ino));
    }

    // Filesystem does not report a stable inode: fall back to a fingerprint.
    Result<FileIdentity> fingerprint =
        fingerprintIdentity(path, static_cast<qint64>(st.st_size), mtimeNsFromStat(st));
    if (fingerprint) {
        return fingerprint;
    }

    // Last resort: degrade explicitly to a path-only identity.
    return FileIdentity{encodePathIdentity(path), IdentityKind::PathOnly};
}

} // namespace qg
