#pragma once

// Supported media extensions — the single source of truth (DESIGN.md §5.1,
// §17.3). Data, not logic: adding a format is a one-line change here. Every
// consumer (enumeration, drag-and-drop, reveal validation) calls this registry
// so the list never diverges.

#include <QSet>
#include <QString>
#include <QStringList>

namespace qg {

// Base supported video containers/formats (extension without the dot).
inline const QStringList &supportedVideoExtensions()
{
    static const QStringList kExtensions = {
        QStringLiteral("mp4"),  QStringLiteral("mkv"),  QStringLiteral("webm"),
        QStringLiteral("avi"),  QStringLiteral("mov"),  QStringLiteral("m4v"),
        QStringLiteral("mpeg"), QStringLiteral("mpg"),  QStringLiteral("ts"),
        QStringLiteral("m2ts"), QStringLiteral("mts"),  QStringLiteral("ogv"),
        QStringLiteral("3gp"),  QStringLiteral("flv"),  QStringLiteral("wmv"),
    };
    return kExtensions;
}

// Case-insensitive: does `nameOrPath` end in a supported video extension?
inline bool isSupportedVideoName(const QString &nameOrPath)
{
    const int dot = nameOrPath.lastIndexOf(QLatin1Char('.'));
    if (dot < 0 || dot == nameOrPath.size() - 1) {
        return false;
    }

    static const QSet<QString> kSet(supportedVideoExtensions().begin(),
                                    supportedVideoExtensions().end());
    return kSet.contains(nameOrPath.mid(dot + 1).toLower());
}

} // namespace qg
