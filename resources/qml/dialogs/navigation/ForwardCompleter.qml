// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../delegates/"
import "../../ui"
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Popup {
    id: forwardMessagePopup

    property string mid: ""
    property var messageEventIds: []
    property int selectionCount: 0
    property var roomSource: null
    readonly property var activeRoom: roomSource
    property var timelineSource: null
    property var timelineViewSource: null
    readonly property var timeline: timelineSource
    readonly property var timelineView: timelineViewSource
    property bool showReplyPreview: true
    property int textHeight: Math.round(Komai.fontPixelSize * 2.4)
    property int textMargin: Komai.paddingSmall
    property string pendingRoomId: ""
    property string pendingRoomName: ""
    property string pendingRoomAvatarUrl: ""
    property bool confirming: false
    readonly property int messageCount: messageEventIds.length
    readonly property int effectiveSelectionCount: Math.max(selectionCount, messageCount)
    readonly property bool darkPopupChrome: palette.window.hslLightness < 0.5
    readonly property color popupOutlineColor: Qt.tint(
        palette.mid,
        Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, darkPopupChrome ? 0.22 : 0.32))

    function normalizedMessageEventIds(eventIdsIn) {
        const sourceIds = eventIdsIn || [];
        const normalizedIds = [];
        const seenIds = ({});

        for (let index = 0; index < sourceIds.length; index++) {
            const eventId = String(sourceIds[index] || "");
            if (!eventId || seenIds[eventId])
                continue;

            seenIds[eventId] = true;
            normalizedIds.push(eventId);
        }

        return normalizedIds;
    }

    function setMessageEventId(mid_in) {
        setMessageEventIds([mid_in], 1);
    }
    function setMessageEventIds(eventIdsIn, selectionCountIn) {
        messageEventIds = normalizedMessageEventIds(eventIdsIn);
        selectionCount = Math.max(Number(selectionCountIn || 0), messageEventIds.length);
        mid = messageEventIds.length > 0 ? String(messageEventIds[0] || "") : "";
    }
    function cancelConfirmation() {
        confirming = false;
        roomTextInput.forceActiveFocus();
    }
    function confirmForward() {
        if (activeRoom) {
            for (let index = 0; index < forwardMessagePopup.messageEventIds.length; index++)
                activeRoom.forwardMessage(String(forwardMessagePopup.messageEventIds[index] || ""),
                                          forwardMessagePopup.pendingRoomId);
        }
        forwardMessagePopup.close();
    }

    function titleText() {
        if (messageCount === 1 && effectiveSelectionCount <= 1)
            return qsTr("Forward message?");
        if (effectiveSelectionCount > messageCount)
            return qsTr("Forward %1 of %2 messages?").arg(messageCount).arg(effectiveSelectionCount);

        return qsTr("Forward %n messages?", "", messageCount);
    }

    function hintText() {
        if (messageCount <= 1)
            return qsTr("Forwarding sends this content (without revealing its sender) to another room.");
        if (effectiveSelectionCount > messageCount) {
            if (messageCount === 1)
                return qsTr("Only 1 of %1 selected messages can be forwarded. Unsupported messages will be skipped.").arg(effectiveSelectionCount);
            return qsTr("Only %1 of %2 selected messages can be forwarded. Unsupported messages will be skipped.").arg(messageCount).arg(effectiveSelectionCount);
        }

        return qsTr("Forwarding sends these messages (without revealing their sender) to another room.");
    }

    function confirmationText(roomName) {
        const resolvedRoomName = roomName || pendingRoomId;
        if (messageCount === 1 && effectiveSelectionCount <= 1)
            return qsTr("Forward to <b>%1</b>?").arg(resolvedRoomName);
        if (effectiveSelectionCount > messageCount) {
            if (messageCount === 1)
                return qsTr("Forward 1 of %1 selected messages to <b>%2</b>?").arg(effectiveSelectionCount).arg(resolvedRoomName);
            return qsTr("Forward %1 of %2 selected messages to <b>%3</b>?").arg(messageCount).arg(effectiveSelectionCount).arg(resolvedRoomName);
        }

        return qsTr("Forward %n selected messages to <b>%1</b>?", "", messageCount).arg(resolvedRoomName);
    }

    padding: Komai.paddingMedium
    modal: true
    focus: true

    // Workaround palettes not inheriting for popups
    palette: timelineRoot.palette
    parent: Overlay.overlay
    width: timelineRoot.width * 0.8
    x: Math.round(parent.width / 2 - width / 2)
    y: Math.max(Komai.paddingLarge, Math.round((parent.height - height) / 2))

    Overlay.modal: Rectangle {
        color: timelineRoot.overlayBackdropColor
    }
    background: Rectangle {
        color: palette.alternateBase
        radius: 8
        border.color: forwardMessagePopup.popupOutlineColor
        border.width: 2
    }

    Shortcut {
        sequences: [StandardKey.Cancel, "Escape"]
        context: Qt.ApplicationShortcut
        enabled: forwardMessagePopup.visible && forwardMessagePopup.confirming
        onActivated: forwardMessagePopup.cancelConfirmation()
    }

    Shortcut {
        sequences: [StandardKey.InsertParagraphSeparator]
        context: Qt.ApplicationShortcut
        enabled: forwardMessagePopup.visible && forwardMessagePopup.confirming
        onActivated: forwardMessagePopup.confirmForward()
    }

    onOpened: {
        confirming = false;
        pendingRoomId = "";
        pendingRoomName = "";
        pendingRoomAvatarUrl = "";
        roomTextInput.text = "";
        completerPopup.changeCompleter();
        if (completerPopup.completer)
            completerPopup.completer.searchString = "";
        // In image-overlay flow the closing overlay window can steal focus for a tick.
        Qt.callLater(() => {
            forwardMessagePopup.forceActiveFocus();
            roomTextInput.forceActiveFocus();
        });
    }

    contentItem: Column {
        id: forwardColumn

        spacing: Komai.paddingSmall

        Row {
            spacing: Komai.paddingSmall
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2

            Image {
                anchors.verticalCenter: parent.verticalCenter
                height: titleLabel.font.pixelSize
                width: height
                mirror: true
                source: "image://colorimage/:/icons/icons/ui/reply.svg?" + palette.text
                sourceSize.height: height * Screen.devicePixelRatio
                sourceSize.width: width * Screen.devicePixelRatio
            }

            Label {
                id: titleLabel

                color: palette.text
                font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.6)
                font.bold: true
                text: forwardMessagePopup.titleText()
            }

            Item {
                height: 1
                width: parent.width - titleLabel.implicitWidth - titleLabel.font.pixelSize - closeButton.width - parent.spacing * 3
            }

            ImageButton {
                id: closeButton

                toolTipText: qsTr("Close")
                toolTipVisible: hovered
                anchors.verticalCenter: parent.verticalCenter
                height: titleLabel.font.pixelSize
                width: height
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: forwardMessagePopup.close()
            }
        }

        Label {
            id: hintLabel

            color: palette.buttonText
            font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.4)
            text: forwardMessagePopup.hintText()
            leftPadding: Komai.paddingSmall
            topPadding: Komai.paddingMedium
            bottomPadding: Komai.paddingMedium
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2
            wrapMode: Text.Wrap
        }

        Loader {
            id: replyPreviewLoader

            active: forwardMessagePopup.showReplyPreview && forwardMessagePopup.messageCount === 1
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2
            sourceComponent: replyPreviewComponent
        }

        Component {
            id: replyPreviewComponent

            Reply {
                id: replyPreview

                enabled: false
                eventId: mid
                room_: activeRoom
                property bool isReplyFromCurrentUser: {
                    const currentUser = Komai.currentUser;
                    const currentUserId = (currentUser && currentUser.userid)
                            ? String(currentUser.userid)
                            : "";
                    return currentUserId.length > 0 && replyPreview.userId === currentUserId;
                }
                readonly property color previewWindowColor: (Komai.colors && Komai.colors.window !== undefined)
                    ? Komai.colors.window
                    : forwardMessagePopup.palette.window
                readonly property color previewBaseColor: (Komai.colors && Komai.colors.base !== undefined)
                    ? Komai.colors.base
                    : forwardMessagePopup.palette.base
                bubblePalette: activeRoom ? TimelineManager.roomUserBubblePalette(activeRoom.roomId, replyPreview.userId, roomColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userBubblePalette(replyPreview.userId, roomColor)
                userColor: isReplyFromCurrentUser
                    ? Komai.theme.userColorSelf
                    : activeRoom ? TimelineManager.roomUserColor(activeRoom.roomId, replyPreview.userId, previewWindowColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyPreview.userId, previewWindowColor)
                roomColor: isReplyFromCurrentUser
                    ? Komai.theme.userColorSelf
                    : activeRoom ? TimelineManager.roomUserColor(activeRoom.roomId, replyPreview.userId, previewBaseColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyPreview.userId, previewBaseColor)
                limitHeight: true
                width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2
                maxWidth: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2
            }
        }

        // Room search (visible when not confirming)
        MatrixTextField {
            id: roomTextInput

            color: palette.text
            font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.6)
            placeholderText: qsTr("Room name, address or id...")
            radius: Komai.paddingSmall
            visible: !forwardMessagePopup.confirming
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2

            Keys.onPressed: (event) => {
                if (event.key == Qt.Key_Up || event.key == Qt.Key_Backtab) {
                    event.accepted = true;
                    completerPopup.up();
                } else if (event.key == Qt.Key_Down || event.key == Qt.Key_Tab) {
                    event.accepted = true;
                    if (event.key == Qt.Key_Tab && (event.modifiers & Qt.ShiftModifier))
                        completerPopup.up();
                    else
                        completerPopup.down();
                } else if (event.matches(StandardKey.InsertParagraphSeparator)) {
                    completerPopup.finishCompletion();
                    event.accepted = true;
                }
            }
            onTextEdited: {
                if (completerPopup.completer)
                    completerPopup.completer.searchString = text;
            }
        }

        Completer {
            id: completerPopup

            avatarHeight: Komai.listIconSize
            avatarWidth: Komai.listIconSize
            bottomToTop: false
            completerType: "room"
            backendModel: "forwardRoom"
            fullWidth: true
            rowMargin: Math.round(forwardMessagePopup.textMargin / 2)
            rowSpacing: forwardMessagePopup.textMargin
            visible: !forwardMessagePopup.confirming
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2

            onCompletionSelected: (id) => {
                var targetRoom = Rooms.getRoomById(id);
                forwardMessagePopup.pendingRoomId = id;
                forwardMessagePopup.pendingRoomName = targetRoom ? targetRoom.plainRoomName : id;
                forwardMessagePopup.pendingRoomAvatarUrl = targetRoom ? targetRoom.roomAvatarUrl : "";
                forwardMessagePopup.confirming = true;
                Qt.callLater(() => forwardButton.forceActiveFocus(Qt.TabFocusReason));
            }
            onCountChanged: {
                if (completerPopup.count > 0
                        && (completerPopup.currentIndex < 0 || completerPopup.currentIndex >= completerPopup.count))
                    completerPopup.currentIndex = 0;
            }
        }

        // Confirmation (visible when confirming)
        Row {
            id: confirmRow

            spacing: Komai.paddingSmall
            visible: forwardMessagePopup.confirming
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2

            Components.Avatar {
                id: confirmAvatar

                anchors.verticalCenter: parent.verticalCenter
                height: Komai.listIconSize
                width: Komai.listIconSize
                displayName: forwardMessagePopup.pendingRoomName
                roomid: forwardMessagePopup.pendingRoomId
                url: forwardMessagePopup.pendingRoomAvatarUrl.replace("mxc://", "image://MxcImage/")
                enabled: false
            }

            Label {
                id: confirmLabel

                anchors.verticalCenter: parent.verticalCenter
                color: palette.text
                font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.5)
                text: forwardMessagePopup.confirmationText(forwardMessagePopup.pendingRoomName)
                textFormat: Text.StyledText
                width: confirmRow.width - confirmAvatar.width - confirmRow.spacing
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            id: confirmButtons

            spacing: Komai.paddingMedium
            visible: forwardMessagePopup.confirming
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2

            Components.KomaiButton {
                id: cancelButton

                activeFocusOnTab: true
                focusPolicy: Qt.StrongFocus
                text: qsTr("Cancel")
                onClicked: forwardMessagePopup.cancelConfirmation()
                Keys.onEnterPressed: event => {
                    forwardMessagePopup.cancelConfirmation();
                    event.accepted = true;
                }
                Keys.onReturnPressed: event => {
                    forwardMessagePopup.cancelConfirmation();
                    event.accepted = true;
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Components.KomaiButton {
                id: forwardButton

                activeFocusOnTab: true
                focusPolicy: Qt.StrongFocus
                highlighted: true
                text: qsTr("Forward")
                onClicked: forwardMessagePopup.confirmForward()
                Keys.onEnterPressed: event => {
                    forwardMessagePopup.confirmForward();
                    event.accepted = true;
                }
                Keys.onReturnPressed: event => {
                    forwardMessagePopup.confirmForward();
                    event.accepted = true;
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Escape) {
                        forwardMessagePopup.cancelConfirmation();
                        event.accepted = true;
                    }
                }
            }
        }
    }
}
