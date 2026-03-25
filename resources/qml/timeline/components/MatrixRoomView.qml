// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../room/components"
import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

ColumnLayout {
    id: root

    required property var roomPreview
    required property bool showBackButton

    readonly property bool hasTimeline: TimelineManager.matrixTimelineItemCount > 0
    readonly property bool loading: TimelineManager.matrixTimelineLoading
    readonly property var composerShell: composerContainer
    property int lastPaginationTriggerCount: -1

    function focusTextInput() {
        composerInput.forceActiveFocus();
        return true;
    }

    function appendText(text) {
        if (!text)
            return false;

        composerInput.forceActiveFocus();
        composerInput.insert(composerInput.cursorPosition, text);
        return true;
    }

    function trySendMessage() {
        const body = composerInput.text;
        if (!TimelineManager.sendActiveMatrixTextMessage(body))
            return false;

        composerInput.text = "";
        composerInput.forceActiveFocus();
        return true;
    }

    anchors.fill: parent
    enabled: visible
    spacing: 0
    visible: !!roomPreview && roomPreview.isMatrixSummary

    RoomHeader {
        Layout.fillWidth: true
        room: null
        roomModel: null
        roomId: root.roomPreview ? root.roomPreview.roomid : ""
        roomName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarDisplayName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarUrl: root.roomPreview ? root.roomPreview.roomAvatarUrl : ""
        directChatOtherUserId: root.roomPreview ? root.roomPreview.directChatOtherUserId : ""
        isDirect: !!root.roomPreview && root.roomPreview.isDirect
        isEncrypted: !!root.roomPreview && root.roomPreview.isEncrypted
        roomTopic: root.roomPreview ? root.roomPreview.roomTopic : ""
        showBackButton: root.showBackButton
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: palette.mid
    }

    Rectangle {
        Layout.fillHeight: true
        Layout.fillWidth: true
        color: palette.base

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ListView {
                    id: matrixTimelineList

                    anchors.fill: parent
                    anchors.margins: Komai.paddingLarge
                    clip: true
                    model: TimelineManager.matrixTimelineModel
                    spacing: Komai.paddingMedium
                    visible: root.hasTimeline

                    onAtYBeginningChanged: {
                        if (atYBeginning
                                && root.hasTimeline
                                && !root.loading
                                && root.lastPaginationTriggerCount !== TimelineManager.matrixTimelineItemCount) {
                            if (TimelineManager.paginateActiveMatrixTimelineBackwards(0))
                                root.lastPaginationTriggerCount = TimelineManager.matrixTimelineItemCount;
                        }
                    }
                    onContentYChanged: {
                        if (contentY > 0 && root.lastPaginationTriggerCount === TimelineManager.matrixTimelineItemCount)
                            root.lastPaginationTriggerCount = -1;
                    }

                    delegate: Item {
                        function formatBytes(bytes) {
                            const value = Number(bytes);
                            if (!isFinite(value) || value <= 0)
                                return "";

                            const units = ["B", "KB", "MB", "GB"];
                            let size = value;
                            let unitIndex = 0;
                            while (size >= 1024 && unitIndex < units.length - 1) {
                                size /= 1024;
                                unitIndex += 1;
                            }

                            return (size >= 10 || unitIndex === 0 ? Math.round(size) : size.toFixed(1)) + " " + units[unitIndex];
                        }

                        function formatDuration(durationMs) {
                            const value = Number(durationMs);
                            if (!isFinite(value) || value <= 0)
                                return "";

                            const totalSeconds = Math.round(value / 1000);
                            const minutes = Math.floor(totalSeconds / 60);
                            const seconds = totalSeconds % 60;
                            return minutes + ":" + (seconds < 10 ? "0" + seconds : seconds);
                        }

                        required property string itemKind
                        required property string itemId
                        required property string eventId
                        required property string senderDisplayName
                        required property string senderAvatarUrl
                        required property string senderId
                        required property string body
                        required property string replySenderDisplayName
                        required property string replyBody
                        required property string reactionsSummary
                        required property string fileName
                        required property string mimeType
                        required property string mediaUrl
                        required property string thumbnailUrl
                        required property double mediaWidth
                        required property double mediaHeight
                        required property double mediaDurationMs
                        required property double mediaSizeBytes
                        required property bool mediaIsEncrypted
                        required property double timestamp
                        required property bool isEdited
                        required property bool isOwn

                        readonly property bool isMediaItem: ["image", "video", "audio", "file", "sticker"].indexOf(itemKind) >= 0
                        readonly property string effectiveFileName: fileName.length > 0 ? fileName : (body.length > 0 ? body : qsTr("Attachment"))
                        readonly property bool showCaption: isMediaItem && body.length > 0 && body !== effectiveFileName
                        readonly property bool hasReplyPreview: replyBody.length > 0
                        readonly property bool showVisualPreview: ["image", "video", "sticker"].indexOf(itemKind) >= 0 && itemId.length > 0
                        readonly property string replySourceBody: body.length > 0 ? body : effectiveFileName
                        readonly property var quickReactions: Settings.timelineMessageActionsPinnedReactions.split(",").map(function (s) {
                            return s.trim();
                        }).filter(function (s) {
                            return s.length > 0;
                        }).slice(0, 4)
                        readonly property double safePreviewAspectRatio: mediaWidth > 0 && mediaHeight > 0 ? (mediaHeight / mediaWidth) : 0.75
                        readonly property string footerMetaText: {
                            const parts = [];
                            if (isEdited)
                                parts.push(qsTr("edited"));
                            parts.push(Qt.formatTime(new Date(timestamp), "h:mm ap"));
                            return parts.join(" · ");
                        }
                        readonly property string mediaKindLabel: {
                            switch (itemKind) {
                            case "image":
                                return qsTr("Image");
                            case "video":
                                return qsTr("Video");
                            case "audio":
                                return qsTr("Audio");
                            case "sticker":
                                return qsTr("Sticker");
                            case "file":
                            default:
                                return qsTr("File");
                            }
                        }
                        readonly property string mediaMetaText: {
                            const parts = [];
                            if (mimeType.length > 0)
                                parts.push(mimeType);
                            const dimensions = mediaWidth > 0 && mediaHeight > 0 ? (Math.round(mediaWidth) + "×" + Math.round(mediaHeight)) : "";
                            if (dimensions.length > 0)
                                parts.push(dimensions);
                            const durationText = formatDuration(mediaDurationMs);
                            if (durationText.length > 0)
                                parts.push(durationText);
                            const sizeText = formatBytes(mediaSizeBytes);
                            if (sizeText.length > 0)
                                parts.push(sizeText);
                            return parts.join(" · ");
                        }

                        width: ListView.view.width
                        height: itemKind === "date_divider" ? dateDivider.implicitHeight : messageRow.implicitHeight

                        Rectangle {
                            id: dateDivider

                            anchors.horizontalCenter: parent.horizontalCenter
                            color: palette.mid
                            height: dividerLabel.implicitHeight + Komai.paddingSmall * 2
                            radius: height / 2
                            visible: itemKind === "date_divider"
                            width: dividerLabel.implicitWidth + Komai.paddingLarge * 2

                            MatrixText {
                                id: dividerLabel

                                anchors.centerIn: parent
                                color: palette.base
                                text: Qt.formatDateTime(new Date(timestamp), "dddd, d MMMM")
                                textFormat: TextEdit.PlainText
                            }
                        }

                        Item {
                            id: messageRow

                            anchors.left: parent.left
                            anchors.right: parent.right
                            implicitHeight: messageRowLayout.implicitHeight
                            visible: itemKind !== "date_divider"

                            RowLayout {
                                id: messageRowLayout

                                anchors.left: isOwn ? undefined : parent.left
                                anchors.right: isOwn ? parent.right : undefined
                                spacing: Komai.paddingSmall
                                width: Math.min(parent.width * 0.9, Math.max(320, parent.width * 0.7))

                                Avatar {
                                    Layout.alignment: Qt.AlignTop
                                    crop: true
                                    displayName: senderDisplayName
                                    implicitHeight: Komai.listIconSize
                                    implicitWidth: Komai.listIconSize
                                    roomid: root.roomPreview ? root.roomPreview.roomid : ""
                                    url: senderAvatarUrl.replace("mxc://", "image://MxcImage/")
                                    userid: senderId
                                    visible: !isOwn
                                }

                                Item {
                                    Layout.preferredWidth: visible ? Komai.paddingSmall : 0
                                    visible: !isOwn
                                }

                                ColumnLayout {
                                    Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                    Layout.preferredWidth: Math.min(messageRow.width * 0.82, Math.max(280, messageRow.width * 0.6))
                                    spacing: Komai.paddingSmall

                                    MatrixText {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        color: palette.buttonText
                                        text: senderDisplayName
                                        textFormat: TextEdit.PlainText
                                    }

                                    Rectangle {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        color: isOwn ? palette.highlight : palette.alternateBase
                                        implicitHeight: bubbleContent.implicitHeight + Komai.paddingMedium * 2
                                        implicitWidth: Math.min(parent.width, bubbleContent.implicitWidth + Komai.paddingLarge * 2)
                                        radius: Komai.paddingMedium * 2

                                        ColumnLayout {
                                            id: bubbleContent
                                            anchors.fill: parent
                                            anchors.margins: Komai.paddingMedium
                                            spacing: Komai.paddingSmall
                                            width: parent.width - Komai.paddingMedium * 2

                                            Rectangle {
                                                Layout.fillWidth: true
                                                color: Qt.rgba(palette.base.r, palette.base.g, palette.base.b, isOwn ? 0.22 : 0.5)
                                                implicitHeight: replyPreviewLayout.implicitHeight + Komai.paddingSmall * 2
                                                radius: Komai.paddingMedium
                                                visible: hasReplyPreview

                                                ColumnLayout {
                                                    id: replyPreviewLayout

                                                    anchors.fill: parent
                                                    anchors.margins: Komai.paddingSmall
                                                    spacing: Math.max(2, Math.round(Komai.paddingSmall / 2))

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.text
                                                        font.bold: true
                                                        text: replySenderDisplayName.length > 0 ? replySenderDisplayName : qsTr("Reply")
                                                        textFormat: TextEdit.PlainText
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.buttonText
                                                        text: replyBody
                                                        textFormat: TextEdit.PlainText
                                                        wrapMode: Text.WordWrap
                                                    }
                                                }
                                            }

                                            MatrixText {
                                                id: bubbleBody

                                                Layout.fillWidth: true
                                                color: isOwn ? palette.highlightedText : palette.text
                                                text: body
                                                textFormat: TextEdit.PlainText
                                                visible: !isMediaItem
                                                wrapMode: Text.WordWrap
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                color: Qt.rgba(palette.base.r, palette.base.g, palette.base.b, isOwn ? 0.2 : 0.55)
                                                implicitHeight: mediaCardLayout.implicitHeight + Komai.paddingMedium * 2
                                                radius: Komai.paddingMedium * 1.5
                                                visible: isMediaItem

                                                ColumnLayout {
                                                    id: mediaCardLayout

                                                    anchors.fill: parent
                                                    anchors.margins: Komai.paddingMedium
                                                    spacing: Komai.paddingSmall

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.text
                                                        text: mediaKindLabel
                                                        textFormat: TextEdit.PlainText
                                                    }

                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        color: palette.base
                                                        implicitHeight: Math.round(Math.min(280, Math.max(120, width * safePreviewAspectRatio)))
                                                        radius: Komai.paddingMedium
                                                        visible: showVisualPreview

                                                        Image {
                                                            anchors.fill: parent
                                                            anchors.margins: 1
                                                            asynchronous: true
                                                            fillMode: Image.PreserveAspectFit
                                                            source: parent.visible
                                                                ? ("image://MxcImage/matrix-timeline:" + itemId + "?scale")
                                                                : ""
                                                            sourceSize.width: Math.max(320, width * Screen.devicePixelRatio)
                                                            sourceSize.height: Math.max(180, parent.height * Screen.devicePixelRatio)
                                                            smooth: true
                                                        }

                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor

                                                            onClicked: TimelineManager.openActiveMatrixTimelineMedia(itemId, effectiveFileName)
                                                        }
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.text
                                                        font.bold: true
                                                        text: effectiveFileName
                                                        textFormat: TextEdit.PlainText
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.text
                                                        text: body
                                                        textFormat: TextEdit.PlainText
                                                        visible: showCaption
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.buttonText
                                                        text: mediaMetaText
                                                        textFormat: TextEdit.PlainText
                                                        visible: mediaMetaText.length > 0
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.buttonText
                                                        text: mediaIsEncrypted
                                                            ? qsTr("Encrypted attachment. Open and save are handled through the Rust matrix-sdk backend.")
                                                            : qsTr("Open and save are handled through the Rust matrix-sdk backend.")
                                                        textFormat: TextEdit.PlainText
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        spacing: Komai.paddingSmall

                                                        Components.KomaiButton {
                                                            text: qsTr("Open")

                                                            onClicked: TimelineManager.openActiveMatrixTimelineMedia(itemId, effectiveFileName)
                                                        }

                                                        Components.KomaiButton {
                                                            text: qsTr("Save")

                                                            onClicked: TimelineManager.saveActiveMatrixTimelineMedia(itemId, effectiveFileName)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        color: Qt.rgba(palette.base.r, palette.base.g, palette.base.b, isOwn ? 0.18 : 0.5)
                                        implicitHeight: reactionsLabel.implicitHeight + Komai.paddingSmall * 2
                                        implicitWidth: reactionsLabel.implicitWidth + Komai.paddingMedium * 2
                                        radius: implicitHeight / 2
                                        visible: reactionsSummary.length > 0

                                        MatrixText {
                                            id: reactionsLabel

                                            anchors.centerIn: parent
                                            color: isOwn ? palette.highlightedText : palette.text
                                            text: reactionsSummary
                                            textFormat: TextEdit.PlainText
                                        }
                                    }

                                    MatrixText {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        color: palette.buttonText
                                        text: footerMetaText
                                        textFormat: TextEdit.PlainText
                                    }

                                    Flow {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        Layout.fillWidth: true
                                        spacing: Komai.paddingSmall
                                        visible: eventId.length > 0

                                        Repeater {
                                            model: quickReactions

                                            delegate: Rectangle {
                                                required property var modelData

                                                color: Qt.rgba(palette.base.r, palette.base.g, palette.base.b, isOwn ? 0.2 : 0.45)
                                                implicitHeight: reactionLabel.implicitHeight + Komai.paddingSmall * 2
                                                implicitWidth: reactionLabel.implicitWidth + Komai.paddingMedium * 2
                                                radius: implicitHeight / 2

                                                MatrixText {
                                                    id: reactionLabel

                                                    anchors.centerIn: parent
                                                    color: isOwn ? palette.highlightedText : palette.text
                                                    text: modelData
                                                    textFormat: TextEdit.PlainText
                                                }

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor

                                                    onClicked: TimelineManager.toggleActiveMatrixTimelineReaction(eventId, modelData)
                                                }
                                            }
                                        }

                                        Rectangle {
                                            color: Qt.rgba(palette.base.r, palette.base.g, palette.base.b, isOwn ? 0.2 : 0.45)
                                            implicitHeight: replyActionLabel.implicitHeight + Komai.paddingSmall * 2
                                            implicitWidth: replyActionLabel.implicitWidth + Komai.paddingMedium * 2
                                            radius: implicitHeight / 2

                                            MatrixText {
                                                id: replyActionLabel

                                                anchors.centerIn: parent
                                                color: isOwn ? palette.highlightedText : palette.text
                                                text: qsTr("Reply")
                                                textFormat: TextEdit.PlainText
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor

                                                onClicked: TimelineManager.queueActiveMatrixReply(eventId, senderDisplayName, replySourceBody)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Komai.paddingMedium
                    visible: !root.hasTimeline
                    width: Math.min(parent.width - Komai.paddingLarge * 2, 560)

                    MatrixText {
                        Layout.fillWidth: true
                        horizontalAlignment: TextEdit.AlignHCenter
                        text: root.loading
                            ? qsTr("Loading room timeline…")
                            : qsTr("No timeline items are loaded for this room yet.")
                        wrapMode: Text.WordWrap
                    }

                    MatrixText {
                        Layout.fillWidth: true
                        color: palette.buttonText
                        horizontalAlignment: TextEdit.AlignHCenter
                        text: qsTr("This room is now backed by the Rust matrix-sdk timeline. Plain text sending is available while richer composer features are migrated.")
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                id: composerContainer

                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                Layout.preferredHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                color: palette.window
                implicitHeight: composerLayout.implicitHeight + Komai.paddingMedium * 2

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    color: palette.mid
                    height: 1
                }

                ColumnLayout {
                    id: composerLayout

                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingSmall

                    Rectangle {
                        Layout.fillWidth: true
                        color: palette.alternateBase
                        implicitHeight: replyComposerLayout.implicitHeight + Komai.paddingMedium * 2
                        radius: Komai.paddingMedium
                        visible: TimelineManager.matrixTimelineReplyEventId.length > 0

                        RowLayout {
                            id: replyComposerLayout

                            anchors.fill: parent
                            anchors.margins: Komai.paddingMedium
                            spacing: Komai.paddingMedium

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Math.max(2, Math.round(Komai.paddingSmall / 2))

                                MatrixText {
                                    Layout.fillWidth: true
                                    color: palette.text
                                    font.bold: true
                                    text: TimelineManager.matrixTimelineReplySenderDisplayName.length > 0
                                        ? qsTr("Replying to %1").arg(TimelineManager.matrixTimelineReplySenderDisplayName)
                                        : qsTr("Replying to this message")
                                    textFormat: TextEdit.PlainText
                                    wrapMode: Text.WordWrap
                                }

                                MatrixText {
                                    Layout.fillWidth: true
                                    color: palette.buttonText
                                    text: TimelineManager.matrixTimelineReplyBody
                                    textFormat: TextEdit.PlainText
                                    wrapMode: Text.WordWrap
                                }
                            }

                            Components.KomaiButton {
                                text: qsTr("Cancel")

                                onClicked: TimelineManager.clearActiveMatrixReply()
                            }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(Math.max(56, composerInput.contentHeight + Komai.paddingMedium * 2), 160)
                        Layout.maximumHeight: 160
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        contentWidth: availableWidth
                        padding: 0

                        Components.KomaiTextArea {
                            id: composerInput

                            placeholderText: qsTr("Write a message…")
                            selectByMouse: true
                            textFormat: TextEdit.PlainText
                            wrapMode: TextEdit.Wrap

                            Keys.onPressed: event => {
                                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                        && !(event.modifiers & Qt.ShiftModifier)
                                        && !(event.modifiers & Qt.ControlModifier)
                                        && !(event.modifiers & Qt.AltModifier)
                                        && !(event.modifiers & Qt.MetaModifier)) {
                                    event.accepted = root.trySendMessage();
                                }
                            }

                            background: Rectangle {
                                border.color: palette.mid
                                border.width: 1
                                color: palette.base
                                radius: Komai.paddingMedium
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        MatrixText {
                            Layout.fillWidth: true
                            color: palette.buttonText
                            text: qsTr("Shift+Enter inserts a newline. Attachments are sent in order while the Rust room composer keeps growing toward the old room view.")
                            wrapMode: Text.WordWrap
                        }

                        Components.KomaiButton {
                            enabled: !TimelineManager.matrixTimelineAttachmentSending
                            text: TimelineManager.matrixTimelineAttachmentSending ? qsTr("Sending…") : qsTr("Attach")

                            onClicked: TimelineManager.openActiveMatrixAttachmentSelection()
                        }

                        Components.KomaiButton {
                            enabled: composerInput.text.trim().length > 0
                            text: qsTr("Send")

                            onClicked: root.trySendMessage()
                        }
                    }
                }
            }
        }
    }

    Connections {
        function onFocusInput() {
            root.focusTextInput();
        }

        target: TimelineManager
    }
}
