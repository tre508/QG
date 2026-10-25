#pragma once
#include <QObject>
#include <QString>
#include <QSet>
#include <QVector>
#include <QPair>

#include "core/types.hpp"

class SelectionState : public QObject {
    Q_OBJECT
public:
    explicit SelectionState(QObject* parent = nullptr);

    // Identity-based selection (keyed by identity fingerprint from core/identity)
    Q_INVOKABLE bool isSelected(const QString& identityStr) const;
    Q_INVOKABLE int selectedCount() const;
    Q_INVOKABLE QStringList selectedIdentities() const;

    // Focus index in current view order (not identity-based)
    Q_INVOKABLE int focusIndex() const;
    Q_INVOKABLE void setFocusIndex(int idx);

    // Range anchor for shift-range selection
    Q_INVOKABLE int anchorIndex() const;
    Q_INVOKABLE void setAnchorIndex(int idx);

    // Operations
    Q_INVOKABLE void selectOne(const QString& identityStr, int viewIndex);
    Q_INVOKABLE void toggle(const QString& identityStr, int viewIndex);
    Q_INVOKABLE void extendRange(const QString& identityStr, int viewIndex); // shift-range from anchor
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clear();

    // Focus movement helpers (row-major with wrap)
    Q_INVOKABLE void moveFocusDown();
    Q_INVOKABLE void moveFocusUp();
    Q_INVOKABLE void moveFocusHome();
    Q_INVOKABLE void moveFocusEnd();
    Q_INVOKABLE void moveFocusPageUp();
    Q_INVOKABLE void moveFocusPageDown();

    // StatusStrip helpers
    Q_INVOKABLE QString selectionSummary() const;

signals:
    void selectionChanged();
    void focusChanged();
    void selectionSummaryChanged();

private:
    QSet<QString> selectedIdentities_;
    int focusIndex_ = -1;
    int anchorIndex_ = -1;
};
