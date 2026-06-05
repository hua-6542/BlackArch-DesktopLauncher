// SPDX-License-Identifier: MIT
import QtQuick

Item {
    id: root
    property var frames: []     // QStringList of file:// urls
    property int frameMs: 160

    width: 96; height: 96
    visible: frames.length > 0

    Image {
        id: img
        anchors.fill: parent
        source: root.frames.length > 0 ? root.frames[idx] : ""
        fillMode: Image.PreserveAspectFit
        smooth: true
        sourceSize.width: 192; sourceSize.height: 192

        property int idx: 0
        Timer {
            interval: root.frameMs
            repeat: true
            running: root.frames.length > 1
            onTriggered: img.idx = (img.idx + 1) % root.frames.length
        }
    }
}
