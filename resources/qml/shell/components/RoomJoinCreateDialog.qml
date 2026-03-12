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
        property string shortcutSequence: ""
        property string shortcutDisplayText: ""

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
        readonly property real actionIconSize: Math.round(Settings.uiFontSizePt * 2)

        Shortcut {
            enabled: root.visible && actionBtn.shortcutSequence !== ""
            sequence: actionBtn.shortcutSequence
            context: Qt.ApplicationShortcut

            onActivated: actionBtn.clicked()
        }

        contentItem: RowLayout {
            id: contentColumn

            spacing: Komai.paddingMedium

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: actionBtn.actionIconSize
                Layout.preferredHeight: actionBtn.actionIconSize
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

            Components.ShortcutKeyBadge {
                Layout.alignment: Qt.AlignVCenter
                text: actionBtn.shortcutDisplayText
                highlighted: actionBtn.activeState
                showKeyboardIcon: true
                liveModifierHighlight: true
                keyTextColor: actionBtn.descriptionColor
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
            shortcutSequence: "Alt+J"
            shortcutDisplayText: qsTr("Alt+J")
            onClicked: {
                root.close();
                Komai.openJoinRoomDialog();
            }
        }

        ActionButton {
            labelText: qsTr("Explore public rooms")
            descriptionText: qsTr("Browse the public room directory")
            iconSource: ":/icons/icons/ui/compass-northwest.svg"
            shortcutSequence: "Alt+E"
            shortcutDisplayText: qsTr("Alt+E")
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
            descriptionText: qsTr("A 1-on-1 conversation with another user where you both get the same power level")
            iconSource: ":/icons/icons/ui/person.svg"
            shortcutSequence: "Alt+D"
            shortcutDisplayText: qsTr("Alt+D")
            onClicked: {
                root.close();
                root.profileContextMenu.openCreateDirectDialog();
            }
        }

        ActionButton {
            labelText: qsTr("New room")
            descriptionText: qsTr("A public or private room for group conversations")
            iconSource: ":/icons/icons/ui/people-community.svg"
            shortcutSequence: "Alt+R"
            shortcutDisplayText: qsTr("Alt+R")
            onClicked: {
                root.close();
                root.profileContextMenu.openCreateRoomDialog({});
            }
        }

        ActionButton {
            labelText: qsTr("New space")
            descriptionText: qsTr("Create a new public or private collection of rooms")
            iconSource: ":/icons/icons/ui/squares-nested.svg"
            shortcutSequence: "Alt+S"
            shortcutDisplayText: qsTr("Alt+S")
            onClicked: {
                root.close();
                root.profileContextMenu.openCreateRoomDialog({
                        "space": true
                    });
            }
        }
    }
}
