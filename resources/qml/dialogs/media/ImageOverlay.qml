// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Window 2.15
import Qt5Compat.GraphicalEffects

import "../../ui"
import "./components"

import cc.etke.komai 1.0

Window {
    id: imageOverlay

    ComponentCatalog {
        id: componentCatalog
    }

    required property string url
    required property string eventId
    required property Room room
    required property int originalWidth
    required property double proportionalHeight
    readonly property bool galleryMode: !!room && eventId !== ""
    property var timelineContext: null
    property var timelineViewContext: null
    property var popupParent: null
    property color modalOverlayColor: Qt.rgba(0.2, 0.2, 0.2, 0.66)
    property color actionButtonColor: "white"
    property color actionButtonHoverColor: actionButtonColor
    property color actionBarColor: Qt.rgba(0, 0, 0, 0.35)
    property color actionButtonHoverBackgroundColor: Qt.rgba(0, 0, 0, 0.45)
    property int actionBarHorizontalPadding: 0
    property int actionBarVerticalPadding: 0
    property int actionButtonIconSize: 24
    property int imageViewportGap: Komai.paddingLarge * 2
    property int imageCornerRadius: Komai.paddingMedium
    // Keep close reachable via top-right corner (Fitts's law).
    property int actionBarScreenInset: 0
    // Minimum number of images to keep available in each direction before
    // requesting backpagination from the server.
    readonly property int galleryPrefetchReserve: 5

    flags: Qt.FramelessWindowHint

    //visibility: Window.FullScreen
    color: modalOverlayColor
    Component.onCompleted: {
        Komai.setWindowRole(imageOverlay, "imageoverlay");
        hintTimer.start();
        ensureGalleryReserve();
    }
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(() => {
                imageOverlay.requestActivate();
                keyCatcher.forceActiveFocus();
            });
        }
    }

    Connections {
        target: imageOverlay.room
        function onFetchedMore() {
            imageOverlay.ensureGalleryReserve();
        }
    }

    function copyCurrentMedia()
    {
        if (room)
            room.copyMedia(eventId);
        else
            TimelineManager.copyImage(url);
    }

    function saveCurrentMedia()
    {
        if (room)
            room.saveMedia(eventId);
        else
            TimelineManager.saveMedia(url);
    }

    function openCurrentMediaExternally()
    {
        if (room && eventId)
            room.openMedia(eventId);
        else
            TimelineManager.openMedia(url);
    }

    function canForwardCurrentMessage()
    {
        return !!room && !!eventId;
    }

    function closeOverlaySoon()
    {
        // Defer closing to the next tick so we don't destroy the overlay from inside
        // an active button signal handler (Qt can fatal in nested event-loop paths).
        Qt.callLater(() => {
            imageOverlay.hide();
            imageOverlay.close();
        });
    }

    function firstVisibleActionButton()
    {
        return forwardButton.visible ? forwardButton : openButton;
    }

    function lastVisibleActionButton()
    {
        return closeButton;
    }

    function navigateTo(imageData) {
        if (!imageData || !imageData.eventId) return;
        imgContainer.scale = 1.0;
        imgContainer.x = Qt.binding(() => (imageOverlay.width - imgContainer.width) / 2);
        imgContainer.y = Qt.binding(() => (imageOverlay.height - imgContainer.height) / 2);
        imageOverlay.eventId = imageData.eventId;
        imageOverlay.url = imageData.url;
        imageOverlay.originalWidth = imageData.originalWidth ?? 0;
        imageOverlay.proportionalHeight = imageData.proportionalHeight ?? 0;
        ensureGalleryReserve();
    }

    function navigatePrev() {
        if (!galleryMode) return;
        navigateTo(room.adjacentImageEvent(eventId, -1));
    }

    function navigateNext() {
        if (!galleryMode) return;
        navigateTo(room.adjacentImageEvent(eventId, +1));
    }

    function ensureGalleryReserve() {
        if (!galleryMode) return;
        if (!room.canPaginateBack() || room.paginationInProgress) return;
        var behind = room.countNearbyImages(eventId, -1, galleryPrefetchReserve);
        if (behind < galleryPrefetchReserve) {
            console.log("[ImageOverlay] gallery reserve low (backward:", behind,
                        "/", galleryPrefetchReserve, ") — requesting backpagination");
            room.requestMore();
        }
    }

    function openForwardDialogForCurrentMessage(forwardRoom, forwardEventId, forwardTimeline, forwardTimelineView, forwardPopupParent)
    {
        const resolvedRoom = forwardRoom ?? room;
        const resolvedEventId = forwardEventId ?? eventId;
        const resolvedTimeline = forwardTimeline ?? timelineContext;
        const resolvedTimelineView = forwardTimelineView ?? timelineViewContext;
        const resolvedPopupParent = forwardPopupParent ?? popupParent;

        if (!resolvedRoom || !resolvedEventId)
            return;

        if (resolvedPopupParent && resolvedPopupParent.showForwardMessageDialog) {
            resolvedPopupParent.showForwardMessageDialog(resolvedRoom, resolvedEventId, resolvedTimeline, resolvedTimelineView);
            return;
        }

        const component = Qt.createComponent(componentCatalog.navigationForwardCompleterDialog);
        if (component.status !== Component.Ready) {
            console.error("Failed to create component: " + component.errorString());
            return;
        }

        const host = imageOverlay;
        const dialog = component.createObject(host, {
                "roomSource": resolvedRoom,
                "timelineSource": resolvedTimeline,
                "timelineViewSource": resolvedTimelineView,
                "showReplyPreview": !!resolvedTimeline && !!resolvedTimelineView
            });
        if (!dialog) {
            console.error("Failed to create ForwardCompleter object");
            return;
        }

        dialog.setMessageEventId(resolvedEventId);
        dialog.open();
        if (dialog.aboutToHide !== undefined)
            dialog.aboutToHide.connect(() => dialog.destroy(1000));
    }

    Shortcut {
        sequences: [StandardKey.Cancel, "Escape"]
        context: Qt.ApplicationShortcut
        onActivated: imageOverlay.close()
        onActivatedAmbiguously: imageOverlay.close()
    }

    Shortcut {
        sequences: [StandardKey.Copy]
        onActivated: imageOverlay.copyCurrentMedia()
    }

    Item {
        id: keyCatcher

        anchors.fill: parent
        focus: true

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Escape) {
                event.accepted = true;
                imageOverlay.close();
                return;
            }

            if (imageOverlay.galleryMode) {
                if (event.key === Qt.Key_Left)  { event.accepted = true; imageOverlay.navigatePrev(); return; }
                if (event.key === Qt.Key_Right) { event.accepted = true; imageOverlay.navigateNext(); return; }
            }

            const isTab = event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab;
            const hasNavigationModifier = event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier);
            if (isTab && !hasNavigationModifier && !actionsRow.containsActiveFocus) {
                const isBacktab = event.key === Qt.Key_Backtab || ((event.modifiers & Qt.ShiftModifier) && event.key === Qt.Key_Tab);
                const button = isBacktab ? imageOverlay.lastVisibleActionButton() : imageOverlay.firstVisibleActionButton();
                if (button) {
                    event.accepted = true;
                    button.forceActiveFocus(Qt.TabFocusReason);
                }
            }
        }
    }

    TapHandler {
        onSingleTapped: imageOverlay.close();
    }


    Item {
        id: imgContainer

        property int imgSrcWidth: (imageOverlay.originalWidth && imageOverlay.originalWidth > 100) ? imageOverlay.originalWidth : Screen.width
        property int imgSrcHeight: imageOverlay.proportionalHeight ? imgSrcWidth * imageOverlay.proportionalHeight : Screen.height
        property int viewportWidth: Math.max(1, imageOverlay.width - imageOverlay.imageViewportGap * 2)
        property int viewportHeight: Math.max(1, imageOverlay.height - imageOverlay.imageViewportGap * 2)
        readonly property bool staticImageReady: img.status === Image.Ready
        readonly property bool animatedImageReady: mxcimage.loaded
        readonly property bool mediaReady: staticImageReady || animatedImageReady
        readonly property bool mediaFailed: img.status === Image.Error && !animatedImageReady

        property double initialScale: Math.min(viewportHeight / imgSrcHeight, viewportWidth / imgSrcWidth, 1.0)

        height: imgSrcHeight * initialScale
        width: imgSrcWidth * initialScale

        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        Item {
            id: imageClipper

            anchors.fill: parent
            layer.enabled: true
            layer.effect: OpacityMask {
                maskSource: Rectangle {
                    width: imageClipper.width
                    height: imageClipper.height
                    radius: imageOverlay.imageCornerRadius
                }
            }

            Image {
                id: img

                visible: !mxcimage.loaded
                anchors.fill: parent
                source: url.replace("mxc://", "image://MxcImage/")
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                property bool loaded: status == Image.Ready
            }

            MxcAnimatedImage {
                id: mxcimage

                visible: loaded
                anchors.fill: parent
                roomm: imageOverlay.room
                play: !Settings.timelineMediaAnimateOnHover || mouseArea.hovered
                eventId: imageOverlay.eventId
            }
        }

        Spinner {
            anchors.centerIn: parent
            height: Math.max(40, Math.min(imgContainer.width, imgContainer.height) * 0.08)
            visible: !imgContainer.mediaReady && !imgContainer.mediaFailed
            running: visible
        }

        // Absorb taps on the image so they don't propagate to the root
        // TapHandler which closes the overlay.
        TapHandler {
            gesturePolicy: TapHandler.WithinBounds
        }

        onScaleChanged: {
            if (scale > 10) scale = 10;
            if (scale < 0.1) scale = 0.1
        }
    }

    Item {
        anchors.fill: parent


        PinchHandler {
            target: imgContainer
            maximumScale: 10
            minimumScale: 0.1
        }

        WheelHandler {
            property: "scale"
            // workaround for QTBUG-87646 / QTBUG-112394 / QTBUG-112432:
            // Magic Mouse pretends to be a trackpad but doesn't work with PinchHandler
            // and we don't yet distinguish mice and trackpads on Wayland either
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            target: imgContainer
        }

        DragHandler {
            target: imgContainer
        }

        HoverHandler {
            id: mouseArea
        }
    }



    // One-shot hint animation timer — fires 1s after the overlay opens.
    Timer {
        id: hintTimer
        interval: 1000
        repeat: false
        onTriggered: {
            if (!Settings.uiMotionAnimationsEnabled)
                return;
            if (prevHitArea.visible)
                prevShakeAnimation.start();
            if (nextHitArea.visible)
                nextShakeAnimation.start();
            closeShakeAnimation.start();
        }
    }

    // Left navigation hit area
    Rectangle {
        id: prevHitArea

        visible: imageOverlay.galleryMode && hasPrev
        anchors.left: parent.left
        anchors.top: actionBar.bottom
        anchors.topMargin: Komai.paddingLarge
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Komai.paddingLarge + actionBar.height
        width: actionBar.height
        radius: Komai.paddingMedium
        color: "transparent"

        property var prevImageData: imageOverlay.galleryMode ? room.adjacentImageEvent(imageOverlay.eventId, -1) : ({})
        readonly property bool hasPrev: !!prevImageData && !!prevImageData.eventId
        property bool hinting: false

        SequentialAnimation {
            id: prevShakeAnimation
            PropertyAction  { target: prevHitArea; property: "hinting"; value: true }
            NumberAnimation  { target: prevIcon; property: "rotation"; to: -15; duration: 50 }
            NumberAnimation  { target: prevIcon; property: "rotation"; to: 15;  duration: 80 }
            NumberAnimation  { target: prevIcon; property: "rotation"; to: -10; duration: 70 }
            NumberAnimation  { target: prevIcon; property: "rotation"; to: 10;  duration: 60 }
            NumberAnimation  { target: prevIcon; property: "rotation"; to: 0;   duration: 50 }
            PropertyAction  { target: prevHitArea; property: "hinting"; value: false }
        }

        // Button fills the whole bar
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: prevMouseArea.containsMouse ? actionButtonHoverBackgroundColor : actionBarColor

            // Square off the left corners so the bar sits flush against the screen edge.
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                width: prevHitArea.radius
                height: prevHitArea.radius
                color: parent.color
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: prevHitArea.radius
                height: prevHitArea.radius
                color: parent.color
            }

            Image {
                id: prevIcon
                anchors.centerIn: parent
                width: actionButtonIconSize
                height: actionButtonIconSize
                source: "image://colorimage/:/icons/icons/ui/angle-arrow-left.svg?" + (prevHitArea.hinting ? imageOverlay.palette.highlight : actionButtonColor)
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
            }
        }

        MouseArea {
            id: prevMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: imageOverlay.navigatePrev()
        }
    }

    // Right navigation hit area
    Rectangle {
        id: nextHitArea

        visible: imageOverlay.galleryMode && hasNext
        anchors.right: parent.right
        anchors.top: actionBar.bottom
        anchors.topMargin: Komai.paddingLarge
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Komai.paddingLarge + actionBar.height
        width: actionBar.height
        radius: Komai.paddingMedium
        color: "transparent"

        property var nextImageData: imageOverlay.galleryMode ? room.adjacentImageEvent(imageOverlay.eventId, +1) : ({})
        readonly property bool hasNext: !!nextImageData && !!nextImageData.eventId
        property bool hinting: false

        SequentialAnimation {
            id: nextShakeAnimation
            PropertyAction  { target: nextHitArea; property: "hinting"; value: true }
            NumberAnimation  { target: nextIcon; property: "rotation"; to: -15; duration: 50 }
            NumberAnimation  { target: nextIcon; property: "rotation"; to: 15;  duration: 80 }
            NumberAnimation  { target: nextIcon; property: "rotation"; to: -10; duration: 70 }
            NumberAnimation  { target: nextIcon; property: "rotation"; to: 10;  duration: 60 }
            NumberAnimation  { target: nextIcon; property: "rotation"; to: 0;   duration: 50 }
            PropertyAction  { target: nextHitArea; property: "hinting"; value: false }
        }

        // Button fills the whole bar
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: nextMouseArea.containsMouse ? actionButtonHoverBackgroundColor : actionBarColor

            // Square off the right corners so the bar sits flush against the screen edge.
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                width: nextHitArea.radius
                height: nextHitArea.radius
                color: parent.color
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: nextHitArea.radius
                height: nextHitArea.radius
                color: parent.color
            }

            Image {
                id: nextIcon
                anchors.centerIn: parent
                width: actionButtonIconSize
                height: actionButtonIconSize
                source: "image://colorimage/:/icons/icons/ui/collapsed.svg?" + (nextHitArea.hinting ? imageOverlay.palette.highlight : actionButtonColor)
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
            }
        }

        MouseArea {
            id: nextMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: imageOverlay.navigateNext()
        }
    }

    Rectangle {
        id: actionBar

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: actionBarScreenInset
        implicitWidth: actionsRow.implicitWidth + actionBarHorizontalPadding * 2
        implicitHeight: actionsRow.implicitHeight + actionBarVerticalPadding * 2
        width: implicitWidth
        height: implicitHeight
        color: actionBarColor
        radius: Komai.paddingMedium

        // Keep only three corners rounded so the bar can remain flush against
        // the right screen edge while retaining the original left-side shape.
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            width: actionBar.radius
            height: actionBar.radius
            color: actionBar.color
        }

        Row {
            id: actionsRow
            property int uniformActionWidth: Math.max(forwardButton.visible ? forwardButton.implicitWidth : 0,
                                                      openButton.implicitWidth,
                                                      copyButton.implicitWidth,
                                                      downloadButton.implicitWidth,
                                                      closeButton.implicitWidth)

            spacing: Komai.paddingLarge
            anchors.centerIn: parent

            ImageOverlayActionButton {
                id: forwardButton
                visible: imageOverlay.canForwardCurrentMessage()
                width: visible ? actionsRow.uniformActionWidth : 0
                KeyNavigation.tab: openButton
                KeyNavigation.backtab: closeButton

                iconSource: ":/icons/icons/ui/reply.svg"
                iconMirror: true
                labelText: qsTr("Forward")
                textColor: actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor
                iconSize: actionButtonIconSize

                onClicked: {
                    const forwardRoom = imageOverlay.room;
                    const forwardEventId = imageOverlay.eventId;
                    const forwardTimeline = imageOverlay.timelineContext;
                    const forwardTimelineView = imageOverlay.timelineViewContext;
                    const forwardPopupParent = imageOverlay.popupParent;

                    imageOverlay.hide();
                    imageOverlay.close();
                    Qt.callLater(() => imageOverlay.openForwardDialogForCurrentMessage(forwardRoom, forwardEventId, forwardTimeline, forwardTimelineView, forwardPopupParent));
                }
            }

            ImageOverlayActionButton {
                id: openButton
                width: actionsRow.uniformActionWidth
                KeyNavigation.tab: copyButton
                KeyNavigation.backtab: forwardButton.visible ? forwardButton : closeButton

                iconSource: ":/icons/icons/ui/open-externally.svg"
                labelText: qsTr("Open")
                textColor: actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor
                iconSize: actionButtonIconSize

                onClicked: {
                    const roomRef = imageOverlay.room;
                    const eventRef = imageOverlay.eventId;
                    const urlRef = imageOverlay.url;

                    // Run external-open after the overlay is queued to close so this handler
                    // returns quickly and does not keep UI objects alive across nested loops.
                    imageOverlay.closeOverlaySoon();
                    Qt.callLater(() => {
                        if (roomRef && eventRef)
                            roomRef.openMedia(eventRef);
                        else
                            TimelineManager.openMedia(urlRef);
                    });
                }
            }

            ImageOverlayActionButton {
                id: copyButton
                width: actionsRow.uniformActionWidth
                KeyNavigation.tab: downloadButton
                KeyNavigation.backtab: openButton

                iconSource: ":/icons/icons/ui/copy.svg"
                labelText: qsTr("Copy")
                textColor: actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor
                iconSize: actionButtonIconSize

                onClicked: {
                    const roomRef = imageOverlay.room;
                    const eventRef = imageOverlay.eventId;
                    const urlRef = imageOverlay.url;

                    // Keep copy action out of the immediate click handler for the same
                    // lifetime-safety reason as other actions.
                    imageOverlay.closeOverlaySoon();
                    Qt.callLater(() => {
                        if (roomRef && eventRef)
                            roomRef.copyMedia(eventRef);
                        else
                            TimelineManager.copyImage(urlRef);
                    });
                }
            }

            ImageOverlayActionButton {
                id: downloadButton
                width: actionsRow.uniformActionWidth
                KeyNavigation.tab: closeButton
                KeyNavigation.backtab: copyButton

                iconSource: ":/icons/icons/ui/download.svg"
                labelText: qsTr("Save")
                textColor: actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor
                iconSize: actionButtonIconSize

                onClicked: {
                    const roomRef = imageOverlay.room;
                    const eventRef = imageOverlay.eventId;
                    const urlRef = imageOverlay.url;

                    // Save opens a blocking native file dialog; defer it until after this
                    // click handler unwinds to avoid "destroyed while handler in progress".
                    imageOverlay.closeOverlaySoon();
                    Qt.callLater(() => {
                        if (roomRef && eventRef)
                            roomRef.saveMedia(eventRef);
                        else
                            TimelineManager.saveMedia(urlRef);
                    });
                }
            }

            ImageOverlayActionButton {
                id: closeButton
                width: actionsRow.uniformActionWidth
                KeyNavigation.tab: forwardButton.visible ? forwardButton : openButton
                KeyNavigation.backtab: downloadButton

                property bool hinting: false

                // Keep close action flat at the screen edge so the full actions row appears
                // as one continuous bar anchored to the right.
                flatTopRightCorner: true

                iconSource: ":/icons/icons/ui/dismiss.svg"
                labelText: qsTr("Close")
                textColor: hinting ? imageOverlay.palette.highlight : actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor
                iconSize: actionButtonIconSize

                SequentialAnimation {
                    id: closeShakeAnimation
                    PropertyAction  { target: closeButton; property: "hinting"; value: true }
                    NumberAnimation  { target: closeButton; property: "rotation"; to: -15; duration: 50 }
                    NumberAnimation  { target: closeButton; property: "rotation"; to: 15;  duration: 80 }
                    NumberAnimation  { target: closeButton; property: "rotation"; to: -10; duration: 70 }
                    NumberAnimation  { target: closeButton; property: "rotation"; to: 10;  duration: 60 }
                    NumberAnimation  { target: closeButton; property: "rotation"; to: 0;   duration: 50 }
                    PropertyAction  { target: closeButton; property: "hinting"; value: false }
                }

                onClicked: imageOverlay.closeOverlaySoon()
            }
        }
    }

}
