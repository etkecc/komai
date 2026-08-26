// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window

import "../../ui"
import "./components"

import cc.etke.komai 1.0

Window {
    id: mediaOverlay

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    ComponentCatalog {
        id: componentCatalog
    }

    required property string url
    required property string eventId
    property var room
    required property int originalWidth
    required property double proportionalHeight
    property int mediaType: -1
    property int mediaDuration: 0
    property string thumbnailUrl: ""
    readonly property bool isVideo: mediaType === MtxEvent.VideoMessage
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

    color: modalOverlayColor
    Component.onCompleted: {
        Komai.setWindowRole(mediaOverlay, "imageoverlay");
        hintTimer.start();
        ensureGalleryReserve();
        neighbourPrefetchTimer.restart();
        if (mediaOverlay.isVideo)
            videoContent.startDownload();
    }
    Component.onDestruction: {
        videoContent.stopPlayback();
    }
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(() => {
                mediaOverlay.requestActivate();
                keyCatcher.forceActiveFocus();
            });
        } else {
            // Immediately kill video playback when the overlay hides,
            // regardless of which code path triggered the close.
            videoContent.stopPlayback();
        }
    }

    Connections {
        target: mediaOverlay.room
        function onFetchedMore() {
            mediaOverlay.ensureGalleryReserve();
        }
    }

    // Hidden neighbour prefetch: loading each source through the shared image
    // provider warms its byte/SDK cache (and a tiny decode) so navigating prev/next
    // is instant. Tiny sourceSize keeps the decode and texture negligible; the goal
    // is to prime the fetch, not to hold a full-size image.
    Item {
        visible: false

        Repeater {
            model: mediaOverlay.neighbourPrefetchSources

            delegate: Image {
                required property string modelData

                source: modelData
                asynchronous: true
                cache: true
                sourceSize.width: 64
                sourceSize.height: 64
            }
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
        if (mediaOverlay.isVideo)
            videoContent.stopPlayback();
        Qt.callLater(() => {
            mediaOverlay.hide();
            mediaOverlay.close();
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

    function navigateTo(mediaData) {
        if (!mediaData || !mediaData.eventId) return;
        var wasVideo = mediaOverlay.isVideo;
        // Update mediaType FIRST so isVideo is correct before we stop the
        // player — otherwise auto-play handlers see the old type.
        mediaOverlay.mediaType = mediaData.type ?? -1;
        if (wasVideo)
            videoContent.stopPlayback();
        imgContainer.scale = 1.0;
        imgContainer.x = Qt.binding(() => (mediaOverlay.width - imgContainer.width) / 2);
        imgContainer.y = Qt.binding(() => (mediaOverlay.height - imgContainer.height) / 2);
        mediaOverlay.eventId = mediaData.eventId;
        mediaOverlay.url = mediaData.url;
        mediaOverlay.originalWidth = mediaData.originalWidth ?? 0;
        mediaOverlay.proportionalHeight = mediaData.proportionalHeight ?? 0;
        mediaOverlay.mediaDuration = mediaData.duration ?? 0;
        mediaOverlay.thumbnailUrl = mediaData.thumbnailUrl ?? "";
        ensureGalleryReserve();
        // Start download for newly navigated-to video
        if (mediaOverlay.isVideo)
            videoContent.startDownload();
    }

    function navigatePrev() {
        if (!galleryMode) return;
        var data = room.adjacentMediaEvent(eventId, -1);
        if (data && data.eventId)
            navigateTo(data);
        else
            prevShakeAnimation.start();
    }

    function navigateNext() {
        if (!galleryMode) return;
        var data = room.adjacentMediaEvent(eventId, +1);
        if (data && data.eventId)
            navigateTo(data);
        else
            nextShakeAnimation.start();
    }

    function ensureGalleryReserve() {
        if (!galleryMode) return;
        if (!room.canPaginateBack() || room.paginationInProgress) return;
        var behind = room.countNearbyMedia(eventId, -1, galleryPrefetchReserve);
        if (behind < galleryPrefetchReserve) {
            console.log("[ImageOverlay] gallery reserve low (backward:", behind,
                        "/", galleryPrefetchReserve, ") — requesting backpagination");
            room.requestMore();
        }
    }

    // Full-quality provider URL for a neighbour media descriptor (from
    // adjacentMediaEvent), or "" when it can't/shouldn't be prewarmed. Images and
    // stickers only: a video's ?full would pull the whole file. Mirrors the URL
    // forms in ImageOverlayImageContent.
    function fullSourceForMedia(data) {
        if (!data || !data.eventId || String(data.eventId).length === 0)
            return "";
        var type = Number(data.type);
        if (type !== MtxEvent.ImageMessage && type !== MtxEvent.Sticker)
            return "";
        if (room && room.isActiveMatrixTimelineRoom === true)
            return "image://MxcImage/matrix-timeline:" + data.eventId + "?full";
        if (data.url && String(data.url).length > 0)
            return data.url.replace("mxc://", "image://MxcImage/") + "?full"
                + (room ? "&room=" + room.roomId : "");
        return "";
    }

    // Provider URLs of the immediate prev/next media, fed to the hidden prefetch
    // Images below. Recomputed off the UI thread's critical path (via the timer)
    // so it doesn't compete with the current image's initial load.
    property var neighbourPrefetchSources: []
    function refreshNeighbourPrefetch() {
        if (!galleryMode) {
            neighbourPrefetchSources = [];
            return;
        }
        var out = [];
        var prev = fullSourceForMedia(room.adjacentMediaEvent(eventId, -1));
        var next = fullSourceForMedia(room.adjacentMediaEvent(eventId, +1));
        if (prev.length > 0) out.push(prev);
        if (next.length > 0) out.push(next);
        neighbourPrefetchSources = out;
    }

    // Debounce so a fast walk through the gallery doesn't fire prefetches for
    // every intermediate item, and so the current image loads first.
    Timer {
        id: neighbourPrefetchTimer
        interval: 250
        onTriggered: mediaOverlay.refreshNeighbourPrefetch()
    }
    onEventIdChanged: neighbourPrefetchTimer.restart()

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

        const host = mediaOverlay;
        const dialog = component.createObject(host, {
                "roomSource": resolvedRoom,
                // Qt.binding() keeps this tracking the live viewport width; a
                // plain value here would freeze at whatever it was when the
                // dialog opened and never follow a later window resize.
                "dialogViewportWidth": Qt.binding(() => host.width),
                "modalOverlayColor": mediaOverlay.modalOverlayColor,
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
        onActivated: mediaOverlay.closeOverlaySoon()
        onActivatedAmbiguously: mediaOverlay.closeOverlaySoon()
    }

    Shortcut {
        sequences: [StandardKey.Copy]
        onActivated: mediaOverlay.copyCurrentMedia()
    }

    // --- Visual tree ---
    // Stacking order (bottom to top):
    //   1. keyCatcher (focus/key handling, no visuals)
    //   2. closeMouseArea (behind everything, closes on click)
    //   3. imgContainer (media content — image or video)
    //   4. nav buttons (prev/next, on top for click handling)
    //   5. action bar (Forward/Open/Copy/Save/Close)
    //
    // Each content component (ImageContent, VideoContent) has its own
    // MouseArea that absorbs clicks within its bounds. This prevents
    // closeMouseArea from firing when clicking on the media.

    Item {
        id: keyCatcher

        anchors.fill: parent
        focus: true

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Escape) {
                event.accepted = true;
                mediaOverlay.closeOverlaySoon();
                return;
            }

            if (event.key === Qt.Key_Space && mediaOverlay.isVideo) {
                event.accepted = true;
                videoContent.togglePlayback();
                return;
            }

            if (mediaOverlay.galleryMode) {
                if (event.key === Qt.Key_Left)  { event.accepted = true; mediaOverlay.navigatePrev(); return; }
                if (event.key === Qt.Key_Right) { event.accepted = true; mediaOverlay.navigateNext(); return; }

                const hasNavigationModifier = event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier);
                if (!hasNavigationModifier) {
                    if (LayoutAgnosticKeys.matchesLatinKey(LayoutAgnosticKeys.LatinKey.H, event.key, event.nativeScanCode)) {
                        event.accepted = true;
                        mediaOverlay.navigatePrev();
                        return;
                    }
                    if (LayoutAgnosticKeys.matchesLatinKey(LayoutAgnosticKeys.LatinKey.L, event.key, event.nativeScanCode)) {
                        event.accepted = true;
                        mediaOverlay.navigateNext();
                        return;
                    }
                }
            }

            const isTab = event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab;
            const hasNavigationModifier = event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier);
            if (isTab && !hasNavigationModifier && !actionsRow.containsActiveFocus) {
                const isBacktab = event.key === Qt.Key_Backtab || ((event.modifiers & Qt.ShiftModifier) && event.key === Qt.Key_Tab);
                const button = isBacktab ? mediaOverlay.lastVisibleActionButton() : mediaOverlay.firstVisibleActionButton();
                if (button) {
                    event.accepted = true;
                    button.forceActiveFocus(Qt.TabFocusReason);
                }
            }
        }
    }

    // Close overlay when clicking empty space (outside media content).
    // Content components absorb their own clicks so this only fires
    // for clicks on the dark backdrop.
    MouseArea {
        anchors.fill: parent
        onClicked: mediaOverlay.closeOverlaySoon()
    }

    Item {
        id: imgContainer

        property int imgSrcWidth: (mediaOverlay.originalWidth && mediaOverlay.originalWidth > 100) ? mediaOverlay.originalWidth
                                 : (imageContent.sourceWidth > 0 ? imageContent.sourceWidth : Screen.width)
        property int imgSrcHeight: mediaOverlay.proportionalHeight ? imgSrcWidth * mediaOverlay.proportionalHeight
                                 : (imageContent.sourceHeight > 0 ? imageContent.sourceHeight : Screen.height)
        property int viewportWidth: Math.max(1, mediaOverlay.width - mediaOverlay.imageViewportGap * 2)
        // Reserve space for the action bar at the top so media doesn't appear behind it.
        property int viewportHeight: Math.max(1, mediaOverlay.height - mediaOverlay.imageViewportGap * 2 - actionBar.height)

        property double initialScale: Math.min(viewportHeight / imgSrcHeight, viewportWidth / imgSrcWidth, 1.0)

        height: imgSrcHeight * initialScale
        width: imgSrcWidth * initialScale

        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        // --- Content components ---
        // Only one is visible at a time. Each owns its input handling.

        ImageOverlayImageContent {
            id: imageContent

            visible: !mediaOverlay.isVideo
            anchors.fill: parent
            url: mediaOverlay.url
            eventId: mediaOverlay.eventId
            room: mediaOverlay.room
            cornerRadius: mediaOverlay.imageCornerRadius
            animateOnHover: Settings.timelineMediaAnimateOnHover
            hovered: mouseArea.hovered
            zoomScale: imgContainer.scale
            nativeWidth: mediaOverlay.originalWidth > 100 ? mediaOverlay.originalWidth : 0
            nativeHeight: (mediaOverlay.originalWidth > 100 && mediaOverlay.proportionalHeight > 0)
                ? Math.round(mediaOverlay.originalWidth * mediaOverlay.proportionalHeight) : 0
        }

        ImageOverlayVideoContent {
            id: videoContent

            visible: mediaOverlay.isVideo
            anchors.fill: parent
            room: mediaOverlay.room
            eventId: mediaOverlay.isVideo ? mediaOverlay.eventId : ""
            thumbnailUrl: mediaOverlay.thumbnailUrl
            mediaDuration: mediaOverlay.mediaDuration
            cornerRadius: mediaOverlay.imageCornerRadius
        }

        // Image zoom handlers — only active for images.
        // Placed directly in imgContainer (targeting itself) so there's
        // no interaction layer blocking input to content components.
        PinchHandler {
            target: imgContainer
            maximumScale: 10
            minimumScale: 0.1
            enabled: !mediaOverlay.isVideo
        }

        WheelHandler {
            property: "scale"
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            target: imgContainer
            enabled: !mediaOverlay.isVideo
        }

        DragHandler {
            target: imgContainer
            enabled: !mediaOverlay.isVideo
        }

        HoverHandler {
            id: mouseArea
        }

        onScaleChanged: {
            if (scale > 10) scale = 10;
            if (scale < 0.1) scale = 0.1
        }
    }

    // One-shot hint animation timer — fires 1s after the overlay opens.
    Timer {
        id: hintTimer
        interval: 400
        repeat: false
        onTriggered: {
            if (!Settings.uiMotionAnimationsEnabled)
                return;
            if (prevHitArea.visible)
                prevHintAnimation.start();
            if (nextHitArea.visible)
                nextHintAnimation.start();
            closeHintAnimation.start();
        }
    }

    // Left navigation hit area — always visible in gallery mode.
    // Shows a chevron when there's a previous item, prohibited icon at the edge.
    Rectangle {
        id: prevHitArea

        visible: mediaOverlay.galleryMode
        anchors.left: parent.left
        anchors.top: actionBar.bottom
        anchors.topMargin: Komai.paddingLarge
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Komai.paddingLarge + actionBar.height
        width: actionBar.height
        radius: Komai.paddingMedium
        color: "transparent"

        property var prevImageData: mediaOverlay.galleryMode ? room.adjacentMediaEvent(mediaOverlay.eventId, -1) : ({})
        readonly property bool hasPrev: !!prevImageData && !!prevImageData.eventId
        property bool hinting: false

        SequentialAnimation {
            id: prevHintAnimation
            PropertyAction  { target: prevHitArea; property: "hinting"; value: true }
            PauseAnimation  { duration: 600 }
            PropertyAction  { target: prevHitArea; property: "hinting"; value: false }
        }

        // Button fills the whole bar
        Rectangle {
            id: prevButtonBg
            anchors.fill: parent
            radius: parent.radius
            color: prevHitArea.hinting ? Qt.rgba(mediaOverlay.palette.highlight.r, mediaOverlay.palette.highlight.g, mediaOverlay.palette.highlight.b, 0.35)
                 : prevMouseArea.containsMouse && prevHitArea.hasPrev ? actionButtonHoverBackgroundColor
                 : actionBarColor

            Behavior on color { ColorAnimation { duration: 250 } }

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

            Item {
                anchors.centerIn: parent
                width: actionButtonIconSize
                height: actionButtonIconSize
                clip: false

                Image {
                    id: prevIcon
                    width: parent.width
                    height: parent.height
                    source: prevHitArea.hasPrev
                        ? "image://colorimage/:/icons/icons/ui/angle-arrow-left.svg?" + actionButtonColor
                        : "image://colorimage/:/icons/icons/ui/prohibited.svg?" + actionButtonColor
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                    opacity: prevHitArea.hasPrev ? 1.0 : 0.4

                    SequentialAnimation {
                        id: prevShakeAnimation
                        NumberAnimation { target: prevIcon; property: "x"; to: -4; duration: 40 }
                        NumberAnimation { target: prevIcon; property: "x"; to: 4; duration: 70 }
                        NumberAnimation { target: prevIcon; property: "x"; to: -3; duration: 60 }
                        NumberAnimation { target: prevIcon; property: "x"; to: 0; duration: 50 }
                    }
                }
            }
        }

        MouseArea {
            id: prevMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: prevHitArea.hasPrev ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: {
                if (prevHitArea.hasPrev)
                    mediaOverlay.navigatePrev();
                else
                    prevShakeAnimation.start();
            }
        }
    }

    // Right navigation hit area — always visible in gallery mode.
    // Shows a chevron when there's a next item, prohibited icon at the edge.
    Rectangle {
        id: nextHitArea

        visible: mediaOverlay.galleryMode
        anchors.right: parent.right
        anchors.top: actionBar.bottom
        anchors.topMargin: Komai.paddingLarge
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Komai.paddingLarge + actionBar.height
        width: actionBar.height
        radius: Komai.paddingMedium
        color: "transparent"

        property var nextImageData: mediaOverlay.galleryMode ? room.adjacentMediaEvent(mediaOverlay.eventId, +1) : ({})
        readonly property bool hasNext: !!nextImageData && !!nextImageData.eventId
        property bool hinting: false

        SequentialAnimation {
            id: nextHintAnimation
            PropertyAction  { target: nextHitArea; property: "hinting"; value: true }
            PauseAnimation  { duration: 600 }
            PropertyAction  { target: nextHitArea; property: "hinting"; value: false }
        }

        // Button fills the whole bar
        Rectangle {
            id: nextButtonBg
            anchors.fill: parent
            radius: parent.radius
            color: nextHitArea.hinting ? Qt.rgba(mediaOverlay.palette.highlight.r, mediaOverlay.palette.highlight.g, mediaOverlay.palette.highlight.b, 0.35)
                 : nextMouseArea.containsMouse && nextHitArea.hasNext ? actionButtonHoverBackgroundColor
                 : actionBarColor

            Behavior on color { ColorAnimation { duration: 250 } }

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

            Item {
                anchors.centerIn: parent
                width: actionButtonIconSize
                height: actionButtonIconSize
                clip: false

                Image {
                    id: nextIcon
                    width: parent.width
                    height: parent.height
                    source: nextHitArea.hasNext
                        ? "image://colorimage/:/icons/icons/ui/collapsed.svg?" + actionButtonColor
                        : "image://colorimage/:/icons/icons/ui/prohibited.svg?" + actionButtonColor
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                    opacity: nextHitArea.hasNext ? 1.0 : 0.4

                    SequentialAnimation {
                        id: nextShakeAnimation
                        NumberAnimation { target: nextIcon; property: "x"; to: 4; duration: 40 }
                        NumberAnimation { target: nextIcon; property: "x"; to: -4; duration: 70 }
                        NumberAnimation { target: nextIcon; property: "x"; to: 3; duration: 60 }
                        NumberAnimation { target: nextIcon; property: "x"; to: 0; duration: 50 }
                    }
                }
            }
        }

        MouseArea {
            id: nextMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: nextHitArea.hasNext ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: {
                if (nextHitArea.hasNext)
                    mediaOverlay.navigateNext();
                else
                    nextShakeAnimation.start();
            }
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

            // Flush buttons (no inter-button gap), matching the room header and the
            // Element Call control bars: each button is a compact icon-left /
            // label-right pill that sizes to its own content. Invisible buttons
            // (e.g. Forward outside a room) are skipped by the Row automatically.
            spacing: 0
            anchors.centerIn: parent

            ImageOverlayActionButton {
                id: forwardButton
                visible: mediaOverlay.canForwardCurrentMessage()
                KeyNavigation.tab: openButton
                KeyNavigation.backtab: closeButton

                iconSource: ":/icons/icons/ui/reply.svg"
                iconMirror: true
                labelText: qsTr("Forward")
                textColor: actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor

                onClicked: {
                    const forwardRoom = mediaOverlay.room;
                    const forwardEventId = mediaOverlay.eventId;
                    const forwardTimeline = mediaOverlay.timelineContext;
                    const forwardTimelineView = mediaOverlay.timelineViewContext;
                    const forwardPopupParent = mediaOverlay.popupParent;

                    mediaOverlay.hide();
                    mediaOverlay.close();
                    Qt.callLater(() => mediaOverlay.openForwardDialogForCurrentMessage(forwardRoom, forwardEventId, forwardTimeline, forwardTimelineView, forwardPopupParent));
                }
            }

            ImageOverlayActionButton {
                id: openButton
                KeyNavigation.tab: copyButton
                KeyNavigation.backtab: forwardButton.visible ? forwardButton : closeButton

                iconSource: ":/icons/icons/ui/open-externally.svg"
                labelText: qsTr("Open")
                textColor: actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor

                onClicked: {
                    const roomRef = mediaOverlay.room;
                    const eventRef = mediaOverlay.eventId;
                    const urlRef = mediaOverlay.url;

                    mediaOverlay.closeOverlaySoon();
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
                KeyNavigation.tab: downloadButton
                KeyNavigation.backtab: openButton

                iconSource: ":/icons/icons/ui/copy.svg"
                labelText: qsTr("Copy")
                textColor: actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor

                onClicked: {
                    const roomRef = mediaOverlay.room;
                    const eventRef = mediaOverlay.eventId;
                    const urlRef = mediaOverlay.url;

                    mediaOverlay.closeOverlaySoon();
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
                KeyNavigation.tab: closeButton
                KeyNavigation.backtab: copyButton

                iconSource: ":/icons/icons/ui/download.svg"
                labelText: qsTr("Save")
                textColor: actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor

                onClicked: {
                    const roomRef = mediaOverlay.room;
                    const eventRef = mediaOverlay.eventId;
                    const urlRef = mediaOverlay.url;

                    mediaOverlay.closeOverlaySoon();
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
                KeyNavigation.tab: forwardButton.visible ? forwardButton : openButton
                KeyNavigation.backtab: downloadButton

                property bool hinting: false

                flatTopRightCorner: true

                iconSource: ":/icons/icons/ui/dismiss.svg"
                labelText: qsTr("Close")
                textColor: actionButtonColor
                hoverIconColor: actionButtonHoverColor
                hoverTextColor: actionButtonHoverColor
                hoverBackgroundColor: actionButtonHoverBackgroundColor

                background: Rectangle {
                    radius: Komai.paddingMedium
                    color: closeButton.hinting ? Qt.rgba(mediaOverlay.palette.highlight.r, mediaOverlay.palette.highlight.g, mediaOverlay.palette.highlight.b, 0.35)
                         : closeButton.hovered || closeButton.pressed || closeButton.visualFocus ? closeButton.hoverBackgroundColor
                         : "transparent"

                    Behavior on color { ColorAnimation { duration: 250 } }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        width: parent.radius
                        height: parent.radius
                        color: parent.color
                        visible: closeButton.flatTopRightCorner && (closeButton.hinting || closeButton.hovered || closeButton.pressed || closeButton.visualFocus)
                    }
                }

                SequentialAnimation {
                    id: closeHintAnimation
                    PropertyAction  { target: closeButton; property: "hinting"; value: true }
                    PauseAnimation  { duration: 600 }
                    PropertyAction  { target: closeButton; property: "hinting"; value: false }
                }

                onClicked: mediaOverlay.closeOverlaySoon()
            }
        }
    }

}
