// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    required property var dialogHost

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

        ActionButton {
            labelText: qsTr("New direct chat")
            descriptionText: qsTr("A 1-on-1 conversation with another user where you both get the same power level")
            iconSource: ":/icons/icons/ui/person.svg"
            shortcutSequence: "D"
            shortcutDisplayText: qsTr("D")
            onClicked: {
                root.close();
                root.dialogHost.openCatalogDialog(
                    "qrc:/resources/qml/dialogs/room/CreateDirect.qml", {});
            }
        }

        ActionButton {
            labelText: qsTr("New room")
            descriptionText: qsTr("A public or private room for group conversations")
            iconSource: ":/icons/icons/ui/people-community.svg"
            shortcutSequence: "R"
            shortcutDisplayText: qsTr("R")
            onClicked: {
                root.close();
                root.dialogHost.openCatalogDialog(
                    "qrc:/resources/qml/dialogs/room/CreateRoom.qml", {});
            }
        }

        ActionButton {
            labelText: qsTr("New space")
            descriptionText: qsTr("Create a new public or private collection of rooms")
            iconSource: ":/icons/icons/ui/squares-nested.svg"
            shortcutSequence: "S"
            shortcutDisplayText: qsTr("S")
            onClicked: {
                root.close();
                root.dialogHost.openCatalogDialog(
                    "qrc:/resources/qml/dialogs/room/CreateRoom.qml", {"space": true});
            }
        }
    }
}
