#pragma once

// Browser navigation controller (DESIGN.md §14.3, §15.2, §17.1, §23.2).
//
// Owns the navigation stack, the current path, the generation token, the
// directory watcher, and population of the canonical `FolderModel`. Enumeration
// runs on the I/O pool; the aggregate snapshot is applied on the GUI thread
// (reset on navigation, identity diff on refresh) so the previous contents stay
// visible until new data is ready (no empty flash). Thin QObject: no visual
// implementation.

#include "core/types.hpp"
#include "filesystem/watcher.hpp"
#include "models/folder_model.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace qg {

class TaskToken;
class SettingsRepository;

class BrowserController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString folderName READ folderName NOTIFY currentPathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(qint64 freeSpaceBytes READ freeSpaceBytes NOTIFY freeSpaceChanged)
    Q_PROPERTY(int sortKey READ sortKeyValue NOTIFY sortSpecChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending NOTIFY sortSpecChanged)
    Q_PROPERTY(bool foldersFirst READ foldersFirst NOTIFY sortSpecChanged)
    Q_PROPERTY(QString orderLabel READ orderLabel NOTIFY sortSpecChanged)
    Q_PROPERTY(int density READ densityValue NOTIFY densityChanged)

public:
    explicit BrowserController(QObject *parent = nullptr);
    ~BrowserController() override;

    FolderModel *model() const { return m_model; }

    // Exposed to QML as `controller.folderModel`.
    Q_INVOKABLE QObject *folderModel() const { return m_model; }

    // The user's home directory — the empty-folder recovery action (DESIGN §12).
    Q_INVOKABLE QString homePath() const;

    QString currentPath() const { return m_currentPath; }
    // Last path component ("/" for the filesystem root); presentation for strips.
    QString folderName() const;
    bool canGoBack() const { return !m_backStack.isEmpty(); }
    bool canGoForward() const { return !m_forwardStack.isEmpty(); }
    bool busy() const { return m_busy; }
    // Available space of the current volume (0 when unknown).
    qint64 freeSpaceBytes() const noexcept { return m_freeSpaceBytes; }

    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool showHidden);

    // Optional hook so the application can persist recent locations.
    void setRecentRecorder(std::function<void(const QString &)> recorder)
    {
        m_recentRecorder = std::move(recorder);
    }

    // Optional persistence for per-folder view settings (sort spec + density).
    // Null means browse with defaults and no persistence.
    void setSettings(SettingsRepository *settings) { m_settings = settings; }

    // Active per-folder view settings (loaded on entry, written on change).
    SortSpec sortSpec() const { return m_sortSpec; }
    ViewDensity density() const { return m_density; }
    void setSortSpec(const SortSpec &spec);
    void setDensity(ViewDensity density);

    // QML-facing projections (ints/bools; enum order matches QML).
    int sortKeyValue() const { return static_cast<int>(m_sortSpec.key); }
    bool sortDescending() const { return m_sortSpec.direction == SortDirection::Descending; }
    bool foldersFirst() const { return m_sortSpec.foldersFirst; }
    int densityValue() const { return static_cast<int>(m_density); }
    QString orderLabel() const;

    Q_INVOKABLE void setSort(int key, bool descending, bool foldersFirst);
    Q_INVOKABLE void setDensityValue(int density);

    // Focus restoration support (NAV-05 / §13.3): when parent() navigates up,
    // it captures the child folder name so the focus cursor can be restored.
    Q_INVOKABLE QString savedChildName() const;

    // Re-apply the current sort to the model (coalesced resort consumer, P33).
    // Idempotent: re-sorting an already-ordered model is a no-op on order.
    void resort();

public slots:
    // Returns false (and emits errorOccurred) when `path` is not a directory.
    bool navigate(const QString &path);
    void back();
    void forward();
    void parent();
    void refresh();

signals:
    void currentPathChanged(const QString &path);
    void historyChanged();
    void busyChanged(bool busy);
    void errorOccurred(const QString &path, const QString &message);
    void freeSpaceChanged();
    void sortSpecChanged();
    void densityChanged();
    // Focus restoration: parent() navigates up and signals the folder that
    // was just left so the browser can restore focus to it (§13.3 / NAV-05).
    void parentRestored(const QString& childName);

private:
    void navigateTo(const QString &path, bool recordHistory);
    void startEnumeration(bool reset);
    void setBusy(bool busy);
    void updateFreeSpace();
    void loadFolderSettings();

    FolderModel *m_model = nullptr;
    DirectoryWatcher *m_watcher = nullptr;
    std::shared_ptr<TaskToken> m_token;

    QString m_currentPath;
    QStringList m_backStack;
    QStringList m_forwardStack;
    bool m_busy = false;
    bool m_showHidden = false;
    bool m_resetPending = true;
    qint64 m_freeSpaceBytes = 0;
    std::function<void(const QString &)> m_recentRecorder;

    SortSpec m_sortSpec;         // per-folder (default Name asc, folders-first)
    ViewDensity m_density = ViewDensity::Standard;
    SettingsRepository *m_settings = nullptr;

    QString savedChildName_;      // captured before parent() for focus restore (§13.3 / NAV-05)
};

} // namespace qg
