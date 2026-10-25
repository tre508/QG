---
type: architecture
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - architecture
  - playback
  - mpv
  - render-api
source:
  - DESIGN.md §19
  - ARCHITECTURE_CURRENT.md
  - src/playback/
---

# QG Playback Architecture

## Overview

Single mpv handle, Render API integrated with Qt Quick OpenGL scene graph, in-window Cinema (no surface reparenting).

## Component Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│ PLAYBACK STACK                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  PlaybackController (GUI)                                          │
│    │                                                                │
│    ├─► Builds PlaybackContext from FolderModel (sort + filter)    │
│    ├─► Owns Stage/Cinema layout state (reelVisible, cinemaActive) │
│    └─► Bridges UI intent → PlaybackManager                         │
│            │                                                        │
│            ▼                                                        │
│  PlaybackManager (Singleton, GUI)                                  │
│    │                                                                │
│    ├─► Owns single MpvBackend (lazy creation)                     │
│    ├─► Owns PlaybackSession (observable state)                    │
│    ├─► Manages PlaybackContext (ordered videos + filter)          │
│    ├─► 5s position tick → StateController.recordPosition()        │
│    ├─► Resume policy: Always/Ask/Never (injected loader)          │
│    └─► Deferred loadfile until renderContextReady                 │
│            │                                                        │
│            ├─► MpvBackend (GUI) — SOLE mpv handle owner           │
│            │     │                                                  │
│            │     ├─► mpv_create() with LC_NUMERIC=C               │
│            │     ├─► mpv_render_context_create (OpenGL)           │
│            │     ├─► Property observation: pause, duration,       │
│            │     │     time-pos, seekable, eof-reached,           │
│            │     │     idle-active, volume, mute                  │
│            │     ├─► wakeupCallback → MpvIdleEvent → Qt thread    │
│            │     └─► mpv_command for playback control             │
│            │                                                        │
│            └─► VideoSurfaceItem (QQuickItem + QSGRenderNode)     │
│                  │                                                  │
│                  ├─► Renders on Qt Scene Graph RENDER THREAD      │
│                  ├─► mpv_render_context_render() into Qt FBO      │
│                  ├─► renderContextReady() handshake               │
│                  └─► Presents mpv frames via OpenGL               │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## Key Classes

### MpvBackend (`src/playback/mpv_backend.cpp`)

**Sole owner of mpv handle** — no other component creates/destroys mpv.

```cpp
// Creation (lazy, on first play)
void createHandle() {
    std::setlocale(LC_NUMERIC, "C");  // REQUIRED: mpv_create() returns NULL otherwise
    handle_ = mpv_create();
    mpv_set_option_string(handle_, "vo", "libmpv");
    mpv_set_option_string(handle_, "hwdec", "auto-safe");
    mpv_set_wakeup_callback(handle_, wakeupCallback, this);
    mpv_initialize(handle_);
    
    // Observe properties → emit propertyChanged(name, value)
    mpv_observe_property(handle_, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, "time-pos", MPV_FORMAT_DOUBLE);
    // ... seekable, eof-reached, idle-active, volume, mute
}

// Render context (created when VideoSurfaceItem first paints)
mpv_render_context* renderContext() {
    if (!renderCtxCreated_) {
        mpv_opengl_init_params glParams = { .get_proc_address = qgMpvGlGetProcAddress };
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glParams},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        mpv_render_context_create(&renderCtx_, handle_, params);
        renderCtxCreated_ = true;
    }
    return renderCtx_;
}

// Wakeup callback (mpv thread → Qt thread)
static void wakeupCallback(void* userData) {
    auto* self = static_cast<MpvBackend*>(userData);
    QCoreApplication::postEvent(self, new MpvIdleEvent());
}

// Event loop processes mpv events on Qt thread
bool event(QEvent* ev) {
    if (ev->type() == MpvIdleEvent::kType) {
        while (handle_) {
            mpv_event* e = mpv_wait_event(handle_, 0);
            if (e->event_id == MPV_EVENT_NONE) break;
            // Handle PROPERTY_CHANGE, FILE_LOADED, END_FILE
            emit propertyChanged(name, value);
        }
        return true;
    }
    return QObject::event(ev);
}
```

### VideoSurfaceItem (`src/playback/video_surface.cpp`)

**Only place mpv frames enter the scene graph.**

```cpp
class VideoSurfaceItem : public QQuickItem {
    // QSGRenderNode subclass
    class VideoRenderNode : public QSGRenderNode {
        void render() override {
            // Called on RENDER THREAD
            mpv_render_context_render(backend->renderContext(), &params);
        }
    };
    
    // Handshake: first paint → renderContextReady()
    void paint(QPainter*) override { /* no-op, uses QSGRenderNode */ }
};
```

### PlaybackManager (`src/playback/playback_manager.cpp`)

**Process singleton** — orchestrates the single playback session.

```cpp
// Single handle policy
static PlaybackManager* instance() { static PlaybackManager mgr; return &mgr; }

// Deferred load until render context ready
void openWithIdentity(path, identity, resumeStart) {
    flushPosition();  // Persist outgoing session
    pendingLoad_ = {path, identity, resumeStart};
    if (backend->renderContext()) {
        doLoad();  // mpv_command("loadfile", path, "start=" + resumeStart)
    }
    // Else: VideoSurfaceItem calls renderContextReady() → doLoad()
}

// Position persistence (throttled 5s + on pause/stop/close/EOF)
void onPositionTick(identity, seconds) {
    if (shouldPersist(identity, seconds)) {
        positionLoader_(identity);  // Injected: reads MediaStateRepository
    }
}

// Resume policy
qint64 resumeStartFor(identity) {
    // Eligibility: >5s, <duration-10s, not watched
    // Policy: Always → return position; Ask → emit resumePromptRequested; Never → 0
}
```

