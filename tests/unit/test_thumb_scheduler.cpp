// Thumbnail scheduler + LRU tests (Prompt 25).

#include <QtTest/QtTest>

#include <QImage>

#include "media/thumbnails.hpp"

using namespace qg;

class ThumbSchedulerTest : public QObject
{
    Q_OBJECT

private slots:
    void visibleItemsAreRequestedFirst();
    void viewportMoveDemotesAndDrops();
    void lruIsBounded();
    void bulkOperationsDontStarveVisible();
    void cancelledRequestsDontConsumeResources();
    void priorityInversionPrevention();

private:
    static ThumbnailRequest makeRequest(int index)
    {
        ThumbnailRequest request;
        request.identity = QStringLiteral("id-%1").arg(index);
        request.path = QStringLiteral("/v/%1.mp4").arg(index);
        return request;
    }
};

void ThumbSchedulerTest::visibleItemsAreRequestedFirst()
{
    ThumbnailScheduler scheduler;
    for (int i = 0; i < 100; ++i) {
        scheduler.request(i, makeRequest(i));
    }
    scheduler.setViewport(40, 49, 100);

    QCOMPARE(scheduler.priorityOf(45),
             static_cast<int>(ThumbnailScheduler::Priority::Interactive));
    QCOMPARE(scheduler.priorityOf(35), static_cast<int>(ThumbnailScheduler::Priority::Normal));
    QCOMPARE(scheduler.priorityOf(5), static_cast<int>(ThumbnailScheduler::Priority::Background));

    // The first item taken is a visible (Interactive) row, starting at 40.
    const auto first = scheduler.takeNext();
    QVERIFY(first.has_value());
    QVERIFY(first->index >= 40 && first->index <= 49);
    QCOMPARE(first->index, 40);
}

void ThumbSchedulerTest::viewportMoveDemotesAndDrops()
{
    ThumbnailScheduler scheduler;
    for (int i = 0; i < 100; ++i) {
        scheduler.request(i, makeRequest(i));
    }

    scheduler.setViewport(40, 49, 100);
    QCOMPARE(scheduler.priorityOf(40),
             static_cast<int>(ThumbnailScheduler::Priority::Interactive));

    // Scrolling to the top demotes the old visible band and drops far rows.
    scheduler.setViewport(0, 9, 100);
    QCOMPARE(scheduler.priorityOf(40),
             static_cast<int>(ThumbnailScheduler::Priority::Background));
    QCOMPARE(scheduler.priorityOf(1),
             static_cast<int>(ThumbnailScheduler::Priority::Interactive));
    QCOMPARE(scheduler.priorityOf(90), -1); // beyond horizon: dropped

    const auto next = scheduler.takeNext();
    QVERIFY(next.has_value());
    QCOMPARE(next->index, 0);
}

void ThumbSchedulerTest::lruIsBounded()
{
    ThumbnailScheduler scheduler;
    scheduler.setDecodedCapacity(3);

    for (int i = 0; i < 4; ++i) {
        QImage image(4, 4, QImage::Format_ARGB32);
        image.fill(Qt::black);
        scheduler.storeDecoded(QStringLiteral("id-%1").arg(i), image);
    }

    QCOMPARE(scheduler.decodedCount(), 3);
    QVERIFY(!scheduler.hasDecoded(QStringLiteral("id-0"))); // oldest evicted
    QVERIFY(scheduler.hasDecoded(QStringLiteral("id-3")));  // most recent kept
}

void ThumbSchedulerTest::bulkOperationsDontStarveVisible()
{
    // Simulate a bulk operation (background thumbnail requests)
    // while visible items are requested - verify visible items get priority
    ThumbnailScheduler scheduler;
    
    // Queue background requests within horizon so they're not dropped
    for (int i = 0; i < 50; ++i) {
        scheduler.request(10 + i, makeRequest(10 + i));
    }
    
    // Now request 10 visible items (indices 0-9)
    for (int i = 0; i < 10; ++i) {
        scheduler.request(i, makeRequest(i));
    }
    scheduler.setViewport(0, 9, 70, 60); // horizon=60, so items up to 9+60=69 are kept
    
    // Visible items should have Interactive priority
    for (int i = 0; i < 10; ++i) {
        QCOMPARE(scheduler.priorityOf(i), 
                 static_cast<int>(ThumbnailScheduler::Priority::Interactive));
    }
    
    // Background items within horizon: distance 1-10 from view are Normal, 11+ are Background
    // span = 10 (view size), so distance 1-10 = Normal, distance 11-60 = Background
    for (int i = 10; i < 20; ++i) {
        QCOMPARE(scheduler.priorityOf(i), 
                 static_cast<int>(ThumbnailScheduler::Priority::Normal));
    }
    for (int i = 20; i < 60; ++i) {
        QCOMPARE(scheduler.priorityOf(i), 
                 static_cast<int>(ThumbnailScheduler::Priority::Background));
    }
    
    // First items taken should be the visible ones (Interactive priority)
    for (int i = 0; i < 10; ++i) {
        const auto next = scheduler.takeNext();
        QVERIFY(next.has_value());
        QVERIFY(next->index >= 0 && next->index <= 9);
    }
    
    // Next items taken should be Normal priority (distance 1-10 from view)
    for (int i = 0; i < 10; ++i) {
        const auto next = scheduler.takeNext();
        QVERIFY(next.has_value());
        QVERIFY(next->index >= 10 && next->index < 20);
    }
    
    // After Normal items exhausted, next should be Background
    const auto nextBg = scheduler.takeNext();
    QVERIFY(nextBg.has_value());
    QVERIFY(nextBg->index >= 20 && nextBg->index < 60);
}

void ThumbSchedulerTest::cancelledRequestsDontConsumeResources()
{
    // Request many items, then clear - verify they don't consume resources
    ThumbnailScheduler scheduler;
    
    for (int i = 0; i < 100; ++i) {
        scheduler.request(i, makeRequest(i));
    }
    
    scheduler.clear();
    
    // After cancellation, no items should be available
    for (int i = 0; i < 100; ++i) {
        QCOMPARE(scheduler.priorityOf(i), -1);
    }
    
    const auto next = scheduler.takeNext();
    QVERIFY(!next.has_value());
}

void ThumbSchedulerTest::priorityInversionPrevention()
{
    // High-priority request after low-priority should still get served first
    ThumbnailScheduler scheduler;
    
    // Queue 100 background requests
    for (int i = 0; i < 100; ++i) {
        scheduler.request(i, makeRequest(i));
    }
    
    // Then add 5 interactive requests
    for (int i = 100; i < 105; ++i) {
        scheduler.request(i, makeRequest(i));
    }
    scheduler.setViewport(100, 104, 105);
    
    // Interactive items should be served first
    for (int i = 0; i < 5; ++i) {
        const auto next = scheduler.takeNext();
        QVERIFY(next.has_value());
        QVERIFY(next->index >= 100 && next->index <= 104);
    }
    
    // Then background items
    const auto next = scheduler.takeNext();
    QVERIFY(next.has_value());
    QVERIFY(next->index < 100);
}

QTEST_GUILESS_MAIN(ThumbSchedulerTest)
#include "test_thumb_scheduler.moc"
