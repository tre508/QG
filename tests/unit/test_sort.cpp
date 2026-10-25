// SortService + SortSpec deterministic total order tests (Prompt 33).

#include <QtTest/QtTest>

#include <QSignalSpy>

#include <array>
#include <optional>

#include "controllers/probe_controller.hpp"
#include "models/folder_model.hpp"
#include "sorting/sort_service.hpp"

using namespace qg;

namespace {

Entry makeVideo(const QString &name,
                const QString &identity,
                qint64 mtimeNs = 0,
                qint64 size = 0,
                const std::optional<qint64> &duration = std::nullopt,
                bool watched = false,
                bool favorite = false)
{
    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = name;
    entry.path = QStringLiteral("/v/") + name;
    entry.identity = identity;
    entry.mtimeNs = mtimeNs;
    entry.size = size;
    entry.durationMs = duration;
    entry.watched = watched;
    entry.favorite = favorite;
    return entry;
}

Entry makeFolder(const QString &name, const QString &identity)
{
    Entry entry;
    entry.kind = EntryKind::Folder;
    entry.name = name;
    entry.path = QStringLiteral("/v/") + name;
    entry.identity = identity;
    return entry;
}

QStringList namesOf(const QList<Entry> &rows)
{
    QStringList result;
    for (const Entry &entry : rows) {
        result.append(entry.name);
    }
    return result;
}

} // namespace

class SortTest : public QObject
{
    Q_OBJECT

private slots:
    void nameSortsNaturallyAndTieBreaksDeterministically();
    void repeatedSortIsIdentical();
    void foldersFirstHonoredInEveryMode();
    void unknownDurationsLastAscFirstDesc();
    void modifiedSizeWatchedFavoriteKeys();
    void modelReorderEmitsLayoutChanged();
    void probeResortConsumesCoalescedSignal();
};

void SortTest::nameSortsNaturallyAndTieBreaksDeterministically()
{
    QList<Entry> rows;
    rows << makeVideo(QStringLiteral("v10.mp4"), QStringLiteral("i10"))
         << makeVideo(QStringLiteral("v2.mp4"), QStringLiteral("i2"))
         << makeVideo(QStringLiteral("apple.mp4"), QStringLiteral("ia"))
         << makeVideo(QStringLiteral("Apple.mp4"), QStringLiteral("iA"));

    SortSpec spec; // Name asc, folders-first on (no folders here)
    const QList<Entry> ordered = SortService::apply(rows, spec);

    // Natural numeric + case-insensitive, then case-sensitive tie-break.
    QCOMPARE(namesOf(ordered),
             QStringList({QStringLiteral("Apple.mp4"), QStringLiteral("apple.mp4"),
                          QStringLiteral("v2.mp4"), QStringLiteral("v10.mp4")}));
}

void SortTest::repeatedSortIsIdentical()
{
    QList<Entry> rows;
    rows << makeVideo(QStringLiteral("c.mp4"), QStringLiteral("ic"), 30, 300)
         << makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 10, 100)
         << makeFolder(QStringLiteral("sub"), QStringLiteral("is"))
         << makeVideo(QStringLiteral("b.mp4"), QStringLiteral("ib"), 20, 200);

    SortSpec spec;
    spec.key = SortKey::Modified;
    spec.direction = SortDirection::Descending;

    const QList<Entry> first = SortService::apply(rows, spec);
    const QList<Entry> second = SortService::apply(rows, spec);
    QCOMPARE(namesOf(first), namesOf(second));

    // Also stable across a fresh copy of the same input.
    QList<Entry> copy = rows;
    QCOMPARE(namesOf(SortService::apply(copy, spec)), namesOf(first));
}

void SortTest::foldersFirstHonoredInEveryMode()
{
    const std::array<SortKey, 6> keys = {SortKey::Name,    SortKey::Modified,
                                         SortKey::Duration, SortKey::Size,
                                         SortKey::Watched,  SortKey::Favorite};

    for (SortKey key : keys) {
        QList<Entry> rows;
        rows << makeVideo(QStringLiteral("a.mp4"), QStringLiteral("iv"), 1, 10, 5000)
             << makeFolder(QStringLiteral("f1"), QStringLiteral("f1"))
             << makeFolder(QStringLiteral("f2"), QStringLiteral("f2"))
             << makeVideo(QStringLiteral("b.mp4"), QStringLiteral("iv2"), 2, 20, 3000);

        SortSpec spec;
        spec.key = key;
        spec.foldersFirst = true;
        const QList<Entry> ordered = SortService::apply(rows, spec);
        QVERIFY2(ordered.at(0).isFolder() && ordered.at(1).isFolder(),
                 QStringLiteral("folders not first for key %1").arg(int(key)).toUtf8());
        QVERIFY(ordered.at(2).isVideo());
        QVERIFY(ordered.at(3).isVideo());
    }
}

