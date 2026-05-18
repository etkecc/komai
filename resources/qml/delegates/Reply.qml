// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Effects
import cc.etke.komai

Pane {
    id: r

    property color userColor: "red"
    property color roomColor: userColor
    required property var bubblePalette
    property bool keepFullText: false
    property var previewData: ({})
    property var roomModelOverride: null
    property var timelineViewOverride: null
    property var replyContextMenuOverride: null
    // Whether tapping/long-pressing the preview opens the original event or
    // its context menu. Callers that show the preview purely as informational
    // (forward dialog, the matrix-side replying-to header) set this to false
    // rather than `enabled: false` — disabling the root would cascade to the
    // inner Flickable + ScrollBar and break scrolling.
    property bool clickable: true

    required property string eventId

    property var room_: (typeof room !== "undefined") ? room : null
    property var timelineView_: timelineViewOverride !== null
        ? timelineViewOverride
        : ((typeof timelineView !== "undefined") ? timelineView : null)
    readonly property var effectiveReplyContextMenu: replyContextMenuOverride !== null ? replyContextMenuOverride : ((typeof replyContextMenu !== "undefined") ? replyContextMenu : null)
    readonly property var effectiveRoomContext: roomModelOverride !== null ? roomModelOverride : room_
    readonly property bool hasExplicitPreviewData: {
        if (!previewData)
            return false;
        return Object.keys(previewData).length > 0;
    }
    readonly property var roleDataSource: {
        if (roomModelOverride && typeof roomModelOverride.dataById === "function")
            return roomModelOverride;
        if (room_ && typeof room_.dataById === "function")
            return room_;
        return null;
    }
    readonly property var previewDataSource: {
        if (roomModelOverride && typeof roomModelOverride.previewDataForEvent === "function")
            return roomModelOverride;
        if (room_ && typeof room_.previewDataForEvent === "function")
            return room_;
        return null;
    }
    readonly property var previewFromRoleData: buildRolePreviewData()
    readonly property var previewFromDataSource: {
        if (!previewDataSource || !eventId)
            return ({});
        const preview = previewDataSource.previewDataForEvent(eventId);
        return (preview === undefined || preview === null) ? ({}) : preview;
    }
    readonly property var effectivePreviewData: {
        const merged = {};
        mergePreviewData(merged, previewFromRoleData);
        mergePreviewData(merged, previewFromDataSource);
        mergePreviewData(merged, previewData || {});
        if (merged.eventId === undefined || merged.eventId === null || String(merged.eventId).length === 0)
            merged.eventId = eventId;
        return merged;
    }
    readonly property int previewType: effectivePreviewData && effectivePreviewData.type !== undefined
        ? Number(effectivePreviewData.type)
        : MtxEvent.UnknownMessage
    readonly property bool usesCompactMediaPreview: previewType === MtxEvent.ImageMessage
        || previewType === MtxEvent.Sticker
        || previewType === MtxEvent.VideoMessage

    readonly property string previewEventId: String((effectivePreviewData && effectivePreviewData.eventId) || eventId || "")
    readonly property string previewBodyText: String((effectivePreviewData && effectivePreviewData.body) || "")
    readonly property string previewBodySummaryText: previewBodyText.trim()
    readonly property string previewFilename: {
        const explicitFilename = String((effectivePreviewData && effectivePreviewData.filename) || "");
        if (explicitFilename.length > 0)
            return explicitFilename;
        if (previewBodyText.length > 0)
            return previewBodyText;
        switch (previewType) {
        case MtxEvent.ImageMessage:
            return qsTr("Image");
        case MtxEvent.Sticker:
            return qsTr("Sticker");
        case MtxEvent.VideoMessage:
            return qsTr("Video");
        default:
            return qsTr("Attachment");
        }
    }
    readonly property string previewMimeType: String((effectivePreviewData && effectivePreviewData.mimetype) || "")
    readonly property string previewUrl: String((effectivePreviewData && effectivePreviewData.url) || "")
    readonly property string previewThumbnailUrl: String((effectivePreviewData && effectivePreviewData.thumbnailUrl) || "")
    readonly property string previewMediaUrl: previewType === MtxEvent.VideoMessage
        ? (previewThumbnailUrl.length > 0 ? previewThumbnailUrl : previewUrl)
        : (previewUrl.length > 0 ? previewUrl : previewThumbnailUrl)
    readonly property string previewBlurhash: String((effectivePreviewData && effectivePreviewData.blurhash) || "")
    readonly property int previewOriginalWidth: Math.max(0, Number((effectivePreviewData && effectivePreviewData.originalWidth) || 0))
    readonly property int previewOriginalHeight: Math.max(0, Number((effectivePreviewData && effectivePreviewData.originalHeight) || 0))
    readonly property real previewSafeProportionalHeight: {
        const explicitProportion = Number((effectivePreviewData && effectivePreviewData.proportionalHeight) || 0);
        if (explicitProportion > 0)
            return explicitProportion;
        if (previewOriginalWidth > 0 && previewOriginalHeight > 0)
            return previewOriginalHeight / previewOriginalWidth;
        return 0.75;
    }
    readonly property real compactMediaBubbleRoom: Math.max(
        64, maxWidth - leftPadding - rightPadding - Komai.paddingMedium * 4)
    // 144 is the thumbnail-size cap for typical reply previews. For banner-shaped
    // images (aspect wider than 6:1) this cap pins the natural-fit height to a
    // hair-thin strip (e.g., 1199x56 → 144x7), so lift it and let the image use
    // the full available bubble width — the height cap (72) still limits overall
    // size, and the image gains usable vertical pixels.
    readonly property bool compactMediaIsWideBanner: previewOriginalWidth > 0
        && previewOriginalHeight > 0
        && previewOriginalWidth > 6 * previewOriginalHeight
    readonly property real compactMediaMaxWidth: compactMediaIsWideBanner
        ? compactMediaBubbleRoom
        : Math.min(compactMediaBubbleRoom, 144)
    readonly property real compactMediaMaxHeight: limitHeight ? 56 : 72
    readonly property int compactMediaWidth: {
        if (previewOriginalWidth > 0 && previewOriginalHeight > 0) {
            const scale = Math.min(compactMediaMaxWidth / previewOriginalWidth,
                                   compactMediaMaxHeight / previewOriginalHeight,
                                   1.0);
            return Math.max(40, Math.round(previewOriginalWidth * scale));
        }

        return Math.round(Math.max(56, compactMediaMaxHeight / previewSafeProportionalHeight));
    }
    // Asymmetric floor: keep width >= 40 so tall/narrow images stay recognizable
    // next to caption text in the row. Height floor stays modest (16) since wide
    // banners now take full bubble width and naturally gain height; the floor
    // only catches super-extreme aspects in narrow bubbles.
    readonly property int compactMediaHeight: {
        if (previewOriginalWidth > 0 && previewOriginalHeight > 0) {
            const scale = Math.min(compactMediaMaxWidth / previewOriginalWidth,
                                   compactMediaMaxHeight / previewOriginalHeight,
                                   1.0);
            return Math.max(16, Math.round(previewOriginalHeight * scale));
        }

        return Math.round(compactMediaMaxHeight);
    }
    readonly property bool mediaPreviewHasCaption: {
        if (previewBodySummaryText.length === 0)
            return false;
        if (previewFilename.length > 0 && previewBodySummaryText === previewFilename.trim())
            return false;
        return !previewBodySummaryText.match(/\.[A-Za-z0-9]{2,8}$/);
    }
    readonly property string mediaPreviewSummaryText: mediaPreviewHasCaption ? previewBodySummaryText : ""
    // Use the external width cap rather than this item's current width to avoid
    // a width -> implicitWidth feedback loop when the bubble sizes itself from
    // the reply preview's implicit width.
    readonly property real compactPreviewContentWidthLimit: Math.max(0, maxWidth - leftPadding - rightPadding)
    readonly property real compactPreviewTextAvailableWidth: Math.max(0, compactPreviewContentWidthLimit - compactMediaWidth - Komai.paddingSmall)
    readonly property real compactPreviewTextWidth: mediaPreviewSummaryText.length > 0
        ? Math.min(compactPreviewTextMetrics.advanceWidth, compactPreviewTextAvailableWidth)
        : 0
    readonly property real compactPreviewImplicitWidth: compactMediaWidth
        + (compactPreviewTextWidth > 0 ? (Komai.paddingSmall + compactPreviewTextWidth) : 0)
    readonly property bool compactPreviewNeedsRoomContext: !!effectiveRoomContext
        && ((effectiveRoomContext.roomId !== undefined
                && String(effectiveRoomContext.roomId || "").length > 0)
            || typeof effectiveRoomContext.showImage === "function")
    readonly property var compactPreviewMediaRoomContext: compactPreviewNeedsRoomContext
        ? compactPreviewRoomContext
        : null
    readonly property string compactPreviewRoomId: compactPreviewMediaRoomContext
        && compactPreviewMediaRoomContext.roomId !== undefined
        ? String(compactPreviewMediaRoomContext.roomId || "")
        : ""
    readonly property bool useActiveMatrixTimelineSource: !!effectiveRoomContext
        && effectiveRoomContext.isActiveMatrixTimelineRoom === true
    readonly property string compactPreviewImageSource: {
        if (previewMediaUrl.length === 0)
            return "";

        if (useActiveMatrixTimelineSource && previewEventId.length > 0)
            return "image://MxcImage/matrix-timeline:" + previewEventId + "?scale";

        const providerSource = previewMediaUrl.replace("mxc://", "image://MxcImage/");
        return compactPreviewRoomId.length > 0
            ? (providerSource + "?scale&room=" + compactPreviewRoomId)
            : (providerSource + "?scale");
    }

    property string userId: String((effectivePreviewData && effectivePreviewData.userId) || "")
    property string userName: String((effectivePreviewData && effectivePreviewData.userName) || "")
    implicitHeight: replyContainer.height + topPadding + bottomPadding
    implicitWidth: replyContainer.implicitWidth + leftPadding + rightPadding

    leftPadding: Komai.paddingSmall + Komai.paddingMedium
    rightPadding: Komai.paddingMedium
    topPadding: Komai.paddingMedium
    bottomPadding: Komai.paddingMedium

    palette.window: bubblePalette.window
    palette.windowText: bubblePalette.windowText
    palette.base: bubblePalette.base
    palette.alternateBase: bubblePalette.alternateBase
    palette.text: bubblePalette.text
    palette.brightText: bubblePalette.brightText
    palette.button: bubblePalette.button
    palette.buttonText: bubblePalette.buttonText
    palette.light: bubblePalette.light
    palette.mid: bubblePalette.mid
    palette.dark: bubblePalette.dark
    palette.highlight: bubblePalette.highlight
    palette.highlightedText: bubblePalette.highlightedText
    palette.link: bubblePalette.link
    palette.toolTipBase: bubblePalette.toolTipBase
    palette.toolTipText: bubblePalette.toolTipText
    palette.inactive.text: bubblePalette.buttonText
    palette.inactive.windowText: bubblePalette.buttonText
    palette.inactive.buttonText: bubblePalette.buttonText

    required property int maxWidth
    property bool limitHeight: false
    // Additional content height (px) that callers can add to the legacy
    // timelineView/10 ceiling used when limitHeight is true. Lets the
    // composer's "Replying to ..." popup grow the preview when the user
    // drags its top edge. Default 0 → behaviour unchanged for all other
    // callers (timeline bubbles, forward dialog, message actions dialog).
    property real additionalHeight: 0

    TextMetrics {
        id: compactPreviewTextMetrics

        text: mediaPreviewSummaryText
        font: mediaSummaryProbe.font
    }

    Text {
        id: mediaSummaryProbe

        visible: false
        text: mediaPreviewSummaryText
        wrapMode: Text.Wrap
        maximumLineCount: r.keepFullText ? 3 : 2
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: r.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    function roleValue(role, fallbackValue) {
        if (!roleDataSource || !eventId)
            return fallbackValue;

        const value = roleDataSource.dataById(eventId, role, "");
        return (value === undefined || value === null) ? fallbackValue : value;
    }

    function mergePreviewData(target, source) {
        if (!source)
            return;

        for (const key of Object.keys(source)) {
            const value = source[key];
            if (value === undefined || value === null)
                continue;
            if (typeof value === "string" && value.length === 0)
                continue;
            target[key] = value;
        }
    }

    function buildRolePreviewData() {
        if (!roleDataSource || !eventId)
            return ({});

        return {
            "eventId": String(roleValue(Room.EventId, eventId)),
            "type": Number(roleValue(Room.Type, MtxEvent.UnknownMessage)),
            "userId": String(roleValue(Room.UserId, "")),
            "userName": String(roleValue(Room.UserName, "")),
            "body": String(roleValue(Room.Body, "")),
            "formattedBody": String(roleValue(Room.FormattedBody, "")),
            "isOnlyEmoji": Number(roleValue(Room.IsOnlyEmoji, 0)),
            "url": String(roleValue(Room.Url, "")),
            "thumbnailUrl": String(roleValue(Room.ThumbnailUrl, "")),
            "duration": Number(roleValue(Room.Duration, 0)),
            "blurhash": String(roleValue(Room.Blurhash, "")),
            "filename": String(roleValue(Room.Filename, "")),
            "filesize": String(roleValue(Room.Filesize, "")),
            "filesizeBytes": Number(roleValue(Room.FilesizeBytes, 0)),
            "mimetype": String(roleValue(Room.MimeType, "")),
            "originalHeight": Number(roleValue(Room.OriginalHeight, 0)),
            "originalWidth": Number(roleValue(Room.OriginalWidth, 0)),
            "proportionalHeight": Number(roleValue(Room.ProportionalHeight, 0)),
            "fileTypeIconSource": String(roleValue(Room.FileTypeIconSource, "")),
            "formattedStateEvent": String(roleValue(Room.FormattedStateEvent, "")),
            "stateEventIconSource": String(roleValue(Room.StateEventIconSource, "")),
            "stateEventIconColorCategory": String(roleValue(Room.StateEventIconColorCategory, "")),
            "callType": String(roleValue(Room.CallType, "")),
            "isEdited": Boolean(roleValue(Room.IsEdited, false)),
            "isEditable": Boolean(roleValue(Room.IsEditable, false)),
            "isEncrypted": Boolean(roleValue(Room.IsEncrypted, false)),
            "isStateEvent": Boolean(roleValue(Room.IsStateEvent, false)),
            "replyTo": String(roleValue(Room.ReplyTo, "")),
            "threadId": String(roleValue(Room.ThreadId, ""))
        };
    }

    // Left-click and long-press handlers live on a TapHandler rather than on
    // AbstractButton.onClicked/onPressAndHold, because AbstractButton grabs the
    // mouse press for its click detection and prevents the inner Flickable +
    // ScrollBar from receiving drag/wheel events. TapHandler with
    // gesturePolicy: ReleaseWithinBounds is non-grabbing on press, so the
    // Flickable can claim drag gestures while taps still resolve correctly.
    TapHandler {
        id: leftTapHandler

        enabled: r.clickable
        acceptedButtons: Qt.LeftButton
        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad | PointerDevice.TouchScreen
        gesturePolicy: TapHandler.ReleaseWithinBounds

        onTapped: eventPoint => {
            const previewDelegate = usesCompactMediaPreview ? mediaPreviewLoader.item : timelineEvent.main;
            let link = previewDelegate && previewDelegate.linkAt != undefined && previewDelegate.linkAt(eventPoint.position.x - Komai.paddingSmall, eventPoint.position.y - userName_.implicitHeight + replyFlickable.contentY);
            if (link) {
                Komai.openLink(link)
            } else {
                if (effectiveRoomContext && typeof effectiveRoomContext.showEvent === "function")
                    effectiveRoomContext.showEvent(r.eventId)
            }
        }

        onLongPressed: {
            const previewDelegate = usesCompactMediaPreview ? mediaPreviewLoader.item : timelineEvent.main;
            if (!effectiveReplyContextMenu || !previewDelegate)
                return;

            const pt = leftTapHandler.point.position;
            effectiveReplyContextMenu.show(previewDelegate.copyText,
                                           previewDelegate.linkAt(pt.x - Komai.paddingSmall,
                                                                  pt.y - userName_.implicitHeight + replyFlickable.contentY),
                                           r.eventId)
        }
    }

    TapHandler {
        enabled: r.clickable
        acceptedButtons: Qt.RightButton
        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
        gesturePolicy: TapHandler.ReleaseWithinBounds

        onSingleTapped: eventPoint => {
            const previewDelegate = usesCompactMediaPreview ? mediaPreviewLoader.item : timelineEvent.main;
            if (!effectiveReplyContextMenu || !previewDelegate)
                return;

            effectiveReplyContextMenu.show(
                        previewDelegate.copyText,
                        previewDelegate.linkAt(eventPoint.position.x - Komai.paddingSmall,
                                               eventPoint.position.y - userName_.implicitHeight + replyFlickable.contentY),
                        r.eventId)
        }
    }

    // qmllint disable required
    contentItem: Item {
        id: replyContainer

        readonly property real previewDelegateWidth: usesCompactMediaPreview
            ? r.compactPreviewImplicitWidth
            : (timelineEvent.main ? timelineEvent.main.implicitWidth : 0)
        readonly property real previewDelegateHeight: usesCompactMediaPreview
            ? (mediaPreviewLoader.item ? mediaPreviewLoader.item.height : 0)
            : (timelineEvent.main ? timelineEvent.main.height : 0)
        readonly property real maxScrolledContentHeight: Math.max(0,
            (timelineView_ ? timelineView_.height : Screen.height) / 10
            + r.additionalHeight)
        readonly property real clampedContentHeight: r.limitHeight
            ? Math.min(previewDelegateHeight, maxScrolledContentHeight)
            : previewDelegateHeight
        readonly property real unclampedHeight: usernameBtn.height + previewDelegateHeight
        readonly property bool needsScrolling: r.limitHeight && previewDelegateHeight > clampedContentHeight + 0.5

        readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
        readonly property bool scrollbarVisible: {
            if (!r.limitHeight || previewDelegateHeight <= 0)
                return false;
            switch (scrollbarPolicy) {
            case Settings.ScrollbarPolicy.Always:
                return true;
            case Settings.ScrollbarPolicy.Never:
                return false;
            case Settings.ScrollbarPolicy.WhenNeeded:
            default:
                return needsScrolling;
            }
        }
        readonly property real reservedScrollbarWidth: scrollbarVisible
            ? Math.max(replyScrollBar.width, replyScrollBar.implicitWidth) + Komai.paddingSmall
            : 0

        implicitWidth: Math.max(usernameBtn.implicitWidth, previewDelegateWidth + reservedScrollbarWidth)
        implicitHeight: unclampedHeight
        height: r.limitHeight
            ? clampedContentHeight + usernameBtn.height
            : implicitHeight
        clip: r.limitHeight

        AbstractButton {
            id: usernameBtn

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            topPadding: 0
            bottomPadding: 0
            topInset: 0
            bottomInset: 0
            height: (visible && r.userName.length > 0) ? implicitHeight : 0

            contentItem: Label {
                id: userName_
                // HACK: To ensure the username gets rendered in newer Qt,
                // we need to always have some text in here. The name should
                // never be empty, since it falls back to the mxid, but if
                // we have no text there, Qt culls the item before we fill it.
                text: r.userName || "."
                color: Komai.readableAccentTextColor(r.userColor, r.roomColor)
                font.pointSize: Settings.uiFontSizePt
                textFormat: Text.RichText
                width: usernameBtn.width
            }
            onClicked: {
                if (effectiveRoomContext && typeof effectiveRoomContext.openUserProfile === "function")
                    effectiveRoomContext.openUserProfile(r.userId);
            }
        }

        Flickable {
            id: replyFlickable

            anchors.top: usernameBtn.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: replyContainer.reservedScrollbarWidth
            height: r.limitHeight ? replyContainer.clampedContentHeight : contentHeight
            contentWidth: width
            contentHeight: replyContainer.previewDelegateHeight
            interactive: replyContainer.needsScrolling
            clip: r.limitHeight
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

            TimelineEvent {
                id: timelineEvent

                visible: !r.usesCompactMediaPreview
                width: replyFlickable.width
                implicitWidth: main ? main.implicitWidth : width
                implicitHeight: main ? main.implicitHeight : height
                height: main ? main.height : 0
                isStateEvent: false
                room: (r.roleDataSource instanceof EventDataSource) ? r.roleDataSource : null
                eventId: r.eventId
                replyTo: ""
                mainInset: Komai.paddingSmall + Komai.paddingMedium
                maxWidth: Math.max(0, r.maxWidth - replyContainer.reservedScrollbarWidth)
                limitAsReply: true
                previewData: r.effectivePreviewData
                roomModelOverride: (r.roleDataSource instanceof EventDataSource) ? null : (r.roleDataSource ?? r.effectiveRoomContext)
            }

            Binding {
                target: timelineEvent.main
                property: "roomAdapter"
                when: !!timelineEvent.main && typeof timelineEvent.main.roomAdapter !== "undefined"
                value: r.effectiveRoomContext
            }

            Loader {
                id: mediaPreviewLoader

                active: r.usesCompactMediaPreview
                    && r.resolvedContentWidth > 0
                    && r.compactMediaWidth > 0
                    && r.compactMediaHeight > 0
                visible: active
                width: Math.max(1, replyFlickable.width)
                sourceComponent: compactMediaPreviewComponent
            }

            ScrollBar.vertical: ScrollBar {
                id: replyScrollBar

                policy: replyContainer.scrollbarVisible
                    ? ScrollBar.AlwaysOn
                    : ScrollBar.AlwaysOff
            }
        }
    }
    // qmllint enable required

    Component {
        id: compactMediaPreviewComponent

        Item {
            id: compactMediaPreview

            implicitWidth: compactPreviewRow.implicitWidth
            implicitHeight: compactPreviewRow.implicitHeight
            height: implicitHeight

            property string copyText: r.mediaPreviewSummaryText

            function linkAt(_x, _y) {
                return "";
            }

            Rectangle {
                id: mediaThumbMask

                width: r.compactMediaWidth
                height: r.compactMediaHeight
                radius: Komai.paddingMedium
                layer.enabled: true
                visible: false
            }

            Row {
                id: compactPreviewRow

                spacing: Komai.paddingSmall

                Item {
                    id: mediaThumbFrame

                    width: r.compactMediaWidth
                    height: r.compactMediaHeight
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: mediaThumbMask
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: Komai.paddingMedium
                        color: Qt.rgba(r.palette.alternateBase.r,
                                       r.palette.alternateBase.g,
                                       r.palette.alternateBase.b,
                                       0.9)
                        border.width: 1
                        border.color: Qt.rgba(r.palette.buttonText.r,
                                              r.palette.buttonText.g,
                                              r.palette.buttonText.b,
                                              0.25)
                    }

                    Image {
                        id: mediaThumbImage

                        anchors.fill: parent
                        asynchronous: true
                        cache: true
                        fillMode: Image.PreserveAspectFit
                        horizontalAlignment: Image.AlignHCenter
                        verticalAlignment: Image.AlignVCenter
                        smooth: true
                        mipmap: true
                        source: r.compactPreviewImageSource
                        readonly property int _sourcePx: Math.max(1, Math.round(Math.max(parent.width, parent.height) * Screen.devicePixelRatio))
                        sourceSize.width: _sourcePx
                        sourceSize.height: _sourcePx

                    }

                    Item {
                        anchors.fill: parent
                        visible: mediaThumbImage.status !== Image.Ready

                        Image {
                            anchors.centerIn: parent
                            width: Math.min(parent.width, parent.height) * 0.45
                            height: width
                            source: "image://colorimage/:/icons/icons/ui/image-failed.svg?" + r.palette.buttonText
                            sourceSize.width: width
                            sourceSize.height: height
                            fillMode: Image.PreserveAspectFit
                        }
                    }

                    Item {
                        anchors.fill: parent
                        visible: r.previewType === MtxEvent.VideoMessage

                        Rectangle {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            radius: 14
                            color: Qt.rgba(0, 0, 0, 0.55)
                        }

                        Image {
                            anchors.centerIn: parent
                            source: "qrc:/icons/icons/ui/video.svg"
                            width: 14
                            height: 14
                            sourceSize.width: width
                            sourceSize.height: height
                        }
                    }
                }

                Text {
                    id: mediaSummary

                    visible: text.length > 0
                    width: visible ? r.compactPreviewTextWidth : 0
                    y: Math.max(0, Math.round((mediaThumbFrame.height - height) / 2))
                    color: r.palette.text
                    text: r.mediaPreviewSummaryText
                    wrapMode: Text.Wrap
                    maximumLineCount: r.keepFullText ? 3 : 2
                    elide: Text.ElideRight
                }
            }
        }
    }

    background: Rectangle {
        id: backgroundItem

        z: -1
        color: r.roomColor
        radius: Komai.paddingMedium
        border.width: 1
        border.color: Qt.darker(r.roomColor, 1.3)
    }

    QtObject {
        id: compactPreviewRoomContext

        readonly property string roomId: effectiveRoomContext && effectiveRoomContext.roomId !== undefined
            ? String(effectiveRoomContext.roomId || "")
            : ""

        function showImage() {
            if (effectiveRoomContext && typeof effectiveRoomContext.showImage === "function")
                return effectiveRoomContext.showImage();
            return true;
        }
    }

}
