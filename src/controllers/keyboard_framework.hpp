#pragma once

// Central keyboard routing (DESIGN.md §13.2 shortcut map, §13.3 focus movement).
//
// One registry and one router for every *window-scoped* shortcut in the §13.2
// map. The QML shell declares exactly one `Shortcut` per binding (the
// presentation of the map) and forwards every activation here via `dispatch(id)`;
// this class owns the routing decision, so the map's behaviour lives in one
// place instead of being duplicated across PathBar/Main/ContentView.
//
// Receivers:
//   - navigation and playback hit the composed controllers directly
//     (BrowserController, PlaybackManager/PlaybackController);
//   - shell-owned surfaces (Path Bar entry/search/order, Places, Reel, density,
//     the inline rename editor, the context dialogs) are reached through the
//     intent signals below, which Main.qml routes to the owning component.
//
// Text-entry suppression is enforced by the Shortcuts themselves (each is
// disabled while the shell's `textEditing` is true), so `dispatch()` is only ever
// reached from an enabled, non-editor context.
//
// Selection / range / type-ahead keystrokes are deliberately NOT here: they are
// the ContentView + SelectionState concern owned by the selection-integration
// task (REMEDIATION-013). This class routes the window-scoped map only — never a
// second selection model.

#include <QObject>
#include <QString>
#include <QStringList>

namespace qg {
class BrowserController;
class FolderModel;
}

class PlaybackController;
class PlaybackManager;
class SelectionState;

class KeyboardFramework : public QObject {
    Q_OBJECT
public:
    explicit KeyboardFramework(QObject *parent = nullptr);

    // Non-owning wiring by the composition root (CoreServices).
    void setBrowserController(qg::BrowserController *controller) { browser_ = controller; }
    void setFolderModel(qg::FolderModel *model) { folderModel_ = model; }
    void setSelectionState(SelectionState *selection) { selection_ = selection; }
    void setPlaybackController(PlaybackController *controller) { playbackController_ = controller; }
    void setPlaybackManager(PlaybackManager *manager) { playbackManager_ = manager; }

    // The canonical §13.2 window-scoped action ids in one place. A test asserts
    // this covers the map exactly once (no duplicates, no omissions).
    Q_INVOKABLE QStringList actionIds() const;

    // Single routing entry point. Returns true when `actionId` is a known action
    // (whether or not a receiver was available at the time), false for an unknown
    // id — an unbound key is a programming error, not a silent no-op.
    Q_INVOKABLE bool dispatch(const QString &actionId);

    // DESIGN §13.3 focus restoration after navigation: entering a folder focuses
    // the first entry; going up focuses the entry whose path is `previousPath`
    // (the folder just left). Returns the row to focus, or -1 when empty.
    Q_INVOKABLE int focusRestoreIndexFor(const QString &previousPath) const;

signals:
    // Shell-owned surfaces (Main.qml routes each to the component that owns it).
    void locationFocusRequested();      // Ctrl+L  → Path Bar entry
    void searchFocusRequested();        // Ctrl+F  → Path Bar search morph
    void orderMenuRequested();          // Ctrl+O  → Path Bar order menu
    void placesToggleRequested();       // F9      → Places sidebar
    void reelToggleRequested();         // Ctrl+R  → Reel panel
    void densityRequested(int density); // Ctrl+1/2/3 → density 0/1/2

    // Focused-row intents (the shell owns the inline editor and the dialogs).
    void openRequested(int index);            // Enter
    void renameRequested(int index);          // F2
    void trashRequested(int index);           // Delete
    void permanentDeleteRequested(int index); // Shift+Delete (confirm)
    void favoriteToggleRequested(int index);  // Ctrl+Shift+F
    void watchedToggleRequested(int index);   // Ctrl+Shift+W
    void revealRequested(int index);          // Ctrl+Shift+R
    void propertiesRequested(int index);      // Alt+Enter

private:
    int focusedRow() const;

    qg::BrowserController *browser_ = nullptr;
    qg::FolderModel *folderModel_ = nullptr;
    SelectionState *selection_ = nullptr;
    PlaybackController *playbackController_ = nullptr;
    PlaybackManager *playbackManager_ = nullptr;
};
