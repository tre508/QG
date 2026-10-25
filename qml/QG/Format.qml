pragma Singleton

// Presentation-only display formatting (durations, sizes, dates, resolution).
// Shared by cards and rows so formatting never diverges. No domain logic.

import QtQuick

QtObject {
    readonly property var monthNames: ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]

    function pad(n) {
        return n < 10 ? "0" + n : "" + n
    }

    function duration(ms) {
        if (ms === undefined || ms === null || ms < 0)
            return "--:--"
        const total = Math.round(ms / 1000)
        const h = Math.floor(total / 3600)
        const m = Math.floor((total % 3600) / 60)
        const s = total % 60
        return h > 0 ? (h + ":" + pad(m) + ":" + pad(s)) : (m + ":" + pad(s))
    }

    function size(bytes) {
        if (!bytes || bytes <= 0)
            return "0 B"
        const units = ["B", "KB", "MB", "GB", "TB"]
        let value = bytes
        let i = 0
        while (value >= 1024 && i < units.length - 1) {
            value /= 1024
            ++i
        }
        const text = (i === 0 || value >= 10) ? Math.round(value) : value.toFixed(1)
        return text + " " + units[i]
    }

    function date(ns) {
        if (!ns)
            return ""
        const d = new Date(ns / 1000000)
        return d.getDate() + " " + monthNames[d.getMonth()] + " " + d.getFullYear()
    }

    function resolution(width, height) {
        if (!width || !height)
            return ""
        const shortEdge = Math.min(width, height)
        if (shortEdge >= 2000)
            return "4K"
        if (shortEdge >= 1000)
            return "1080p"
        if (shortEdge >= 700)
            return "720p"
        return shortEdge + "p"
    }
}
