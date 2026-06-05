// SPDX-License-Identifier: MIT
//
// Multi-background carousel with three transition styles: fade / slide / zoom.
//
// The carousel fades its own opacity in when sources become available and out
// when sources go empty, so the gradient base layer always shows through during
// loading gaps — no hard cut.
//
// Implementation notes:
//  * Two stacked Image layers (current + next).  When time to switch, the
//    *next* layer is loaded with the upcoming pixmap, then animated in by
//    fading its opacity (and optionally translating or scaling).  After the
//    transition finishes, we promote the next layer into the current slot.
//  * A vignette overlay keeps foreground text readable regardless of wallpaper.
//  * Broken images are skipped automatically.

import QtQuick

Item {
    id: root
    clip: true

    property var sources: []
    property int intervalMs: 30000
    property int durationMs: 2000
    property string style: "fade"
    property bool rollEnabled: true
    property string sourceLabel: ""

    // Whether the carousel should show its content.  The caller toggles this
    // when sources become populated / empty, triggering a smooth crossfade
    // between the gradient base layer and the loaded wallpapers.
    property bool active: true
    property real carouselOpacity: active ? 1.0 : 0.0

    property int currentIndex: 0
    property int _nextIndex: 0
    property bool _animating: false
    property var _brokenImages: ({})

    // ═══════════════════════════════════════════════════════════════════════════
    // Base: dark colour visible through any transparent gap.
    // ═══════════════════════════════════════════════════════════════════════════
    Rectangle { anchors.fill: parent; color: "#05070b" }

    // ═══════════════════════════════════════════════════════════════════════════
    // Layer A — current image
    // ═══════════════════════════════════════════════════════════════════════════
    Image {
        id: layerA
        anchors.fill: parent
        source: root.sources.length > 0 ? root.sources[root.currentIndex] : ""
        fillMode: Image.PreserveAspectCrop
        cache: true
        asynchronous: true
        smooth: true
        mipmap: false
        opacity: 1.0

        onStatusChanged: {
            if (status === Image.Error && root.sources.length > 0) {
                _brokenImages[source.toString()] = true
                root._skipCurrentAndAdvance()
            } else if (status === Image.Ready) {
                var s = source.toString()
                if (_brokenImages[s] !== undefined) delete _brokenImages[s]
                if (!root._entranceDone) root._doEntrance()
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Layer B — upcoming image, animated in during transitions
    // ═══════════════════════════════════════════════════════════════════════════
    Image {
        id: layerB
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        cache: true
        asynchronous: true
        smooth: true
        opacity: 0.0
        transform: [
            Translate { id: layerBTranslate; x: 0; y: 0 },
            Scale    { id: layerBScale; origin.x: layerB.width/2; origin.y: layerB.height/2; xScale: 1.0; yScale: 1.0 }
        ]
        onStatusChanged: {
            if (status === Image.Error) {
                _brokenImages[source.toString()] = true
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Vignette
    // ═══════════════════════════════════════════════════════════════════════════
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0.02, 0.027, 0.043, 0.35)
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Source label — bottom-right badge
    // ═══════════════════════════════════════════════════════════════════════════
    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 12
        anchors.bottomMargin: 12
        width: badgeText.implicitWidth + 18
        height: badgeText.implicitHeight + 10
        radius: 7
        color: Qt.rgba(0.03, 0.035, 0.06, 0.68)
        border.color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
        visible: root.sourceLabel !== ""

        Text {
            id: badgeText
            anchors.centerIn: parent
            text: root.sourceLabel
            color: "#c0b0d8"
            font.pixelSize: 12
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Whole-carousel opacity — animated for smooth entrance / exit
    // ═══════════════════════════════════════════════════════════════════════════
    property bool _entranceDone: false

    NumberAnimation {
        id: carouselOpacityAnim
        target: root
        property: "carouselOpacity"
        duration: 1200
        easing.type: Easing.InOutCubic
    }

    opacity: carouselOpacity

    function _doEntrance() {
        if (root._entranceDone) return
        root._entranceDone = true
        carouselOpacityAnim.from = root.carouselOpacity
        carouselOpacityAnim.to = 1.0
        carouselOpacityAnim.start()
    }

    function _doExit() {
        root._entranceDone = false
        carouselOpacityAnim.from = root.carouselOpacity
        carouselOpacityAnim.to = 0.0
        carouselOpacityAnim.start()
    }

    onActiveChanged: {
        if (active && sources.length > 0) {
            _doEntrance()
        } else if (!active) {
            _doExit()
        }
    }

    onSourcesChanged: {
        _brokenImages = ({})
        _entranceDone = false
        if (sources.length > 0) {
            currentIndex = 0
            layerA.source = sources[0]
            // If we were faded out, start fade-in.
            if (root.carouselOpacity < 0.05) {
                _doEntrance()
            }
        } else {
            // No sources — fade to transparent so the gradient shows through.
            root.carouselOpacity = 0.0
            layerA.source = ""
            layerB.source = ""
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Transition animations
    // ═══════════════════════════════════════════════════════════════════════════
    NumberAnimation {
        id: animFadeIn
        target: layerB; property: "opacity"; from: 0.0; to: 1.0
        duration: root.durationMs
        easing.type: Easing.InOutCubic
        onFinished: root._commit()
    }
    NumberAnimation {
        id: animSlide
        target: layerBTranslate; property: "x"
        from: root.width; to: 0
        duration: root.durationMs
        easing.type: Easing.OutCubic
    }
    NumberAnimation {
        id: animZoomX
        target: layerBScale; property: "xScale"
        from: 1.08; to: 1.0
        duration: root.durationMs
        easing.type: Easing.OutCubic
    }
    NumberAnimation {
        id: animZoomY
        target: layerBScale; property: "yScale"
        from: 1.08; to: 1.0
        duration: root.durationMs
        easing.type: Easing.OutCubic
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Ticker
    // ═══════════════════════════════════════════════════════════════════════════
    Timer {
        id: ticker
        interval: root.intervalMs
        repeat: true
        running: root.rollEnabled && root.sources.length > 1 && root.active
        onTriggered: root.next()
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Public API
    // ═══════════════════════════════════════════════════════════════════════════
    function next()  { _step(+1) }
    function prev()  { _step(-1) }

    function _step(direction) {
        if (root.sources.length === 0) return
        if (root._animating) return

        if (root.sources.length === 1) {
            layerA.source = root.sources[0]
            root.currentIndex = 0
            return
        }

        root._animating = true

        var start = root.currentIndex
        var attempts = 0
        var candidate = start
        do {
            candidate = (candidate + direction + root.sources.length) % root.sources.length
            if (candidate === start || attempts >= root.sources.length) break
            attempts++
        } while (_brokenImages[root.sources[candidate]] === true)

        if (candidate === root.currentIndex) {
            root._animating = false
            return
        }
        root._nextIndex = candidate

        layerBTranslate.x = 0; layerBTranslate.y = 0
        layerBScale.xScale = 1.0; layerBScale.yScale = 1.0
        layerB.opacity = 0
        layerB.source = root.sources[root._nextIndex]

        animFadeIn.from = 0.0; animFadeIn.to = 1.0
        animFadeIn.duration = root.durationMs
        animFadeIn.start()

        if (root.style === "slide") {
            animSlide.from = direction > 0 ? root.width : -root.width
            animSlide.to = 0
            animSlide.duration = root.durationMs
            animSlide.start()
        } else if (root.style === "zoom") {
            animZoomX.from = 1.08; animZoomX.to = 1.0; animZoomX.duration = root.durationMs
            animZoomY.from = 1.08; animZoomY.to = 1.0; animZoomY.duration = root.durationMs
            animZoomX.start(); animZoomY.start()
        }
    }

    function _skipCurrentAndAdvance() {
        if (root.sources.length <= 1) {
            layerA.source = ""
            return
        }
        var next = (root.currentIndex + 1) % root.sources.length
        var tries = 0
        while (_brokenImages[root.sources[next]] === true && tries < root.sources.length) {
            next = (next + 1) % root.sources.length
            tries++
        }
        root.currentIndex = next
        layerA.source = root.sources[next]
    }

    function _commit() {
        root.currentIndex = root._nextIndex
        layerA.source = root.sources[root.currentIndex]
        layerB.opacity = 0
        layerBTranslate.x = 0; layerBTranslate.y = 0
        layerBScale.xScale = 1.0; layerBScale.yScale = 1.0
        root._animating = false
    }
}
