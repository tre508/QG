#include "selection_state.hpp"

#include <algorithm>

// AUD-004 / FIX-SEL-01: every mutator bumps `selectionRevision_` and emits
// `selectionChanged()` so QML bindings that read either the count or the
// revision re-evaluate. `focusChanged()` is emitted only when the cursor
// actually moves: focus and selection are independent (§13.1).

SelectionState::SelectionState(QObject* parent) : QObject(parent) {}

bool SelectionState::isSelected(const QString& identityStr) const {
    return selectedIdentities_.contains(identityStr);
}

int SelectionState::selectedCount() const {
    return static_cast<int>(selectedIdentities_.size());
}

QStringList SelectionState::selectedIdentities() const {
    // Sorted for determinism: QSet iteration order is unspecified, and the
    // status strip / bulk verbs walk this list.
    QStringList list(selectedIdentities_.cbegin(), selectedIdentities_.cend());
    list.sort();
    return list;
}

int SelectionState::focusIndex() const { return focusIndex_; }

void SelectionState::setFocusIndex(int idx) {
    if (focusIndex_ != idx) {
        focusIndex_ = idx;
        emit focusChanged();
    }
}

int SelectionState::anchorIndex() const { return anchorIndex_; }

void SelectionState::setAnchorIndex(int idx) {
    anchorIndex_ = idx;
}

void SelectionState::selectOne(const QString& identityStr, int viewIndex) {
    selectedIdentities_.clear();
    if (!identityStr.isEmpty()) {
        selectedIdentities_.insert(identityStr);
    }
    focusIndex_ = viewIndex;
    anchorIndex_ = viewIndex;
    ++selectionRevision_;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}

void SelectionState::toggle(const QString& identityStr, int viewIndex) {
    if (identityStr.isEmpty()) {
        return;
    }
    if (selectedIdentities_.contains(identityStr)) {
        selectedIdentities_.remove(identityStr);
    } else {
        selectedIdentities_.insert(identityStr);
    }
    // Ctrl+click moves the cursor to the clicked row and becomes the new range
    // anchor (native file-manager behaviour: Shift+click after Ctrl+click
    // ranges from the last clicked row).
    focusIndex_ = viewIndex;
    anchorIndex_ = viewIndex;
    ++selectionRevision_;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}

void SelectionState::extendRangeTo(const QStringList& identities, int focusIndex) {
    if (identities.isEmpty()) {
        return;
    }
    selectedIdentities_.clear();
    for (const QString& identity : identities) {
        if (!identity.isEmpty()) {
            selectedIdentities_.insert(identity);
        }
    }
    focusIndex_ = focusIndex;
    ++selectionRevision_;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}

void SelectionState::selectAll(const QStringList& identities) {
    selectedIdentities_.clear();
    for (const QString& identity : identities) {
        if (!identity.isEmpty()) {
            selectedIdentities_.insert(identity);
        }
    }
    // Select-all never moves an existing cursor; with none, the first row
    // becomes focused so the keyboard has somewhere to act (§13.2).
    if (focusIndex_ < 0 && !selectedIdentities_.isEmpty()) {
        focusIndex_ = 0;
        anchorIndex_ = 0;
        emit focusChanged();
    }
    ++selectionRevision_;
    emit selectionChanged();
    emit selectionSummaryChanged();
}

void SelectionState::clear() {
    // Focus is independent of selection (§13.1): clearing the selection must not
    // destroy the keyboard cursor, otherwise arrow navigation dies with it.
    // The cursor becomes the new range anchor.
    selectedIdentities_.clear();
    anchorIndex_ = focusIndex_;
    ++selectionRevision_;
    emit selectionChanged();
    emit selectionSummaryChanged();
}

void SelectionState::clearFocus() {
    focusIndex_ = -1;
    anchorIndex_ = -1;
    emit focusChanged();
}

// --- Legacy C++-only API (see the header) -----------------------------------

void SelectionState::selectAll() {
    // The old stub selected nothing; it is kept only so the pre-AUD-004 unit test
    // compiles. QML reaches the real implementation, selectAll(identities).
    ++selectionRevision_;
    emit selectionChanged();
    emit selectionSummaryChanged();
}

void SelectionState::extendRange(const QString& identityStr, int viewIndex) {
    // A one-row range: the caller gave exactly one identity, so that is the whole
    // range. The anchor is untouched (a range does not re-anchor).
    if (identityStr.isEmpty()) {
        return;
    }
    selectedIdentities_.clear();
    selectedIdentities_.insert(identityStr);
    focusIndex_ = viewIndex;
    ++selectionRevision_;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}

void SelectionState::moveFocusDown() { setFocusIndex(focusIndex_ + 1); }
void SelectionState::moveFocusUp() { setFocusIndex(std::max(0, focusIndex_ - 1)); }
void SelectionState::moveFocusHome() { setFocusIndex(0); }
void SelectionState::moveFocusEnd() { setFocusIndex(9999); } // symbolic; view handles wrap
void SelectionState::moveFocusPageUp() { setFocusIndex(std::max(0, focusIndex_ - 10)); }
void SelectionState::moveFocusPageDown() { setFocusIndex(focusIndex_ + 10); }

QString SelectionState::selectionSummary() const {
    const int n = selectedCount();
    if (n == 0) return QStringLiteral("No selection");
    return QStringLiteral("%1 selected").arg(n);
}
