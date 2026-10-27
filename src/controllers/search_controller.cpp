#include "search_controller.hpp"

#include "models/recursive_search_model.hpp"
#include "models/search_proxy.hpp"
#include "search/recursive_search_service.hpp"

SearchController::~SearchController()
{
    // Destructor must live in the .cpp file (after RecursiveSearchService and
    // RecursiveSearchModel headers are included) because unique_ptr members
    // have incomplete types in the header.
}

SearchController::SearchController(QObject* parent) : QObject(parent)
{
    debounceTimer_.setSingleShot(true);
    debounceTimer_.setInterval(kDebounceMs);
    connect(&debounceTimer_, &QTimer::timeout, this, &SearchController::flushQuery);

    recursiveModel_ = std::make_unique<RecursiveSearchModel>(this);
    recursiveService_ = std::make_unique<RecursiveSearchService>(this);

    connect(recursiveService_.get(), &RecursiveSearchService::batchReady, this,
            [this](const QVector<RecursiveSearchResult>& batch, std::uint64_t generation) {
                if (generation != recursiveGeneration_ || !scopeRecursive_) {
                    return; // stale or superseded walk
                }
                recursiveModel_->appendResults(batch);
                emit resultsChanged(summary());
            });
    connect(recursiveService_.get(), &RecursiveSearchService::finished, this,
            [this](std::uint64_t generation, bool cancelled) {
                Q_UNUSED(cancelled);
                if (generation != recursiveGeneration_) {
                    return;
                }
                emit resultsChanged(summary());
            });
}

void SearchController::setQuery(const QString& query)
{
    if (pendingQuery_ == query) {
        return;
    }
    pendingQuery_ = query;
    if (query.isEmpty()) {
        clear();
        return;
    }
    debounceTimer_.start();
}

void SearchController::setScope(bool recursive)
{
    if (scopeRecursive_ == recursive) {
        return;
    }
    scopeRecursive_ = recursive;
    if (recursiveService_) {
        recursiveService_->cancel();
    }
    recursiveGeneration_ = 0;
    if (!scopeRecursive_ && recursiveModel_) {
        recursiveModel_->clear();
    }
    emit scopeChanged();
    // Re-apply the current query under the new scope (§20.4).
    if (!currentQuery_.isEmpty()) {
        flushQuery();
    }
}

void SearchController::setSearchRoot(const QString& path)
{
    if (searchRoot_ == path) {
        return;
    }
    searchRoot_ = path;
    // Navigation invalidates an in-flight walk: stale batches are dropped.
    if (scopeRecursive_ && !currentQuery_.isEmpty()) {
        startRecursiveWalk();
    }
}

void SearchController::clear()
{
    pendingQuery_.clear();
    currentQuery_.clear();
    debounceTimer_.stop();
    if (recursiveService_) {
        recursiveService_->cancel();
    }
    recursiveGeneration_ = 0;
    if (recursiveModel_) {
        recursiveModel_->clear();
    }
    if (proxy_) {
        proxy_->setQuery(QString());
    }
    // Notify `active` first (the shell's handler restores Browse), then the
    // summary/count: after a clear the summary is empty, and a binding on
    // `summary`/`resultCount` must be told so. Previously `clear()` emitted
    // `activeChanged(false)` only, so a bound summary kept the stale text after
    // Esc. `setActive` is a no-op when the value did not change, so a repeated
    // clear does not re-notify the property.
    setActive(false);
    emit resultsChanged(summary());
}

void SearchController::flushQuery()
{
    currentQuery_ = pendingQuery_;
    if (scopeRecursive_) {
        startRecursiveWalk();
    } else if (proxy_) {
        proxy_->setQuery(currentQuery_);
    }
    // `active` is published before `resultsChanged` so the shell's handler can
    // install the folder/sort context that `summary()` embeds.
    setActive(!currentQuery_.isEmpty());
    emit resultsChanged(summary());
}

void SearchController::setActive(bool active)
{
    if (active_ == active) {
        return;
    }
    active_ = active;          // member first: the notification never precedes it
    emit activeChanged(active_);
}

bool SearchController::active() const
{
    return active_;
}

void SearchController::startRecursiveWalk()
{
    if (currentQuery_.isEmpty() || !recursiveService_ || !recursiveModel_) {
        return;
    }
    recursiveModel_->clear();
    recursiveModel_->setQuery(currentQuery_);
    recursiveGeneration_ = recursiveService_->start(currentQuery_, searchRoot_);
}

int SearchController::resultCount() const
{
    if (currentQuery_.isEmpty()) {
        return 0;
    }
    return scopeRecursive_
        ? (recursiveModel_ ? recursiveModel_->rowCount() : 0)
        : (proxy_ ? proxy_->resultCount() : 0);
}

QString SearchController::summary() const
{
    if (currentQuery_.isEmpty()) {
        return {};
    }
    const int n = resultCount();
    const QString scopeStr = folderName_.isEmpty()
        ? (scopeRecursive_ ? QStringLiteral("in subfolders") : QStringLiteral("in this folder"))
        : (scopeRecursive_
               ? QStringLiteral("in %1 and subfolders").arg(folderName_)
               : QStringLiteral("in %1").arg(folderName_));
    const QString sortStr = sortLabel_.isEmpty()
        ? QString()
        : QStringLiteral(" · %1").arg(sortLabel_);
    return QStringLiteral("%1 results for \"%2\" %3%4")
        .arg(n).arg(currentQuery_, scopeStr, sortStr);
}

QString SearchController::scope() const
{
    return scopeRecursive_ ? QStringLiteral("Subfolders") : QStringLiteral("This folder");
}

QString SearchController::query() const
{
    return currentQuery_;
}

void SearchController::setProxy(SearchProxy* proxy)
{
    proxy_ = proxy;
}

void SearchController::setFolderName(const QString& name)
{
    folderName_ = name;
}

void SearchController::setSortLabel(const QString& label)
{
    sortLabel_ = label;
}