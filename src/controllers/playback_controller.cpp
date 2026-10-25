#include "playback_controller.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_context.hpp"
#include "playback/playback_session.hpp"
#include "models/folder_model.hpp"

#include <QVector>

namespace {
PlaybackContext::Filter filterFromInt(int value)
{
    switch (value) {
    case 1: return PlaybackContext::Favorites;
    case 2: return PlaybackContext::Unwatched;
    default: return PlaybackContext::All;
    }
}
} // namespace

PlaybackController::PlaybackController(QObject* parent) : QObject(parent) {}

bool PlaybackController::playing() const {
    return currentSession_ && currentSession_->state() == PlaybackSession::Playing;
}

bool PlaybackController::cinemaActive() const { return cinemaActive_; }
PlaybackSession* PlaybackController::currentSession() const { return currentSession_; }

void PlaybackController::setPlaybackManager(PlaybackManager* mgr) { manager_ = mgr; }
void PlaybackController::setPlaybackContext(PlaybackContext* ctx) { context_ = ctx; }

void PlaybackController::setFolderModel(qg::FolderModel* model)
{
    if (folderModel_ == model)
        return;
    if (folderModel_)
        folderModel_->disconnect(this);
    folderModel_ = model;
    if (folderModel_) {
        // Any structural change to the canonical model (navigation reset, sort,
        // external add/remove) invalidates the ordered context and the Reel
        // (DESIGN.md §17.5, §20.1). Data changes update rows minimally.
        connect(folderModel_, &QAbstractItemModel::modelReset,
                this, &PlaybackController::onModelStructureChanged);
        connect(folderModel_, &QAbstractItemModel::layoutChanged,
                this, &PlaybackController::onModelStructureChanged);
        connect(folderModel_, &QAbstractItemModel::rowsRemoved,
                this, &PlaybackController::onModelStructureChanged);
        connect(folderModel_, &QAbstractItemModel::rowsInserted,
                this, &PlaybackController::onModelStructureChanged);
        connect(folderModel_, &QAbstractItemModel::dataChanged,
                this, &PlaybackController::onModelDataChanged);
    }
}

void PlaybackController::setReelModel(ReelModel* reel) { reelModel_ = reel; }

void PlaybackController::refreshContext()
{
    if (!context_ || !folderModel_)
        return;
    const QString identity = currentIdentity();
    context_->build(folderModel_, identity, filterFromInt(reelFilter_));
    refreshReel();
    emit contextChanged();
}

// Build the Reel projection from the ordered context, reading the canonical
// model for duration/size/state.
void PlaybackController::refreshReel()
{
    if (!reelModel_ || !context_ || !folderModel_)
        return;

    const QVector<ContextEntry> entries = context_->entries();
    QVector<ReelEntry> rows;
    rows.reserve(entries.size());
    for (int i = 0; i < entries.size(); ++i)
        rows.append(reelEntryFor(i));

    reelModel_->setEntries(rows);
    reelModel_->setCurrentIndex(context_->currentIndex());
}

ReelEntry PlaybackController::reelEntryFor(int contextIndex) const
{
    ReelEntry row;
    if (!context_ || !folderModel_)
        return row;

    const ContextEntry ce = context_->entryAt(contextIndex);
    if (ce.identity.isEmpty())
        return row;

    row.index = contextIndex + 1;
    row.identity = ce.identity;
    row.name = ce.name;
    row.path = ce.path;

    // Prefer the row captured at build time; fall back to an identity lookup when
    // the model has shifted under us.
    const qg::Entry* entry = folderModel_->entryAt(ce.modelRow);
    if (entry == nullptr || entry->identity != ce.identity)
        entry = folderModel_->entryAt(folderModel_->indexOfIdentity(ce.identity));
    if (entry != nullptr && entry->identity == ce.identity) {
        row.name = entry->name;
        row.path = entry->path;
        row.durationMs = entry->durationMs.has_value() ? *entry->durationMs : -1;
        row.sizeBytes = entry->size;
        row.favorite = entry->favorite;
        row.watched = entry->watched;
        row.progress = entry->progress.has_value() ? *entry->progress : -1.0;
    }
    return row;
}

void PlaybackController::play(const QString& path, const QString& identity)
{
    if (!manager_)
        return;
    if (context_ && folderModel_)
        context_->build(folderModel_, identity, filterFromInt(reelFilter_));

    // Resume: stored position + policy + eligibility, decided with the model's
    // probed duration and watched state (DESIGN.md §19.6).
    double start = 0;
    if (!identity.isEmpty()) {
        start = manager_->resumeStartFor(identity,
                                         modelDurationSeconds(identity),
                                         modelWatched(identity));
    }
    manager_->openWithIdentity(path, identity, start);
    currentSession_ = manager_->session();
    refreshReel();
    emit playingChanged();
    emit currentSessionChanged();
    emit contextChanged();
    emit playbackStarted(path);
}

void PlaybackController::playCurrent()
{
    if (!context_ || !manager_)
        return;
    const ContextEntry entry = context_->currentEntry();
    if (entry.path.isEmpty())
        return;
    // Keep the session identity in step with the context so the Reel highlight
    // and resume lookups agree (identity is not the path — DESIGN.md §17.5).
    // Switching media also honours resume (`start=` on the same handle, §19.4).
    const double start = manager_->resumeStartFor(entry.identity,
                                                  modelDurationSeconds(entry.identity),
                                                  modelWatched(entry.identity));
    manager_->openWithIdentity(entry.path, entry.identity, start);
    currentSession_ = manager_->session();
    refreshReel();
    emit playingChanged();
    emit currentSessionChanged();
    emit contextChanged();
    emit playbackStarted(entry.path);
}

