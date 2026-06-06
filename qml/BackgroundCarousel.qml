// SPDX-License-Identifier: MIT
//
// Multi-background carousel — fade / slide / zoom transitions.
//
// Two-layer design: the next image is preloaded into a hidden layer (layerB)
// before the transition starts, so there's no flash of missing content.

import QtQuick

Item {
    id: root
    clip: true

    property var sources: []
    property int intervalMs: 30000
    property int durationMs: 2000
    property string style: "fade"
    property bool rollEnabled: true
    property bool active: true
    property real carouselOpacity: active ? 1.0 : 0.0

    property int currentIndex: 0
    property int _nextIndex: 0
    property bool _animating: false
    property var _brokenImages: ({})

    // Dark base behind both layers.
    Rectangle { anchors.fill: parent; color: "#05070b" }

    // ── Layer A (current) ─────────────────────────────────────────────
    Image {
        id: layerA
        anchors.fill: parent
        source: root.sources.length > 0 ? root.sources[root.currentIndex] : ""
        fillMode: Image.PreserveAspectCrop
        cache: true
        asynchronous: true
        smooth: true
        mipmap: true
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

    // ── Layer B (next) ────────────────────────────────────────────────
    // asynchronous: false is deliberate — this layer is hidden (opacity 0)
    // during loading, so synchronous load won't cause UI stutter.  It
    // guarantees onStatusChanged fires for every source change, avoiding
    // the race conditions that break the polling-timer approach.
    Image {
        id: layerB
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        cache: true
        asynchronous: false
        smooth: true
        mipmap: true
        opacity: 0.0
        transform: [
            Translate { id: layerBTranslate; x: 0; y: 0 },
            Scale    { id: layerBScale; origin.x: layerB.width/2; origin.y: layerB.height/2; xScale: 1.0; yScale: 1.0 }
        ]

        onStatusChanged: {
            if (status === Image.Error) {
                _brokenImages[source.toString()] = true
                root._advanceToNextCandidate()
            } else if (status === Image.Ready && root._animating) {
                root._startTransitionAnimations()
            }
        }
    }

    // ── Vignette ──────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.10)
    }

    // ── Entrance / exit opacity ───────────────────────────────────────
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
        if (active && sources.length > 0) _doEntrance()
        else if (!active) _doExit()
    }
    onSourcesChanged: {
        _brokenImages = ({})
        _entranceDone = false
        if (sources.length > 0) {
            currentIndex = 0
            layerA.source = sources[0]
            if (root.carouselOpacity < 0.05) _doEntrance()
        } else {
            root.carouselOpacity = 0.0
            layerA.source = ""
            layerB.source = ""
        }
    }

    // ── Transition animations ─────────────────────────────────────────
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
        duration: root.durationMs
        easing.type: Easing.OutCubic
    }
    NumberAnimation {
        id: animZoomX
        target: layerBScale; property: "xScale"
        duration: root.durationMs
        easing.type: Easing.OutCubic
    }
    NumberAnimation {
        id: animZoomY
        target: layerBScale; property: "yScale"
        duration: root.durationMs
        easing.type: Easing.OutCubic
    }

    function _startTransitionAnimations() {
        animFadeIn.from = 0.0; animFadeIn.to = 1.0
        animFadeIn.duration = root.durationMs
        animFadeIn.start()

        if (root.style === "slide") {
            animSlide.from = root._slideDirection > 0 ? root.width : -root.width
            animSlide.to = 0
            animSlide.duration = root.durationMs
            animSlide.start()
        } else if (root.style === "zoom") {
            animZoomX.from = 1.08; animZoomX.to = 1.0; animZoomX.duration = root.durationMs
            animZoomY.from = 1.08; animZoomY.to = 1.0; animZoomY.duration = root.durationMs
            animZoomX.start(); animZoomY.start()
        }
    }

    // ── Timer ─────────────────────────────────────────────────────────
    Timer {
        id: ticker
        interval: root.intervalMs
        repeat: true
        running: root.rollEnabled && root.sources.length > 1 && root.active
        onTriggered: root.next()
    }

    // ── Public API ────────────────────────────────────────────────────
    function next()  { _step(+1) }
    function prev()  { _step(-1) }

    property int _slideDirection: 1

    function _step(direction) {
        if (root.sources.length === 0) return
        if (root._animating) return

        if (root.sources.length === 1) {
            layerA.source = root.sources[0]
            root.currentIndex = 0
            return
        }

        var start = root.currentIndex
        var attempts = 0
        var candidate = start
        do {
            candidate = (candidate + direction + root.sources.length) % root.sources.length
            if (candidate === start || attempts >= root.sources.length) break
            attempts++
        } while (_brokenImages[root.sources[candidate]] === true)

        if (candidate === root.currentIndex) return

        root._animating = true
        root._nextIndex = candidate
        root._slideDirection = direction

        layerB.opacity = 0
        layerBTranslate.x = 0; layerBTranslate.y = 0
        layerBScale.xScale = 1.0; layerBScale.yScale = 1.0

        // With asynchronous:false, setting source triggers synchronous
        // load, and onStatusChanged fires reliably when done.
        layerB.source = root.sources[root._nextIndex]
    }

    function _advanceToNextCandidate() {
        if (root.sources.length <= 1) {
            root._animating = false
            return
        }
        var candidate = root._nextIndex
        var start = candidate
        var attempts = 0
        do {
            candidate = (candidate + root._slideDirection + root.sources.length) % root.sources.length
            if (candidate === start || attempts >= root.sources.length) break
            attempts++
        } while (_brokenImages[root.sources[candidate]] === true)

        if (candidate === root._nextIndex || attempts >= root.sources.length) {
            root._animating = false
            return
        }
        root._nextIndex = candidate
        layerB.source = root.sources[root._nextIndex]
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
        layerB.source = ""
        layerBTranslate.x = 0; layerBTranslate.y = 0
        layerBScale.xScale = 1.0; layerBScale.yScale = 1.0
        root._animating = false
    }
}
