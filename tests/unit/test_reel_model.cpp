// ReelModel tests (REMEDIATION-007 / D-23).
//
// The Reel is the ordered playback-context projection shown on the right. These
// tests pin the display indexing, the current-item highlight, identity-keyed
// minimal updates (rename / state) and single-row removal with renumbering.

#include <QtTest/QtTest>

#include "models/reel_model.hpp"

namespace {

ReelEntry makeEntry(const QString& identity, const QString& name,
                    qint64 durationMs = 60000, bool favorite = false,
                    bool watched = false, double progress = -1.0)
{
    ReelEntry e;
    e.identity = identity;
    e.name = name;
    e.path = QStringLiteral("/videos/") + name;
    e.durationMs = durationMs;
    e.sizeBytes = 1000;
    e.favorite = favorite;
    e.watched = watched;
    e.progress = progress;
    return e;
}

} // namespace

class ReelModelTest : public QObject
{
    Q_OBJECT

private slots:
    void entriesGetDisplayIndexAndRoles();
    void currentIndexClampsAndSignals();
    void currentByIdentity();
    void updateEntryIsSingleRow();
    void removeByIdentityRenumbers();
    void summaryReflectsPosition();
};

void ReelModelTest::entriesGetDisplayIndexAndRoles()
{
    ReelModel model;
    model.setEntries({makeEntry(QStringLiteral("i1"), QStringLiteral("a.mp4"), -1),
                      makeEntry(QStringLiteral("i2"), QStringLiteral("b.mp4"),
                                125000, true, false, 0.25)});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.data(model.index(0, 0), ReelModel::IndexRole).toInt(), 1);
    QCOMPARE(model.data(model.index(1, 0), ReelModel::IndexRole).toInt(), 2);
    QCOMPARE(model.data(model.index(1, 0), ReelModel::NameRole).toString(),
             QStringLiteral("b.mp4"));
    QCOMPARE(model.data(model.index(1, 0), ReelModel::IdentityRole).toString(),
             QStringLiteral("i2"));
    QCOMPARE(model.data(model.index(1, 0), ReelModel::DurationMsRole).toLongLong(), 125000);
    QCOMPARE(model.data(model.index(1, 0), ReelModel::FavoriteRole).toBool(), true);
    QCOMPARE(model.data(model.index(1, 0), ReelModel::ProgressRole).toDouble(), 0.25);

    // Unknown duration is absent (not zero) — the view shows the placeholder.
    QVERIFY(!model.data(model.index(0, 0), ReelModel::DurationMsRole).isValid());

    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.value(ReelModel::NameRole), QByteArrayLiteral("reelName"));
    QCOMPARE(roles.value(ReelModel::IsCurrentRole), QByteArrayLiteral("reelIsCurrent"));
}

void ReelModelTest::currentIndexClampsAndSignals()
{
    ReelModel model;
    model.setEntries({makeEntry(QStringLiteral("i1"), QStringLiteral("a.mp4")),
                      makeEntry(QStringLiteral("i2"), QStringLiteral("b.mp4"))});

    // Spies after the initial reset so counts reflect only the highlight change.
    QSignalSpy spy(&model, &ReelModel::currentIndexChanged);
    QSignalSpy rows(&model, &QAbstractItemModel::dataChanged);

    model.setCurrentIndex(1);
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(rows.count() >= 1, true);
    QCOMPARE(model.data(model.index(1, 0), ReelModel::IsCurrentRole).toBool(), true);
    QCOMPARE(model.data(model.index(0, 0), ReelModel::IsCurrentRole).toBool(), false);

    // Out-of-range clears the highlight instead of holding a stale index.
    model.setCurrentIndex(99);
    QCOMPARE(model.currentIndex(), -1);
}

