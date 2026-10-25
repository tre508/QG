// Per-folder sort + density settings tests (Prompt 31).

#include <QtTest/QtTest>

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "controllers/browser_controller.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"

using namespace qg;

namespace {

QString dbPath(const QTemporaryDir &dir)
{
    return QDir(dir.path()).filePath(QStringLiteral("qg.db"));
}

} // namespace

class SettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void densityRoundTripsPerFolderWithDefaults();
    void sortSpecRoundTripsPerFolderWithDefaults();
    void controllerLoadsOnEnterAndWritesOnChange();
};

void SettingsTest::densityRoundTripsPerFolderWithDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dbPath(dir);

    {
        Database db;
        QVERIFY(db.open(path));
        SettingsRepository settings(db);
        QVERIFY(settings.setDensity(QStringLiteral("/a"), ViewDensity::Compact));
        QVERIFY(settings.setDensity(QStringLiteral("/b"), ViewDensity::Gallery));
    }

    Database db;
    QVERIFY(db.open(path));
    SettingsRepository settings(db);
    QCOMPARE(settings.density(QStringLiteral("/a")), ViewDensity::Compact);
    QCOMPARE(settings.density(QStringLiteral("/b")), ViewDensity::Gallery);
    // Unseen folder yields the default density.
    QCOMPARE(settings.density(QStringLiteral("/other")), ViewDensity::Standard);
}

void SettingsTest::sortSpecRoundTripsPerFolderWithDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dbPath(dir);

    {
        Database db;
        QVERIFY(db.open(path));
        SettingsRepository settings(db);
        // Default for unseen folders.
        QCOMPARE(settings.sortSpec(QStringLiteral("/x")).key, SortKey::Name);

        SortSpec spec;
        spec.key = SortKey::Duration;
        spec.direction = SortDirection::Descending;
        spec.foldersFirst = false;
        QVERIFY(settings.setSortSpec(QStringLiteral("/x"), spec));
    }

    Database db;
    QVERIFY(db.open(path));
    SettingsRepository settings(db);
    const SortSpec restored = settings.sortSpec(QStringLiteral("/x"));
    QCOMPARE(restored.key, SortKey::Duration);
    QCOMPARE(restored.direction, SortDirection::Descending);
    QCOMPARE(restored.foldersFirst, false);

    // Unseen folder keeps folders-first default on.
    QCOMPARE(settings.sortSpec(QStringLiteral("/y")).foldersFirst, true);
}

void SettingsTest::controllerLoadsOnEnterAndWritesOnChange()
{
    QTemporaryDir dbDir;
    QVERIFY(dbDir.isValid());
    QTemporaryDir folderA;
    QVERIFY(folderA.isValid());
    QTemporaryDir folderB;
    QVERIFY(folderB.isValid());

    Database db;
    QVERIFY(db.open(dbPath(dbDir)));
    SettingsRepository settings(db);

    BrowserController controller;
    controller.setSettings(&settings);

    // Unseen folder -> defaults applied on entry.
    QVERIFY(controller.navigate(folderA.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    QCOMPARE(controller.sortSpec().key, SortKey::Name);
    QCOMPARE(controller.sortSpec().direction, SortDirection::Ascending);
    QCOMPARE(controller.sortSpec().foldersFirst, true);
    QCOMPARE(controller.density(), ViewDensity::Standard);

    // Change -> writes through and emits.
    QSignalSpy sortSpy(&controller, &BrowserController::sortSpecChanged);
    QSignalSpy densitySpy(&controller, &BrowserController::densityChanged);
    SortSpec spec;
    spec.key = SortKey::Size;
    spec.direction = SortDirection::Descending;
    spec.foldersFirst = false;
    controller.setSortSpec(spec);
    controller.setDensity(ViewDensity::Compact);
    QCOMPARE(sortSpy.count(), 1);
    QCOMPARE(densitySpy.count(), 1);

    // Persisted for that folder only.
    const QString folderAPath = QDir::cleanPath(folderA.path());
    QCOMPARE(settings.sortSpec(folderAPath).key, SortKey::Size);
    QCOMPARE(settings.sortSpec(folderAPath).direction, SortDirection::Descending);
    QCOMPARE(settings.density(folderAPath), ViewDensity::Compact);

    // A different unseen folder gets defaults.
    QVERIFY(controller.navigate(folderB.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    QCOMPARE(controller.sortSpec().key, SortKey::Name);
    QCOMPARE(controller.density(), ViewDensity::Standard);

    // Returning restores the persisted settings.
    QVERIFY(controller.navigate(folderA.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    QCOMPARE(controller.sortSpec().key, SortKey::Size);
    QCOMPARE(controller.sortSpec().direction, SortDirection::Descending);
    QCOMPARE(controller.density(), ViewDensity::Compact);

    // QML-facing setters map ints/bools and update the label.
    controller.setSort(static_cast<int>(SortKey::Watched), true, true);
    QCOMPARE(controller.sortKeyValue(), static_cast<int>(SortKey::Watched));
    QCOMPARE(controller.sortDescending(), true);
    QCOMPARE(controller.foldersFirst(), true);
    QCOMPARE(controller.orderLabel(), QStringLiteral("Watched ") + QChar(0x2193));

    controller.setDensityValue(static_cast<int>(ViewDensity::Gallery));
    QCOMPARE(controller.densityValue(), static_cast<int>(ViewDensity::Gallery));
    QCOMPARE(controller.density(), ViewDensity::Gallery);
}

QTEST_GUILESS_MAIN(SettingsTest)
#include "test_settings.moc"