// SPDX-License-Identifier: MIT
// Add-tool dialog: search BlackArch repo, pick category/type/icon, install.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root
    anchors.fill: parent
    z: 100
    visible: false

    property bool shown: false
    onShownChanged: {
        if (shown) { root.visible = true; searchField.forceActiveFocus() }
        else root.visible = false
    }

    // Category data matching toolmodel.cpp kRules
    readonly property var categories: [
        { tag: "Recon",    label: "信息收集 / Recon",     color: "#5b8def" },
        { tag: "Web",      label: "Web 测试",             color: "#7a4eb8" },
        { tag: "Exploit",  label: "攻防 / 内网",          color: "#d9534f" },
        { tag: "Cred",     label: "凭据 / 密码",          color: "#f5b041" },
        { tag: "Reverse",  label: "逆向",                 color: "#3da556" },
        { tag: "Pwn",      label: "Pwn",                  color: "#ec7063" },
        { tag: "Forensic", label: "取证",                 color: "#16a085" },
        { tag: "Mobile",   label: "移动端",               color: "#9b6dff" },
        { tag: "Traffic",  label: "流量 / 抓包",         color: "#1abc9c" },
        { tag: "Crypto",   label: "Crypto",               color: "#a87c50" },
        { tag: "Other",    label: "其它",                 color: "#888888" }
    ]

    property int selectedCatIdx: 0
    property bool isTerminal: true
    property string customIconPath: ""
    property string selectedPkgName: ""
    property string selectedPkgDesc: ""

    // ── Backdrop ────────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        MouseArea {
            anchors.fill: parent
            onClicked: root.shown = false
        }
    }

    // ── Dialog panel ────────────────────────────────────────────────────────
    GlassPane {
        width: 560
        height: Math.min(700, parent.height - 60)
        anchors.centerIn: parent
        tint: Qt.rgba(0.04, 0.06, 0.10, 0.96)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            // ── Title row ──────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "添加工具"
                    color: "#e6e9ef"
                    font.pixelSize: 18
                    font.bold: true
                    Layout.fillWidth: true
                }
                Text {
                    text: "✕"
                    color: "#7c8aa3"
                    font.pixelSize: 16
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.shown = false
                    }
                }
            }

            // ── Search row ────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    radius: 8
                    color: searchField.activeFocus ? Qt.rgba(0.357, 0.553, 0.937, 0.08) : Qt.rgba(0, 0, 0, 0.12)
                    border.color: searchField.activeFocus ? "#5b8def" : Qt.rgba(1, 1, 1, 0.14)
                    border.width: 1
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10; anchors.rightMargin: 8
                        spacing: 6
                        Text { text: "🔍"; font.pixelSize: 12; color: "#7c8aa3" }
                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: "搜索 BlackArch 软件包…"
                            color: "#e6e9ef"
                            placeholderTextColor: "#7c8aa3"
                            background: null
                            selectByMouse: true
                            font.pixelSize: 12
                            onAccepted: Backend.searchBlackArchTools(text)
                        }
                    }
                }
                Rectangle {
                    implicitWidth: searchBtn.implicitWidth + 20
                    implicitHeight: 34
                    radius: 8
                    color: searchBtn.hovered ? Qt.rgba(0.357, 0.553, 0.937, 0.25) : "#5b8def"
                    Text {
                        id: searchBtn
                        text: "搜索"
                        color: "#ffffff"
                        font.pixelSize: 12; font.bold: true
                        anchors.centerIn: parent
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: Backend.searchBlackArchTools(searchField.text)
                    }
                }
            }

            // ── Results list ──────────────────────────────────────────
            Text {
                visible: resultsList.count === 0 && searchField.text.length > 0
                text: "未找到匹配的软件包，或已安装。"
                color: "#7c8aa3"
                font.pixelSize: 12
                Layout.fillWidth: true
            }

            ListView {
                id: resultsList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(180, resultsList.count * 56)
                visible: resultsList.count > 0
                clip: true
                model: Backend.searchResults
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 6; radius: 3
                        color: Qt.rgba(1, 1, 1, 0.18)
                    }
                }
                delegate: Rectangle {
                    width: resultsList.width
                    height: 52
                    radius: 6
                    color: {
                        if (root.selectedPkgName === modelData.name)
                            return Qt.rgba(0.357, 0.553, 0.937, 0.22)
                        return mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"
                    }
                    Behavior on color { ColorAnimation { duration: 100 } }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 2
                        Text {
                            text: modelData.name + "  <font color='#7c8aa3'>" + modelData.version + "</font>"
                            color: "#e6e9ef"
                            font.pixelSize: 13
                            font.bold: true
                            textFormat: Text.RichText
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: modelData.description
                            color: "#a6b0c2"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.selectedPkgName = modelData.name
                            root.selectedPkgDesc = modelData.description
                            root.customIconPath = ""
                        }
                    }
                }
            }

            // ── Form section (visible when a tool is selected) ────────
            ColumnLayout {
                visible: root.selectedPkgName !== ""
                Layout.fillWidth: true
                spacing: 10

                // Category picker
                Text {
                    text: "分类"
                    color: "#7c8aa3"
                    font.pixelSize: 11
                    font.bold: true
                }
                ListView {
                    id: catList
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    orientation: ListView.Horizontal
                    clip: true
                    model: root.categories
                    spacing: 6
                    delegate: Rectangle {
                        width: catLabel.implicitWidth + 18
                        height: 30
                        radius: 6
                        color: index === root.selectedCatIdx
                               ? Qt.rgba(0.357, 0.553, 0.937, 0.25)
                               : Qt.rgba(1, 1, 1, 0.06)
                        border.color: index === root.selectedCatIdx
                                      ? "#5b8def"
                                      : Qt.rgba(1, 1, 1, 0.10)
                        border.width: 1
                        Text {
                            id: catLabel
                            anchors.centerIn: parent
                            text: modelData.label
                            color: index === root.selectedCatIdx ? "#e6e9ef" : "#a6b0c2"
                            font.pixelSize: 11
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedCatIdx = index
                        }
                    }
                }

                // Terminal / GUI toggle (auto-detected, manual override)
                RowLayout {
                    spacing: 16
                    Text {
                        text: "类型"
                        color: "#7c8aa3"
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Rectangle {
                        width: 120; height: 30; radius: 6
                        color: Qt.rgba(1, 1, 1, 0.06)
                        border.color: Qt.rgba(1, 1, 1, 0.10); border.width: 1
                        Row {
                            anchors.fill: parent
                            Rectangle {
                                width: 60; height: 30; radius: 6
                                color: root.isTerminal ? Qt.rgba(0.96, 0.69, 0.25, 0.25) : "transparent"
                                border.color: root.isTerminal ? "#f5b041" : "transparent"
                                border.width: root.isTerminal ? 1 : 0
                                Text {
                                    anchors.centerIn: parent
                                    text: "CLI"
                                    color: root.isTerminal ? "#f5b041" : "#7c8aa3"
                                    font.pixelSize: 11; font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.isTerminal = true
                                }
                            }
                            Rectangle {
                                width: 60; height: 30; radius: 6
                                color: !root.isTerminal ? Qt.rgba(0.357, 0.553, 0.937, 0.25) : "transparent"
                                border.color: !root.isTerminal ? "#5b8def" : "transparent"
                                border.width: !root.isTerminal ? 1 : 0
                                Text {
                                    anchors.centerIn: parent
                                    text: "GUI"
                                    color: !root.isTerminal ? "#5b8def" : "#7c8aa3"
                                    font.pixelSize: 11; font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.isTerminal = false
                                }
                            }
                        }
                    }
                    Text {
                        text: "安装后根据软件包内容自动检测"
                        color: "#7c8aa3"
                        font.pixelSize: 9
                    }
                }

                // Icon picker
                RowLayout {
                    spacing: 12
                    Text {
                        text: "图标"
                        color: "#7c8aa3"
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Rectangle {
                        width: 48; height: 48; radius: 8
                        color: "transparent"
                        border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
                        Image {
                            anchors.fill: parent; anchors.margins: 6
                            source: root.customIconPath ? "file://" + root.customIconPath : ""
                            fillMode: Image.PreserveAspectFit
                            visible: root.customIconPath !== "" && status === Image.Ready
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: root.customIconPath === "" || parent.children[0].status !== Image.Ready
                            text: root.selectedPkgName ? root.selectedPkgName.charAt(0).toUpperCase() : "?"
                            color: root.categories[root.selectedCatIdx].color
                            font.pixelSize: 22; font.bold: true
                        }
                    }
                    Rectangle {
                        implicitWidth: pickIconLabel.implicitWidth + 16
                        implicitHeight: 30; radius: 6
                        color: pickIconArea.containsMouse ? Qt.rgba(1,1,1,0.12) : Qt.rgba(1,1,1,0.06)
                        border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
                        Text {
                            id: pickIconLabel
                            text: root.customIconPath ? "更换图标" : "选择图标"
                            color: "#e6e9ef"
                            font.pixelSize: 11
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            id: pickIconArea
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: iconFileDialog.open()
                        }
                    }
                    Text {
                        visible: root.customIconPath === ""
                        text: "留空则自动匹配已有图标"
                        color: "#7c8aa3"
                        font.pixelSize: 10
                    }
                }
            }

            Item { Layout.fillHeight: true }

            // ── Status / Install log ──────────────────────────────────
            Text {
                visible: Backend.isInstalling
                text: Backend.installStatus
                color: "#f5b041"
                font.pixelSize: 12
                font.bold: true
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                visible: Backend.isInstalling && Backend.installLog.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(180, installLogText.implicitHeight + 16)
                radius: 6
                color: Qt.rgba(0, 0, 0, 0.3)
                border.color: Qt.rgba(1, 1, 1, 0.08)
                border.width: 1
                Flickable {
                    id: logFlick
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    contentHeight: installLogText.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    function scrollToBottom() {
                        contentY = Math.max(0, contentHeight - height)
                    }
                    Component.onCompleted: scrollToBottom()
                    Text {
                        id: installLogText
                        width: parent.width
                        text: Backend.installLog
                        color: "#a6b0c2"
                        font.family: "monospace"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        lineHeight: 1.5
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: 10
                visible: root.selectedPkgName !== "" && !Backend.isInstalling
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: installBtnArea.containsMouse ? "#6e9bff" : "#5b8def" }
                    GradientStop { position: 1.0; color: installBtnArea.containsMouse ? "#8b5cd0" : "#7a4eb8" }
                }
                Text {
                    anchors.centerIn: parent
                    text: "安装 " + root.selectedPkgName
                    color: "#ffffff"
                    font.pixelSize: 13; font.bold: true
                }
                MouseArea {
                    id: installBtnArea
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: {
                        var catTag = root.categories[root.selectedCatIdx].tag
                        Backend.installTool(root.selectedPkgName, catTag, root.isTerminal, root.customIconPath)
                    }
                    enabled: !Backend.isInstalling
                }
            }
        }
    }

    // ── File dialog for icon picking ─────────────────────────────────────
    FileDialog {
        id: iconFileDialog
        title: "选择工具图标"
        nameFilters: ["图标文件 (*.png *.svg *.jpg *.jpeg *.ico)", "所有文件 (*)"]
        onAccepted: {
            if (selectedFile) {
                root.customIconPath = String(selectedFile).replace("file://", "")
            }
        }
    }

    // ── Listen for install results ──────────────────────────────────────
    Connections {
        target: Backend
        function onInstallLogChanged() {
            Qt.callLater(function() { logFlick.scrollToBottom() })
        }
        function onInstallFinished(success, message) {
            if (success) {
                root.shown = false
                root.selectedPkgName = ""
                root.selectedPkgDesc = ""
                root.customIconPath = ""
                searchField.clear()
            }
        }
    }
}
