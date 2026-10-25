// VideoCard state-matrix tests (Prompt 18).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "VideoCard"
    when: windowShown

    Component { id: cardComponent; VideoCard {} }

    function makeCard(props) {
        const base = { width: 280, height: 220, fileName: "sunset_sea.mp4",
                       durationMs: 252000, sizeBytes: 2254857830, mtimeNs: 1717000000000000000 }
        const merged = Object.assign({}, base, props || {})
        return createTemporaryObject(cardComponent, null, merged)
    }

    function findByName(item, name) {
        if (!item) return null
        if (item.objectName === name) return item
        const kids = item.children
        if (!kids) return null
        for (let i = 0; i < kids.length; ++i) {
            const found = findByName(kids[i], name)
            if (found) return found
        }
        return null
    }

    function test_idleDefaults() {
        const card = makeCard()
        verify(findByName(card, "videoCard") === card)
        compare(findByName(card, "favoriteBadge").visible, false)
        compare(findByName(card, "watchedBadge").visible, false)
        compare(findByName(card, "durationChip").visible, true)
        compare(findByName(card, "progressBar").visible, false)
        compare(findByName(card, "focusRing").visible, false)
        compare(findByName(card, "selectionBorder").visible, false)
    }

    function test_favoriteAndWatched() {
        const card = makeCard({ favorite: true, watched: true })
        compare(findByName(card, "favoriteBadge").visible, true)
        compare(findByName(card, "watchedBadge").visible, true)

        const well = findByName(card, "thumbnailWell")
        const bar = findByName(card, "progressBar")
        compare(bar.visible, true)
        verify(Math.abs(bar.width - well.width) < 2) // watched = full bar
    }

    function test_partialProgress() {
        const card = makeCard({ progress: 0.5 })
        const well = findByName(card, "thumbnailWell")
        const bar = findByName(card, "progressBar")
        compare(bar.visible, true)
        verify(Math.abs(bar.width - well.width * 0.5) < 3)
    }

    function test_focusSelectedAndPlaying() {
        const card = makeCard({ focused: true, selected: true, playing: true })
        compare(findByName(card, "focusRing").visible, true)
        compare(findByName(card, "selectionBorder").visible, true)
        compare(findByName(card, "playingRibbon").visible, true)
        compare(findByName(card, "playingDot").visible, true)
    }

    function test_hoverRevealsActions() {
        const card = makeCard({ hovered: true })
        compare(findByName(card, "quickActions").visible, true)
        compare(findByName(card, "hoverPlayGlyph").visible, true)
    }

    function test_thumbnailFailureAndUnreadable() {
        const failed = makeCard({ thumbnailState: 3 })
        compare(findByName(failed, "thumbFailedGlyph").visible, true)
        compare(findByName(failed, "thumbFilmstrip").visible, false)

        const unreadable = makeCard({ thumbnailState: 4 })
        compare(findByName(unreadable, "thumbLockBadge").visible, true)
    }

    function test_retrySignal() {
        const card = makeCard({ thumbnailState: 3 })
        let retried = 0
        card.thumbnailRetryRequested.connect(function () { retried++ })
        findByName(card, "thumbRetry").clicked()
        compare(retried, 1)
    }

    function test_unknownDurationShowsPlaceholder() {
        const card = makeCard({ durationMs: -1 })
        const chip = findByName(card, "durationChip")
        verify(chip.visible)
        compare(findByName(card, "durationText").text, "--:--")
    }

    // --- VIS-010: a ready thumbnail must actually paint ---------------------
    // The model delivers a URL (file:// in production). This 16x9 PNG keeps the
    // check self-contained while exercising the same Image.source path.
    readonly property string kThumbPng: "data:image/png;base64,"
        + "iVBORw0KGgoAAAANSUhEUgAAABAAAAAJCAIAAAC0SDtlAAAAFElEQVR4nGM4oaBAEmIY1TAoNAAAT/mUgTsFODcAAAAASUVORK5CYII="

    function test_thumbnailImagePaintsForUrlSource() {
        const card = makeCard({ thumbnailState: 2, thumbnail: kThumbPng })
        const image = findByName(card, "thumbImage")
        verify(image !== null, "thumbnail Image exists")
        compare(image.visible, true)
        compare(image.source.toString(), kThumbPng)
        compare(findByName(card, "thumbFilmstrip").visible, false,
                "placeholder is hidden once a thumbnail URL is present")
        tryCompare(image, "status", Image.Ready)
        verify(image.paintedWidth > 0 && image.paintedHeight > 0,
               "thumbnail paints real pixels")
    }

    function test_thumbnailImageHiddenWithoutSource() {
        // Ready state but no URL must not paint (defensive: never a broken image).
        const card = makeCard({ thumbnailState: 2, thumbnail: null })
        const image = findByName(card, "thumbImage")
        compare(image.visible, false)
        compare(image.source.toString(), "")
        compare(findByName(card, "thumbFilmstrip").visible, true)
    }

    function test_thumbnailImageFillsWellWithoutDistorting() {
        const card = makeCard({ thumbnailState: 2, thumbnail: kThumbPng })
        const image = findByName(card, "thumbImage")
        const well = findByName(card, "thumbnailWell")
        tryCompare(image, "status", Image.Ready)
        compare(image.fillMode, Image.PreserveAspectCrop)
        verify(Math.abs(image.width - well.width) < 1)
        verify(Math.abs(image.height - well.height) < 1)
    }
}
