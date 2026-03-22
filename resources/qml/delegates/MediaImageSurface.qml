// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Qt5Compat.GraphicalEffects
import cc.etke.komai

Item {
    id: surface

    required property int originalWidth
    required property double safeProportionalHeight
    required property string url
    required property string blurhash
    required property string eventId
    property var roomContext: null
    property bool showImage: roomContext ? roomContext.showImage() : true
    property bool hovered: false
    property bool interactive: false
    property bool revealEnabled: true
    property int cornerRadius: 8

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

    Item {
        id: imageClipper

        anchors.fill: parent
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: imageClipper.width
                height: imageClipper.height
                radius: surface.cornerRadius
            }
        }

        Image {
            id: img

            visible: !mxcimage.loaded
            anchors.fill: parent
            source: (surface.url != "" && surface.showImage)
                ? (surface.url.replace("mxc://", "image://MxcImage/") + "?scale" + (surface.roomContext ? "&room=" + surface.roomContext.roomId : ""))
                : ""
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignLeft
            smooth: true
            mipmap: true

            sourceSize.width: Math.min(Screen.desktopAvailableWidth, surface.originalWidth < 1 ? Screen.desktopAvailableWidth : surface.originalWidth) * Screen.devicePixelRatio
            sourceSize.height: Math.min(Screen.desktopAvailableHeight, (surface.originalWidth < 1 ? Screen.desktopAvailableHeight : surface.originalWidth * surface.safeProportionalHeight)) * Screen.devicePixelRatio
        }

        MxcAnimatedImage {
            id: mxcimage

            visible: loaded
            roomm: surface.roomContext
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
