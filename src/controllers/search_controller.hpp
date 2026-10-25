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
public:
    explicit SearchController(QObject* parent = nullptr);
    ~SearchController() override;

    Q_INVOKABLE void setQuery(const QString& query);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString query() const;   // the currently applied (debounced) query
    Q_INVOKABLE QString summary() const; // "6 results for \"sunset\" in Summer trip · Name ↑"
    Q_INVOKABLE QString scope() const;   // "This folder" / "Subfolders"

    // Scope toggle (§20.4): false = "This folder" (default, instant), true =
    // "Subfolders" (recursive walk on the I/O pool).
    Q_INVOKABLE void setScope(bool recursive);
    Q_INVOKABLE bool scopeRecursive() const { return scopeRecursive_; }

    // The directory the recursive walk roots at (updated on navigation/open).
    Q_INVOKABLE void setSearchRoot(const QString& path);

    void setProxy(SearchProxy* proxy);
    void setFolderName(const QString& name);
    void setSortLabel(const QString& label); // e.g. "Name ↑"

    // Recursive result model (null-safe; always non-null after construction).
    Q_INVOKABLE RecursiveSearchModel* recursiveResultsModel() const { return recursiveModel_.get(); }

signals:
    void resultsChanged(const QString& summary);
    void activeChanged(bool active);
    void scopeChanged();

private:
    void flushQuery();
    void startRecursiveWalk();

    QString pendingQuery_;
    QString currentQuery_;
    QString folderName_;
    QString sortLabel_;
    QString searchRoot_;
    bool scopeRecursive_ = false;
    std::uint64_t recursiveGeneration_ = 0;

    SearchProxy* proxy_ = nullptr;
    std::unique_ptr<RecursiveSearchModel> recursiveModel_;
    std::unique_ptr<RecursiveSearchService> recursiveService_;
    QTimer debounceTimer_;

    static constexpr int kDebounceMs = 80;
};