### PlaybackSession (`src/playback/playback_session.cpp`)

**Observable state** — QML binds to this, never polls.

```cpp
// Properties (all NOTIFY signals)
identity, path, duration, position, state (Idle/Loading/Playing/Paused/Ended/Error)
volume (0..1), muted, seekable, errorMessage
```

### PlaybackContext (`src/playback/playback_context.cpp`)

**Ordered playback sequence** derived from canonical FolderModel.

```cpp
void build(FolderModel* model, SortSpec sort, ReelFilter filter) {
    // Walk model rows in order, collect videos matching filter
    // Store as vector<Entry> with identity
    // Find currentIndex by identity
}

// Navigation
bool step(int delta) {  // ±1 for prev/next
    // Stops at ends (emits endReached)
    // Honors sort + filter
}
```

### PlaybackController (`src/controllers/playback_controller.cpp`)

**UI bridge** — connects QML intent to PlaybackManager.

```cpp
void play(path, identity) {
    buildContextFromFolderModel();  // Uses active sort + Reel filter
    playbackManager->openWithIdentity(path, identity, resumeStart);
    emit playbackStarted(path);
    reelVisible = true;
}

void previous()/next() {
    if (playbackContext->step(±1)) {
        play(playbackContext->currentPath(), playbackContext->currentIdentity());
    } else {
        emit endOfFolder();  // Toast: "End of folder"
    }
}

void toggleCinema() { cinemaActive = !cinemaActive; }
void exitCinema() { cinemaActive = false; }
void stop() { playbackManager->stop(); reelVisible = false; }
```

## In-Window Cinema (DEC-010)

```
Main.qml (ApplicationWindow)
    │
    ├─► readonly property bool cinema = playbackController.cinemaActive
    │
    ├─► Regions hidden when cinema=true:
    │     PathBar, ContextStrip, Sidebar, Reel, StatusStrip (implicitHeight: 0, visible: false)
    │
    └─► Stage.cinema = true
          │
          ├─► Stage fills entire window (anchors.fill: parent)
          ├─► TransportBar + top bar fade in/out
          ├─► Cursor hides after 2s idle (cinemaOverlay + cursorIdleTimer)
          └─► Any input → wakeControls()
```

**Critical**: Same mpv session continues — no reload, no rebuffer, no surface reparenting.

## Transport State Flow

```
mpv property change (pause, duration, time-pos, seekable, eof-reached, idle-active, volume, mute)
    │
    ▼
MpvBackend.propertyChanged(name, value) [Qt thread via MpvIdleEvent]
    │
    ▼
PlaybackManager.updateSessionFromProperty(name, value)
    │
    ▼
PlaybackSession emits NOTIFY signals (durationChanged, positionChanged, stateChanged, ...)
    │
    ▼
QML binds: Stage.session.duration, TransportBar.session.position, etc.
```

**No polling** — all state observed via mpv property callbacks.

## Resume Logic

```cpp
// Stored position keyed by identity (survives rename via paths table)
qint64 MediaStateRepository::positionMs(identity)

// Eligibility (DEC-009):
//   position > 5s AND position < duration - 10s AND not watched

// Resume policy (SettingsRepository.resumePolicy):
//   Always  → auto-resume
//   Ask     → emit resumePromptRequested (QML shows dialog)
//   Never   → start from 0

// Implementation:
qint64 PlaybackManager::resumeStartFor(identity) {
    if (policy == Never) return 0;
    qint64 pos = positionLoader(identity);  // Injected from MediaStateRepository
    if (!eligible(pos)) return 0;
    if (policy == Ask) {
        emit resumePromptRequested(identity, pos);
        return -1;  // Wait for user response
    }
    return pos;
}
```

## Error Handling

| Error | Source | Handling |
|-------|--------|----------|
| `mpv_create()` NULL | `LC_NUMERIC` not C | Fixed: `setlocale(LC_NUMERIC, "C")` before create |
| No render context | Stage not painted yet | Defer `loadfile` until `renderContextReady()` |
| `loadfile` fails | Unsupported format, missing file | `PlaybackSession.errorMessage` → StateOverlay |
| Hardware decode fail | VDPAU/VA-API unavailable | `hwdec=auto-safe` falls back to software |
| EOF reached | `eof-reached` property | `PlaybackController.next()` or stop at end |

## Testing

| Test | Coverage |
|------|----------|
| `qg_playback_lifecycle` | open/play/pause/stop/seek/EOF |
| `qg_transport_state` | property observation → session state |
| `qg_video_render` | render context creation, frame presentation |
| `qg_playback_context` | ordered context, prev/next, filter |
| `qg_reel_model` / `qg_reel_wiring` | Reel projection, current highlight |
| `qg_resume_policy` | Always/Ask/Never, eligibility |
| `qg_resume_playback` | position persistence, resume on reopen |

All 9 playback tests PASS (real mpv + Xvfb GL).