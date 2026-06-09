// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: root
    visible: true
    width: 1200
    height: 760
    minimumWidth: 920
    minimumHeight: 600
    title: "BlackArch 工具启动器"
    color: "#05070b"

    // ── Background orchestration ────────────────────────────────────────────
    BackgroundManager {
        id: bgmgr
        backendSources: Backend.backgrounds
    }

    // Layer 0: gradient — always present.  The carousel fades its opacity
    //          over this when wallpapers are available.
    FallbackBackground {
        id: fallback
        anchors.fill: parent
    }

    // Layer 1: wallpaper carousel.  Opacity animates to 1 when images are
    //          ready, to 0 when all sources disappear — a smooth crossfade
    //          with the gradient layer below.
    BackgroundCarousel {
        id: bg
        anchors.fill: parent
        sources: bgmgr.backgroundList
        intervalMs: Backend.rollIntervalMs
        durationMs: Backend.rollDurationMs
        style: Backend.transitionStyle
        rollEnabled: Backend.rollEnabled
        active: !bgmgr.useGradientFallback
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        TitleBar {
            id: titleBar
            Layout.fillWidth: true
        }

        SplitView {
            id: split
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle { implicitWidth: 6; color: "transparent" }

            ToolTree {
                id: tree
                SplitView.preferredWidth: 340
                SplitView.minimumWidth: 250
            }

            DetailsPane {
                id: details
                SplitView.fillWidth: true
                SplitView.minimumWidth: 400
                onLaunchRequested: {
                    var ei = Backend.selectedToolEntryIndex
                    if (ei >= 0) Backend.launchEntry(ei)
                }
                onOpenDesktopRequested: {
                    var ei = Backend.selectedToolEntryIndex
                    if (ei >= 0) Backend.openDesktopEntry(ei)
                }
            }
        }
    }

    Binding {
        target: Backend
        property: "query"
        value: titleBar.searchText
    }

    CornerSprite {
        frames: Backend.cornerFrames
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 18
        anchors.bottomMargin: 18
        opacity: 0.95
        width: 128; height: 128
    }

    Shortcut { sequence: "Esc";              onActivated: root.close() }
    Shortcut { sequence: "Return";           onActivated: { var ei = Backend.selectedToolEntryIndex; if (ei >= 0) Backend.launchEntry(ei) } }
    Shortcut { sequence: "Enter";            onActivated: { var ei = Backend.selectedToolEntryIndex; if (ei >= 0) Backend.launchEntry(ei) } }
    Shortcut { sequence: "Ctrl+F";           onActivated: titleBar.forceActiveFocus() }
    Shortcut { sequence: "Ctrl+Shift+Right"; onActivated: bg.next() }
    Shortcut { sequence: "Ctrl+Shift+Left";  onActivated: bg.prev() }
    Shortcut { sequence: "Ctrl+R";           onActivated: bgmgr.scanBackgrounds() }

    Connections {
        target: Backend
        function onError(msg) {
            errorPopup.text = msg
            errorPopup.color = Qt.rgba(0.85, 0.30, 0.30, 0.92)
            errorPopup.visible = true
            errorTimer.restart()
        }
        function onInstallFinished(success, message) {
            errorPopup.text = message
            errorPopup.visible = true
            errorPopup.color = success ? Qt.rgba(0.24, 0.65, 0.34, 0.92) : Qt.rgba(0.85, 0.30, 0.30, 0.92)
            errorTimer.restart()
        }
    }

    // TitleBar buttons → show dialogs
    Connections {
        target: titleBar
        function onAddToolClicked() { addToolDialog.shown = true }
        function onShowLogsClicked() { installLogDialog.shown = true }
    }

    // Add tool dialog (modal overlay)
    AddToolDialog {
        id: addToolDialog
    }

    // Install log viewer (modal overlay)
    InstallLogDialog {
        id: installLogDialog
    }

    Rectangle {
        id: errorPopup
        property string text: ""
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 32
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 64, 600)
        height: errLabel.implicitHeight + 24
        radius: 10
        color: Qt.rgba(0.85, 0.30, 0.30, 0.92)
        visible: false
        z: 100
        Text {
            id: errLabel
            anchors.fill: parent
            anchors.margins: 12
            text: errorPopup.text
            color: "#ffffff"
            wrapMode: Text.WordWrap
        }
    }
    Timer { id: errorTimer; interval: 4500; onTriggered: errorPopup.visible = false }
}
