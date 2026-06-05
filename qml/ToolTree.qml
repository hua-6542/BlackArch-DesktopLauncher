// SPDX-License-Identifier: MIT
// Two-level expandable tree: category → tool.  Mirrors the screenshot mockup.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

GlassPane {
    id: root
    tint: "transparent"

    function selectFirstLeaf() {
        for (let r = 0; r < tree.rows; ++r) {
            const idx = tree.modelIndex(r, 0)
            if (Backend.toolTree.isCategory(idx)) {
                if (!tree.isExpanded(r)) tree.expand(r)
                return
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "transparent"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                spacing: 0
                Text {
                    text: "工具"
                    color: "#7c8aa3"
                    font.pixelSize: 11
                    font.bold: true
                    Layout.preferredWidth: parent.width * 0.55
                }
                Text {
                    text: "类型"
                    color: "#7c8aa3"
                    font.pixelSize: 11
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
            }
            Rectangle {
                anchors.bottom: parent.bottom; anchors.left: parent.left
                anchors.right: parent.right; height: 1
                color: Qt.rgba(1, 1, 1, 0.06)
            }
        }

        TreeView {
            id: tree
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Backend.toolTree
            selectionModel: ItemSelectionModel { model: Backend.toolTree }
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: 6; radius: 3
                    color: Qt.rgba(1, 1, 1, 0.18)
                }
            }

            delegate: Item {
                id: d
                required property TreeView treeView
                required property int row
                required property int depth
                required property bool isTreeNode
                required property bool expanded
                required property bool hasChildren
                required property string display
                property bool isLeaf: !hasChildren
                property string emoji: model.emoji || ""
                property int childCount: model.childCount || 0
                property string tag: model.tag || ""
                property string tagColor: model.tagColor || "#888888"
                property var iconUrl: model.iconUrl
                property string generic: model.generic || ""
                property bool isTerminal: model.isTerminal === true

                implicitWidth: tree.width
                implicitHeight: isLeaf ? 36 : 38

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 4; anchors.rightMargin: 4
                    radius: 6
                    color: d.treeView.selectionModel.currentIndex === d.treeView.modelIndex(d.row, 0)
                           ? Qt.rgba(0.357, 0.553, 0.937, 0.32)
                           : "transparent"
                    Behavior on color { ColorAnimation { duration: 110 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8 + d.depth * 18
                        anchors.rightMargin: 12
                        spacing: 8

                        Item {
                            Layout.preferredWidth: 14; Layout.preferredHeight: 14
                            visible: d.hasChildren
                            Text {
                                anchors.centerIn: parent
                                text: d.expanded ? "▾" : "▸"
                                color: d.isLeaf ? "#7c8aa3" : d.tagColor
                                font.pixelSize: 11
                            }
                        }

                        Item {
                            Layout.preferredWidth: d.isLeaf ? 22 : 18
                            Layout.preferredHeight: d.isLeaf ? 22 : 18
                            visible: d.isLeaf || d.emoji !== ""
                            Text {
                                visible: !d.isLeaf; anchors.centerIn: parent
                                text: d.emoji; font.pixelSize: 14
                            }
                            Image {
                                visible: d.isLeaf && status === Image.Ready
                                anchors.fill: parent
                                source: d.iconUrl ? d.iconUrl : ""
                                sourceSize.width: 44; sourceSize.height: 44
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true; smooth: true
                            }
                            Rectangle {
                                visible: d.isLeaf && (parent.children[1].status !== Image.Ready)
                                anchors.fill: parent; radius: 4
                                color: Qt.rgba(1, 1, 1, 0.05)
                                Text {
                                    anchors.centerIn: parent
                                    text: d.display.length > 0 ? d.display.charAt(0).toUpperCase() : "?"
                                    color: d.tagColor; font.bold: true; font.pixelSize: 11
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: d.isLeaf
                                ? (d.generic ? d.display + "    <font color='#7c8aa3'>— " + d.generic + "</font>" : d.display)
                                : d.display
                            color: d.isLeaf ? "#e6e9ef" : "#cdd2dc"
                            font.bold: !d.isLeaf; font.pixelSize: 13
                            elide: Text.ElideRight
                            textFormat: d.isLeaf ? Text.RichText : Text.PlainText
                        }

                        Rectangle {
                            visible: !d.isLeaf
                            Layout.preferredHeight: 18
                            Layout.preferredWidth: countTxt.implicitWidth + 12
                            radius: 9; color: Qt.rgba(1, 1, 1, 0.07)
                            Text {
                                id: countTxt; anchors.centerIn: parent
                                text: d.childCount; color: "#a6b0c2"; font.pixelSize: 11
                            }
                        }
                        Rectangle {
                            visible: d.isLeaf
                            Layout.preferredHeight: 18
                            Layout.preferredWidth: kindTxt.implicitWidth + 14
                            radius: 4
                            color: d.isTerminal ? Qt.rgba(0.96, 0.69, 0.25, 0.18) : Qt.rgba(0.357, 0.553, 0.937, 0.18)
                            Text {
                                id: kindTxt; anchors.centerIn: parent
                                text: d.isTerminal ? "CLI" : "GUI"
                                color: d.isTerminal ? "#f5b041" : "#5b8def"
                                font.pixelSize: 10; font.bold: true
                            }
                        }
                    }

                    TapHandler {
                        onTapped: function(ev, btn) {
                            const idx = d.treeView.modelIndex(d.row, 0)
                            d.treeView.selectionModel.setCurrentIndex(idx, 0x0030)
                            if (d.hasChildren) {
                                if (d.expanded) d.treeView.collapse(d.row)
                                else d.treeView.expand(d.row)
                            } else {
                                Backend.selectTool(model.entryIndex)
                            }
                        }
                        onDoubleTapped: {
                            if (d.isLeaf) {
                                Backend.selectTool(model.entryIndex)
                                Backend.launchEntry(model.entryIndex)
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: Backend
        function onToolsChanged() {
            Qt.callLater(function() {
                if (tree.rows > 0) tree.expand(0)
            })
        }
        function onQueryChanged() {
            Qt.callLater(function() {
                if (Backend.query.length > 0) {
                    for (let r = 0; r < tree.rows; ++r) {
                        const idx = tree.modelIndex(r, 0)
                        if (Backend.toolTree.isCategory(idx) && !tree.isExpanded(r))
                            tree.expand(r)
                    }
                } else if (tree.rows > 0) {
                    tree.expand(0)
                }
            })
        }
    }

    // Detect selection changes at the TreeView level (works alongside TapHandler).
    Connections {
        target: tree.selectionModel
        function onCurrentChanged(current, previous) {
            if (!current || !current.valid) return
            if (Backend.toolTree.isCategory(current)) return
            var ei = Backend.toolTree.entryIndexAt(current)
            if (ei >= 0) Backend.selectTool(ei)
        }
    }

    Component.onCompleted: Qt.callLater(function() {
        if (tree.rows > 0) tree.expand(0)
    })
}
