// SPDX-License-Identifier: MIT
//
// Minimal title bar to match the screenshot: "BlackArch" in green at top-left,
// container status next to it, search box on the right.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    implicitHeight: 48

    property alias searchText: searchField.text

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        spacing: 16

        Text {
            text: "BlackArch"
            color: "#3da556"
            font.pixelSize: 22
            font.bold: true
            font.family: "Noto Sans CJK SC, sans-serif"
            // subtle glow to match the cartoon aesthetic
            style: Text.Outline
            styleColor: Qt.rgba(0, 0, 0, 0.45)
        }

        Row {
            spacing: 8
            Rectangle {
                width: 8; height: 8; radius: 4
                color: Backend.containerStatusColor
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: 200 } }
            }
            Text {
                text: "容器：" + Backend.containerStatus + " · " + Backend.totalCount + " 工具"
                color: "#cdd2dc"
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 12
            }
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            Layout.preferredWidth: 280
            Layout.preferredHeight: 32
            radius: 8
            color: searchField.activeFocus ? Qt.rgba(0.357, 0.553, 0.937, 0.08) : Qt.rgba(0, 0, 0, 0.12)
            border.color: searchField.activeFocus ? "#5b8def" : Qt.rgba(1, 1, 1, 0.14)
            border.width: 1
            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 6

                Text { text: "🔍"; font.pixelSize: 12; color: "#7c8aa3" }

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: "搜索工具…  (Ctrl+F)"
                    color: "#e6e9ef"
                    placeholderTextColor: "#7c8aa3"
                    background: null
                    selectByMouse: true
                    font.pixelSize: 12
                }
                Text {
                    visible: searchField.text.length > 0
                    text: "✕"
                    color: "#7c8aa3"
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: searchField.clear()
                    }
                }
            }
        }
    }
}
