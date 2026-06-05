// SPDX-License-Identifier: MIT
// Lightweight "ghost" panel: just a hairline border on a fully transparent
// background.  All visual weight comes from the wallpaper underneath; the
// panel is defined only by its rounded border so content inside stays
// grouped without obscuring the photo.

import QtQuick

Rectangle {
    id: root
    radius: 14
    color: tint
    border.color: Qt.rgba(1, 1, 1, 0.14)
    border.width: 1

    // Default: completely transparent fill.  The panel is just a frame.
    // Caller can still override `tint` for emphasis.
    property color tint: "transparent"
    property bool elevated: false   // kept for backwards compat; unused now
}
