// PlaybackContext tests (REMEDIATION-007 / D-07, D-24).
//
// The ordered playback context is the single list Previous/Next traverse and the
// Reel displays. These tests pin: canonical model order (videos only, folders
// never media), identity-based current lookup, the All/Favorites/Unwatched
// filter, deterministic end behaviour, and the empty/missing cases.

#include <QtTest/QtTest>

#include "models/folder_model.hpp"
#include "playback/playback_context.hpp"

namespace {

qg::Entry video(int ino, const QString& name, bool favorite = false, bool watched = false)
{
    qg::Entry e;
    e.identity = QStringLiteral("inode:1:%1").arg(ino);
    e.path = QStringLiteral("/videos/") + name;
    e.name = name;
    e.kind = qg::EntryKind::Video;
    e.size = 1000 * ino;
    e.durationMs = 60000;
    e.favorite = favorite;
    e.watched = watched;
    return e;
}

qg::Entry folder(const QString& name)
{
    qg::Entry e;
    e.identity = QStringLiteral("inode:2:%1").arg(name);
    e.path = QStringLiteral("/videos/") + name;
    e.name = name;
    e.kind = qg::EntryKind::Folder;
    return e;
}

void fill(qg::FolderModel& model, const QList<qg::Entry>& entries)
{
    qg::FolderSnapshot snap;
    snap.path = QStringLiteral("/videos");
    snap.entries = entries;
    snap.complete = true;
    model.applySnapshot(snap);
}

} // namespace

class PlaybackContextTest : public QObject
{
    Q_OBJECT

private slots:
    void orderIsCanonicalAndVideosOnly();
    void currentIndexIsLocatedByIdentity();
    void missingIdentityLeavesNoCurrent();
    void emptyModelIsSafe();
    void favoritesAndUnwatchedFilters();
    void stepsStopAtTheEnds();
    void followsTheActiveSortOrder();
    void setCurrentByIdentityMovesCursor();
};

void PlaybackContextTest::orderIsCanonicalAndVideosOnly()
{
    qg::FolderModel model;
    fill(model, {folder(QStringLiteral("sub")),
                 video(101, QStringLiteral("a.mp4")),
                 folder(QStringLiteral("more")),
                 video(102, QStringLiteral("b.mp4")),
                 video(103, QStringLiteral("c.mp4"))});

    PlaybackContext ctx;
    ctx.build(&model, QStringLiteral("inode:1:102"));

    QCOMPARE(ctx.totalCount(), 3); // folders excluded
    const QVector<ContextEntry> entries = ctx.entries();
    QCOMPARE(entries.at(0).name, QStringLiteral("a.mp4"));
    QCOMPARE(entries.at(1).name, QStringLiteral("b.mp4"));
    QCOMPARE(entries.at(2).name, QStringLiteral("c.mp4"));
    // modelRow points back at the canonical model row (folders included).
    QCOMPARE(entries.at(0).modelRow, 1);
    QCOMPARE(entries.at(1).modelRow, 3);
    QCOMPARE(entries.at(2).modelRow, 4);
    QCOMPARE(entries.at(1).identity, QStringLiteral("inode:1:102"));
    QVERIFY(entries.at(0).modelRow < entries.at(1).modelRow); // order preserved
}

void PlaybackContextTest::currentIndexIsLocatedByIdentity()
{
    qg::FolderModel model;
    fill(model, {video(201, QStringLiteral("a.mp4")),
                 video(202, QStringLiteral("b.mp4")),
                 video(203, QStringLiteral("c.mp4"))});

    PlaybackContext ctx;
    ctx.build(&model, QStringLiteral("inode:1:203"));
    QCOMPARE(ctx.currentIndex(), 2);
    QCOMPARE(ctx.currentEntry().name, QStringLiteral("c.mp4"));
    QCOMPARE(ctx.indexOfIdentity(QStringLiteral("inode:1:202")), 1);
    QCOMPARE(ctx.indexOfIdentity(QStringLiteral("nope")), -1);
}

void PlaybackContextTest::missingIdentityLeavesNoCurrent()
{
    qg::FolderModel model;
    fill(model, {video(301, QStringLiteral("a.mp4")), video(302, QStringLiteral("b.mp4"))});

    PlaybackContext ctx;
    ctx.build(&model, QStringLiteral("inode:1:999")); // not present
    QCOMPARE(ctx.totalCount(), 2);
    QCOMPARE(ctx.currentIndex(), -1);                 // no bogus index
    QVERIFY(ctx.currentEntry().path.isEmpty());
    // No anchor means "before the list": Next starts at the first entry, while
    // Previous is still a boundary (there is nothing before the start).
    QCOMPARE(ctx.canStep(1), true);
    QCOMPARE(ctx.canStep(-1), false);
    QCOMPARE(ctx.step(1), true);
    QCOMPARE(ctx.currentIndex(), 0);

    ctx.build(&model, QString());                     // empty identity
    QCOMPARE(ctx.currentIndex(), -1);
}

