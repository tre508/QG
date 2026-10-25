---
type: root-cause-analysis
project: QG
created: 2026-10-22
updated: 2026-10-22
status: fixed
tags:
  - runtime
  - launch
  - gsettings
  - ffmpeg
  - qml
  - libmpv
  - gpu
aliases:
  - fix-runtime-002
  - launch-warning-investigation
source:
  - HERMES AGENT — FIX-RUNTIME-002 (user-supplied launch log)
  - src/playback/playback_session.hpp
  - src/media/probe.cpp
  - scripts/fix-xsettings-compat.sh
  - qml/QG/playback/Stage.qml
  - qml/QG/Main.qml
  - src/playback/mpv_backend.cpp
---

# FIX-RUNTIME-002 — Launch Warning Root-Cause Investigation

Related: [[Current-State]] · [[Known-Issues]] · [[Deferred-Test-Register]] · [[startup-glib-gio-settings-crash]]

## Symptom

Application `./build/src/qg` reaches its main window but emits these warning categories at startup:

**A. GLib-GIO GSettings CRITICALs:**
```
g_settings_get_enum() called on key 'antialiasing' which is not associated with an enumerated type
g_settings_get_enum() called on key 'hinting' which is not associated with an enumerated type
g_settings_get_enum() called on key 'rgba-order' which is not associated with an enumerated type
```

**B. FFmpeg media probe warnings:**
```
Could not find codec parameters for stream 0
Video: h264, none, 1280x720
unspecified pixel format
Consider increasing: analyzeduration (0) probesize (5000000)
```

**C. QML Stage binding failures:**
```
Stage.qml:251:9: Unable to assign [undefined] to double
Stage.qml:252:9: Unable to assign [undefined] to double
Stage.qml:253:9: Unable to assign [undefined] to double
Stage.qml:254:9: Unable to assign [undefined] to bool
Stage.qml:255:9: Unable to assign [undefined] to bool
```

**D. QML Main binding failures:**
```
Main.qml:717:21: Unable to assign [undefined] to QString
Main.qml:720:21: Unable to assign [undefined] to QString
```

**E. Hardware-acceleration backend messages:**
```
Cannot load libcuda.so.1
Failed to open VDPAU backend libvdpau_nvidia.so: cannot open shared object file
```

## Environment

- OS: Ubuntu Linux, X11/xcb (Qt xcb platform plugin)
- Qt: 6.10.2
- Qt Quick graphics API: OpenGL
- Build: Release (ninja), `build/` directory
- libmpv: C API, `mpv_render_context` + Qt Quick scene graph
- FFmpeg: libavformat/libavcodec/libswscale (in-process probe + thumbnail extraction)

## Investigation Log

| Date | Action | Result |
|------|--------|--------|
| 2026-10-22 | Grep `src/` and `qml/` for `g_settings_get_enum`, `GSettings`, `antialiasing`, `xsettings` | **0 matches** — QG contains no GSettings access code |
| 2026-10-22 | Inspect system schema `/usr/share/glib-2.0/schemas/org.gnome.settings-daemon.plugins.xsettings.gschema.xml` | Non-deprecated schema has only `disabled-gtk-modules`, `enabled-gtk-modules`, `overrides`. `antialiasing`/`hinting`/`rgba-order` live in the **deprecated** schema as `enum` types (`GsdFontAntialiasingMode`, `GsdFontHinting`, `GsdFontRgbaOrder`) |
| 2026-10-22 | Inspect QG shim `scripts/fix-xsettings-compat.sh` | Shim added the three keys as `type="i"` (int32) with defaults 0/1/5. `g_settings_get_enum()` requires `G_SETTINGS_TYPE_ENUM`, so int32 keys trigger CRITICAL — not abort, but noisy |
| 2026-10-22 | Inspect `src/playback/playback_session.hpp` | **Zero `Q_PROPERTY` declarations.** Class registered via `qmlRegisterUncreatableType<PlaybackSession>` (composition.cpp:61). QML reading `session.position/duration/volume/muted/seekable/state/path/errorMessage` receives `undefined` |
| 2026-10-22 | Inspect `src/media/probe.cpp` | `avformat_open_input()` called with `nullptr` options — no explicit `probesize`/`analyzeduration`. FFmpeg default behaviour; warning is informational for pixel format which QG does not require |
| 2026-10-22 | Inspect `src/playback/mpv_backend.cpp` line 50 | `mpv_set_option_string(handle_, "hwdec", "auto-safe")` — mpv auto-detects hardware backends, logs failures for missing CUDA/VDPAU, falls back to software. Expected behaviour, not a defect |
| 2026-10-22 | Cross-component review | Warnings are independent: GSettings (external GTK theme), FFmpeg (probe informational), QML bindings (missing Q_PROPERTY), CUDA/VDPAU (mpv backend discovery). No shared upstream cause |

## Root Cause

| Warning category | Confirmed cause |
|------------------|-----------------|
| GSettings antialiasing/hinting/rgba-order CRITICAL | Qt's `libqgtk3.so` platform theme calls `g_settings_get_enum()` on keys that QG's shim typed as `int32` (`type="i"`). `g_settings_get_enum()` strictly requires `G_SETTINGS_TYPE_ENUM`. QG itself has no GSettings code. |
| FFmpeg "unspecified pixel format" | `probeMedia()` calls `avformat_open_input()` without probe options. FFmpeg reports missing codec parameters/pixel format. QG reads width/height/duration from `AVCodecParameters` directly — pixel format is not required. Warning is informational. |
| Stage.qml lines 251–255 undefined bindings | `PlaybackSession` exposes plain accessors (`position()`, `duration()`, etc.) but no `Q_PROPERTY`. QML reads return `undefined`, which cannot be assigned to typed TransportBar properties (`double`, `bool`). |
| Main.qml lines 717, 720 undefined bindings | Same root cause — `path` and `errorMessage` not exposed as `Q_PROPERTY` on `PlaybackSession`. |
| CUDA / VDPAU messages | `hwdec = "auto-safe"` causes mpv to probe multiple hardware backends; missing CUDA/VDPAU libraries produce log messages but mpv falls back to software decoding. Expected behaviour on non-NVIDIA systems. |

