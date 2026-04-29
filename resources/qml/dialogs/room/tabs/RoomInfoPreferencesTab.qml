// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../../components" as Components
import "../../moderation"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Item {
    id: preferencesTab

    property var roomSettings
    property var members
    property var room
    property var appRoot

    ScrollView {
        id: scrollView

        anchors.fill: parent
        ScrollBar.vertical.policy: Settings.uiScrollbarPolicy === Settings.ScrollbarPolicy.Always ? ScrollBar.AlwaysOn : Settings.uiScrollbarPolicy === Settings.ScrollbarPolicy.Never ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: Komai.paddingSmall

            // --- Message visibility section ---
            Components.SettingsSection {
                label: qsTr("Message visibility")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
            }

            // Locally hidden events
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: hiddenEventsRowContent.implicitHeight
                HoverHandler { id: hiddenEventsRowHover; blocking: false }
                Rectangle { anchors.fill: hiddenEventsRowContent; color: hiddenEventsRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                RowLayout {
                    id: hiddenEventsRowContent
                    width: parent.width

                    Label {
                        text: qsTr("Locally hidden events")
                        color: hiddenEventsRowHover.hovered ? palette.brightText : palette.text
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                    }

                    Item { Layout.fillWidth: true }

                    HiddenEventsDialog {
                        id: hiddenEventsDialog
                        roomid: preferencesTab.roomSettings ? preferencesTab.roomSettings.roomId : ""
                    }

                    Components.KomaiButton {
                        text: qsTr("Configure")
                        onClicked: hiddenEventsDialog.open()
                        Layout.rightMargin: Komai.paddingMedium
                    }
                }
            }

            // Collapse thread replies (per-room override)
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: collapseRepliesRowContent.implicitHeight
                HoverHandler { id: collapseRepliesRowHover; blocking: false }
                Rectangle { anchors.fill: collapseRepliesRowContent; color: collapseRepliesRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                ColumnLayout {
                    id: collapseRepliesRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Collapse thread replies")
                            color: collapseRepliesRowHover.hovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                        }

                        Item { Layout.fillWidth: true }

                        Components.KomaiComboBox {
                            id: collapseRepliesCombo

                            readonly property string roomId: preferencesTab.roomSettings ? preferencesTab.roomSettings.roomId : ""
                            readonly property bool globalValue: Settings.timelineThreadsCollapseReplies

                            model: [
                                qsTr("Global Default (currently: %1)").arg(globalValue ? qsTr("On") : qsTr("Off")),
                                qsTr("On"),
                                qsTr("Off")
                            ]

                            currentIndex: {
                                var override = Settings.timelineThreadsCollapseRepliesOverrideForRoom(roomId);
                                if (override !== undefined && override !== null)
                                    return override ? 1 : 2;
                                return 0;
                            }

                            onActivated: function(index) {
                                var roomId = collapseRepliesCombo.roomId;
                                if (!roomId) return;
                                if (index === 0) {
                                    Settings.removeTimelineThreadsCollapseRepliesForRoom(roomId);
                                } else {
                                    Settings.setTimelineThreadsCollapseRepliesForRoom(roomId, index === 1);
                                }
                            }
                        }
                    }

                    Label {
                        text: qsTr("Hides thread replies from the main timeline, showing only thread root messages.<br>⚠️ Per-thread unread tracking is not supported, so you may miss replies in older threads.")
                        color: collapseRepliesRowHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                        textFormat: Text.RichText
                    }
                }
            }
        }
    }
}
