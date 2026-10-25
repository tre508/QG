#include "controllers/browser_controller.hpp"
#include <QStandardPaths>

#include "filesystem/enumerator.hpp"
#include "persistence/repositories.hpp"
#include "workers/priority_pool.hpp"
#include "workers/task_token.hpp"
#include "workers/tasks.hpp"

#include <QDir>
#include <QFileInfo>

#include <sys/statvfs.h>

namespace qg {

BrowserController::BrowserController(QObject *parent)
    : QObject(parent)
{
    m_model = new FolderModel(this);
    m_watcher = new DirectoryWatcher(this);

    connect(m_watcher, &DirectoryWatcher::changed, this,
            [this](const QString &) { refresh(); });
    connect(m_watcher, &DirectoryWatcher::refreshRequested, this,
            [this](const QString &) { refresh(); });
}

BrowserController::~BrowserController() = default;

QString BrowserController::homePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

QString BrowserController::folderName() const
{
    if (m_currentPath.isEmpty()) {
        return {};
    }
    if (m_currentPath == QLatin1String("/")) {
        return QStringLiteral("/");
    }
    const QString name = QFileInfo(m_currentPath).fileName();
    return name.isEmpty() ? m_currentPath : name;
}

bool BrowserController::navigate(const QString &path)
{
    if (path.isEmpty()) {
        emit errorOccurred(path, QStringLiteral("Not a folder"));
        return false;
    }

    QFileInfo info(path);
    if (!info.exists()) {
        emit errorOccurred(path, QStringLiteral("Not a folder"));
        return false;
    }
    if (!info.isDir()) {
        emit errorOccurred(path, QStringLiteral("Not a folder"));
        return false;
    }
    if (!info.isReadable()) {
        emit errorOccurred(path, QStringLiteral("Permission denied"));
        return false;
    }
    navigateTo(path, true);
    return true;
}

void BrowserController::navigateTo(const QString &path, bool recordHistory)
{
    const QString clean = QDir::cleanPath(path);

    if (clean == m_currentPath) {
        refresh();
        return;
    }

    if (recordHistory) {
        if (!m_currentPath.isEmpty()) {
            m_backStack.append(m_currentPath);
        }
        m_forwardStack.clear();
    }

    m_currentPath = clean;
    emit currentPathChanged(clean);
    emit historyChanged();
    updateFreeSpace();
    loadFolderSettings();

    m_watcher->setPath(clean);
    if (m_recentRecorder) {
        m_recentRecorder(clean);
    }

    startEnumeration(true);
}

void BrowserController::back()
{
    if (m_backStack.isEmpty()) {
        return;
    }
    const QString target = m_backStack.takeLast();
    if (!m_currentPath.isEmpty()) {
        m_forwardStack.prepend(m_currentPath);
    }
    emit historyChanged();
    navigateTo(target, false);
}

void BrowserController::forward()
{
    if (m_forwardStack.isEmpty()) {
        return;
    }
    const QString target = m_forwardStack.takeFirst();
    if (!m_currentPath.isEmpty()) {
        m_backStack.append(m_currentPath);
    }
    emit historyChanged();
    navigateTo(target, false);
}

void BrowserController::parent()
{
    if (m_currentPath.isEmpty()) {
        return;
    }
    // Save the folder just navigated into (the child) so parent() can restore
    // keyboard focus to that entry (NAV-05 / §13.3): focus restoration on parent.
    const QString childName = QFileInfo(m_currentPath).fileName();
    savedChildName_ = childName.isEmpty() ? m_currentPath : childName;
    const QString up = QFileInfo(m_currentPath).absolutePath();
    if (up != m_currentPath) {
        navigate(up);
        emit parentRestored(savedChildName_); // delivered after new folder loads
    }
}

QString BrowserController::savedChildName() const {
    return savedChildName_;
}

void BrowserController::refresh()
{
    if (m_currentPath.isEmpty()) {
        return;
    }
    startEnumeration(false);
}

void BrowserController::setShowHidden(bool showHidden)
{
    if (m_showHidden == showHidden) {
        return;
    }
    m_showHidden = showHidden;
    refresh();
}

void BrowserController::startEnumeration(bool reset)
{
    m_resetPending = reset;
    m_token = std::make_shared<TaskToken>();
    const auto token = m_token;
    const QString path = m_currentPath;
    const bool showHidden = m_showHidden;
    auto result = std::make_shared<FolderSnapshot>();

    setBusy(true);

    submitTask(
        WorkerPools::io(),
        TaskPriority::Interactive,
        token,
        this,
        [path, token, showHidden, result]() {
            EnumeratorOptions options;
            options.showHidden = showHidden;
            FilesystemEnumerator enumerator; // worker-local; results delivered below
            *result = enumerator.enumerate(path, *token, options);
        },
        [this, token, result, reset, path]() {
            if (token != m_token || token->isCancelled()) {
                return; // stale: a newer navigation superseded this one
            }

            if (reset) {
                m_model->applySnapshot(*result);
            } else {
                m_model->applyDiff(computeFolderDiff(m_model->entries(), result->entries));
            }
            m_model->reorder(m_sortSpec);
            setBusy(false);

            if (!result->complete) {
                emit errorOccurred(path, QStringLiteral("Folder enumeration did not complete"));
            }
        });
}

void BrowserController::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged(busy);
}

