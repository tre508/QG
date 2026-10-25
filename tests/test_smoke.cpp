// Smoke test for the build foundation (Prompt 01).
//
// Proves the CTest wiring executes a real Qt Test binary. It deliberately tests
// nothing about QG behaviour yet; domain tests arrive with their prompts.

#include <QtTest/QtTest>

class SmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void qtRuntimeIsUsable()
    {
        QVERIFY(!QString(qVersion()).isEmpty());
    }
};

QTEST_APPLESS_MAIN(SmokeTest)
#include "test_smoke.moc"
