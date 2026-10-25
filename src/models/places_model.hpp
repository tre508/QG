#pragma once

// Sidebar data model (DESIGN.md §3.3, §11.2, §14.2). Groups in order:
// Pinned / Places / Devices / Recent.
//
// The model owns *presentation-ready* semantics: every row carries the icon
// name to draw, the human-readable label, the navigation path, a tooltip, the
// section it belongs to, and the device capabilities that decide which
// affordances QML may show. Delegates therefore contain no filesystem
// heuristics (DESIGN.md §14.1: "presentation only — no business logic").

#include "filesystem/mounts.hpp"

#include <QAbstractListModel>
#include <QStringList>

namespace qg {

class PlacesModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        LabelRole = Qt::UserRole + 1,
        PathRole,
        IconRole,
        GroupRole,
        ConnectedRole,
        RemovableRole,
        EjectableRole,
        VolumeIdRole,
        // Semantic roles added for FIX-UI-003: enough information for QML to
        // render the correct icon, label and action without re-deriving any
        // classification.
        KindRole,       // "pinned" | "place" | "recent" | <VolumeKind key>
        TooltipRole,    // human-readable secondary line (path / device status)
        MountedRole,    // false for a retained (disconnected) device
        CanUnmountRole, // authoritative capability (§17.4)
        CanEjectRole,   // authoritative capability (§17.4)
        HeaderRole,     // true for a group heading row
    };
    Q_ENUM(Roles)

    enum Group {
        Pinned = 0,
        Places = 1,
        Devices = 2,
        Recent = 3,
    };
    Q_ENUM(Group)

    explicit PlacesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setPinned(const QStringList &paths);
    void setRecent(const QStringList &paths);
    void setVolumes(const QList<VolumeInfo> &volumes);

    // Pin intent: the model does not own bookmark persistence (that is the
    // SQLite BookmarkRepository, §21). It forwards the request so the
    // application can persist it and feed the new list back through
    // setPinned(), keeping exactly one source of truth.
    Q_INVOKABLE void requestPin(const QString &path);
    Q_INVOKABLE void requestUnpin(const QString &path);

    // Test/introspection helpers.
    int countInGroup(Group group) const;

    // True when `row` is a group heading rather than a location. QML uses this
    // to skip headings during keyboard navigation. Group headings are model
    // rows (not ListView section delegates) so that the list has a single
    // uniform delegate height and its content height is exact rather than
    // estimated — see DECISIONS.md (FIX-UI-003).
    Q_INVOKABLE bool isHeaderAt(int row) const;

signals:
    void pinRequested(const QString &path);
    void unpinRequested(const QString &path);

private:
    struct Row {
        QString label;
        QString path;
        QString icon;
        QString volumeId;
        QString kind;
        QString tooltip;
        Group group = Places;
        bool header = false;
        bool connected = true;
        bool removable = false;
        bool ejectable = false;
        bool mounted = true;
        bool canUnmount = false;
        bool canEject = false;
    };

    void rebuild();

    QStringList m_pinned;
    QStringList m_recent;
    QList<VolumeInfo> m_volumes;
    QList<Row> m_rows;
};

} // namespace qg