void BrowserController::updateFreeSpace()
{
    qint64 freeBytes = 0;
    if (!m_currentPath.isEmpty()) {
        struct statvfs info {};
        if (::statvfs(QFile::encodeName(m_currentPath).constData(), &info) == 0) {
            freeBytes = static_cast<qint64>(info.f_bavail) * static_cast<qint64>(info.f_frsize);
        }
    }
    if (freeBytes != m_freeSpaceBytes) {
        m_freeSpaceBytes = freeBytes;
        emit freeSpaceChanged();
    }
}

void BrowserController::loadFolderSettings()
{
    const SortSpec spec =
        m_settings != nullptr ? m_settings->sortSpec(m_currentPath) : SortSpec{};
    const ViewDensity density =
        m_settings != nullptr ? m_settings->density(m_currentPath) : ViewDensity::Standard;

    if (!(spec == m_sortSpec)) {
        m_sortSpec = spec;
        emit sortSpecChanged();
    }
    if (density != m_density) {
        m_density = density;
        emit densityChanged();
    }
}

void BrowserController::setSortSpec(const SortSpec &spec)
{
    if (spec == m_sortSpec) {
        return;
    }
    m_sortSpec = spec;
    if (m_settings != nullptr) {
        m_settings->setSortSpec(m_currentPath, spec);
    }
    emit sortSpecChanged();
    m_model->reorder(m_sortSpec);
}

void BrowserController::resort()
{
    m_model->reorder(m_sortSpec);
}

void BrowserController::setDensity(ViewDensity density)
{
    if (density == m_density) {
        return;
    }
    m_density = density;
    if (m_settings != nullptr) {
        m_settings->setDensity(m_currentPath, density);
    }
    emit densityChanged();
}

void BrowserController::setSort(int key, bool descending, bool foldersFirst)
{
    SortSpec spec;
    const int k = (key < 0 || key > static_cast<int>(SortKey::Favorite))
                      ? static_cast<int>(SortKey::Name)
                      : key;
    spec.key = static_cast<SortKey>(k);
    spec.direction = descending ? SortDirection::Descending : SortDirection::Ascending;
    spec.foldersFirst = foldersFirst;
    setSortSpec(spec);
}

void BrowserController::setDensityValue(int density)
{
    switch (density) {
    case static_cast<int>(ViewDensity::Gallery):
        setDensity(ViewDensity::Gallery);
        break;
    case static_cast<int>(ViewDensity::Compact):
        setDensity(ViewDensity::Compact);
        break;
    default:
        setDensity(ViewDensity::Standard);
        break;
    }
}

QString BrowserController::orderLabel() const
{
    QString keyName;
    switch (m_sortSpec.key) {
    case SortKey::Modified:
        keyName = QStringLiteral("Modified");
        break;
    case SortKey::Duration:
        keyName = QStringLiteral("Duration");
        break;
    case SortKey::Size:
        keyName = QStringLiteral("Size");
        break;
    case SortKey::Watched:
        keyName = QStringLiteral("Watched");
        break;
    case SortKey::Favorite:
        keyName = QStringLiteral("Favorite");
        break;
    case SortKey::Name:
    default:
        keyName = QStringLiteral("Name");
        break;
    }
    const QChar arrow(m_sortSpec.direction == SortDirection::Descending ? QChar(0x2193)
                                                                        : QChar(0x2191));
    return keyName + QLatin1Char(' ') + arrow;
}

} // namespace qg