void PlaybackController::previous()
{
    if (context_ && context_->step(-1))
        playCurrent();
    else
        emit endOfFolder();          // stop at the ends (DESIGN.md §23.8)
}

void PlaybackController::next()
{
    if (context_ && context_->step(1))
        playCurrent();
    else
        emit endOfFolder();
}

void PlaybackController::stop()
{
    if (manager_) {
        manager_->stop();
        // Closing the stage always leaves Cinema (DESIGN.md §9.3): the layout
        // returns to Browse.
        if (cinemaActive_) {
            cinemaActive_ = false;
            emit cinemaActiveChanged();
        }
        emit playingChanged();
        emit playbackEnded();
    }
}

void PlaybackController::enterCinema()
{
    // Cinema requires an active (non-idle) session (D-22): entering with no
    // media would collapse all browser chrome around an empty stage.
    if (!hasActiveSession())
        return;
    if (!cinemaActive_) {
        cinemaActive_ = true;
        emit cinemaActiveChanged();
    }
}

void PlaybackController::exitCinema()
{
    if (cinemaActive_) {
        cinemaActive_ = false;
        emit cinemaActiveChanged();
    }
}

void PlaybackController::toggleCinema()
{
    if (cinemaActive_)
        exitCinema();
    else
        enterCinema();
}

bool PlaybackController::hasActiveSession() const {
    return currentSession_ != nullptr && currentSession_->state() != PlaybackSession::Idle;
}

QString PlaybackController::currentIdentity() const
{
    if (currentSession_ != nullptr && !currentSession_->identity().isEmpty())
        return currentSession_->identity();
    return QString();
}

int PlaybackController::contextCount() const
{
    return context_ != nullptr ? context_->totalCount() : 0;
}

int PlaybackController::contextIndex() const
{
    return context_ != nullptr ? context_->currentIndex() : -1;
}

QString PlaybackController::positionLabel() const
{
    if (context_ == nullptr)
        return QString();
    const int total = context_->totalCount();
    const int index = context_->currentIndex();
    if (total == 0 || index < 0)
        return QStringLiteral("0 / %1").arg(total);
    return QStringLiteral("%1 / %2").arg(index + 1).arg(total);
}

bool PlaybackController::canGoPrevious() const
{
    return context_ != nullptr && context_->canStep(-1);
}

bool PlaybackController::canGoNext() const
{
    return context_ != nullptr && context_->canStep(1);
}

void PlaybackController::setReelFilter(int filter)
{
    const int clamped = (filter < 0 || filter > 2) ? 0 : filter;
    if (reelFilter_ == clamped)
        return;
    reelFilter_ = clamped;
    // Rebuild so the Reel AND prev/next traverse the filtered list (§10.2).
    if (context_ && folderModel_) {
        context_->build(folderModel_, currentIdentity(), filterFromInt(reelFilter_));
        refreshReel();
    }
    emit reelFilterChanged();
    emit contextChanged();
}

void PlaybackController::selectReelRow(int row)
{
    if (!context_)
        return;
    const ContextEntry entry = context_->entryAt(row);
    if (entry.path.isEmpty())
        return;
    context_->setCurrentByIdentity(entry.identity);
    playCurrent();
}

void PlaybackController::onModelStructureChanged()
{
    if (!context_ || !folderModel_)
        return;

    const QString identity = currentIdentity();
    // Did the playing file leave the model entirely? Then playback must stop:
    // its path no longer exists (rename/removal/navigation — §17.5).
    const bool playingGone = !identity.isEmpty()
                             && folderModel_->indexOfIdentity(identity) < 0;

    context_->build(folderModel_, playingGone ? QString() : identity,
                    filterFromInt(reelFilter_));
    refreshReel();
    emit contextChanged();

    if (playingGone && manager_ && currentSession_ != nullptr
            && currentSession_->state() != PlaybackSession::Idle) {
        stop();
    }
}

void PlaybackController::onModelDataChanged(const QModelIndex& topLeft,
                                            const QModelIndex& bottomRight,
                                            const QList<int>& roles)
{
    Q_UNUSED(roles);
    if (!context_ || !folderModel_ || !reelModel_)
        return;

    // Minimal propagation: refresh only the reel rows whose canonical entry
    // changed (rename, favorite/watched/progress), then re-read the context names.
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        const qg::Entry* entry = folderModel_->entryAt(row);
        if (entry == nullptr || !entry->isVideo())
            continue;
        const int ctxRow = context_->indexOfIdentity(entry->identity);
        if (ctxRow >= 0)
            reelModel_->updateEntry(reelEntryFor(ctxRow));
    }
}

void PlaybackController::applyResumeDecision(const QString& identity, bool resume,
                                             double positionSeconds)
{
    if (!manager_)
        return;
    if (resume)
        manager_->applyResume(identity, positionSeconds);
    // Declining resumes simply leaves the session at its start — the auto-resume
    // was already suppressed by resumeStartFor()'s Ask branch (§19.6).
}

// Probed duration in seconds, or <= 0 when unknown (never resume on a guess).
double PlaybackController::modelDurationSeconds(const QString& identity) const
{
    if (folderModel_ == nullptr || identity.isEmpty())
        return -1;
    const qg::Entry* entry = folderModel_->entryAt(folderModel_->indexOfIdentity(identity));
    if (entry == nullptr || !entry->durationMs.has_value())
        return -1;
    return static_cast<double>(*entry->durationMs) / 1000.0;
}

bool PlaybackController::modelWatched(const QString& identity) const
{
    if (folderModel_ == nullptr || identity.isEmpty())
        return false;
    const qg::Entry* entry = folderModel_->entryAt(folderModel_->indexOfIdentity(identity));
    return entry != nullptr && entry->watched;
}
