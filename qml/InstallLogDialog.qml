// SPDX-License-Identifier: MIT
// Past install log viewer: list of log files + detail view.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent
    z: 100
    visible: false

    property bool shown: false
    onShownChanged: {
        if (shown) {
            root.visible = true
            logFiles.model = Backend.installLogFiles()
        } else {
            root.visible = false
            logDetail.text = ""
        }
    }

    property var logList: []

    function refresh() {
        logFiles.model = Backend.installLogFiles()
    }

    // ── Backdrop ────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        MouseArea {
            anchors.fill: parent
            onClicked: root.shown = false
        }
    }

    // ── Dialog panel ────────────────────────────────────────────────────
    GlassPane {
        width: 680
        height: Math.min(520, parent.height - 80)
        anchors.centerIn: parent
        tint: Qt.rgba(0.04, 0.06, 0.10, 0.96)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10

            // ── Title ──────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "安装日志"
                    color: "#e6e9ef"
                    font.pixelSize: 18
                    font.bold: true
                    Layout.fillWidth: true
                }
                Text {
                    text: "↻"
                    color: "#7c8aa3"
                    font.pixelSize: 14
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.refresh()
                    }
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

            // ── Split: list + detail ───────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // Left: log file list
                Rectangle {
                    Layout.preferredWidth: 260
                    Layout.fillHeight: true
                    radius: 8
                    color: Qt.rgba(0, 0, 0, 0.15)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    ListView {
                        id: logFiles
                        anchors.fill: parent
                        anchors.margins: 4
                        clip: true
                        model: []
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 4; radius: 2
                                color: Qt.rgba(1, 1, 1, 0.15)
                            }
                        }
                        delegate: Rectangle {
                            width: logFiles.width - 8
                            height: 52
                            radius: 6
                            color: logFiles.currentIndex === index
                                   ? Qt.rgba(0.357, 0.553, 0.937, 0.22)
                                   : logMouse.containsMouse ? Qt.rgba(1,1,1,0.04) : "transparent"
                            Behavior on color { ColorAnimation { duration: 100 } }
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 2
                                Text {
                                    text: modelData.name
                                    color: "#e6e9ef"
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: modelData.date + " · " + (modelData.size / 1024).toFixed(1) + " KB"
                                    color: "#7c8aa3"
                                    font.pixelSize: 10
                                }
                            }
                            MouseArea {
                                id: logMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    logFiles.currentIndex = index
                                    logDetail.text = Backend.readInstallLogFile(modelData.path)
                                }
                            }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: logFiles.count === 0 ? "暂无安装日志" : ""
                            color: "#7c8aa3"
                            font.pixelSize: 12
                        }
                    }
                }

                // Right: log content
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: Qt.rgba(0, 0, 0, 0.25)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 12
                        clip: true
                        contentWidth: logDetail.implicitWidth
                        contentHeight: logDetail.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 4; radius: 2
                                color: Qt.rgba(1, 1, 1, 0.15)
                            }
                        }
                        Text {
                            id: logDetail
                            width: parent.width
                            text: "选择左侧日志文件查看详情"
                            color: logDetail.text ? "#a6b0c2" : "#7c8aa3"
                            font.family: "monospace"
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            lineHeight: 1.6
                        }
                    }
                }
            }
        }
    }
}
