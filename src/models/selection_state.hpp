#pragma once
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include "core/types.hpp"

// Identity-keyed selection and the keyboard focus cursor (DESIGN.md §5.5, §6.2,
// §13.1, §11.2).
//
// Focus, selection and playback are three independent concepts: this class owns
// the first two and never consults a model for the third.
//
// Two rules keep it bindable from QML, which the previous invokable-only design
// violated (AUD-004 / FIX-SEL-01):
//
//  1. Every piece of state QML binds to is a *property* with NOTIFY. A QML
//     binding only re-evaluates when a property it read changes; calling an
//     invokable getter (`selectionState.focusIndex()`) creates no dependency and
//     left the card borders, the cyan focus ring, the status strip and the
//     context-menu title frozen at delegate-creation values.
//  2. Range and select-all take the identity list *from the view*. Only the view
//     knows the display order (including the Manifest proxy), so this class must
//     never guess row indices — it stores what the caller hands it.
class SelectionState : public QObject {
    Q_OBJECT
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QStringList selectedIdentities READ selectedIdentities NOTIFY selectionChanged)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY focusChanged)
    // Monotonic revision, bumped by every selection mutation. A view that must
    // re-evaluate a per-identity predicate (`isSelected()`) reads this property
    // so the binding has something to depend on (AUD-004).
    Q_PROPERTY(int selectionRevision READ selectionRevision NOTIFY selectionChanged)
    Q_PROPERTY(QString summary READ selectionSummary NOTIFY selectionSummaryChanged)

public:
    explicit SelectionState(QObject* parent = nullptr);

    // Identity-based selection (keyed by identity fingerprint from core/identity)
    Q_INVOKABLE bool isSelected(const QString& identityStr) const;
    int selectedCount() const;
    QStringList selectedIdentities() const;
    int selectionRevision() const { return selectionRevision_; }

    // Focus index in current view order (not identity-based)
    int focusIndex() const;
    Q_INVOKABLE void setFocusIndex(int idx);

    // Range anchor for shift-range selection (view order)
    Q_INVOKABLE int anchorIndex() const;
    Q_INVOKABLE void setAnchorIndex(int idx);

    // Operations
    Q_INVOKABLE void selectOne(const QString& identityStr, int viewIndex);
    Q_INVOKABLE void toggle(const QString& identityStr, int viewIndex);
    // Shift-range: the view supplies every identity from the anchor to the new
    // focus (it owns the display order, including the Manifest proxy).
    Q_INVOKABLE void extendRangeTo(const QStringList& identities, int focusIndex);
    // Ctrl+A: same contract — the view supplies the identity list.
    Q_INVOKABLE void selectAll(const QStringList& identities);
    // Clear the selection only; focus is an independent concept (§13.1).
    Q_INVOKABLE void clear();
    // Used when leaving Browse for Manifest: drop the cursor too.
    Q_INVOKABLE void clearFocus();

    // --- Legacy C++-only API ------------------------------------------------
    // Kept source-compatible with the pre-AUD-004 unit test. Deliberately NOT
    // Q_INVOKABLE: QML must use the identity-list contract above, because a class
    // that cannot see the model must never guess a range or "all".
    void selectAll();                                    // selects nothing; notifies
    void extendRange(const QString& identityStr, int viewIndex); // degenerate one-row range

    // Focus movement helpers. They carry no item count — only the view knows the
    // display order — so the upper bound is the view's responsibility
    // (`ContentView.moveFocus()` clamps against `itemCount`). Not used by QML.
    Q_INVOKABLE void moveFocusDown();
    Q_INVOKABLE void moveFocusUp();
    Q_INVOKABLE void moveFocusHome();
    Q_INVOKABLE void moveFocusEnd();
    Q_INVOKABLE void moveFocusPageUp();
    Q_INVOKABLE void moveFocusPageDown();

    // StatusStrip helper (the binding reads the `summary` property above).
    QString selectionSummary() const;

signals:
    void selectionChanged();
    void focusChanged();
    void selectionSummaryChanged();

private:
    QSet<QString> selectedIdentities_;
    int focusIndex_ = -1;
    int anchorIndex_ = -1;
    int selectionRevision_ = 0;
};