## Fix

### 1. `src/playback/playback_session.hpp` — Add `Q_PROPERTY` declarations

Added `Q_PROPERTY` for every session field read from QML:

- `identity` (QString), `path` (QString), `duration` (double), `position` (double),
  `state` (State enum), `volume` (double), `muted` (bool), `seekable` (bool),
  `errorMessage` (QString)

Each maps to the existing getter/setter/signal pair. Getter/setter methods and NOTIFY signals already existed; only the `Q_PROPERTY` declarations were missing. This also fixes a latent bug in `Main.qml:115` (`window.activeSession.state !== 0` was `undefined !== 0` = always true).

### 2. `scripts/fix-xsettings-compat.sh` — Correct shim key types

Changed the three shimmed keys from `type="i"` (int32) to `type="e"` (enum) with proper `<enum>` references matching the deprecated schema's enum types:

- `antialiasing` → `type="e"`, enum `org.gnome.settings-daemon.GsdFontAntialiasingMode`, default `'grayscale'`
- `hinting` → `type="e"`, enum `org.gnome.settings-daemon.GsdFontHinting`, default `'slight'`
- `rgba-order` → `type="e"`, enum `org.gnome.settings-daemon.GsdFontRgbaOrder`, default `'rgb'`

Defaults corrected to match gnome-settings-daemon (previous `0`/`5` were wrong for antialiasing/rgba-order). The shim already copies `org.gnome.settings-daemon.enums.xml` which defines these enum types.

Note: existing installations with the old `type="i"` shim must run `--remove` then install to pick up the type correction.

### 3. `src/media/probe.cpp` — Explicit bounded probe options

Added `AVDictionary` with `probesize=5000000` and `analyzeduration=5000000` to `avformat_open_input()`, making the probe budget explicit rather than relying on implicit defaults. Matches FFmpeg defaults; does not increase probing cost. Pixel format remains optional — QG does not require it.

## What was NOT changed (and why)

- **CUDA / VDPAU messages**: Not a defect. `hwdec=auto-safe` is the correct policy — try hardware, fall back to software. No NVIDIA libraries are required. Documented as expected backend discovery behaviour.
- **No system schema modification**: The shim is user-level (`$XDG_DATA_HOME/glib-2.0/schemas`), not `/usr/share`. No system packages, drivers, or GNOME schemas were touched.
- **No libmpv backend changes**: One mpv handle, `vo=libmpv`, `hwdec=auto-safe` — all consistent with DESIGN.md §19 and DEC-007/DEC-008.

## Static Review

- C++: `Q_PROPERTY` READ/WRITE/NOTIFY signatures match existing accessors and signals; enum `Q_ENUM(State)` already registered; no new includes needed; header/source consistent.
- QML: Stage.qml and Main.qml bindings now resolve to real Q_PROPERTY values with valid defaults (`0`, `0`, `0`, `1.0`, `false`, `false`, `Idle`, `""`). Fallback values in ternary expressions (`stage.session ? ... : 0`) are type-correct.
- FFmpeg: `av_dict_set` + `avformat_open_input` with `&opts` — FFmpeg consumes the dictionary per API contract; no `av_dict_free()` call to avoid double-free. Error paths unchanged.
- Threading: No change to worker pool, TaskToken, or cancellation behaviour. Probe remains off GUI thread.
- Resources: No new allocations; `AVDictionary` is stack-managed via FFmpeg consumption.

## Hypotheses Still Requiring Runtime Verification

- GSettings CRITICALs eliminated at startup with the enum-typed shim (requires real DISPLAY session, not offscreen).
- FFmpeg probe warning frequency reduced (pixel format warning may still appear for specific H.264 streams where codec parameters are genuinely incomplete — harmless).
- QML binding failures eliminated (Stage.qml and Main.qml no longer emit "Unable to assign [undefined]").
- CUDA/VDPAU messages persist as expected on systems without NVIDIA libraries (documented, not a fix target).

## Deferred Validation

Registered in [[Deferred-Test-Register]] as FIX-RUNTIME-002:

- Startup emits no GSettings `g_settings_get_enum` CRITICALs
- FFmpeg probe completes for representative MP4/MKV files; width/height/duration correct
- Stage.qml and Main.qml emit no undefined-binding warnings
- Stage opens/closes; playback state updates; navigation state correct
- Hardware decoding falls back to software when CUDA/VDPAU unavailable
- Application startup, navigation, media enumeration, inline playback all functional

Status: **IMPLEMENTED / STATICALLY REVIEWED / TESTING DEFERRED**

## Related Issues

- [[startup-glib-gio-settings-crash]] — Prior GSettings abort fix (missing key → shim). This note addresses the follow-up CRITICAL (wrong key type).
- DEC-007 (one mpv instance), DEC-008 (one playback session), DEC-036 (bounded media probe)

## References

- Source: `src/playback/playback_session.hpp:9-23`
- Source: `src/media/probe.cpp:57-85`
- Source: `scripts/fix-xsettings-compat.sh:108-122`
- QML: `qml/QG/playback/Stage.qml:250-256`
- QML: `qml/QG/Main.qml:711-720, 111-115`
- DESIGN.md §19 (playback), §18.6 (media probe), §8.2–§8.3 (transport)
- GLib: `g_settings_get_enum()` requires `G_SETTINGS_TYPE_ENUM`