void ReelModelTest::currentByIdentity()
{
    ReelModel model;
    model.setEntries({makeEntry(QStringLiteral("i1"), QStringLiteral("a.mp4")),
                      makeEntry(QStringLiteral("i2"), QStringLiteral("b.mp4"))});

    QCOMPARE(model.setCurrentByIdentity(QStringLiteral("i2")), true);
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.indexOfIdentity(QStringLiteral("i2")), 1);
    QCOMPARE(model.identityAt(0), QStringLiteral("i1"));

    QCOMPARE(model.setCurrentByIdentity(QStringLiteral("ghost")), false);
    QCOMPARE(model.currentIndex(), 1); // unchanged
}

void ReelModelTest::updateEntryIsSingleRow()
{
    ReelModel model;
    model.setEntries({makeEntry(QStringLiteral("i1"), QStringLiteral("a.mp4")),
                      makeEntry(QStringLiteral("i2"), QStringLiteral("b.mp4")),
                      makeEntry(QStringLiteral("i3"), QStringLiteral("c.mp4"))});

    QSignalSpy rows(&model, &QAbstractItemModel::dataChanged);
    const bool ok = model.updateEntry(makeEntry(QStringLiteral("i2"),
                                                QStringLiteral("renamed.mp4"),
                                                90000, true, true, 0.5));
    QCOMPARE(ok, true);
    // Minimal: exactly one row touched.
    QCOMPARE(rows.count(), 1);
    QCOMPARE(rows.at(0).at(0).toModelIndex().row(), 1);
    QCOMPARE(rows.at(0).at(1).toModelIndex().row(), 1);
    QCOMPARE(model.data(model.index(1, 0), ReelModel::NameRole).toString(),
             QStringLiteral("renamed.mp4"));
    QCOMPARE(model.data(model.index(1, 0), ReelModel::FavoriteRole).toBool(), true);
    QCOMPARE(model.data(model.index(0, 0), ReelModel::NameRole).toString(),
             QStringLiteral("a.mp4")); // untouched

    QCOMPARE(model.updateEntry(makeEntry(QStringLiteral("ghost"), QStringLiteral("x"))), false);
    QCOMPARE(rows.count(), 1);
}

void ReelModelTest::removeByIdentityRenumbers()
{
    ReelModel model;
    model.setEntries({makeEntry(QStringLiteral("i1"), QStringLiteral("a.mp4")),
                      makeEntry(QStringLiteral("i2"), QStringLiteral("b.mp4")),
                      makeEntry(QStringLiteral("i3"), QStringLiteral("c.mp4"))});
    model.setCurrentIndex(2); // c.mp4

    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy count(&model, &ReelModel::countChanged);

    QCOMPARE(model.removeByIdentity(QStringLiteral("i1")), true);
    QCOMPARE(removed.count(), 1);
    QCOMPARE(model.count(), 2);
    QCOMPARE(count.count(), 1);
    // Tail renumbered and the current item tracked its shift.
    QCOMPARE(model.data(model.index(0, 0), ReelModel::IndexRole).toInt(), 1);
    QCOMPARE(model.data(model.index(0, 0), ReelModel::NameRole).toString(),
             QStringLiteral("b.mp4"));
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.identityAt(1), QStringLiteral("i3"));

    // Removing the current item clears the highlight (no stale index).
    QCOMPARE(model.removeByIdentity(QStringLiteral("i3")), true);
    QCOMPARE(model.currentIndex(), -1);
}

void ReelModelTest::summaryReflectsPosition()
{
    ReelModel model;
    model.setEntries({makeEntry(QStringLiteral("i1"), QStringLiteral("a.mp4")),
                      makeEntry(QStringLiteral("i2"), QStringLiteral("b.mp4")),
                      makeEntry(QStringLiteral("i3"), QStringLiteral("c.mp4"))});
    model.setCurrentIndex(1);
    QCOMPARE(model.summary(), QStringLiteral("2 / 3"));
    model.setCurrentIndex(-1);
    QCOMPARE(model.summary(), QStringLiteral("0 / 3"));
}

QTEST_MAIN(ReelModelTest)
#include "test_reel_model.moc"
