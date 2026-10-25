pragma Singleton

// QG semantic theme tokens (DESIGN.md §2.2–§2.8, Appendix A).
//
// Presentation only: this singleton is the single source of the approved
// visual language. No raw hex may appear outside this file (Prompt 03
// constraint). Values are transcribed verbatim from DESIGN.md Appendix A;
// the §2.9 High-Contrast variants below are the conditional equivalents.

import QtQuick

QtObject {
    id: theme

    // --- Surfaces (depth through value, not shadow) -------------------------
    // High-Contrast (§2.9): larger luminance steps between adjacent levels so
    // stacked surfaces stay distinguishable; separators brighten into visible
    // hairlines instead of faint blue.
    readonly property color ink: theme.highContrast ? "#000000" : "#05070D"
    readonly property color surface0: theme.highContrast ? "#000000" : "#080C15"
    readonly property color surface1: theme.highContrast ? "#0D1117" : "#0E1524"
    readonly property color surface2: theme.highContrast ? "#1A212E" : "#141D31"
    readonly property color surface3: theme.highContrast ? "#2A3447" : "#1B2740"
    readonly property color surface4: theme.highContrast ? "#3E4C66" : "#24314F"
    readonly property color hairline: theme.highContrast ? "#6B7A99" : "#22304F"
    readonly property color hairlineStrong: theme.highContrast ? "#92A2C2" : "#2E3E63"

    // --- Text ---------------------------------------------------------------
    readonly property color textHi: theme.highContrast ? "#FFFFFF" : "#E9EFFB"
    readonly property color textMid: theme.highContrast ? "#C7D2E8" : "#A9B8D4"
    readonly property color textLow: theme.highContrast ? "#93A4C2" : "#72829F"
    readonly property color textInverse: theme.highContrast ? "#000000" : "#05070D"

    // --- Intent + state neons (used sparingly) ------------------------------
    // High-Contrast (§2.9): fully saturated equivalents; semantics unchanged.
    readonly property color signal: theme.highContrast ? "#3399FF" : "#4DA3FF"
    readonly property color signalDeep: theme.highContrast ? "#0055CC" : "#2E6FE0"
    readonly property color focus: theme.highContrast ? "#00FFFF" : "#38E1E1"
    readonly property color play: theme.highContrast ? "#00FF88" : "#3DE0A0"
    readonly property color favorite: theme.highContrast ? "#FF33CC" : "#FF5CC8"
    readonly property color watched: theme.highContrast ? "#8833FF" : "#9B7BFF"
    readonly property color danger: theme.highContrast ? "#FF3347" : "#FF5D6C"
    readonly property color warn: theme.highContrast ? "#FFB300" : "#FFC65C"
    // High-Contrast outline: neon affordances (focus ring, controls conveying
    // meaning) gain a bright outline so a monochrome user never relies on hue.
    readonly property color neonOutline: theme.highContrast ? "#FFFFFF" : "transparent"

    // Selection fill: rgba(77,163,255,.10) from DESIGN.md §2.7; High-Contrast
    // raises the fill so a selected card separates from its neighbours.
    readonly property color selectionFill: theme.highContrast
        ? Qt.rgba(0.20, 0.60, 1.0, 0.24)
        : Qt.rgba(0.302, 0.639, 1.0, 0.10)

    // Translucent backing for controls that sit on top of imagery (duration
    // chip, hover action chip). One token so both zones share one treatment.
    readonly property color chipScrim: Qt.rgba(5 / 255, 7 / 255, 13 / 255, 0.72)

    // --- Geometry (radius, px) ---------------------------------------------
    readonly property int radiusChip: 999
    readonly property int radiusButton: 8
    readonly property int radiusCard: 10
    readonly property int radiusPanel: 12
    readonly property int radiusStage: 14
    readonly property int radiusMenu: 10
    readonly property int radiusThumbnail: 8

    // --- Spacing scale (px) -------------------------------------------------
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 20
    readonly property int space6: 24
    readonly property int space7: 32
    readonly property int space8: 48

    // --- High-DPI scale factor ---------------------------------------------
    // Effective logical pixel scale factor. Reads from the primary screen's
    // devicePixelRatio, clamped to common fractional scales (1.0, 1.25, 1.5, 2.0).
    // In offscreen/test environments without a screen, defaults to 1.0.
    readonly property real scaleFactor: {
        const screen = Qt.primaryScreen
        if (screen && screen.devicePixelRatio > 1) {
            const dpr = screen.devicePixelRatio
            // Snap to known fractional scales for consistent geometry
            if (dpr >= 1.875) return 2.0      // 200%
            if (dpr >= 1.375) return 1.5      // 150%
            if (dpr >= 1.125) return 1.25     // 125%
            return 1.0
        }
        return 1.0
    }

    // Helper: scale a logical pixel value by the effective scale factor
    function scale(px) { return Math.round(px * theme.scaleFactor) }

    // --- Motion -------------------------------------------------------------
    readonly property int motionMicro: 120
    readonly property int motionStandard: 180
    readonly property int motionBloom: 220
    readonly property int motionCinema: 260
    // cubic-bezier(.2,.8,.2,1) as [x1, y1, x2, y2] for Easing.Bezier.
    readonly property var easeCurve: [0.2, 0.8, 0.2, 1.0]

    // --- Accessibility toggles (Prompt 38) ----------------------------------
    // reducedMotion: when true, all animations become instant swaps (≤1ms).
    // App-level authoritative: Qt 6.10 exposes NO reduced-animation hint
    // (verified in QStyleHints/QAccessibilityHints — `reducedAnimation()` does
    // not exist), so DESIGN §2.8's platform clause is satisfied by this toggle
    // alone. Do not bind it to a nonexistent system signal.
    property bool reducedMotion: false
    // highContrast: when true, surfaces raise separation and neons become fully
    // saturated outlined equivalents while keeping semantics (§2.9). Writable so
    // app/tests can override; the platform hint updates it via the Connections
    // below and the initial read at construction.
    property bool highContrast: false

    // Derived: motionEnabled respects both the toggle and the original design.
    readonly property bool motionEnabled: !reducedMotion

    // --- Typography (relative steps; interface scales with the system font) -
    readonly property int fontDisplay: 22
    readonly property int fontTitle: 15
    readonly property int fontBody: 13
    readonly property int fontMeta: 12
    readonly property int fontMicro: 10

    readonly property string fontUi: "Ubuntu, Inter, Noto Sans, system-ui, sans-serif"
    readonly property string fontMono: "Ubuntu Mono, JetBrains Mono, monospace"

    // --- Platform accessibility hint (§2.9 / UI-12, UI-13) ------------------
    // Qt 6.10 exposes `Qt.styleHints.accessibility.contrastPreference` (enum
    // `Qt::ContrastPreference`: NoPreference=0, HighContrast=1) with notify
    // signal `contrastPreferenceChanged`. There is NO reduced-motion hint.
    // A bare child Connections needs a default property, which QtObject lacks;
    // hold it in a property so it stays alive for the singleton's lifetime.
    property Connections systemContrast: Connections {
        target: Qt.styleHints.accessibility
        function onContrastPreferenceChanged(preference) {
            theme.highContrast = (preference === Qt.HighContrast)
        }
    }

    Component.onCompleted: {
        // Initialize once from the current system hint (guarded for platforms
        // where the accessibility object is absent, so nothing throws).
        const acc = Qt.styleHints ? Qt.styleHints.accessibility : null
        if (acc) {
            theme.highContrast = (acc.contrastPreference === Qt.HighContrast)
        }
    }
}