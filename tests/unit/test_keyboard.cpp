// Central keyboard routing tests (REMEDIATION-012 / finding D-11).
//
// Verifies the KeyboardFramework contract: the §13.2 window-scoped action table
// is complete, duplicate-free and total (every id dispatches, unknown ids are
// rejected); navigation reaches the BrowserController directly; focused-row file
// actions are emitted with the SelectionState focus index; and §13.3 focus
// restoration returns the right row. Runs offscreen (no display required).

#include <QtTest/QtTest>
#include <QDir>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "controllers/browser_controller.hpp"
#include "controllers/keyboard_framework.hpp"
#include "core/types.hpp"
#include "models/folder_model.hpp"
#include "models/selection_state.hpp"

using namespace qg;

namespace {

Entry makeEntry(const QString& name, const QString& path, bool folder)
{
    Entry entry;
    entry.kind = folder ? EntryKind::Folder : EntryKind::Video;
    entry.name = name;
    entry.path = path;
    entry.identity = QStringLiteral("inode:1:") + name;
    entry.size = 1024;
    entry.mtimeNs = 42;
    return entry;
}

// The §13.2 actions the central router MUST own (navigation, view toggles,
// focused-row file actions, playback). Membership is the contract under test.
const QStringList &requiredIds()
{
    static const QStringList ids = {
        "nav.back", "nav.forward", "nav.parent", "nav.refresh", "nav.home",
        "nav.location", "view.search", "view.order", "view.places", "view.reel",
        "view.density1", "view.density2", "view.density3",
        "item.open", "item.rename", "item.trash", "item.deletePermanent",
        "item.favorite", "item.watched", "item.reveal", "item.properties",
        "playback.playPause", "playback.seekBack", "playback.seekForward",
        "playback.previous", "playback.next", "playback.volumeUp",
        "playback.volumeDown", "playback.mute", "playback.fullscreen",
        "playback.stop",
    };
    return ids;
}

} // namespace

class KeyboardTest : public QObject
{
    Q_OBJECT

private slots:
    void actionTableIsCompleteAndDuplicateFree();
    void unknownIdIsRejected();
    void everyIdDispatchesWithoutAReceiver();
    void navigationRoutesToBrowserController();
    void focusedRowIntentsCarrySelectionFocus();
    void focusRestorePrefersFolderCameFrom();
    void focusRestoreFallsBackToFirstEntry();

private:
    QTemporaryDir m_tmp;
};

void KeyboardTest::actionTableIsCompleteAndDuplicateFree()
{
    KeyboardFramework keyboard;
    const QStringList ids = keyboard.actionIds();

    // No duplicates.
    QSet<QString> seen;
    for (const QString& id : ids) {
        QVERIFY2(!seen.contains(id), qPrintable(QStringLiteral("duplicate id: ") + id));
        seen.insert(id);
    }

    // Covers every required §13.2 window-scoped action.
    for (const QString& id : requiredIds()) {
        QVERIFY2(ids.contains(id), qPrintable(QStringLiteral("missing id: ") + id));
    }

    // Playback jump-to-% covers 0–9.
    for (int d = 0; d <= 9; ++d) {
        QVERIFY(ids.contains(QStringLiteral("playback.jump%1").arg(d)));
    }
}

void KeyboardTest::unknownIdIsRejected()
{
    KeyboardFramework keyboard;
    QVERIFY(keyboard.dispatch(QStringLiteral("nav.back")));
    QVERIFY(!keyboard.dispatch(QStringLiteral("does.not.exist")));
    QVERIFY(!keyboard.dispatch(QString()));
}

void KeyboardTest::everyIdDispatchesWithoutAReceiver()
{
    // With no receivers wired, every known action still reports handled (true)
    // and never crashes — an unbound key would be a programming error.
    KeyboardFramework keyboard;
    for (const QString& id : keyboard.actionIds()) {
        QVERIFY2(keyboard.dispatch(id), qPrintable(id));
    }
}

void KeyboardTest::navigationRoutesToBrowserController()
{
    QVERIFY(m_tmp.isValid());
    QVERIFY(QDir().mkpath(m_tmp.filePath(QStringLiteral("child"))));

    BrowserController browser;
    KeyboardFramework keyboard;
    keyboard.setBrowserController(&browser);

    QVERIFY(browser.navigate(m_tmp.filePath(QStringLiteral("child"))));
    const QString child = browser.currentPath();
    QCOMPARE(child, QDir(m_tmp.filePath(QStringLiteral("child"))).absolutePath());

    // Alt+↑ / Backspace → parent.
    QVERIFY(keyboard.dispatch(QStringLiteral("nav.parent")));
    QCOMPARE(browser.currentPath(), m_tmp.path());

    // Alt+← / Alt+→ traverse the stack the navigation built.
    QVERIFY(keyboard.dispatch(QStringLiteral("nav.back")));
    QCOMPARE(browser.currentPath(), child);
    QVERIFY(keyboard.dispatch(QStringLiteral("nav.forward")));
    QCOMPARE(browser.currentPath(), m_tmp.path());
}

