#include "logging.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>

#include <cstdio>

namespace qg::logging {
namespace {

QFile g_logFile;
QMutex g_mutex;
QtMessageHandler g_previousHandler = nullptr;
QString g_logPath;
bool g_installed = false;

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "LOG";
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QString category =
        QString::fromLatin1(context.category != nullptr ? context.category : "default");
    const QString line = QStringLiteral("%1 [%2] %3: %4")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QString::fromLatin1(levelName(type)),
                                  category,
                                  message);

    const QByteArray utf8 = line.toUtf8();
    QMutexLocker locker(&g_mutex);
    if (g_logFile.isOpen()) {
        g_logFile.write(utf8);
        g_logFile.write("\n");
        g_logFile.flush();
    }
    std::fprintf(stderr, "%s\n", utf8.constData());
}

} // namespace

bool initialize(const QString &logFilePath)
{
    QMutexLocker locker(&g_mutex);
    if (g_installed) {
        return true;
    }

    QDir().mkpath(QFileInfo(logFilePath).absolutePath());

    g_logFile.setFileName(logFilePath);
    if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // Fall back to stderr-only logging rather than failing startup.
        g_previousHandler = qInstallMessageHandler(&messageHandler);
        g_installed = true;
        return false;
    }

    g_logPath = logFilePath;
    g_previousHandler = qInstallMessageHandler(&messageHandler);
    g_installed = true;
    return true;
}

void shutdown()
{
    QMutexLocker locker(&g_mutex);
    if (!g_installed) {
        return;
    }
    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = nullptr;
    if (g_logFile.isOpen()) {
        g_logFile.close();
    }
    g_logPath.clear();
    g_installed = false;
}

QString logFilePath()
{
    QMutexLocker locker(&g_mutex);
    return g_logPath;
}

} // namespace qg::logging
