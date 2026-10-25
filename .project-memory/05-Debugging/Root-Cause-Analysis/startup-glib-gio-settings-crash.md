---
type: root-cause-analysis
project: QG
created: 2026-10-21
updated: 2026-10-22
status: resolved
tags:
  - startup
  - crash
  - gsettings
  - glib
  - gtk
  - platform-theme
  - environment
aliases:
  - xsettings-crash
source:
  - FIX-STARTUP-001..008
  - 2026-10-22 re-diagnosis
  - scripts/fix-xsettings-compat.sh
---

# Root Cause: GLib-GIO GSettings Startup Abort (`xsettings` / `antialiasing`)

Related: [[Current-State]] · [[Known-Issues]] · [[Build-Commands]]

## Symptom

    (qg:1517563): GLib-GIO-ERROR **: 01:57:54.523: Settings schema
    'org.gnome.settings-daemon.plugins.xsettings' does not contain a key
    named 'antialiasing'
    Aborted (core dumped)   DISPLAY=:0 ./build/src/qg

The app dies **before** any QG log line — the abort is in GSettings, during
platform/platform-theme initialisation.

## Correction to the 2026-10-21 diagnosis

The earlier note concluded "does not reproduce; no QG code change required; trigger
external and unresolved". The non-reproduction was an **artefact of how the tests were
run**: every run used `QT_QPA_PLATFORM=offscreen` or `env -i` (no
`XDG_CURRENT_DESKTOP`), so Qt never selected the desktop theme integration that reads
the schema. A real session (`XDG_CURRENT_DESKTOP=ubuntu:GNOME`, `DISPLAY=:0`) does.

## Evidence chain (2026-10-22)

| Check | Result |
|-------|--------|
| `gsettings list-keys org.gnome.settings-daemon.plugins.xsettings` | **3** keys: `disabled-gtk-modules`, `enabled-gtk-modules`, `overrides` — no `antialiasing` |
| Keys declared in that XML | **6** — the file also declares `antialiasing`, `hinting`, `rgba-order` |
| Where those three actually live | `<schema id="org.gnome.settings-daemon.plugins.xsettings.deprecated">` in the same file (line 20) — `gsettings list-keys …xsettings.deprecated` returns exactly those three |
| Recompiling all schema XMLs into a temp dir | compiles clean, still **3** keys in the non-deprecated schema → the split is real, not a stale cache |
| QG source | `grep -rn "g_settings\|GSettings\|xsettings\|antialiasing" src/` → **0 matches** |
| QG link map | no linked library contains the schema string; `libgtk-3`, `libqgkt3`, `libQt6Gui` do not reference it; `libgtk-4` does |
| Package | `gnome-settings-daemon-common 50.0-1ubuntu1` |

## Root Cause

gnome-settings-daemon **50** moved `antialiasing` / `hinting` / `rgba-order` out of
`org.gnome.settings-daemon.plugins.xsettings` into a new
`…xsettings.deprecated` schema. A desktop-theme consumer in the process (a
GTK/GNOME theme-integration component — **not QG**) still reads them from the old
schema. GLib treats "key not in schema" as a programmer error: `g_settings_get_value`
calls `g_error()` and aborts the process. QG merely hosts the library that does it.

This is the same class of failure as [[worker-pool-receiver-use-after-free]] in one
respect only: both are third-party/pre-existing paths that kill the process, not QG
logic defects. Unlike that one, this is **environmental and fixable without code**.

## Fix (no root, reversible)

`scripts/fix-xsettings-compat.sh` installs a **user-level schema shim**: a private
copy of the schema with the three legacy keys added back to the non-deprecated
schema, compiled into `$XDG_DATA_HOME/glib-2.0/schemas`. User schema sources take
precedence; all other schemas still resolve from `/usr/share`.

    ./scripts/fix-xsettings-compat.sh          # install
    ./scripts/fix-xsettings-compat.sh --check  # verify
    ./scripts/fix-xsettings-compat.sh --remove # undo

Verified before publishing:

| Step | Result |
|------|--------|
| Patched copy + `glib-compile-schemas` in a temp dir | `list-keys` → 6 keys; `get antialiasing` → `0` |
| Same cache as a user-level source (`HOME=/tmp/fakehome`, no `GSETTINGS_SCHEMA_DIR`) | 6 keys, `antialiasing` = `0` |
| Fall-through after the shim | `gsettings get org.gnome.desktop.interface gtk-theme` → `'Yaru'` (system schemas still resolve) |
| Install / `--check` / re-run / `--remove` against a temp HOME | install OK, check OK, idempotent, clean removal |

**Executed 2026-10-22 (after the first report of "still crashing"):** the shim had not
actually been installed — `~/.local/share/glib-2.0/schemas/` did not exist, so nothing had
changed. After running the script for real:

    ./scripts/fix-xsettings-compat.sh   → OK: 'antialiasing' now resolves (0)
    DISPLAY=:0 timeout 12 ./build/src/qg → exit 124 (still running when the timer
                                            killed it; no abort)
    log: "QG starting (Qt 6.10.2)" → XDG paths → "Qt Quick graphics API: OpenGL"
         → media probing output

The app starts on the real desktop session.

## Alternatives

- `sudo apt install --reinstall gnome-settings-daemon-common libgtk-4-1` then
  `sudo glib-compile-schemas /usr/share/glib-2.0/schemas` — only helps if the distro
  ships a consumer that reads the new schema location.
- A system-level schema edit — rejected: package-owned file, lost on update.
- Disabling the desktop theme for QG — not reliable, because the offending consumer is
  selected by the platform layer, not by QG.

## Lesson

A "flaky, non-reproducible" startup crash must be re-tested with the **real session
environment** (`XDG_CURRENT_DESKTOP`, `DISPLAY`, no `env -i`). Offscreen runs silently
skip whole initialisation paths and will happily "prove" a crash impossible.
