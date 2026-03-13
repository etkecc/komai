// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import Qt5Compat.GraphicalEffects

import "../../../ui"

import cc.etke.komai 1.0

// Image display content for the media overlay.
// Owns its own input: click on image does nothing (absorbs click),
// pinch/drag/wheel for zoom.
Item {
    id: imageContent

    required property string url
    required property string eventId
    required property Room room
    required property int cornerRadius
    property bool animateOnHover: false
    property bool hovered: false

    readonly property bool mediaReady: staticImageReady || animatedImageReady
    readonly property bool mediaFailed: img.status === Image.Error && !animatedImageReady
    readonly property bool staticImageReady: img.status === Image.Ready
    readonly property bool animatedImageReady: mxcimage.loaded

    // Absorb clicks so they don't propagate to the close handler behind us.
    // Clicking an image in the overlay does nothing.
    MouseArea {
        anchors.fill: parent
        onClicked: {} // absorb
    }

    Item {
        id: imageClipper

        anchors.fill: parent
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: imageClipper.width
                height: imageClipper.height
                radius: imageContent.cornerRadius
            }
        }

        Image {
            id: img

            visible: !mxcimage.loaded
            anchors.fill: parent
            source: imageContent.visible ? imageContent.url.replace("mxc://", "image://MxcImage/") : ""
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        MxcAnimatedImage {
            id: mxcimage

            visible: loaded
            anchors.fill: parent
            roomm: imageContent.room
            play: !imageContent.animateOnHover || imageContent.hovered
            eventId: imageContent.visible ? imageContent.eventId : ""
        }
    }

    Spinner {
        anchors.centerIn: parent
        height: Math.max(40, Math.min(parent.width, parent.height) * 0.08)
        visible: !imageContent.mediaReady && !imageContent.mediaFailed
        running: visible
    }
}
