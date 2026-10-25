#pragma once
#include <QAbstractListModel>
#include <QVector>
#include <QString>

// ReelModel: the videos-only projection of the containing folder, in the active
// order, with a current-item highlight (DESIGN.md §10, §19.4). It mirrors the
// PlaybackContext list exactly — one ordered source shared with prev/next.
//
// Values are raw (duration ms / bytes); presentation formats them via the QML
// `Format` singleton so formatting has a single home.
//
// Updates are identity-keyed and minimal: `updateEntry` emits dataChanged for
// one row (rename, favorite/watched/progress); `removeByIdentity` removes one
// row and renumbers the tail; a change of set or order re-feeds `setEntries`.

struct ReelEntry {
    int index = 0;         // 1-based display position
    QString identity;
    QString name;
    QString path;
    qint64 durationMs = -1; // -1 = unknown
    qint64 sizeBytes = 0;
    bool favorite = false;
    bool watched = false;
    double progress = -1;  // 0..1 partial-watch fraction, -1 = none
};

class ReelModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IndexRole = Qt::UserRole + 1,
        IdentityRole,
        NameRole,
        PathRole,
        DurationMsRole,
        SizeRole,
        FavoriteRole,
        WatchedRole,
        ProgressRole,
        IsCurrentRole,
    };
    Q_ENUM(Roles)

    explicit ReelModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replace the whole projection (change of folder / sort / filter).
    Q_INVOKABLE void setEntries(const QVector<ReelEntry>& entries);

    Q_INVOKABLE void setCurrentIndex(int idx);
    // Anchor the highlight on `identity`; returns false when it is absent.
    Q_INVOKABLE bool setCurrentByIdentity(const QString& identity);
    Q_INVOKABLE int currentIndex() const;
    Q_INVOKABLE int count() const { return entries_.size(); }
    Q_INVOKABLE QString identityAt(int row) const;
    Q_INVOKABLE int indexOfIdentity(const QString& identity) const;

    // Minimal single-row update (identity-keyed); no-op when unknown.
    Q_INVOKABLE bool updateEntry(const ReelEntry& entry);
    // Minimal single-row removal + renumber of the tail.
    Q_INVOKABLE bool removeByIdentity(const QString& identity);

    Q_INVOKABLE QString summary() const; // "7 / 42"

signals:
    void currentIndexChanged();
    void countChanged();

private:
    void renumberFrom(int startRow);

    QVector<ReelEntry> entries_;
    int currentIdx_ = -1;
};
