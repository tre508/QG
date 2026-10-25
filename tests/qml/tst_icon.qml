// Icon component tests (REM-10 / DESIGN.md §2.3, §2.4, §2.9).
// Verifies the shared monoline icon renderer: a known name resolves to an SVG
// source and reaches Image.Ready offscreen, implicit size equals the requested
// size, an unknown name degrades to a visible fallback (no crash), and the tint
// color follows the Theme token (single role).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

Item {
    id: root
    width: 200; height: 200

    TestCase {
        id: tc
        name: "Icon"
        when: windowShown

        function test_knownNameResolvesAndSizes() {
            const icon = Qt.createQmlObject(
                'import QtQuick; import QtQuick.Effects; import qg; Icon { name: "play"; size: 20; color: Theme.signal }',
                root, "probeIcon")
            verify(icon !== null, "Icon instantiates")
            verify(icon.fileName !== "", "known name maps to a file")
            verify(icon.source !== "", "known name produces a qrc source")
            compare(icon.implicitWidth, 20, "implicit width equals size")
            compare(icon.implicitHeight, 20, "implicit height equals size")
            compare(icon.width, 20)

            // SVG rasterizes offscreen via the present svg plugin.
            tryCompare(icon, "status", Image.Ready)
        }

        function test_unknownNameFallsBackGracefully() {
            const icon = Qt.createQmlObject(
                'import QtQuick; import QtQuick.Effects; import qg; Icon { name: "no-such-name"; size: 16; color: Theme.textMid }',
                root, "badIcon")
            verify(icon !== null, "unknown-name Icon still instantiates")
            compare(icon.fileName, "", "unknown name maps to empty file")
            compare(icon.source, "", "unknown name yields empty source")
            // The fallback Rectangle is visible (no crash, no blank surface).
            const fb = findChild(icon, "iconFallback")
            verify(fb !== null, "fallback dot exists")
            verify(fb.visible, "fallback dot is visible")
        }

        // Every icon name the sidebar asks for must map to a shipped SVG that
        // rasterizes. An unregistered asset silently renders a fallback dot
        // (the root cause of the earlier missing-icon reports).
        function test_sidebarIconsResolveToShippedAssets() {
            const names = ["home", "videos", "downloads", "desktop", "documents",
                           "recent", "pin", "drive", "drive-removable", "network",
                           "unmount", "eject", "folder", "device"]
            for (let i = 0; i < names.length; ++i) {
                const icon = Qt.createQmlObject(
                    'import QtQuick; import QtQuick.Effects; import qg; Icon { name: "' + names[i]
                    + '"; size: 16; color: Theme.textMid }', root, "sidebarIconProbe")
                verify(icon !== null, names[i] + ": instantiates")
                verify(icon.fileName !== "", names[i] + ": registry entry exists")
                verify(icon.source !== "", names[i] + ": qrc source built")
                tryCompare(icon, "status", Image.Ready)
                icon.destroy()
            }
        }

        function test_colorFollowsThemeToken() {
            const icon = Qt.createQmlObject(
                'import QtQuick; import QtQuick.Effects; import qg; Icon { name: "favorite"; size: 20; color: Theme.favorite }',
                root, "favIcon")
            verify(icon !== null)
            // Tint is the injected color (single role, §2.4). The MultiEffect
            // colorization target matches the assigned color value.
            compare(icon.color.toString(), Theme.favorite.toString(), "tint equals the Theme token")
        }
    }
}