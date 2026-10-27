#include "controllers/state_controller.hpp"

#include <QTimer>

#include "models/folder_model.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"

#include <algorithm>

namespace qg {

StateController::StateController(QObject *parent)
    : QObject(parent)
{
}

void StateController::setModel(FolderModel *model)
{
    if (m_model == model) {
        return;
    }
    if (m_model != nullptr) {
        m_model->disconnect(this);
    }
    m_model = model;
    m_applied.clear();
    if (m_model == nullptr) {
        return;
    }
    // A new folder replaces the rows wholesale (applySnapshot → modelReset);
    // forget what was decorated and re-apply for the new set.
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        m_applied.clear();
        scheduleLoad();
    });
    connect(m_model, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &, int, int) { scheduleLoad(); });
    scheduleLoad();
}

void StateController::scheduleLoad()
{
    if (m_model == nullptr || m_database == nullptr || m_loadScheduled) {
        return;
    }
    m_loadScheduled = true;
    // Zero-delay coalescing: a 200-entry enumeration batch, a sort and a refresh
    // all collapse into one pass on the next event-loop turn.
    QTimer::singleShot(0, this, [this]() {
        m_loadScheduled = false;
        loadIntoModel();
    });
}

void StateController::setFavorite(const QString &identity, bool value)
{
    if (identity.isEmpty()) {
        return;
    }
    if (m_database != nullptr) {
        MediaStateRepository repository(*m_database);
        repository.setFavorite(identity, value);
    }
    if (m_model != nullptr) {
        m_model->updateFavorite(identity, value);
    }
    emit favoriteChanged(identity, value);
}

void StateController::setWatched(const QString &identity, bool value)
{
    if (identity.isEmpty()) {
        return;
    }
    if (m_database != nullptr) {
        MediaStateRepository repository(*m_database);
        repository.setWatched(identity, value);
    }
    if (m_model != nullptr) {
        m_model->updateWatched(identity, value);
    }
    emit watchedChanged(identity, value);
}

void StateController::recordPosition(const QString &identity, qint64 positionMs)
{
    if (identity.isEmpty()) {
        return;
    }
    if (m_database != nullptr) {
        MediaStateRepository repository(*m_database);
        repository.recordPosition(identity, positionMs);
    }

    const qint64 duration = durationFor(identity);
    if (m_model != nullptr && duration > 0) {
        const double progress = std::clamp(static_cast<double>(positionMs) / duration, 0.0, 1.0);
        m_model->updateProgress(identity, progress);
    }
}

bool StateController::toggleFavorite(const QString &identity)
{
    const bool next = !currentFavorite(identity);
    setFavorite(identity, next);
    return next;
}

bool StateController::toggleWatched(const QString &identity)
{
    const bool next = !currentWatched(identity);
    setWatched(identity, next);
    return next;
}

void StateController::loadIntoModel()
{
    if (m_model == nullptr || m_database == nullptr) {
        return;
    }

    MediaStateRepository repository(*m_database);
    const QList<Entry> entries = m_model->entries();
    for (const Entry &entry : entries) {
        if (entry.identity.isEmpty() || m_applied.contains(entry.identity)) {
            continue;
        }
        m_applied.insert(entry.identity);
        m_model->updateFavorite(entry.identity, repository.favorite(entry.identity));

        const bool watched = repository.watched(entry.identity);
        m_model->updateWatched(entry.identity, watched);

        if (!watched && entry.durationMs.has_value() && *entry.durationMs > 0) {
            const qint64 position = repository.positionMs(entry.identity);
            if (position > 0) {
                const double progress = std::clamp(
                    static_cast<double>(position) / static_cast<double>(*entry.durationMs),
                    0.0,
                    1.0);
                m_model->updateProgress(entry.identity, progress);
            }
        }
    }
}

bool StateController::currentFavorite(const QString &identity) const
{
    if (m_model != nullptr) {
        const int row = m_model->indexOfIdentity(identity);
        if (row >= 0) {
            return m_model->data(m_model->index(row), FolderModel::FavoriteRole).toBool();
        }
    }
    if (m_database != nullptr) {
        MediaStateRepository repository(*m_database);
        return repository.favorite(identity);
    }
    return false;
}

bool StateController::currentWatched(const QString &identity) const
{
    if (m_database != nullptr) {
        MediaStateRepository repository(*m_database);
        return repository.watched(identity);
    }
    return false;
}

qint64 StateController::durationFor(const QString &identity) const
{
    if (m_model != nullptr) {
        const int row = m_model->indexOfIdentity(identity);
        if (row >= 0) {
            const Entry *entry = m_model->entryAt(row);
            if (entry != nullptr && entry->durationMs.has_value()) {
                return *entry->durationMs;
            }
        }
    }
    return -1;
}

void StateController::renamePath(const QString &oldPath,
                                 const QString &newPath,
                                 const QString &identity)
{
    if (m_database == nullptr || identity.isEmpty()) {
        return;
    }
    PathRepository repository(*m_database);
    repository.renamePath(oldPath, newPath, identity);
}

} // namespace qg