void SortTest::unknownDurationsLastAscFirstDesc()
{
    QList<Entry> rows;
    rows << makeVideo(QStringLiteral("unknown.mp4"), QStringLiteral("iu"), 0, 1, std::nullopt)
         << makeVideo(QStringLiteral("long.mp4"), QStringLiteral("il"), 0, 1, 9000)
         << makeVideo(QStringLiteral("short.mp4"), QStringLiteral("is"), 0, 1, 1000);

    SortSpec ascending; // Duration asc
    ascending.key = SortKey::Duration;
    ascending.direction = SortDirection::Ascending;
    QCOMPARE(namesOf(SortService::apply(rows, ascending)),
             QStringList({QStringLiteral("short.mp4"), QStringLiteral("long.mp4"),
                          QStringLiteral("unknown.mp4")}));

    SortSpec descending;
    descending.key = SortKey::Duration;
    descending.direction = SortDirection::Descending;
    QCOMPARE(namesOf(SortService::apply(rows, descending)),
             QStringList({QStringLiteral("unknown.mp4"), QStringLiteral("long.mp4"),
                          QStringLiteral("short.mp4")}));
}

void SortTest::modifiedSizeWatchedFavoriteKeys()
{
    QList<Entry> rows;
    rows << makeVideo(QStringLiteral("big.mp4"), QStringLiteral("ib"), 0, 300, std::nullopt, false,
                      false)
         << makeVideo(QStringLiteral("small.mp4"), QStringLiteral("ism"), 0, 100, std::nullopt,
                      false, false)
         << makeVideo(QStringLiteral("seen.mp4"), QStringLiteral("isw"), 0, 100, std::nullopt, true,
                      false)
         << makeVideo(QStringLiteral("fav.mp4"), QStringLiteral("if"), 0, 100, std::nullopt, false,
                      true);

    SortSpec size;
    size.key = SortKey::Size;
    size.direction = SortDirection::Descending;
    QCOMPARE(namesOf(SortService::apply(rows, size)).first(), QStringLiteral("big.mp4"));

    SortSpec watched;
    watched.key = SortKey::Watched;
    watched.direction = SortDirection::Descending; // watched first
    QCOMPARE(namesOf(SortService::apply(rows, watched)).first(), QStringLiteral("seen.mp4"));

    SortSpec favorite;
    favorite.key = SortKey::Favorite;
    favorite.direction = SortDirection::Descending; // favorite first
    QCOMPARE(namesOf(SortService::apply(rows, favorite)).first(), QStringLiteral("fav.mp4"));
}

void SortTest::modelReorderEmitsLayoutChanged()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("b.mp4"), QStringLiteral("ib")),
                        makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia")),
                        makeVideo(QStringLiteral("c.mp4"), QStringLiteral("ic"))};
    model.applySnapshot(snapshot);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::NameRole).toString(),
             QStringLiteral("b.mp4"));

    QSignalSpy layout(&model, &QAbstractItemModel::layoutChanged);
    SortSpec spec;
    model.reorder(spec); // Name asc

    QCOMPARE(layout.count(), 1);
    QCOMPARE(namesOf(model.entries()),
             QStringList({QStringLiteral("a.mp4"), QStringLiteral("b.mp4"),
                          QStringLiteral("c.mp4")}));
}

void SortTest::probeResortConsumesCoalescedSignal()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("unknown.mp4"), QStringLiteral("iu"), 100, 1),
                        makeVideo(QStringLiteral("long.mp4"), QStringLiteral("il"), 100, 1),
                        makeVideo(QStringLiteral("short.mp4"), QStringLiteral("is"), 100, 1)};
    model.applySnapshot(snapshot);

    ProbeController probes;
    probes.setModel(&model);
    probes.setImmediateProcessing(true);
    probes.setProber([](const QString &path) {
        ProbeResult result;
        result.durationMs = path.endsWith(QStringLiteral("short.mp4")) ? 1000 : 9000;
        result.width = 64;
        result.height = 48;
        return result;
    });

    SortSpec durationDesc;
    durationDesc.key = SortKey::Duration;
    durationDesc.direction = SortDirection::Descending;

    QObject::connect(&probes, &ProbeController::resortRequested, &model,
                     [&model, &durationDesc]() { model.reorder(durationDesc); });

    // The unknown.mp4 is not probed (probed files get a duration; unknown stays
    // unknown) — only reorder the two known-duration files via a probe before
    // asserting the coalesced resort places the unknown first (descending).
    QSignalSpy resort(&probes, &ProbeController::resortRequested);
    probes.requestVisible(1, 2); // long + short only; unknown.mp4 stays unknown
    QVERIFY(resort.wait(500));

    const QStringList names = namesOf(model.entries());
    // unknown first (descending), then long (9000), then short (1000).
    QCOMPARE(names.first(), QStringLiteral("unknown.mp4"));
}

QTEST_GUILESS_MAIN(SortTest)
#include "test_sort.moc"