// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import cc.etke.komai
import Qt5Compat.GraphicalEffects

AbstractButton {
    required property int type
    required property int originalWidth
    required property int originalHeight
    required property double proportionalHeight
    required property string url
    required property string blurhash
    required property string body
    required property string filename
    required property string eventId
    required property int containerHeight
    property double divisor: EventDelegateChooser.isReply ? 10 : 4
    property int tempWidth: originalWidth < 1 ? 400 : originalWidth
    readonly property double safeProportionalHeight: proportionalHeight > 0
                                                   ? proportionalHeight
                                                   : ((originalWidth > 0 && originalHeight > 0) ? (originalHeight / originalWidth) : 1.0)
    // Bubble layout resolves width from delegates' implicitWidth. Provide explicit media sizing here
    // so image messages don't collapse to near-zero width in bubble style.
    implicitWidth: Math.max(1, Math.round(tempWidth * Math.min((containerHeight / divisor) / (tempWidth * safeProportionalHeight), 1)))
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: Math.max(1, Math.round(width * safeProportionalHeight))
    implicitHeight: height

    readonly property var roomContext: (typeof room !== "undefined") ? room : null
    property bool showImage: roomContext ? roomContext.showImage() : true
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

    EventDelegateChooser.keepAspectRatio: true
    EventDelegateChooser.maxWidth: originalWidth
    EventDelegateChooser.maxHeight: containerHeight / divisor
    EventDelegateChooser.aspectRatio: proportionalHeight

    hoverEnabled: true
    enabled: !EventDelegateChooser.isReply

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

    // A non-empty body that doesn't look like a filename is treated as a real caption
    readonly property bool hasCaption: body.length > 0 && !body.match(/\.\w{2,5}$/)

    property int metadataWidth
    property bool fitsMetadata: parent != null ? (parent.width - width) > metadataWidth+4 : false

    onClicked: {
        if (!roomContext)
            return;

        Settings.timelineMediaOpenImagesExternal ? roomContext.openMedia(eventId) : TimelineManager.openImageOverlayWithContext(roomContext, url, eventId, originalWidth, proportionalHeight, timeline, timelineView);
    }

    Item {
        id: imageClipper

        anchors.fill: parent
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: imageClipper.width
                height: imageClipper.height
                radius: 8
            }
        }

        Image {
            id: img

            visible: !mxcimage.loaded
            anchors.fill: parent
            source: (url != "" && showImage) ? (url.replace("mxc://", "image://MxcImage/") + "?scale") : ""
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignLeft
            smooth: true
            mipmap: true

            sourceSize.width: Math.min(Screen.desktopAvailableWidth, originalWidth < 1 ? Screen.desktopAvailableWidth : originalWidth) * Screen.devicePixelRatio
            sourceSize.height: Math.min(Screen.desktopAvailableHeight, (originalWidth < 1 ? Screen.desktopAvailableHeight : originalWidth*proportionalHeight)) * Screen.devicePixelRatio
        }

        MxcAnimatedImage {
            id: mxcimage

            visible: loaded
            roomm: roomContext
            play: !Settings.timelineMediaAnimateOnHover || imageClipper.parent.hovered
            eventId: showImage ? imageClipper.parent.eventId : ""

            anchors.fill: parent
        }

        Image {
            id: blurhash_

            source: hasValidBlurhash ? ("image://blurhash/" + encodeURIComponent(blurhash)) : ("image://colorimage/:/icons/icons/ui/image-failed.svg?" + palette.buttonText)
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            sourceSize.width: hasValidBlurhash ? parent.width * Screen.devicePixelRatio : Math.min(parent.width, parent.height)
            sourceSize.height: hasValidBlurhash ? parent.height * Screen.devicePixelRatio : Math.min(parent.width, parent.height)

            anchors.fill: parent
        }

        Item {
            id: overlay

            anchors.fill: parent

            visible: hasCaption || imageClipper.parent.hovered

            Rectangle {
                id: container

                width: parent.width
                implicitHeight: imgcaption.implicitHeight
                anchors.bottom: overlay.bottom
                color: palette.window
                opacity: 0.75
            }

            Text {
                id: imgcaption

                anchors.fill: container
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                // See this MSC: https://github.com/matrix-org/matrix-doc/pull/2530
                text: hasCaption ? body : filename
                color: palette.text
            }

        }
    }

    Button {
        anchors.centerIn: parent
        visible: !showImage && !parent.EventDelegateChooser.isReply
        enabled: visible
        text: qsTr("Show")
        onClicked: {
            showImage = true;
        }
    }
}
