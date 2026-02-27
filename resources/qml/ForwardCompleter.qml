// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./delegates/"
import QtQuick 2.9
import QtQuick.Controls 2.3
import im.nheko 1.0

Popup {
    id: forwardMessagePopup

    property string mid: ""
    property var roomSource: null
    readonly property var activeRoom: roomSource
    property var timelineSource: null
    property var timelineViewSource: null
    readonly property var timeline: timelineSource
    readonly property var timelineView: timelineViewSource
    property bool showReplyPreview: true
    property int textHeight: Math.round(Qt.application.font.pixelSize * 2.4)
    property int textMargin: Nheko.paddingSmall
    property string pendingRoomId: ""
    property string pendingRoomName: ""
    property bool confirming: false

    function setMessageEventId(mid_in) {
        mid = mid_in;
    }
    function cancelConfirmation() {
        confirming = false;
        roomTextInput.forceActiveFocus();
    }
    function confirmForward() {
        if (activeRoom)
            activeRoom.forwardMessage(forwardMessagePopup.mid, forwardMessagePopup.pendingRoomId);
        forwardMessagePopup.close();
    }

    padding: Nheko.paddingMedium
    modal: true
    focus: true

    // Workaround palettes not inheriting for popups
    palette: timelineRoot.palette
    parent: Overlay.overlay
    width: timelineRoot.width * 0.8
    x: Math.round(parent.width / 2 - width / 2)
    y: Math.round(parent.height / 4)

    Overlay.modal: Rectangle {
        color: Qt.rgba(palette.window.r, palette.window.g, palette.window.b, 0.7)
    }
    background: Rectangle {
        color: palette.alternateBase
        radius: 8
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

        spacing: Nheko.paddingSmall

        Row {
            spacing: Nheko.paddingSmall
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
                text: qsTr("Forward Message")
            }

            Item {
                height: 1
                width: parent.width - titleLabel.implicitWidth - titleLabel.font.pixelSize - closeButton.width - parent.spacing * 3
            }

            ImageButton {
                id: closeButton

                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.visible: hovered
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
            text: qsTr("Forwarding sends this content (without revealing its sender) to another room.")
            leftPadding: Nheko.paddingSmall
            topPadding: Nheko.paddingMedium
            bottomPadding: Nheko.paddingMedium
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2
            wrapMode: Text.Wrap
        }

        Loader {
            id: replyPreviewLoader

            active: forwardMessagePopup.showReplyPreview
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
                userColor: activeRoom ? TimelineManager.roomUserColor(activeRoom.roomId, replyPreview.userId, palette.window, palette.highlight) : TimelineManager.userColor(replyPreview.userId, palette.window)
                roomColor: activeRoom ? TimelineManager.roomUserColor(activeRoom.roomId, replyPreview.userId, palette.base, palette.highlight) : TimelineManager.userColor(replyPreview.userId, palette.base)
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
            radius: Nheko.paddingSmall
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

            avatarHeight: forwardMessagePopup.textHeight
            avatarWidth: forwardMessagePopup.textHeight
            bottomToTop: false
            centerRowContent: false
            completerName: "room"
            fullWidth: true
            rowMargin: Math.round(forwardMessagePopup.textMargin / 2)
            rowSpacing: forwardMessagePopup.textMargin
            visible: !forwardMessagePopup.confirming
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2
        }

        // Confirmation (visible when confirming)
        Label {
            id: confirmLabel

            color: palette.text
            font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.5)
            text: qsTr("Forward to <b>%1</b>?").arg(forwardMessagePopup.pendingRoomName)
            textFormat: Text.StyledText
            visible: forwardMessagePopup.confirming
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2
            wrapMode: Text.Wrap
        }

        Row {
            id: confirmButtons

            spacing: Nheko.paddingMedium
            visible: forwardMessagePopup.confirming

            Button {
                id: forwardButton

                activeFocusOnTab: true
                focusPolicy: Qt.StrongFocus
                highlighted: activeFocus
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

            Button {
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
        }
    }
    Connections {
        function onCompletionSelected(id) {
            var targetRoom = Rooms.getRoomById(id);
            forwardMessagePopup.pendingRoomId = id;
            forwardMessagePopup.pendingRoomName = targetRoom ? targetRoom.plainRoomName : id;
            forwardMessagePopup.confirming = true;
            Qt.callLater(() => forwardButton.forceActiveFocus(Qt.TabFocusReason));
        }
        function onCountChanged() {
            if (completerPopup.count > 0 && (completerPopup.currentIndex < 0 || completerPopup.currentIndex >= completerPopup.count))
                completerPopup.currentIndex = 0;
        }

        target: completerPopup
    }
}
