// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../ui"
import QtQuick
import QtQuick.Controls
import im.nheko

Popup {
    id: quickSwitcher

    property int textHeight: Math.round(Qt.application.font.pixelSize * 2.4)
    property int textMargin: Nheko.paddingSmall

    background: Rectangle {
        color: palette.alternateBase
        radius: 8
    }
    padding: Nheko.paddingMedium
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    modal: true

    // Workaround palettes not inheriting for popups
    palette: timelineRoot.palette
    parent: Overlay.overlay
    width: Math.min(Math.max(Math.round(parent.width / 2), 450), parent.width) // limiting width to parent.width/2 can be a bit narrow
    x: Math.round(parent.width / 2 - contentWidth / 2)
    y: Math.round(parent.height / 4)

    Overlay.modal: Rectangle {
        color: timelineRoot.overlayBackdropColor
    }

    onClosed: TimelineManager.focusMessageInput()
    onOpened: {
        roomTextInput.forceActiveFocus();
    }

    contentItem: Column {
        spacing: Nheko.paddingSmall

        Row {
            spacing: Nheko.paddingSmall
            width: parent.width

            Image {
                anchors.verticalCenter: parent.verticalCenter
                height: headerLabel.font.pixelSize
                width: height
                source: "image://colorimage/:/icons/icons/ui/search.svg?" + palette.text
                sourceSize.height: height * Screen.devicePixelRatio
                sourceSize.width: width * Screen.devicePixelRatio
            }

            Label {
                id: headerLabel

                text: qsTr("Find & switch room")
                color: palette.text
                font.pixelSize: Math.ceil(quickSwitcher.textHeight * 0.6)
                font.bold: true
            }

            Item {
                height: 1
                width: parent.width - headerLabel.implicitWidth - headerLabel.font.pixelSize - closeButton.width - parent.spacing * 3
            }

            ImageButton {
                id: closeButton

                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.visible: hovered
                anchors.verticalCenter: parent.verticalCenter
                height: headerLabel.font.pixelSize
                width: height
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: quickSwitcher.close()
            }
        }

        Label {
            id: hintLabel

            color: palette.buttonText
            font.pixelSize: Math.ceil(quickSwitcher.textHeight * 0.4)
            text: qsTr("Searches among rooms you participate in, not across all rooms on Matrix.")
            leftPadding: Nheko.paddingSmall
            topPadding: Nheko.paddingMedium
            bottomPadding: Nheko.paddingMedium
            width: parent.width
            wrapMode: Text.Wrap
        }

        MatrixTextField {
            id: roomTextInput

            color: palette.text
            font.pixelSize: Math.ceil(quickSwitcher.textHeight * 0.6)
            placeholderText: qsTr("Room name, address or id...")
            radius: Nheko.paddingSmall
            width: parent.width

            Keys.onPressed: event => {
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

            avatarHeight: quickSwitcher.textHeight
            avatarWidth: quickSwitcher.textHeight
            bottomToTop: false
            centerRowContent: false
            completerName: "room"
            fullWidth: true
            rowMargin: Math.round(quickSwitcher.textMargin / 2)
            rowSpacing: quickSwitcher.textMargin
            visible: roomTextInput.text.length > 0
            width: parent.width

            onCompletionSelected: (id) => {
                Rooms.setCurrentRoom(id);
                quickSwitcher.close();
            }
            onCountChanged: {
                if (completerPopup.count > 0
                        && (completerPopup.currentIndex < 0 || completerPopup.currentIndex >= completerPopup.count))
                    completerPopup.currentIndex = 0;
            }
        }
    }
}
