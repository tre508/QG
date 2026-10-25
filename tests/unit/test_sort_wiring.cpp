// Sort wiring tests (Prompt 34): choosing a sort re-orders the model and
// persists the choice per folder.

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "controllers/browser_controller.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"

using namespace qg;

namespace {

void writeFile(const QString &path, qint64 size)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QByteArray(int(size), 'x'));
    }
}

QStringList names(const FolderModel &model)
{
    QStringList result;
    for (int row = 0; row < model.rowCount(); ++row) {
        result.append(model.data(model.index(row, 0), FolderModel::NameRole).toString());
    }
    return result;
}

} // namespace

class SortWiringTest : public QObject
{
    Q_OBJECT

private slots:
    void changingSortReordersAndPersistsPerFolder();
};

void SortWiringTest::changingSortReordersAndPersistsPerFolder()
{
    QTemporaryDir dbDir;
    QVERIFY(dbDir.isValid());
    QTemporaryDir folderA;
    QVERIFY(folderA.isValid());
    QTemporaryDir folderB;
    QVERIFY(folderB.isValid());

    const QDir root(folderA.path());
    writeFile(root.filePath(QStringLiteral("one.mp4")), 10);
    writeFile(root.filePath(QStringLiteral("two.mp4")), 100);

    Database db;
    QVERIFY(db.open(QDir(dbDir.path()).filePath(QStringLiteral("qg.db"))));
    SettingsRepository settings(db);

    BrowserController controller;
    controller.setSettings(&settings);

    // Entering an unseen folder sorts by Name ascending.
    QVERIFY(controller.navigate(folderA.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    QCOMPARE(names(*controller.model()),
             QStringList({QStringLiteral("one.mp4"), QStringLiteral("two.mp4")}));

    // Choose Size descending: re-orders the canonical model immediately.
    controller.setSort(static_cast<int>(SortKey::Size), true, false);
    QCOMPARE(names(*controller.model()),
             QStringList({QStringLiteral("two.mp4"), QStringLiteral("one.mp4")}));

    // The choice is persisted for exactly this folder.
    const QString folderPath = QDir::cleanPath(folderA.path());
    QCOMPARE(settings.sortSpec(folderPath).key, SortKey::Size);
    QCOMPARE(settings.sortSpec(folderPath).direction, SortDirection::Descending);
    QCOMPARE(settings.sortSpec(folderPath).foldersFirst, false);

    // Another unseen folder keeps the default (Name asc).
    QVERIFY(controller.navigate(folderB.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    QCOMPARE(controller.sortSpec().key, SortKey::Name);

    // Returning restores the persisted Size-desc order.
    QVERIFY(controller.navigate(folderA.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    QCOMPARE(controller.sortSpec().key, SortKey::Size);
    QCOMPARE(names(*controller.model()),
             QStringList({QStringLiteral("two.mp4"), QStringLiteral("one.mp4")}));
}

QTEST_GUILESS_MAIN(SortWiringTest)
#include "test_sort_wiring.moc"