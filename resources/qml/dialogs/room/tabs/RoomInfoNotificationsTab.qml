// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Item {
    id: notificationsTab

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

            // Notifications row
            Item {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                implicitHeight: notifRowContent.implicitHeight

                HoverHandler { id: notifRowHover; blocking: false }
                Rectangle {
                    anchors.fill: notifRowContent
                    color: palette.alternateBase
                    radius: Komai.paddingMedium
                    visible: notifRowHover.hovered
                    z: -1
                }

                ColumnLayout {
                    id: notifRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Notifications")
                            color: palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            id: notificationsCombo

                            model: [qsTr("Muted"), qsTr("Mentions only"), qsTr("All messages")]
                            currentIndex: notificationsTab.roomSettings ? notificationsTab.roomSettings.notifications : 0
                            onActivated: (index) => {
                                if (notificationsTab.roomSettings)
                                    notificationsTab.roomSettings.changeNotifications(index);
                            }
                            wheelEnabled: activeFocus
                        }
                    }

                    Label {
                        text: qsTr("Configure how you receive notifications for this room.")
                        color: palette.buttonText
                        font.pointSize: 0.9 * Settings.uiFontSizePt
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
