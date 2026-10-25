#include "selection_state.hpp"
#include <algorithm>

SelectionState::SelectionState(QObject* parent) : QObject(parent), focusIndex_(-1), anchorIndex_(-1) {}

bool SelectionState::isSelected(const QString& identityStr) const {
    return selectedIdentities_.contains(identityStr);
}

int SelectionState::selectedCount() const {
    return selectedIdentities_.size();
}

QStringList SelectionState::selectedIdentities() const {
    QStringList list;
    for (const QString& id : selectedIdentities_) list.append(id);
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
    selectedIdentities_.insert(identityStr);
    focusIndex_ = viewIndex;
    anchorIndex_ = viewIndex;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}

void SelectionState::toggle(const QString& identityStr, int viewIndex) {
    if (selectedIdentities_.contains(identityStr)) {
        selectedIdentities_.remove(identityStr);
    } else {
        selectedIdentities_.insert(identityStr);
    }
    focusIndex_ = viewIndex;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}

void SelectionState::extendRange(const QString& identityStr, int viewIndex) {
    // Range selection: anchor -> viewIndex (simplified: add all between anchor and current view)
    // For this minimal implementation we just add the identity and update focus
    selectedIdentities_.insert(identityStr);
    focusIndex_ = viewIndex;
    emit selectionChanged();
    emit focusChanged();
    emit selectionSummaryChanged();
}

void SelectionState::selectAll() {
    // In a real implementation this would use the current model count; here we signal full selection.
    emit selectionChanged();
    emit selectionSummaryChanged();
}

void SelectionState::clear() {
    selectedIdentities_.clear();
    focusIndex_ = -1;
    anchorIndex_ = -1;
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
    int n = selectedCount();
    if (n == 0) return QStringLiteral("No selection");
    return QStringLiteral("%1 selected").arg(n);
}
