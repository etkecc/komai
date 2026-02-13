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
    property int textHeight: Math.round(Qt.application.font.pixelSize * 2.4)
    property int textMargin: Nheko.paddingSmall
    property string pendingRoomId: ""
    property string pendingRoomName: ""
    property bool confirming: false

    function setMessageEventId(mid_in) {
        mid = mid_in;
    }

    padding: Nheko.paddingMedium
    modal: true

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

    onOpened: {
        confirming = false;
        pendingRoomId = "";
        pendingRoomName = "";
        roomTextInput.text = "";
        roomTextInput.forceActiveFocus();
    }

    contentItem: Column {
        id: forwardColumn

        spacing: Nheko.paddingSmall

        Label {
            id: titleLabel

            color: palette.text
            font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.6)
            font.bold: true
            text: qsTr("Forward Message")
        }

        Reply {
            id: replyPreview

            eventId: mid
            userColor: TimelineManager.userColor(replyPreview.userId, palette.window)
            width: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2
            maxWidth: forwardMessagePopup.width - forwardMessagePopup.leftPadding * 2
        }

        // Room search (visible when not confirming)
        MatrixTextField {
            id: roomTextInput

            color: palette.text
            font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.6)
            placeholderText: qsTr("Room name, address or id...")
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

                highlighted: true
                text: qsTr("Forward")
                onClicked: {
                    room.forwardMessage(forwardMessagePopup.mid, forwardMessagePopup.pendingRoomId);
                    forwardMessagePopup.close();
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Escape) {
                        forwardMessagePopup.confirming = false;
                        roomTextInput.forceActiveFocus();
                        event.accepted = true;
                    }
                }
            }

            Button {
                text: qsTr("Cancel")
                onClicked: {
                    forwardMessagePopup.confirming = false;
                    roomTextInput.forceActiveFocus();
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
            forwardButton.forceActiveFocus();
        }
        function onCountChanged() {
            if (completerPopup.count > 0 && (completerPopup.currentIndex < 0 || completerPopup.currentIndex >= completerPopup.count))
                completerPopup.currentIndex = 0;
        }

        target: completerPopup
    }
}
