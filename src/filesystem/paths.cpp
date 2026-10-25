#include "filesystem/paths.hpp"

#include <QDir>
#include <QUrl>

namespace qg {

QString toFileUri(const QString &absolutePath)
{
    return QUrl::fromLocalFile(absolutePath).toString();
}

QString fromUriOrPath(const QString &uriOrPath)
{
    if (uriOrPath.startsWith(QStringLiteral("file:"))) {
        return QUrl(uriOrPath).toLocalFile();
    }
    return uriOrPath;
}

bool isAbsoluteLocalPath(const QString &path)
{
    return QDir::isAbsolutePath(path);
}

QString expandUser(const QString &path, const QString &home)
{
    if (path.isEmpty() || path.at(0) != QLatin1Char('~')) {
        return path;
    }
    const QString homeDir = home.isEmpty() ? QDir::homePath() : home;
    if (path.size() == 1) {
        return homeDir;
    }
    if (path.at(1) == QLatin1Char('/')) {
        return homeDir + path.mid(1);
    }
    // `~user/...` is not supported; return unchanged rather than mis-resolving.
    return path;
}

QString parentPath(const QString &path)
{
    if (path.isEmpty()) {
        return path;
    }
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        return QString();
    }
    if (slash == 0) {
        return QStringLiteral("/");
    }
    return path.left(slash);
}

QString baseName(const QString &path)
{
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? path : path.mid(slash + 1);
}

QString displayPath(const QString &absolutePath, const QString &home)
{
    if (home.isEmpty() || !absolutePath.startsWith(QLatin1Char('/'))) {
        return absolutePath;
    }
    if (absolutePath == home) {
        return QStringLiteral("~");
    }
    const QString prefix = home.endsWith(QLatin1Char('/')) ? home : home + QLatin1Char('/');
    if (absolutePath.startsWith(prefix)) {
        return QStringLiteral("~") + absolutePath.mid(home.size());
    }
    return absolutePath;
}

FilenameValidation validateFilename(const QString &name,
                                    const QStringList &existingNames,
                                    bool caseInsensitive)
{
    if (name.isEmpty() || name.trimmed().isEmpty()) {
        return {FilenameErrorCode::Empty, QStringLiteral("Name cannot be empty")};
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\0'))) {
        return {FilenameErrorCode::InvalidCharacter,
                QStringLiteral("Name cannot contain '/' or NUL")};
    }
    if (name.at(0) == QLatin1Char('-')) {
        return {FilenameErrorCode::LeadingDash,
                QStringLiteral("Name cannot start with '-'")};
    }
    if (name == QStringLiteral(".") || name == QStringLiteral("..")) {
        return {FilenameErrorCode::ReservedName,
                QStringLiteral("Name is reserved")};
    }

    for (const QString &existing : existingNames) {
        const bool equal = caseInsensitive
                               ? existing.compare(name, Qt::CaseInsensitive) == 0
                               : existing == name;
        if (equal) {
            return {FilenameErrorCode::Collision,
                    QStringLiteral("A file with that name already exists")};
        }
    }

    return {};
}

} // namespace qg
