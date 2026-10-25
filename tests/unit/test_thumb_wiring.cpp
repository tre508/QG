// Thumbnail pipeline wiring tests (Prompt 27).

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "controllers/thumbnail_controller.hpp"
#include "media/thumbnails.hpp"
#include "models/folder_model.hpp"

using namespace qg;

namespace {

Entry makeVideo(const QString &name, const QString &identity, qint64 mtimeNs, qint64 size)
{
    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = name;
    entry.path = QStringLiteral("/v/") + name;
    entry.identity = identity;
    entry.mtimeNs = mtimeNs;
    entry.size = size;
    return entry;
}

ThumbnailRequest requestFor(const QString &identity, qint64 mtimeNs, qint64 size)
{
    ThumbnailRequest request;
    request.identity = identity;
    request.path = QStringLiteral("/v/") + identity + QStringLiteral(".mp4");
    request.mtimeNs = mtimeNs;
    request.size = size;
    request.width = 16;
    request.height = 16;
    return request;
}

ThumbnailExtraction goodImage(int w = 16, int h = 16)
{
    ThumbnailExtraction result;
    result.image = QImage(w, h, QImage::Format_ARGB32);
    result.image.fill(Qt::green);
    return result;
}

} // namespace

class ThumbWiringTest : public QObject
{
    Q_OBJECT

private slots:
    void appliesValidatedThumbnailsAndScopesDataChanged();
    void staleGenerationResultDropped();
    void sourceChangedResultDropped();
    void failedExtractionSetsFailedState();
    void repeatedRequestVisibleDoesNotReExtract();
};

void ThumbWiringTest::appliesValidatedThumbnailsAndScopesDataChanged()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 100, 10),
                        makeVideo(QStringLiteral("b.mp4"), QStringLiteral("ib"), 100, 20)};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);
    controller.setImmediateProcessing(true);
    controller.setExtractor([](const ThumbnailRequest &) { return goodImage(); });

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    controller.requestVisible(0, 1);

    QCOMPARE(model.data(model.index(0, 0), FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(ThumbnailState::Ready));
    QVERIFY(model.data(model.index(0, 0), FolderModel::ThumbnailRole).isValid());
    QCOMPARE(model.data(model.index(1, 0), FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(ThumbnailState::Ready));

    QVERIFY(changed.count() >= 2);
    for (int i = 0; i < changed.count(); ++i) {
        const QList<int> roles = changed.at(i).at(2).value<QList<int>>();
        QVERIFY(roles.contains(FolderModel::ThumbnailRole));
        QVERIFY(!roles.contains(FolderModel::NameRole));
        QVERIFY(!roles.contains(FolderModel::FavoriteRole));
    }
}

void ThumbWiringTest::staleGenerationResultDropped()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 100, 10)};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);
    const quint64 oldGeneration = controller.generation(); // 0

    controller.beginGeneration(); // navigation happened; generation now 1

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    controller.injectResultForTesting(requestFor(QStringLiteral("ia"), 100, 10),
                                      oldGeneration, goodImage());
    QCOMPARE(changed.count(), 0); // stale result dropped

    controller.injectResultForTesting(requestFor(QStringLiteral("ia"), 100, 10),
                                      controller.generation(), goodImage());
    QCOMPARE(changed.count(), 1);
}

void ThumbWiringTest::sourceChangedResultDropped()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 100, 10)};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    // Request carries a different mtime than the model row: file replaced.
    controller.injectResultForTesting(requestFor(QStringLiteral("ia"), 999, 10),
                                      controller.generation(), goodImage());
    QCOMPARE(changed.count(), 0);
}

void ThumbWiringTest::failedExtractionSetsFailedState()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 100, 10)};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);

    ThumbnailExtraction failure;
    failure.error = Error::make(ErrorCode::OperationFailed, QStringLiteral("no frame"));
    controller.injectResultForTesting(requestFor(QStringLiteral("ia"), 100, 10),
                                      controller.generation(), failure);

    QCOMPARE(model.data(model.index(0, 0), FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(ThumbnailState::Failed));
    QVERIFY(!model.data(model.index(0, 0), FolderModel::ThumbnailRole).isValid());
}

// REM-01: the ContentView re-reports its viewport on every scroll/resize; rows
// already settled must not be re-extracted. A re-report of an unchanged window
// is a no-op, and a navigation (which resets the model) re-arms the rows.
void ThumbWiringTest::repeatedRequestVisibleDoesNotReExtract()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 100, 10),
                        makeVideo(QStringLiteral("b.mp4"), QStringLiteral("ib"), 100, 20)};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);
    controller.setImmediateProcessing(true);
    int extractions = 0;
    controller.setExtractor([&extractions](const ThumbnailRequest &) {
        ++extractions;
        return goodImage();
    });

    controller.requestVisible(0, 1);
    QCOMPARE(extractions, 2);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(ThumbnailState::Ready));

    // Re-reporting the same visible window must not re-decode Ready rows.
    controller.requestVisible(0, 1);
    QCOMPARE(extractions, 2);

    // Navigation resets the model: states clear, so a fresh viewport re-arms.
    model.applySnapshot(snapshot);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(ThumbnailState::None));
    controller.beginGeneration();
    controller.requestVisible(0, 1);
    QCOMPARE(extractions, 4);
}

QTEST_GUILESS_MAIN(ThumbWiringTest)
#include "test_thumb_wiring.moc"
