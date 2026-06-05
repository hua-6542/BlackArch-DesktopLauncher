// SPDX-License-Identifier: MIT
// Pure gradient fallback — always present as the base layer behind the carousel.
// The carousel fades its opacity over this when real wallpapers are available.

import QtQuick

Rectangle {
    id: root
    anchors.fill: parent

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#0a0a1a" }
        GradientStop { position: 0.45; color: "#0f0c24" }
        GradientStop { position: 1.0; color: "#1a0a2a" }
    }

    // Scanline grid — subtle cyberpunk texture.
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        opacity: 0.05
        Repeater {
            model: Math.max(1, Math.ceil(parent.height / 4))
            delegate: Rectangle {
                x: 0; y: index * 4
                width: parent.width; height: 1
                color: "#aaccff"
            }
        }
    }

    // Large radial glow — centre.
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.55
        height: width
        radius: width / 2
        opacity: 0.09
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#6b3fb0" }
            GradientStop { position: 0.6; color: "#2a1050" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Second smaller hotter glow.
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.2
        height: width
        radius: width / 2
        opacity: 0.12
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#9b5fdf" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
}
