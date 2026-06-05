// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

GlassPane {
    id: root
    tint: "transparent"

    readonly property string toolName: Backend.selectedToolName
    readonly property string toolGeneric: Backend.selectedToolGeneric
    readonly property string toolComment: Backend.selectedToolComment
    readonly property string toolExec: Backend.selectedToolExec
    readonly property int toolEntryIndex: Backend.selectedToolEntryIndex
    readonly property bool toolIsTerminal: Backend.selectedToolIsTerminal
    readonly property string toolIconUrl: Backend.selectedToolIconUrl
    readonly property string toolTag: Backend.selectedToolTag
    readonly property string toolTagColor: Backend.selectedToolTagColor

    function hasData() { return root.toolEntryIndex >= 0 && root.toolName !== "" }

    signal launchRequested()
    signal openDesktopRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        RowLayout {
            spacing: 16

            Item {
                Layout.preferredWidth: 96
                Layout.preferredHeight: 96
                Rectangle {
                    anchors.fill: parent; radius: 14
                    color: "transparent"
                    border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
                }
                Image {
                    anchors.fill: parent; anchors.margins: 12
                    source: root.toolIconUrl
                    fillMode: Image.PreserveAspectFit
                    sourceSize.width: 192; sourceSize.height: 192
                    smooth: true
                    visible: status === Image.Ready
                }
                Text {
                    anchors.centerIn: parent
                    visible: !root.toolIconUrl || (parent.children[1].status !== Image.Ready)
                    text: root.toolName ? root.toolName.charAt(0).toUpperCase() : "?"
                    color: root.toolTagColor ? root.toolTagColor : "#888"
                    font.pixelSize: 48; font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: root.toolName ? root.toolName : "—"
                    color: "#e6e9ef"; font.pixelSize: 24; font.bold: true
                    Layout.fillWidth: true; elide: Text.ElideRight
                }
                Text {
                    text: root.toolGeneric
                    color: "#a6b0c2"; font.pixelSize: 13
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    visible: text.length > 0
                }
                RowLayout {
                    spacing: 8
                    Rectangle {
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: kindLabel.implicitWidth + 16
                        radius: 4
                        color: root.toolIsTerminal ? Qt.rgba(0.96, 0.69, 0.25, 0.18) : Qt.rgba(0.357, 0.553, 0.937, 0.18)
                        visible: root.hasData()
                        Text {
                            id: kindLabel; anchors.centerIn: parent
                            text: root.toolIsTerminal ? "CLI" : "GUI"
                            color: root.toolIsTerminal ? "#f5b041" : "#5b8def"
                            font.pixelSize: 11; font.bold: true
                        }
                    }
                    Rectangle {
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: tagLabel.implicitWidth + 20
                        radius: 4; color: Qt.rgba(1, 1, 1, 0.06)
                        visible: root.toolTag !== ""
                        Row {
                            anchors.centerIn: parent; spacing: 6
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: root.toolTagColor ? root.toolTagColor : "#888"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                id: tagLabel
                                text: root.toolTag
                                color: "#a6b0c2"; font.pixelSize: 11
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }

        // ── Details / usage card ─────────────────────────────────────
        ScrollView {
            id: detailScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: detailScroll.availableWidth
                spacing: 12

                Text { text: "📝  描述"; color: "#7c8aa3"; font.pixelSize: 12; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    text: root.toolComment ? root.toolComment :
                          root.toolGeneric ? root.toolGeneric : "选择左侧的工具查看详情。"
                    color: "#cdd2dc"; wrapMode: Text.WordWrap
                    font.pixelSize: 14; lineHeight: 1.6
                }

                Item { Layout.preferredHeight: 6; visible: root.hasData() }

                Text {
                    text: "💡  基础用法"; color: "#7c8aa3"; font.pixelSize: 12
                    font.bold: true; visible: root.hasData()
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: usageContent.implicitHeight + 20
                    visible: root.hasData()
                    radius: 6; color: Qt.rgba(0, 0, 0, 0.15)
                    border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
                    Text {
                        id: usageContent
                        anchors.fill: parent; anchors.margins: 12
                        text: root.toolName ? Backend.usageFor(root.toolName) : ""
                        color: "#cdd2dc"; font.family: "monospace"
                        font.pixelSize: 13; wrapMode: Text.WordWrap
                        lineHeight: 1.7
                    }
                }

                Item { Layout.preferredHeight: 6; visible: root.toolExec !== "" }

                Text {
                    text: "⌨  执行命令"; color: "#7c8aa3"; font.pixelSize: 12
                    font.bold: true; visible: root.toolExec !== ""
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: execBlock.implicitHeight + 20
                    visible: root.toolExec !== ""
                    radius: 6; color: Qt.rgba(0, 0, 0, 0.12)
                    border.color: Qt.rgba(1, 1, 1, 0.06); border.width: 1
                    Text {
                        id: execBlock
                        anchors.fill: parent; anchors.margins: 12
                        text: "$ " + root.toolExec
                        color: "#7c8aa3"; font.family: "monospace"
                        font.pixelSize: 13; wrapMode: Text.Wrap
                        lineHeight: 1.6
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "\uD83D\uDBB1  单击查看详情 · 双击或回车启动 · Esc 关闭"
                    color: "#7c8aa3"; font.pixelSize: 10
                    visible: root.hasData()
                }
            }

            background: Rectangle {
                radius: 10; color: "transparent"
                border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.rightMargin: 148
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
                text: "打开 .desktop"
                enabled: root.hasData()
                onClicked: root.openDesktopRequested()
                background: Rectangle {
                    radius: 10
                    color: parent.hovered ? Qt.rgba(1,1,1,0.12) : Qt.rgba(1,1,1,0.06)
                    border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                contentItem: Text {
                    text: parent.text; color: "#e6e9ef"
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    leftPadding: 14; rightPadding: 14; topPadding: 10; bottomPadding: 10
                    font.pixelSize: 13
                }
            }

            Button {
                id: launchBtn
                text: "启动 (Enter)"
                enabled: root.hasData()
                onClicked: root.launchRequested()
                background: Rectangle {
                    radius: 10
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: launchBtn.hovered ? "#6e9bff" : "#5b8def" }
                        GradientStop { position: 1.0; color: launchBtn.hovered ? "#8b5cd0" : "#7a4eb8" }
                    }
                }
                contentItem: Text {
                    text: parent.text; color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    leftPadding: 18; rightPadding: 18; topPadding: 10; bottomPadding: 10
                    font.pixelSize: 13; font.bold: true
                }
            }
        }
    }
}
