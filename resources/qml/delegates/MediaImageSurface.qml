// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick
import QtQuick.Window
import QtQuick.Effects
import cc.etke.komai

Item {
    id: surface

    required property int originalWidth
    required property double safeProportionalHeight
    required property string url
    required property string blurhash
    required property string eventId
    property string mimeType: ""
    property var roomContext: null
    property bool showImage: roomContext && typeof roomContext.showImage === "function"
        ? roomContext.showImage()
        : true
    property bool hovered: false
    property bool interactive: false
    property bool revealEnabled: true
    property int cornerRadius: 8
    readonly property bool hasRenderableGeometry: width > 0 && height > 0
    readonly property bool useActiveMatrixTimelineSource: !!roomContext
        && roomContext.isActiveMatrixTimelineRoom === true

    readonly property string blurhashAlphabet: "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#$%*+,-.:;=?@[]^_{|}~"
    readonly property bool blurOverlayActive: !!timeline && !!timeline.windowFocusBlurOverlay && timeline.windowFocusBlurOverlay.active
    readonly property bool hasValidBlurhash: {
        if (!blurhash || blurhash.length < 6)
            return false;

        let sizeFlag = -1;
        for (let i = 0; i < blurhash.length; ++i) {
            const value = blurhashAlphabet.indexOf(blurhash.charAt(i));
            if (value < 0)
                return false;
            if (i === 0)
                sizeFlag = value;
        }

        if (sizeFlag < 0)
            return false;

        const numY = Math.floor(sizeFlag / 9) + 1;
        const numX = (sizeFlag % 9) + 1;
        return blurhash.length === (4 + 2 * numX * numY);
    }

    signal activated()

    state: (img.status != Image.Ready || blurOverlayActive) ? "BlurhashVisible" : "ImageVisible"
    states: [
        State {
            name: "BlurhashVisible"

            PropertyChanges {
                blurhash_ {
                    opacity: (img.status != Image.Ready) || (blurOverlayActive && hasValidBlurhash) ? 1 : 0
                    visible: (img.status != Image.Ready) || (blurOverlayActive && hasValidBlurhash)
                }
            }

            PropertyChanges {
                img.opacity: 0
            }

            PropertyChanges {
                mxcimage.opacity: 0
            }
        },
        State {
            name: "ImageVisible"

            PropertyChanges {
                blurhash_ {
                    opacity: 0
                    visible: false
                }
            }

            PropertyChanges {
                img.opacity: 1
            }

            PropertyChanges {
                mxcimage.opacity: 1
            }
        }
    ]
    transitions: [
        Transition {
            from: "ImageVisible"
            to: "BlurhashVisible"
            reversible: true

            SequentialAnimation {
                PropertyAction {
                    target: blurhash_
                    property: "visible"
                }

                ParallelAnimation {
                    NumberAnimation {
                        target: blurhash_
                        property: "opacity"
                        duration: 300
                        easing.type: Easing.Linear
                    }

                    NumberAnimation {
                        target: img
                        property: "opacity"
                        duration: 300
                        easing.type: Easing.Linear
                    }

                }
            }
        }
    ]

    MouseArea {
        anchors.fill: parent
        enabled: surface.interactive && surface.showImage
        cursorShape: surface.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor

        onClicked: surface.activated()
    }

    Rectangle {
        id: imageMask

        anchors.fill: imageClipper
        radius: surface.cornerRadius
        layer.enabled: true
        visible: false
    }

    Item {
        id: imageClipper

        anchors.fill: parent
        layer.enabled: hasRenderableGeometry
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: imageMask
        }

        Image {
            id: img

            // Per-image retry state. A failed thumbnail fetch leaves the image in
            // Image.Error and Qt will not re-ask the provider for the same URL, so
            // _retryNonce changes the URL to force a fresh request on an
            // incremental backoff (see retryTimer). The blurhash placeholder stays
            // shown meanwhile, so retries only swap pixels in once one succeeds.
            property int _retryNonce: 0
            property int _retryAttempt: 0
            readonly property bool _hasNetworkSource: surface.url != "" && surface.showImage

            visible: !mxcimage.loaded
            anchors.fill: parent
            source: img._hasNetworkSource
                ? (surface.useActiveMatrixTimelineSource
                    ? ("image://MxcImage/matrix-timeline:" + surface.eventId + "?scale&_retry=" + img._retryNonce)
                    : (surface.url.replace("mxc://", "image://MxcImage/") + "?scale" + (surface.roomContext ? "&room=" + surface.roomContext.roomId : "") + "&_retry=" + img._retryNonce))
                : ""
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignLeft
            smooth: true
            mipmap: true

            sourceSize.width: Math.min(Screen.desktopAvailableWidth, surface.originalWidth < 1 ? Screen.desktopAvailableWidth : surface.originalWidth) * Screen.devicePixelRatio
            sourceSize.height: Math.min(Screen.desktopAvailableHeight, (surface.originalWidth < 1 ? Screen.desktopAvailableHeight : surface.originalWidth * surface.safeProportionalHeight)) * Screen.devicePixelRatio

            onStatusChanged: {
                if (!img._hasNetworkSource)
                    return;
                if (img.status === Image.Error) {
                    retryTimer.restart();
                } else if (img.status === Image.Ready) {
                    img._retryAttempt = 0;
                    retryTimer.stop();
                }
            }

            Timer {
                id: retryTimer

                repeat: false
                interval: Math.min(5000 * Math.pow(2, img._retryAttempt), 300000)
                onTriggered: {
                    img._retryAttempt += 1;
                    img._retryNonce += 1;
                }
            }

            // When connectivity is re-established, retry a failed thumbnail
            // immediately instead of waiting out its backoff window.
            Connections {
                target: TimelineManager

                function onIsConnectedChanged(connected) {
                    if (connected && img._hasNetworkSource && img.status === Image.Error) {
                        img._retryAttempt = 0;
                        img._retryNonce += 1;
                    }
                }
            }
        }

        MxcAnimatedImage {
            id: mxcimage

            visible: loaded
            roomm: surface.roomContext
            mimeTypeHint: surface.mimeType
            play: !Settings.timelineMediaAnimateOnHover || surface.hovered
            eventId: surface.showImage ? surface.eventId : ""

            anchors.fill: parent
        }

        Image {
            id: blurhash_

            source: hasValidBlurhash ? ("image://blurhash/" + encodeURIComponent(surface.blurhash)) : ("image://colorimage/:/icons/icons/ui/image-failed.svg?" + palette.buttonText)
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            sourceSize.width: hasValidBlurhash ? parent.width * Screen.devicePixelRatio : Math.min(parent.width, parent.height)
            sourceSize.height: hasValidBlurhash ? parent.height * Screen.devicePixelRatio : Math.min(parent.width, parent.height)

            anchors.fill: parent
        }
    }

    Components.KomaiButton {
        anchors.centerIn: parent
        visible: !surface.showImage && surface.revealEnabled
        enabled: visible
        text: qsTr("Show")

        onClicked: {
            surface.showImage = true;
        }
    }
}
