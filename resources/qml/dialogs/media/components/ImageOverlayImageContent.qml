// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Effects

import "../../../ui"
import "../../../ui/media"

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

    // Rounded corners only matter at rest; once magnified they sit off-screen.
    // Dropping the mask layer while zoomed lets imgFull rasterise straight from
    // its texture (see imageClipper.layer).
    readonly property bool roundCorners: zoomScale <= 1.01
    readonly property bool zoomedIn: zoomScale > 1.01

    // True pixel dimensions of the source (0 when unknown). Cap imgRest's decode
    // so we never Lanczos-upscale past the original.
    property int nativeWidth: 0
    property int nativeHeight: 0
    readonly property bool hasNativeSize: nativeWidth > 0 && nativeHeight > 0

    // Shared provider URL. ?full => the provider returns the original media,
    // Lanczos-downscaled to the requested sourceSize (or the full image when no
    // size is requested). imgRest and imgFull both use it; Qt caches them
    // separately by (source, sourceSize), so each is fetched/decoded once.
    readonly property string mediaSource: imageContent.visible
        ? (imageContent.useActiveMatrixTimelineSource
            ? ("image://MxcImage/matrix-timeline:" + imageContent.eventId + "?full")
            : (imageContent.url.replace("mxc://", "image://MxcImage/") + "?full" + (imageContent.room ? "&room=" + imageContent.room.roomId : "")))
        : ""

    // Debounced on-screen pixel size for the resting (Lanczos) decode. The
    // container resizes through many intermediate values on a cold open (starting
    // sub-pixel), and binding imgRest.sourceSize straight to the live size
    // re-decodes the media at every step (including a useless 1x1). Sample the size
    // only once it stops changing; imgFull (native, loaded immediately) covers the
    // view until the crisp resting decode is ready. Reset on media change.
    property int restPixelWidth: 0
    property int restPixelHeight: 0
    function sampleRestPixelSize() {
        restPixelWidth = imageContent.hasNativeSize
            ? Math.min(imageContent.nativeWidth, Math.round(imageContent.width * Screen.devicePixelRatio))
            : Math.round(imageContent.width * Screen.devicePixelRatio);
        restPixelHeight = imageContent.hasNativeSize
            ? Math.min(imageContent.nativeHeight, Math.round(imageContent.height * Screen.devicePixelRatio))
            : Math.round(imageContent.height * Screen.devicePixelRatio);
    }
    onWidthChanged: restSizeDebounce.restart()
    onHeightChanged: restSizeDebounce.restart()

    Timer {
        id: restSizeDebounce
        interval: 80
        onTriggered: imageContent.sampleRestPixelSize()
    }

    readonly property bool mediaReady: staticImageReady || animatedImageReady
    readonly property bool mediaFailed: (imgRest.status === Image.Error || imgFull.status === Image.Error)
        && !animatedImageReady
    readonly property bool staticImageReady: imgRest.status === Image.Ready || imgFull.status === Image.Ready
    readonly property bool animatedImageReady: mxcimage.loaded

    // True once the media has loaded at least once, so the spinner shows only on
    // the initial load (and on gallery navigation), never on the instant
    // rest<->zoom texture swap. Reset when the media itself changes.
    property bool everReady: false
    onEventIdChanged: {
        everReady = false;
        // Force a fresh size sample for the new media rather than reusing the
        // previous one before the container re-lays-out.
        restPixelWidth = 0;
        restPixelHeight = 0;
        restSizeDebounce.restart();
    }
    onMediaReadyChanged: if (mediaReady) everReady = true

    // Natural dimensions, used by the overlay to size the container when the
    // caller did not supply original dimensions. imgFull carries no sourceSize,
    // so its implicit size is the true native resolution.
    readonly property int sourceWidth: mxcimage.loaded ? mxcimage.implicitWidth : imgFull.implicitWidth
    readonly property int sourceHeight: mxcimage.loaded ? mxcimage.implicitHeight : imgFull.implicitHeight

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

        // Full-resolution view for zooming: decoded once at native size (no
        // sourceSize) and magnified by the GPU, so zoom is instant and seamless
        // with no re-fetch or re-decode. Declared first so imgRest paints on top of
        // it. Also serves as the immediate at-rest placeholder until the crisp
        // resting decode is ready, so a cold open shows the photo right away.
        Image {
            id: imgFull

            visible: !mxcimage.loaded && (imageContent.zoomedIn || imgRest.status !== Image.Ready)
            anchors.fill: parent
            source: imageContent.mediaSource
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        // Crisp resting view: the original media Lanczos-downscaled (in the
        // provider) to the on-screen pixel size. Decoded once, at the debounced
        // display size (restPixelWidth/Height), so layout churn on open doesn't
        // spawn a decode per intermediate size. Shown when not zoomed, on top of
        // imgFull once ready.
        Image {
            id: imgRest

            visible: !mxcimage.loaded && !imageContent.zoomedIn && status === Image.Ready
            anchors.fill: parent
            // Only load once the debounced size has settled to a real value; before
            // that imgFull carries the view.
            source: (imageContent.restPixelWidth > 0 && imageContent.restPixelHeight > 0)
                ? imageContent.mediaSource
                : ""
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            sourceSize.width: imageContent.restPixelWidth
            sourceSize.height: imageContent.restPixelHeight
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

    // The image bytes arrive via the MxcImage provider (no player object to
    // carry progress), so watch the backend's download-progress registry for
    // this event while the initial load is pending.
    MediaDownloadProgressWatcher {
        id: downloadWatcher

        eventId: imageContent.useActiveMatrixTimelineSource ? imageContent.eventId : ""
        active: imageContent.visible && !imageContent.everReady && !imageContent.mediaFailed
    }

    // Indeterminate spinner until the download reports a total; the progress
    // ring takes over once a real percentage is available.
    Spinner {
        anchors.centerIn: parent
        height: Math.max(40, Math.min(parent.width, parent.height) * 0.08)
        visible: !imageContent.everReady && !imageContent.mediaFailed
                 && downloadWatcher.progress < 0
        running: visible
    }

    DownloadProgressIndicator {
        anchors.centerIn: parent
        width: Math.max(84, Math.min(parent.width, parent.height) * 0.15)
        visible: !imageContent.everReady && !imageContent.mediaFailed
                 && downloadWatcher.progress >= 0
        progress: downloadWatcher.progress
    }
}
