---
type: resolved-issue
project: QG
created: 2026-10-21
updated: 2026-10-21
status: resolved
tags:
  - startup
  - crash
  - gsettings
  - glib-gio-error
  - platform-theme
source:
  - FIX-STARTUP-001..008
  - GUIDE_STATE.md
  - 05-Debugging/Root-Cause-Analysis/startup-glib-gio-settings-crash.md
---

# Resolved Issue: Startup Crash (GLib-GIO GSettings Schema)

## Summary

Reported fatal startup crash: `GLib-GIO-ERROR: Settings schema 'org.gnome.settings-daemon.plugins.xsettings' does not contain a key named 'antialiasing'`, followed by `Aborted (core dumped)`.

## Resolution

**No code change required.** The crash does not reproduce in this environment. QG launches successfully (`DISPLAY=:0 ./build/src/qg`), loads QML, and runs cleanly. 0 crash markers in startup log.

## Root Cause

External to QG: Qt's GTK3 platform theme plugin (`libqgtk3.so`) reads `org.gnome.settings-daemon.plugins.xsettings/antialiasing` at theme init. The key is absent from the installed GNOME settings-daemon schema on this host (only `disabled-gtk-modules`, `enabled-gtk-modules`, `overrides` present). QG contains no GSettings access code (0 matches in `src/` and `qml/`).

## Verification

- Build: `cmake --build build` — PASS
- Startup: QG launches, QML loads, no GLib-GIO error — PASS
- Tests: `ctest --test-dir build` — 45/46 PASS (1 pre-existing `qg_qml` infra failure unrelated)

## Deployment Note

If a deployment forces `QT_QPA_PLATFORMTHEME=gtk3`, test startup on that host. If the crash recurs there, either do not force the GTK theme or ensure the settings-daemon schema includes the expected keys. QG does not require GTK theme integration.