void KeyboardTest::focusedRowIntentsCarrySelectionFocus()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeEntry(QStringLiteral("a.mp4"), QStringLiteral("/v/a.mp4"), false),
                        makeEntry(QStringLiteral("b.mp4"), QStringLiteral("/v/b.mp4"), false),
                        makeEntry(QStringLiteral("c.mp4"), QStringLiteral("/v/c.mp4"), false)};
    model.applySnapshot(snapshot);
    QCOMPARE(model.rowCount(), 3);

    SelectionState selection;
    selection.setFocusIndex(2);

    KeyboardFramework keyboard;
    keyboard.setFolderModel(&model);
    keyboard.setSelectionState(&selection);

    QSignalSpy openSpy(&keyboard, &KeyboardFramework::openRequested);
    QSignalSpy renameSpy(&keyboard, &KeyboardFramework::renameRequested);
    QSignalSpy trashSpy(&keyboard, &KeyboardFramework::trashRequested);
    QSignalSpy permSpy(&keyboard, &KeyboardFramework::permanentDeleteRequested);
    QSignalSpy favSpy(&keyboard, &KeyboardFramework::favoriteToggleRequested);
    QSignalSpy watchSpy(&keyboard, &KeyboardFramework::watchedToggleRequested);
    QSignalSpy revealSpy(&keyboard, &KeyboardFramework::revealRequested);
    QSignalSpy propsSpy(&keyboard, &KeyboardFramework::propertiesRequested);

    keyboard.dispatch(QStringLiteral("item.open"));
    keyboard.dispatch(QStringLiteral("item.rename"));
    keyboard.dispatch(QStringLiteral("item.trash"));
    keyboard.dispatch(QStringLiteral("item.deletePermanent"));
    keyboard.dispatch(QStringLiteral("item.favorite"));
    keyboard.dispatch(QStringLiteral("item.watched"));
    keyboard.dispatch(QStringLiteral("item.reveal"));
    keyboard.dispatch(QStringLiteral("item.properties"));

    QCOMPARE(openSpy.count(), 1);
    QCOMPARE(renameSpy.count(), 1);
    QCOMPARE(trashSpy.count(), 1);
    QCOMPARE(permSpy.count(), 1);
    QCOMPARE(favSpy.count(), 1);
    QCOMPARE(watchSpy.count(), 1);
    QCOMPARE(revealSpy.count(), 1);
    QCOMPARE(propsSpy.count(), 1);

    // Every intent carries the focused row (2), never a guess.
    for (QSignalSpy* spy : {&openSpy, &renameSpy, &trashSpy, &permSpy,
                            &favSpy, &watchSpy, &revealSpy, &propsSpy}) {
        QCOMPARE(spy->at(0).at(0).toInt(), 2);
    }
}

void KeyboardTest::focusRestorePrefersFolderCameFrom()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeEntry(QStringLiteral("sub"), QStringLiteral("/parent/sub"), true),
                        makeEntry(QStringLiteral("x.mp4"), QStringLiteral("/parent/x.mp4"), false)};
    model.applySnapshot(snapshot);

    KeyboardFramework keyboard;
    keyboard.setFolderModel(&model);

    // Going up from "sub" (which is now a row in the parent listing) → that row.
    QCOMPARE(keyboard.focusRestoreIndexFor(QStringLiteral("/parent/sub")), 0);
    // A previous path we do not see → first entry.
    QCOMPARE(keyboard.focusRestoreIndexFor(QStringLiteral("/elsewhere")), 0);
}

void KeyboardTest::focusRestoreFallsBackToFirstEntry()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeEntry(QStringLiteral("x.mp4"), QStringLiteral("/d/x.mp4"), false)};
    model.applySnapshot(snapshot);

    KeyboardFramework keyboard;
    keyboard.setFolderModel(&model);

    // Entering a folder with no "came-from" hint → first entry.
    QCOMPARE(keyboard.focusRestoreIndexFor(QString()), 0);
}

QTEST_MAIN(KeyboardTest)
#include "test_keyboard.moc"
