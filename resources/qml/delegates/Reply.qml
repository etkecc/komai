// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai

AbstractButton {
    id: r

    property color userColor: "red"
    property color roomColor: userColor
    required property var bubblePalette
    property bool keepFullText: false
    property var previewData: ({})
    property var roomModelOverride: null
    property var timelineViewOverride: null
    property var replyContextMenuOverride: null

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
    readonly property real compactMediaMaxWidth: Math.min(
        Math.max(64, maxWidth - leftPadding - rightPadding - colorline.width - Komai.paddingMedium * 4),
        144)
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
    readonly property int compactMediaHeight: {
        if (previewOriginalWidth > 0 && previewOriginalHeight > 0) {
            const scale = Math.min(compactMediaMaxWidth / previewOriginalWidth,
                                   compactMediaMaxHeight / previewOriginalHeight,
                                   1.0);
            return Math.max(40, Math.round(previewOriginalHeight * scale));
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
    readonly property real compactPreviewTextAvailableWidth: Math.max(0, resolvedContentWidth - compactMediaWidth - Komai.paddingSmall)
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
    readonly property string compactPreviewImageSource: {
        if (previewMediaUrl.length === 0)
            return "";

        const providerSource = previewMediaUrl.replace("mxc://", "image://MxcImage/");
        return compactPreviewRoomId.length > 0
            ? (providerSource + "?scale&room=" + compactPreviewRoomId)
            : (providerSource + "?scale");
    }

    property string userId: String((effectivePreviewData && effectivePreviewData.userId) || "")
    property string userName: String((effectivePreviewData && effectivePreviewData.userName) || "")
    implicitHeight: replyContainer.height + topPadding + bottomPadding
    implicitWidth: replyContainer.implicitWidth + leftPadding + rightPadding

    leftPadding: 4 + Komai.paddingMedium
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
    readonly property real resolvedContentWidth: Math.max(0, width - leftPadding - rightPadding)

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
        cursorShape: Qt.PointingHandCursor
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
            "callType": String(roleValue(Room.CallType, "")),
            "isEdited": Boolean(roleValue(Room.IsEdited, false)),
            "isEditable": Boolean(roleValue(Room.IsEditable, false)),
            "isEncrypted": Boolean(roleValue(Room.IsEncrypted, false)),
            "isStateEvent": Boolean(roleValue(Room.IsStateEvent, false)),
            "replyTo": String(roleValue(Room.ReplyTo, "")),
            "threadId": String(roleValue(Room.ThreadId, ""))
        };
    }

    onClicked: {
        const previewDelegate = usesCompactMediaPreview ? mediaPreviewLoader.item : timelineEvent.main;
        let link = previewDelegate && previewDelegate.linkAt != undefined && previewDelegate.linkAt(pressX-colorline.width, pressY - userName_.implicitHeight);
        if (link) {
            Komai.openLink(link)
        } else {
            if (effectiveRoomContext && typeof effectiveRoomContext.showEvent === "function")
                effectiveRoomContext.showEvent(r.eventId)
        }
    }
    onPressAndHold: {
        const previewDelegate = usesCompactMediaPreview ? mediaPreviewLoader.item : timelineEvent.main;
        if (!effectiveReplyContextMenu || !previewDelegate)
            return;

        effectiveReplyContextMenu.show(previewDelegate.copyText,
                                       previewDelegate.linkAt(pressX - colorline.width,
                                                              pressY - userName_.implicitHeight),
                                       r.eventId)
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
        gesturePolicy: TapHandler.ReleaseWithinBounds

        onSingleTapped: eventPoint => {
            const previewDelegate = usesCompactMediaPreview ? mediaPreviewLoader.item : timelineEvent.main;
            if (!effectiveReplyContextMenu || !previewDelegate)
                return;

            effectiveReplyContextMenu.show(
                        previewDelegate.copyText,
                        previewDelegate.linkAt(eventPoint.position.x - colorline.width,
                                               eventPoint.position.y - userName_.implicitHeight),
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
        readonly property real unclampedHeight: usernameBtn.height + previewDelegateHeight

        implicitWidth: Math.max(usernameBtn.implicitWidth, previewDelegateWidth)
        implicitHeight: unclampedHeight
        height: r.limitHeight
            ? Math.min(previewDelegateHeight, (timelineView_ ? timelineView_.height : Screen.height) / 10) + usernameBtn.height
            : implicitHeight
        clip: r.limitHeight

        Column {
            id: replyColumn

            spacing: 0
            width: parent.width

            AbstractButton {
                id: usernameBtn

                topPadding: 0
                bottomPadding: 0
                topInset: 0
                bottomInset: 0
                width: replyColumn.width
                height: (visible && r.userName.length > 0) ? implicitHeight : 0

                contentItem: Label {
                    id: userName_
                    // HACK: To ensure the username gets rendered in newer Qt,
                    // we need to always have some text in here. The name should
                    // never be empty, since it falls back to the mxid, but if
                    // we have no text there, Qt culls the item before we fill it.
                    text: r.userName || "."
                    color: Komai.readableAccentTextColor(r.userColor, r.roomColor)
                    textFormat: Text.RichText
                    width: usernameBtn.width
                }
                onClicked: {
                    if (effectiveRoomContext && typeof effectiveRoomContext.openUserProfile === "function")
                        effectiveRoomContext.openUserProfile(r.userId);
                }
            }

            TimelineEvent {
                id: timelineEvent

                visible: !r.usesCompactMediaPreview
                width: replyColumn.width
                implicitWidth: main ? main.implicitWidth : width
                implicitHeight: main ? main.implicitHeight : height
                height: main ? main.height : 0
                isStateEvent: false
                room: r.roleDataSource
                eventId: r.eventId
                replyTo: ""
                mainInset: 4 + Komai.paddingMedium
                maxWidth: r.maxWidth
                limitAsReply: true
                previewData: r.effectivePreviewData
                roomModelOverride: r.roleDataSource ? null : r.effectiveRoomContext
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
                width: Math.max(1, replyColumn.width)
                sourceComponent: compactMediaPreviewComponent
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

            Row {
                id: compactPreviewRow

                spacing: Komai.paddingSmall

                Item {
                    id: mediaThumbFrame

                    width: r.compactMediaWidth
                    height: r.compactMediaHeight
                    clip: true
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
                        horizontalAlignment: Image.AlignLeft
                        smooth: true
                        mipmap: true
                        source: r.compactPreviewImageSource
                        sourceSize.width: Math.max(1, Math.round(parent.width * Screen.devicePixelRatio))
                        sourceSize.height: Math.max(1, Math.round(parent.height * Screen.devicePixelRatio))

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
        clip: true

        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left

            id: colorline
            color: r.roomColor
            width: 4
        }
    }

    // Border overlay drawn on top of content so rounded
    // corners are not hidden by the content item.
    Rectangle {
        anchors.fill: parent
        z: 10
        color: "transparent"
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
