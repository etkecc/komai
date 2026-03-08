// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    required property var profileContextMenu

    title: qsTr("New")
    titleIcon: ":/icons/icons/ui/plus-circle.svg"

    onAboutToShow: contentItem.forceActiveFocus()

    component ActionButton: AbstractButton {
        id: actionBtn

        required property string labelText
        required property string descriptionText
        required property string iconSource

        Layout.fillWidth: true
        implicitHeight: contentColumn.implicitHeight + topPadding + bottomPadding
        leftPadding: Komai.paddingMedium
        rightPadding: Komai.paddingMedium
        topPadding: Komai.paddingMedium
        bottomPadding: Komai.paddingMedium
        hoverEnabled: true
        activeFocusOnTab: true
        focusPolicy: Qt.StrongFocus

        readonly property bool activeState: hovered || pressed || activeFocus
        readonly property color actionTextColor: activeState ? palette.brightText : palette.text
        readonly property color descriptionColor: activeState ? palette.brightText : palette.buttonText

        contentItem: RowLayout {
            id: contentColumn

            spacing: Komai.paddingMedium

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                fillMode: Image.PreserveAspectFit
                source: actionBtn.iconSource !== "" ? "image://colorimage/" + actionBtn.iconSource + "?" + actionBtn.actionTextColor : ""
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: actionBtn.labelText
                    color: actionBtn.actionTextColor
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: actionBtn.descriptionText
                    color: actionBtn.descriptionColor
                    font.pointSize: Settings.uiFontSizePt * 0.9
                    wrapMode: Text.WordWrap
                    visible: text !== ""
                }
            }
        }

        background: Rectangle {
            radius: Komai.paddingMedium
            color: actionBtn.activeState ? palette.dark : palette.window
        }

        KomaiCursorShape {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        // --- Join section ---
        Components.SettingsSection {
            label: qsTr("Join")
            Layout.fillWidth: true
        }

        ActionButton {
            labelText: qsTr("Join room")
            descriptionText: qsTr("Enter a room address or alias to join")
            iconSource: ":/icons/icons/ui/arrow-join.svg"
            onClicked: {
                root.close();
                Komai.openJoinRoomDialog();
            }
        }

        ActionButton {
            labelText: qsTr("Explore public rooms")
            descriptionText: qsTr("Browse the public room directory")
            iconSource: ":/icons/icons/ui/search.svg"
            onClicked: {
                root.close();
                root.profileContextMenu.openRoomDirectoryDialog();
            }
        }

        // --- Create section ---
        Components.SettingsSection {
            label: qsTr("Create")
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingMedium
        }

        ActionButton {
            labelText: qsTr("New direct chat")
            descriptionText: qsTr("A 1-on-1 conversation with another user. Members get the same power level.")
            iconSource: ":/icons/icons/ui/person.svg"
            onClicked: {
                root.close();
                root.profileContextMenu.openCreateDirectDialog();
            }
        }

        ActionButton {
            labelText: qsTr("New room")
            descriptionText: qsTr("A public or private room for group conversations")
            iconSource: ":/icons/icons/ui/people-community.svg"
            onClicked: {
                root.close();
                root.profileContextMenu.openCreateRoomDialog({});
            }
        }

        ActionButton {
            labelText: qsTr("New space")
            descriptionText: qsTr("Create a new public or private collection of rooms")
            iconSource: ":/icons/icons/ui/squares-nested.svg"
            onClicked: {
                root.close();
                root.profileContextMenu.openCreateRoomDialog({
                        "space": true
                    });
            }
        }
    }
}
