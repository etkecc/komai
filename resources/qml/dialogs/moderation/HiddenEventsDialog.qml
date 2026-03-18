// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import QtQuick 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: hiddenEventsDialog

    property string roomid: ""
    property string roomName: ""

    title: roomid ? qsTr("Hidden events for %1").arg(roomName) : qsTr("Hidden events")
    titleIcon: ":/icons/icons/ui/settings.svg"

    HiddenEvents {
        id: hiddenEvents

        roomid: hiddenEventsDialog.roomid
    }

    MatrixText {
        text: roomid ? qsTr("These events will be <b>shown</b> in %1:").arg(roomName) : qsTr("These events will be <b>shown</b> in all rooms:")
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
            id: userEventsLabel

            text: qsTr("User events")
            Layout.fillWidth: true

            HoverHandler {
                id: hh1
            }

            Components.KomaiToolTip {
                anchorItem: userEventsLabel
                anchorX: userEventsLabel.width / 2
                anchorY: 0
                text: qsTr("Joins, leaves, avatar and name changes, bans, …")
                requestedVisible: hh1.hovered
            }
        }

        ToggleButton {
            Layout.alignment: Qt.AlignRight
            checked: !hiddenEvents.hiddenEvents.includes(MtxEvent.Member)
            onToggled: hiddenEvents.toggle(MtxEvent.Member)
        }

        MatrixText {
            id: powerLevelChangesLabel

            text: qsTr("Power level changes")
            Layout.fillWidth: true

            HoverHandler {
                id: hh2
            }

            Components.KomaiToolTip {
                anchorItem: powerLevelChangesLabel
                anchorX: powerLevelChangesLabel.width / 2
                anchorY: 0
                text: qsTr("Sent when a moderator is added/removed or the permissions of a room are changed.")
                requestedVisible: hh2.hovered
            }
        }

        ToggleButton {
            Layout.alignment: Qt.AlignRight
            checked: !hiddenEvents.hiddenEvents.includes(MtxEvent.PowerLevels)
            onToggled: hiddenEvents.toggle(MtxEvent.PowerLevels)
        }

        MatrixText {
            text: qsTr("Stickers")
            Layout.fillWidth: true
        }

        ToggleButton {
            Layout.alignment: Qt.AlignRight
            checked: !hiddenEvents.hiddenEvents.includes(MtxEvent.Sticker)
            onToggled: hiddenEvents.toggle(MtxEvent.Sticker)
        }

        MatrixText {
            text: qsTr("Allowed server changes")
            Layout.fillWidth: true
        }

        ToggleButton {
            Layout.alignment: Qt.AlignRight
            checked: !hiddenEvents.hiddenEvents.includes(MtxEvent.ServerAcl)
            onToggled: hiddenEvents.toggle(MtxEvent.ServerAcl)
        }
    }

    Components.KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Save")
        highlighted: true
        onClicked: {
            hiddenEvents.save();
            hiddenEventsDialog.close();
        }
    }
}