void PlaybackContextTest::emptyModelIsSafe()
{
    PlaybackContext ctx;
    ctx.build(nullptr, QStringLiteral("inode:1:1"));
    QCOMPARE(ctx.totalCount(), 0);
    QCOMPARE(ctx.currentIndex(), -1);
    QVERIFY(ctx.currentEntry().path.isEmpty());
    QCOMPARE(ctx.canStep(1), false);
    QCOMPARE(ctx.step(1), false);

    qg::FolderModel empty;
    ctx.build(&empty, QStringLiteral("x"));
    QCOMPARE(ctx.totalCount(), 0);
    QCOMPARE(ctx.step(-1), false);
}

void PlaybackContextTest::favoritesAndUnwatchedFilters()
{
    qg::FolderModel model;
    fill(model, {video(401, QStringLiteral("a.mp4")),
                 video(402, QStringLiteral("b.mp4"), true),          // favorite
                 video(403, QStringLiteral("c.mp4"), false, true),   // watched
                 video(404, QStringLiteral("d.mp4"), true),          // favorite
                 video(405, QStringLiteral("e.mp4"))});

    PlaybackContext ctx;
    ctx.build(&model, QString(), PlaybackContext::All);
    QCOMPARE(ctx.totalCount(), 5);

    ctx.build(&model, QStringLiteral("inode:1:404"), PlaybackContext::Favorites);
    QCOMPARE(ctx.totalCount(), 2);
    QCOMPARE(ctx.currentIndex(), 1);
    QCOMPARE(ctx.entries().at(0).name, QStringLiteral("b.mp4"));
    QCOMPARE(ctx.entries().at(1).name, QStringLiteral("d.mp4"));

    ctx.build(&model, QString(), PlaybackContext::Unwatched);
    QCOMPARE(ctx.totalCount(), 4); // everything except c.mp4
    for (const ContextEntry& e : ctx.entries())
        QVERIFY(e.name != QStringLiteral("c.mp4"));

    QVERIFY(PlaybackContext::matchesFilter(video(1, QStringLiteral("f"), true),
                                           PlaybackContext::Favorites));
    QCOMPARE(PlaybackContext::matchesFilter(video(1, QStringLiteral("f"), false, true),
                                            PlaybackContext::Unwatched), false);
}

void PlaybackContextTest::stepsStopAtTheEnds()
{
    qg::FolderModel model;
    fill(model, {video(501, QStringLiteral("a.mp4")),
                 video(502, QStringLiteral("b.mp4")),
                 video(503, QStringLiteral("c.mp4"))});

    PlaybackContext ctx;
    ctx.build(&model, QStringLiteral("inode:1:501"));
    QSignalSpy endSpy(&ctx, &PlaybackContext::endReached);
    QSignalSpy idxSpy(&ctx, &PlaybackContext::currentIndexChanged);

    QCOMPARE(ctx.canGoNext(), true);
    QCOMPARE(ctx.canGoPrevious(), false);

    QCOMPARE(ctx.step(-1), false);          // at the first item
    QCOMPARE(endSpy.count(), 1);
    QCOMPARE(ctx.currentIndex(), 0);        // cursor unchanged

    QCOMPARE(ctx.step(1), true);
    QCOMPARE(ctx.currentIndex(), 1);
    QCOMPARE(idxSpy.count(), 1);
    QCOMPARE(ctx.step(1), true);
    QCOMPARE(ctx.currentIndex(), 2);
    QCOMPARE(ctx.canGoNext(), false);

    QCOMPARE(ctx.step(1), false);           // at the last item
    QCOMPARE(endSpy.count(), 2);
    QCOMPARE(ctx.currentIndex(), 2);
}

void PlaybackContextTest::followsTheActiveSortOrder()
{
    qg::FolderModel model;
    fill(model, {video(601, QStringLiteral("a.mp4")),
                 video(602, QStringLiteral("b.mp4")),
                 video(603, QStringLiteral("c.mp4"))});

    qg::SortSpec spec;
    spec.key = qg::SortKey::Name;
    spec.direction = qg::SortDirection::Descending;
    spec.foldersFirst = true;
    model.reorder(spec);

    PlaybackContext ctx;
    ctx.build(&model, QStringLiteral("inode:1:601"));
    QCOMPARE(ctx.totalCount(), 3);
    // The context mirrors the model's displayed order (c, b, a).
    QCOMPARE(ctx.entries().at(0).name, QStringLiteral("c.mp4"));
    QCOMPARE(ctx.entries().at(2).name, QStringLiteral("a.mp4"));
    QCOMPARE(ctx.currentIndex(), 2);
}

void PlaybackContextTest::setCurrentByIdentityMovesCursor()
{
    qg::FolderModel model;
    fill(model, {video(701, QStringLiteral("a.mp4")),
                 video(702, QStringLiteral("b.mp4"))});

    PlaybackContext ctx;
    ctx.build(&model, QStringLiteral("inode:1:701"));
    QCOMPARE(ctx.currentIndex(), 0);

    QCOMPARE(ctx.setCurrentByIdentity(QStringLiteral("inode:1:702")), true);
    QCOMPARE(ctx.currentIndex(), 1);

    QCOMPARE(ctx.setCurrentByIdentity(QStringLiteral("missing")), false);
    QCOMPARE(ctx.currentIndex(), 1); // unchanged on failure
}

QTEST_MAIN(PlaybackContextTest)
#include "test_playback_context.moc"
