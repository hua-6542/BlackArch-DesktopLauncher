// SPDX-License-Identifier: MIT
// Four-level fallback background orchestrator.
//
//   1. backgrounds_compose/  — composited glass-panel wallpapers
//   2. backgrounds/          — user-supplied originals
//   3. fallback/             — built-in backup images
//   4. Pure gradient         — cyberpunk gradient (always present as base layer)
//
// The gradient is always visible underneath; the carousel fades in on top
// when real images are available and fades out when all tiers are empty.

import QtQuick

Item {
    id: root

    // ── Outputs consumed by BackgroundCarousel ─────────────────────────────────

    property var backgroundList: []
    property string sourceLabel: ""
    // Start pessimistic — assume no images until scan confirms otherwise.
    // This gives a smooth gradient→wallpaper fade-in on startup.
    property bool useGradientFallback: true

    // Tier number (1-4) currently active.
    property int activeTier: 4

    // Emitted whenever the resolved list or tier changes.
    signal backgroundsUpdated()

    // ── Inputs from C++ Backend ────────────────────────────────────────────────

    property var backendSources: []

    // ── Internal ───────────────────────────────────────────────────────────────

    property int _rescanIntervalMs: 15000
    property int _lastTier: -1

    function _classify(firstUrl) {
        if (!firstUrl || typeof firstUrl !== "string") return 4
        if (firstUrl.indexOf("backgrounds_compose") >= 0) return 1
        if (firstUrl.indexOf("/backgrounds/") >= 0)   return 2
        if (firstUrl.indexOf("/fallback/") >= 0)      return 3
        if (firstUrl.indexOf("qrc:") === 0 || firstUrl.indexOf(":/") === 0) return 3
        return 2
    }

    function scanBackgrounds() {
        var raw = root.backendSources
        if (!raw || raw.length === 0) {
            root.backgroundList = []
            root.useGradientFallback = true
            root.activeTier = 4
            root.sourceLabel = "纯色渐变 (第 4 级回退)"
        } else {
            root.backgroundList = raw
            root.useGradientFallback = false
            root.activeTier = _classify(raw[0])
            var labels = ["", "合成壁纸 (第 1 级)", "用户壁纸 (第 2 级)", "内置备用 (第 3 级)", "纯色渐变 (第 4 级)"]
            root.sourceLabel = labels[root.activeTier] || "未知来源"
        }
        if (root.activeTier !== root._lastTier) {
            root._lastTier = root.activeTier
            root.backgroundsUpdated()
        }
    }

    Timer {
        id: rescanTimer
        interval: root._rescanIntervalMs
        repeat: true
        running: true
        onTriggered: {
            if (typeof Backend !== "undefined" && Backend.rescanBackgrounds)
                Backend.rescanBackgrounds()
            root.scanBackgrounds()
        }
    }

    onBackendSourcesChanged: scanBackgrounds()
    Component.onCompleted: scanBackgrounds()

    property int generation: 0
    onBackgroundsUpdated: generation++
}
