#include "playback_context.hpp"
#include "models/folder_model.hpp"

PlaybackContext::PlaybackContext(QObject* parent) : QObject(parent) {}

bool PlaybackContext::matchesFilter(const qg::Entry& entry, Filter filter)
{
    switch (filter) {
    case Favorites:
        return entry.favorite;
    case Unwatched:
        return !entry.watched;
    case All:
    default:
        return true;
    }
}

void PlaybackContext::build(const qg::FolderModel* model,
                            const QString& currentIdentity,
                            Filter filter)
{
    // The list is exactly the model's ordered video rows under the active sort
    // (DESIGN.md §10.2, §20.1): one source shared with the grid.
    entries_.clear();
    currentIndex_ = -1;
    filter_ = filter;

    if (model != nullptr) {
        const QList<qg::Entry> rows = model->entries();
        for (int row = 0; row < rows.size(); ++row) {
            const qg::Entry& entry = rows.at(row);
            if (!entry.isVideo())
                continue;                       // folders are never media
            if (!matchesFilter(entry, filter))
                continue;
            ContextEntry ce;
            ce.identity = entry.identity;
            ce.path = entry.path;
            ce.name = entry.name;
            ce.modelRow = row;
            entries_.append(ce);
            if (!currentIdentity.isEmpty() && entry.identity == currentIdentity)
                currentIndex_ = entries_.size() - 1;
        }
    }

    emit totalCountChanged();
    emit filterChanged();
    emit currentIndexChanged();
}

QVector<ContextEntry> PlaybackContext::entries() const { return entries_; }

int PlaybackContext::currentIndex() const { return currentIndex_; }
int PlaybackContext::totalCount() const { return entries_.size(); }

ContextEntry PlaybackContext::entryAt(int index) const
{
    if (index >= 0 && index < entries_.size())
        return entries_.at(index);
    return {};
}

ContextEntry PlaybackContext::currentEntry() const { return entryAt(currentIndex_); }

int PlaybackContext::indexOfIdentity(const QString& identity) const
{
    if (identity.isEmpty())
        return -1;
    for (int i = 0; i < entries_.size(); ++i) {
        if (entries_.at(i).identity == identity)
            return i;
    }
    return -1;
}

bool PlaybackContext::setCurrentByIdentity(const QString& identity)
{
    const int idx = indexOfIdentity(identity);
    if (idx < 0)
        return false;
    if (currentIndex_ != idx) {
        currentIndex_ = idx;
        emit currentIndexChanged();
    }
    return true;
}

bool PlaybackContext::canStep(int delta) const
{
    if (delta == 0 || entries_.isEmpty())
        return false;
    const int next = currentIndex_ + delta;
    return next >= 0 && next < entries_.size();
}

bool PlaybackContext::step(int delta)
{
    if (delta == 0)
        return false;
    const int next = currentIndex_ + delta;
    if (next < 0 || next >= entries_.size()) {
        emit endReached();                      // stop at ends (DESIGN.md §19.4)
        return false;
    }
    currentIndex_ = next;
    emit currentIndexChanged();
    return true;
}

void PlaybackContext::setFilter(Filter f)
{
    if (filter_ != f) {
        filter_ = f;
        emit filterChanged();
    }
}
