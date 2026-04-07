// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

OverlayDialog {
    id: allowedDialog

    property var roomSettings

    title: qsTr("Allowed rooms settings")
    titleIcon: ":/icons/icons/ui/settings.svg"

    MatrixText {
        text: qsTr("List of rooms that allow access to this room. Anyone who is in any of those rooms can join this room.")
        font.pixelSize: Math.floor(fontMetrics.font.pixelSize * 1.1)
        Layout.fillWidth: true
        color: palette.text
        Layout.bottomMargin: Komai.paddingMedium
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 250
        ScrollBar.horizontal.visible: false

        ListView {
            id: view

            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: roomSettings.allowedRoomsModel
            spacing: 4
            cacheBuffer: 50

            delegate: RowLayout {
                width: view.width

                ColumnLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: model.name
                        color: palette.text
                        textFormat: Text.PlainText
                    }

                    Text {
                        Layout.fillWidth: true
                        text: model.isParent ? qsTr("Parent community") : qsTr("Other room")
                        color: palette.buttonText
                        textFormat: Text.PlainText
                    }
                }

                ToggleButton {
                    checked: model.allowed
                    Layout.alignment: Qt.AlignRight
                    onCheckedChanged: model.allowed = checked
                }
            }
        }
    }

    Column {
        id: roomEntryCompleter

        Layout.fillWidth: true
        spacing: 1
        z: 5

        Completer {
            id: roomCompleter

            visible: roomEntry.text.length > 0
            width: parent.width
            roomId: allowedDialog.roomSettings.roomId
            completerType: "room"
            bottomToTop: true
            fullWidth: true
            avatarHeight: Komai.listIconSize / 2
            avatarWidth: Komai.listIconSize / 2
            rowMargin: 2
            rowSpacing: 2
        }

        KomaiTextField {
            id: roomEntry

            width: parent.width
            placeholderText: qsTr("Enter additional rooms not in the list yet...")
            onTextEdited: {
                roomCompleter.completer.searchString = text;
            }
            Keys.onPressed: {
                if (event.key == Qt.Key_Up || event.key == Qt.Key_Backtab) {
                    event.accepted = true;
                    roomCompleter.up();
                } else if (event.key == Qt.Key_Down || event.key == Qt.Key_Tab) {
                    event.accepted = true;
                    if (event.key == Qt.Key_Tab && (event.modifiers & Qt.ShiftModifier))
                        roomCompleter.up();
                    else
                        roomCompleter.down();
                } else if (event.matches(StandardKey.InsertParagraphSeparator)) {
                    roomCompleter.finishCompletion();
                    event.accepted = true;
                }
            }
        }
    }

    Connections {
        function onCompletionSelected(id) {
            console.log("selected: " + id);
            roomSettings.allowedRoomsModel.addRoom(id);
            roomEntry.clear();
        }

        function onCountChanged() {
            if (roomCompleter.count > 0 && (roomCompleter.currentIndex < 0 || roomCompleter.currentIndex >= roomCompleter.count))
                roomCompleter.currentIndex = 0;
        }

        target: roomCompleter
    }

    KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Save")
        highlighted: true
        onClicked: {
            roomSettings.applyAllowedFromModel();
            allowedDialog.close();
        }
    }
}
