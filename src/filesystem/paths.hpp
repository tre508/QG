#pragma once

// Path utilities and filename validation (DESIGN.md §11.3, §17.1, §21.1).
//
// These helpers are pure and preserve Linux semantics: `parentPath` never
// resolves symlinks and no normalization changes a path's meaning.

#include <QString>
#include <QStringList>

namespace qg {

// file:// URI <-> local path.
QString toFileUri(const QString &absolutePath);
QString fromUriOrPath(const QString &uriOrPath);

bool isAbsoluteLocalPath(const QString &path);

// Expands a leading `~` / `~/...` to `home` (or QDir::homePath() when empty).
QString expandUser(const QString &path, const QString &home = QString());

// String-level parent and last component (no symlink resolution).
QString parentPath(const QString &path);
QString baseName(const QString &path);

// `home`-relative shortening for display, e.g. "~/Videos/clip.mp4".
QString displayPath(const QString &absolutePath, const QString &home);

enum class FilenameErrorCode {
    None,
    Empty,
    InvalidCharacter,
    LeadingDash,
    ReservedName,
    Collision,
};

struct FilenameValidation {
    FilenameErrorCode code = FilenameErrorCode::None;
    QString message;

    bool ok() const noexcept { return code == FilenameErrorCode::None; }
};

// Validates a rename target. Rejects empty/whitespace-only names, `/` and NUL,
// leading `-`, and the reserved names `.` / `..`. When `existingNames` is given,
// a name already present is rejected as a collision (case-insensitive when
// `caseInsensitive` is true, matching case-insensitive filesystems).
FilenameValidation validateFilename(const QString &name,
                                    const QStringList &existingNames = {},
                                    bool caseInsensitive = true);

} // namespace qg
