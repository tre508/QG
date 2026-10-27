#pragma once
#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <memory>

class SearchProxy;
class RecursiveSearchService;
class RecursiveSearchModel;
namespace qg { class TaskToken; }

// SearchController: owns query lifecycle — debounce, scope, summary, clear.
// Thin QObject: translates UI intent → SearchProxy commands, emits status.
// Recursive ("Subfolders") scope drives a RecursiveSearchService walker on the
// I/O pool and exposes its result model to the Manifest.
class SearchController : public QObject {
    Q_OBJECT
    // --- Observable state (AUD-005 / FIX-SRCH-01) ---------------------------
    // A QML binding re-evaluates only when a *property* it read changes. The
    // shell read `searchController.active` as if it were a property, but the
    // class declared none, so the read was `undefined`: `manifestActive` stayed
    // permanently false and the surface never entered the Manifest intensity
    // (DESIGN.md §7.2, §25.4). These three are the state QML *binds*; the names
    // QML only *calls* (`query`, `scope`, `scopeRecursive`) stay invokable so no
    // existing call site changes shape.
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY resultsChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultsChanged)

public:
    explicit SearchController(QObject* parent = nullptr);
    ~SearchController() override;

    Q_INVOKABLE void setQuery(const QString& query);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString query() const; // the currently applied (debounced) query
    QString summary() const;           // "6 results for \"sunset\" in Summer trip · Name ↑"
    Q_INVOKABLE QString scope() const; // "This folder" / "Subfolders"

    // Rows the applied query matches in the current scope. `summary()` embeds
    // this same value, so the count the strip announces and the count a binding
    // reads cannot drift apart. In the default "This folder" scope it equals
    // `SearchProxy::resultCount` (which stays the live count for the field); in
    // "Subfolders" scope only this value reflects the walk.
    int resultCount() const;

    // True from the moment the debounced query applied until `clear()` — the
    // state DESIGN.md §7.2 switches the whole surface on. Owned here, not by the
    // view.
    bool active() const;

    // Scope toggle (§20.4): false = "This folder" (default, instant), true =
    // "Subfolders" (recursive walk on the I/O pool).
    Q_INVOKABLE void setScope(bool recursive);
    Q_INVOKABLE bool scopeRecursive() const { return scopeRecursive_; }

    // The directory the recursive walk roots at (updated on navigation/open).
    Q_INVOKABLE void setSearchRoot(const QString& path);

    void setProxy(SearchProxy* proxy);
    // QML-facing: the shell supplies the folder/sort context the summary embeds
    // (DESIGN.md §7.2). As plain members the QML call threw a TypeError inside
    // `onActiveChanged` and aborted the handler *before* `enterManifest()` — the
    // second, independent reason the surface never switched.
    Q_INVOKABLE void setFolderName(const QString& name);
    Q_INVOKABLE void setSortLabel(const QString& label); // e.g. "Name ↑"

    // Recursive result model (null-safe; always non-null after construction).
    Q_INVOKABLE RecursiveSearchModel* recursiveResultsModel() const { return recursiveModel_.get(); }

signals:
    void resultsChanged(const QString& summary);
    void activeChanged(bool active);
    void scopeChanged();

private:
    void flushQuery();
    void startRecursiveWalk();

    // Single owner of the `active` transition: the member is updated *before*
    // the notification, and notification happens only on a real change — so a
    // `clear()` while already inactive does not re-emit, and the value a NOTIFY
    // handler reads can never be the previous one.
    void setActive(bool active);

    QString pendingQuery_;
    QString currentQuery_;
    QString folderName_;
    QString sortLabel_;
    QString searchRoot_;
    bool active_ = false;
    bool scopeRecursive_ = false;
    std::uint64_t recursiveGeneration_ = 0;

    SearchProxy* proxy_ = nullptr;
    std::unique_ptr<RecursiveSearchModel> recursiveModel_;
    std::unique_ptr<RecursiveSearchService> recursiveService_;
    QTimer debounceTimer_;

    static constexpr int kDebounceMs = 80;
};