#include <QtTest>

#include "models/selection_state.hpp"

class TestSelection : public QObject {
     // SKIP_AUTOMOC not needed for simple QObject
private slots:
    void testSelectToggleClear();
    void testFocusIndependentFromSelection();
    void testRangeAnchor();
};

void TestSelection::testSelectToggleClear() {
    SelectionState s;
    s.selectOne("id-1", 0);
    QCOMPARE(s.selectedCount(), 1);
    s.toggle("id-1", 0);
    QCOMPARE(s.selectedCount(), 0);
    s.selectOne("id-2", 5);
    QCOMPARE(s.focusIndex(), 5);
    s.selectAll();
    s.clear();
    QCOMPARE(s.selectedCount(), 0);
    QCOMPARE(s.focusIndex(), -1);
}

void TestSelection::testFocusIndependentFromSelection() {
    SelectionState s;
    s.selectOne("a", 1);
    s.setFocusIndex(3);
    QCOMPARE(s.focusIndex(), 3);
    QCOMPARE(s.isSelected("a"), true);
    s.moveFocusUp();
    QCOMPARE(s.focusIndex(), 2);
}

void TestSelection::testRangeAnchor() {
    SelectionState s;
    s.selectOne("a", 2);
    QCOMPARE(s.anchorIndex(), 2);
    s.extendRange("b", 4);
    QCOMPARE(s.isSelected("b"), true);
    QCOMPARE(s.focusIndex(), 4);
}



int main(int argc, char** argv) { QCoreApplication app(argc, argv); return 0; }
