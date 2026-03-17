// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: dialog

    property string roomid: ""
    property string roomName: ""

    title: roomid ? qsTr("Event expiration for %1").arg(roomName) : qsTr("Event expiration")
    titleIcon: ":/icons/icons/ui/clock.svg"

    EventExpiry {
        id: eventExpiry

        roomid: dialog.roomid
    }

    MatrixText {
        text: roomid ? qsTr("You can configure when your messages will be deleted in %1. This only happens when Komai is open and has permissions to delete messages until Matrix servers support this feature natively. In general 0 means disable.").arg(roomName) : qsTr("You can configure when your messages will be deleted in all rooms unless configured otherwise. This only happens when Komai is open and has permissions to delete messages until Matrix servers support this feature natively. In general 0 means disable.")
        font.pixelSize: Math.floor(fontMetrics.font.pixelSize * 1.2)
        Layout.fillWidth: true
    }

    Components.SyncedToMatrixBadge {
        Layout.fillWidth: true
    }

    GridLayout {
        columns: 2
        rowSpacing: Komai.paddingMedium
        Layout.fillWidth: true

        MatrixText {
            text: qsTr("Expire events after X days")
            ToolTip.text: qsTr("Automatically redacts messages after X days, unless otherwise protected. Set to 0 to disable.")
            ToolTip.visible: hh1.hovered
            Layout.fillWidth: true

            HoverHandler {
                id: hh1
            }
        }

        Components.KomaiSpinBox {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            from: 0
            to: 1000
            stepSize: 1
            value: eventExpiry.expireEventsAfterDays
            onValueChanged: eventExpiry.expireEventsAfterDays = value
            editable: true
        }

        MatrixText {
            text: qsTr("Only keep latest X events")
            ToolTip.text: qsTr("Deletes your events in this room if there are more than X newer messages unless otherwise protected. Set to 0 to disable.")
            ToolTip.visible: hh2.hovered
            Layout.fillWidth: true

            HoverHandler {
                id: hh2
            }
        }

        Components.KomaiSpinBox {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            from: 0
            to: 1000000
            stepSize: 1
            value: eventExpiry.expireEventsAfterCount
            onValueChanged: eventExpiry.expireEventsAfterCount = value
            editable: true
        }

        MatrixText {
            text: qsTr("Always keep latest X events")
            ToolTip.text: qsTr("This prevents events to be deleted by the above 2 settings if they are the latest X messages from you in the room.")
            ToolTip.visible: hh3.hovered
            Layout.fillWidth: true

            HoverHandler {
                id: hh3
            }
        }

        Components.KomaiSpinBox {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            from: 0
            to: 1000000
            stepSize: 1
            value: eventExpiry.protectLatestEvents
            onValueChanged: eventExpiry.protectLatestEvents = value
            editable: true
        }

        MatrixText {
            text: qsTr("Include state events")
            ToolTip.text: qsTr("If this is turned on, old state events also get redacted. The latest state event of any type+key combination is excluded from redaction to not remove the room name and similar state by accident.")
            ToolTip.visible: hh4.hovered
            Layout.fillWidth: true

            HoverHandler {
                id: hh4
            }
        }

        ToggleButton {
            Layout.alignment: Qt.AlignRight
            checked: eventExpiry.expireStateEvents
            onToggled: eventExpiry.expireStateEvents = checked
        }
    }

    MatrixText {
        visible: !Settings.privacyMaintenanceExpireEvents
        text: qsTr("Enable event expiration in Settings -> Privacy, or these rules will not run.")
        color: Komai.theme.attention
        Layout.fillWidth: true
    }

    Components.KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Save")
        highlighted: true
        onClicked: {
            eventExpiry.save();
            dialog.close();
        }
    }
}
