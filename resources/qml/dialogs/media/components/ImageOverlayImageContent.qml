// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Effects

import "../../../ui"

import cc.etke.komai 1.0

// Image display content for the media overlay.
// Owns its own input: click on image does nothing (absorbs click),
// pinch/drag/wheel for zoom.
Item {
    id: imageContent

    required property string url
    required property string eventId
    property var room
    required property int cornerRadius
    readonly property bool useActiveMatrixTimelineSource: !!room
        && room.isActiveMatrixTimelineRoom === true
    property bool animateOnHover: false
    property bool hovered: false
    // Current zoom factor of the overlay container. Used to decide whether the
    // rounded-corner mask layer is safe to keep enabled (see roundCorners).
    property real zoomScale: 1.0

    // The rounded-corner mask renders the image into an offscreen FBO sized to
    // the on-screen (un-zoomed) item. When the container is magnified, that FBO
    // texture is scaled up instead of re-sampling the full-resolution source,
    // which makes zoomed images look blurry. Rounded corners are only visible at
    // (or below) 1x anyway — once magnified they sit off-screen — so we drop the
    // mask while zoomed in and let the image re-rasterize crisply from source.
    readonly property bool roundCorners: zoomScale <= 1.01

    readonly property bool mediaReady: staticImageReady || animatedImageReady
    readonly property bool mediaFailed: img.status === Image.Error && !animatedImageReady
    readonly property bool staticImageReady: img.status === Image.Ready
    readonly property bool animatedImageReady: mxcimage.loaded
    // Natural dimensions of the loaded image, used by the overlay to size
    // the container when the caller did not provide original dimensions.
    readonly property int sourceWidth: mxcimage.loaded ? mxcimage.implicitWidth : img.implicitWidth
    readonly property int sourceHeight: mxcimage.loaded ? mxcimage.implicitHeight : img.implicitHeight

    // Absorb clicks so they don't propagate to the close handler behind us.
    // Clicking an image in the overlay does nothing.
    MouseArea {
        anchors.fill: parent
        onClicked: {} // absorb
    }

    Rectangle {
        id: imageMask

        anchors.fill: imageClipper
        radius: imageContent.cornerRadius
        layer.enabled: true
        visible: false
    }

    Item {
        id: imageClipper

        anchors.fill: parent
        layer.enabled: imageContent.roundCorners
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: imageMask
        }

        Image {
            id: img

            visible: !mxcimage.loaded
            anchors.fill: parent
            source: imageContent.visible
                ? (imageContent.useActiveMatrixTimelineSource
                    ? ("image://MxcImage/matrix-timeline:" + imageContent.eventId)
                    : (imageContent.url.replace("mxc://", "image://MxcImage/") + (imageContent.room ? "?room=" + imageContent.room.roomId : "")))
                : ""
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
