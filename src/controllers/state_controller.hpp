#pragma once

// Favorite / watched / progress state (DESIGN.md §14.3, §21.3, §23.11–§23.12).
//
// State is keyed by media identity: toggles write `media_state` and update the
// canonical model's roles on the GUI thread. Marking watched sets progress to
// 100%; unmarking clears it. Playback position is recorded by identity.

#include <QObject>
#include <QString>

namespace qg {

class Database;
class FolderModel;

class StateController : public QObject {
    Q_OBJECT

public:
    explicit StateController(QObject *parent = nullptr);

    void setDatabase(Database *database) { m_database = database; }
    void setModel(FolderModel *model) { m_model = model; }

    // Apply persisted favorite/watched/progress to the model's current rows.
    void loadIntoModel();

public slots:
    void setFavorite(const QString &identity, bool value);
    void setWatched(const QString &identity, bool value);
    void recordPosition(const QString &identity, qint64 positionMs);

    bool toggleFavorite(const QString &identity);
    bool toggleWatched(const QString &identity);

    // Update the path↔identity mapping after a successful rename (DESIGN §17.5, §21.4).
    Q_INVOKABLE void renamePath(const QString &oldPath, const QString &newPath,
                                const QString &identity);

signals:
    void favoriteChanged(const QString &identity, bool value);
    void watchedChanged(const QString &identity, bool value);

private:
    bool currentFavorite(const QString &identity) const;
    bool currentWatched(const QString &identity) const;
    qint64 durationFor(const QString &identity) const;

    Database *m_database = nullptr;
    FolderModel *m_model = nullptr;
};

} // namespace qg
