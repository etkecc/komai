// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Item {
    id: aboutTab

    property var roomSettings
    property var members
    property var room
    property var appRoot

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Komai.paddingLarge
        spacing: Komai.paddingMedium

        Label {
            text: qsTr("Internal ID")
            color: palette.text
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Komai.paddingSmall

            TextField {
                text: aboutTab.roomSettings ? aboutTab.roomSettings.roomId : ""
                readOnly: true
                font.pointSize: Settings.uiFontSizePt
                Layout.fillWidth: true
            }

            ImageButton {
                id: copyIdBtn

                property bool copied: false

                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                ToolTip.visible: hovered
                ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                onClicked: {
                    if (aboutTab.roomSettings) {
                        Clipboard.text = aboutTab.roomSettings.roomId;
                        copied = true;
                        copyIdFeedbackTimer.start();
                    }
                }

                Timer {
                    id: copyIdFeedbackTimer
                    interval: 2000
                    onTriggered: copyIdBtn.copied = false
                }
            }
        }

        Label {
            text: qsTr("Room Version")
            color: palette.text
            font.bold: true
            Layout.topMargin: Komai.paddingMedium
        }

        Label {
            text: aboutTab.roomSettings ? aboutTab.roomSettings.roomVersion : ""
            color: palette.text
            font.pixelSize: fontMetrics.font.pixelSize
        }

        Label {
            text: qsTr("Determines which features the room supports. <a href=\"https://spec.matrix.org/v1.17/rooms/\">Learn more</a>.")
            color: palette.buttonText
            font.pointSize: 0.9 * Settings.uiFontSizePt
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            textFormat: Text.RichText
            linkColor: palette.highlight
            onLinkActivated: function(link) { Qt.openUrlExternally(link); }

            MouseArea {
                anchors.fill: parent
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                acceptedButtons: Qt.NoButton
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
