// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../../components" as Components
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

    ScrollView {
        id: scrollView

        anchors.fill: parent
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: 0

            // Internal ID row
            Item {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                implicitHeight: idRowContent.implicitHeight

                HoverHandler { id: idRowHover; blocking: false }
                Rectangle {
                    anchors.fill: idRowContent
                    color: palette.alternateBase
                    radius: Komai.paddingMedium
                    visible: idRowHover.hovered
                    z: -1
                }

                RowLayout {
                    id: idRowContent
                    width: parent.width

                    Label {
                        text: qsTr("Internal ID")
                        color: palette.text
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                    }

                    Components.KomaiTextField {
                        text: aboutTab.roomSettings ? aboutTab.roomSettings.roomId : ""
                        readOnly: true
                        font.pointSize: Settings.uiFontSizePt
                        Layout.preferredWidth: scrollView.availableWidth * 0.5
                    }

                    ImageButton {
                        id: copyIdBtn

                        property bool copied: false

                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        Layout.rightMargin: Komai.paddingMedium
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
            }

            // Room Version row
            Item {
                Layout.fillWidth: true
                implicitHeight: versionRowContent.implicitHeight

                HoverHandler { id: versionRowHover; blocking: false }
                Rectangle {
                    anchors.fill: versionRowContent
                    color: palette.alternateBase
                    radius: Komai.paddingMedium
                    visible: versionRowHover.hovered
                    z: -1
                }

                ColumnLayout {
                    id: versionRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Room Version")
                            color: palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.fillWidth: true
                        }

                        Label {
                            text: aboutTab.roomSettings ? aboutTab.roomSettings.roomVersion : ""
                            color: palette.text
                            font.pointSize: Settings.uiFontSizePt
                        }
                    }

                    Label {
                        text: qsTr("Determines which features the room supports. <a href=\"https://spec.matrix.org/v1.17/rooms/\">Learn more</a>.")
                        color: palette.buttonText
                        font.pointSize: 0.9 * Settings.uiFontSizePt
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        textFormat: Text.RichText
                        linkColor: palette.highlight
                        onLinkActivated: function(link) { Qt.openUrlExternally(link); }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }
        }
    }
}
