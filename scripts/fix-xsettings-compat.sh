#!/usr/bin/env bash
# Fix the startup abort:
#
#   GLib-GIO-ERROR **: Settings schema
#   'org.gnome.settings-daemon.plugins.xsettings' does not contain a key
#   named 'antialiasing'
#   Aborted (core dumped)
#
# Cause: gnome-settings-daemon 50 moved `antialiasing`, `hinting` and
# `rgba-order` out of the `org.gnome.settings-daemon.plugins.xsettings` schema
# into a new `…xsettings.deprecated` schema. A desktop-theme consumer built
# against the old layout still reads them from the old schema; GLib treats a
# read of a key that is not in the schema as a programmer error and calls
# g_error() → abort, which kills the whole process (including QG, which merely
# loads that theme integration — QG has no GSettings code of its own).
#
# This installs a *user-level* compatibility shim: a private copy of the schema
# with the three legacy keys added back, compiled into
# $XDG_DATA_HOME/glib-2.0/schemas. User schemas take precedence over system
# schemas and everything else still falls through to /usr/share, so no root
# access and no system files are touched.
#
# Usage:
#   scripts/fix-xsettings-compat.sh            # install the shim
#   scripts/fix-xsettings-compat.sh --remove   # undo it
#   scripts/fix-xsettings-compat.sh --check    # report only, change nothing
#
# Re-running install is safe (idempotent).

set -euo pipefail

SYS_DIR="${QG_SYS_SCHEMA_DIR:-/usr/share/glib-2.0/schemas}"
DEST_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/glib-2.0/schemas"
SCHEMA="org.gnome.settings-daemon.plugins.xsettings.gschema.xml"
ENUMS="org.gnome.settings-daemon.enums.xml"

usage() {
    sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

have_key() {
    # Report whether the *effective* schema already exposes the legacy keys.
    gsettings list-keys org.gnome.settings-daemon.plugins.xsettings 2>/dev/null \
        | grep -qx antialiasing
}

case "${1:-install}" in
    -h|--help) usage 0 ;;
    --check)
        if have_key; then
            echo "OK: 'antialiasing' is present in the effective xsettings schema."
        else
            echo "MISSING: the effective schema has no 'antialiasing' key."
            echo "Run: $0"
            exit 1
        fi
        exit 0
        ;;
    --remove)
        rm -f "$DEST_DIR/$SCHEMA" "$DEST_DIR/$ENUMS"
        if [ -f "$DEST_DIR/gschemas.compiled" ]; then
            # Recompile what remains; drop the cache if nothing is left.
            if compgen -G "$DEST_DIR/*.gschema.xml" > /dev/null; then
                glib-compile-schemas "$DEST_DIR"
            else
                rm -f "$DEST_DIR/gschemas.compiled"
            fi
        fi
        echo "Removed the user-level xsettings compatibility shim."
        have_key && echo "NOTE: 'antialiasing' is still present — something else provides it." || true
        exit 0
        ;;
    install) ;;
    *) echo "Unknown option: $1" >&2; usage 2 ;;
esac

command -v glib-compile-schemas > /dev/null || {
    echo "error: glib-compile-schemas not found (install libglib2.0-bin)" >&2
    exit 1
}
[ -r "$SYS_DIR/$SCHEMA" ] || {
    echo "error: $SYS_DIR/$SCHEMA not readable" >&2
    exit 1
}

mkdir -p "$DEST_DIR"
cp -f "$SYS_DIR/$SCHEMA" "$DEST_DIR/"
if [ -r "$SYS_DIR/$ENUMS" ]; then
    cp -f "$SYS_DIR/$ENUMS" "$DEST_DIR/"
fi

python3 - "$DEST_DIR/$SCHEMA" <<'PY'
import re
import sys

path = sys.argv[1]
text = open(path, encoding="utf-8").read()

# The first </schema> closes the *current* (non-deprecated) schema; the legacy
# keys belong there for compatibility.
first_close = text.index("</schema>")
head = text[:first_close]
if 'name="antialiasing"' in head:
    print("compat keys already present in the source schema; nothing to patch")
    sys.exit(0)

shim = """    <key name="antialiasing" type="e">
      <enum>org.gnome.settings-daemon.GsdFontAntialiasingMode</enum>
      <default>'grayscale'</default>
      <summary>Legacy compatibility key</summary>
      <description>Consumers built against gnome-settings-daemon &lt; 50 read this key from the non-deprecated schema. Added by QG's scripts/fix-xsettings-compat.sh.</description>
    </key>
    <key name="hinting" type="e">
      <enum>org.gnome.settings-daemon.GsdFontHinting</enum>
      <default>'slight'</default>
    </key>
    <key name="rgba-order" type="e">
      <enum>org.gnome.settings-daemon.GsdFontRgbaOrder</enum>
      <default>'rgb'</default>
    </key>
  """
text = head + shim + text[first_close:]
open(path, "w", encoding="utf-8").write(text)
print("added legacy keys to the non-deprecated xsettings schema")
PY

glib-compile-schemas "$DEST_DIR"

if have_key; then
    echo "OK: 'antialiasing' now resolves ($(gsettings get org.gnome.settings-daemon.plugins.xsettings antialiasing))."
    echo "Shim installed at: $DEST_DIR"
    echo "Undo with: $0 --remove"
else
    echo "error: the shim compiled but the key is still missing." >&2
    echo "Another schema source may take precedence; check \$GSETTINGS_SCHEMA_DIR." >&2
    exit 1
fi
