// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import im.nheko

RowLayout {
    id: metadata

    property int iconSize: Math.floor(fontMetrics.ascent * scaling)
    property int buttonSize: Math.round(iconSize * buttonScale)
    required property double scaling
    property double buttonScale: 2
    required property bool isSender
    property bool actionBarActive: false
    readonly property alias actionToggleButton: actionToggleBtn

    signal actionToggled()

    layoutDirection: metadata.isSender ? Qt.RightToLeft : Qt.LeftToRight

    required property string eventId
    required property int status
    required property int trustlevel
    required property bool isEdited
    required property bool isEncrypted
    required property string threadId
    required property date timestamp
    required property Room room
    readonly property string roomEditEventId: room ? room.edit : ""
    readonly property bool roomIsEncrypted: room ? room.isEncrypted : false

    spacing: 2

    Label {
        id: ts

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredWidth: implicitWidth
        ToolTip.delay: Nheko.tooltipDelay
        ToolTip.text: Qt.formatDateTime(metadata.timestamp, Qt.DefaultLocaleLongDate)
        ToolTip.visible: ma.hovered
        color: palette.inactive.text
        font.pointSize: fontMetrics.font.pointSize * parent.scaling
        text: metadata.timestamp.toLocaleTimeString(Locale.ShortFormat)

        HoverHandler {
            id: ma

        }
    }
    StatusIndicator {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.iconSize
        Layout.preferredWidth: parent.iconSize
        visible: metadata.status != MtxEvent.Empty
        eventId: metadata.eventId
        status: metadata.status
    }
    Image {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.iconSize
        Layout.preferredWidth: parent.iconSize
        ToolTip.delay: Nheko.tooltipDelay
        ToolTip.text: qsTr("Edited")
        ToolTip.visible: editHovered.hovered
        source: "image://colorimage/:/icons/icons/ui/edit.svg?" + ((metadata.eventId == metadata.roomEditEventId) ? palette.highlight : palette.buttonText)
        sourceSize.height: parent.iconSize
        sourceSize.width: parent.iconSize
        visible: metadata.isEdited || metadata.eventId == metadata.roomEditEventId
        HoverHandler {
            id: editHovered

        }
    }
    EncryptionIndicator {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.iconSize
        Layout.preferredWidth: parent.iconSize
        encrypted: metadata.isEncrypted
        sourceSize.height: parent.iconSize
        sourceSize.width: parent.iconSize
        trust: metadata.trustlevel
        visible: metadata.roomIsEncrypted
    }
    ImageButton {
        id: actionToggleBtn

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        ToolTip.delay: Nheko.tooltipDelay
        ToolTip.text: qsTr("Message actions")
        ToolTip.visible: hovered && !metadata.actionBarActive
        buttonTextColor: metadata.actionBarActive ? palette.highlight : Qt.rgba(palette.inactive.text.r, palette.inactive.text.g, palette.inactive.text.b, 0.35)
        highlightColor: palette.highlight
        changeColorOnHover: true
        image: ":/icons/icons/ui/plus-circle.svg"
        visible: Settings.showActionButtons

        onClicked: metadata.actionToggled()
    }
    ImageButton {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        ToolTip.delay: Nheko.tooltipDelay
        ToolTip.text: qsTr("Part of a thread")
        ToolTip.visible: hovered
        buttonTextColor: TimelineManager.userColor(metadata.threadId, palette.base)
        image: ":/icons/icons/ui/thread.svg"
        visible: metadata.threadId

        onClicked: {
            if (metadata.room)
                metadata.room.thread = threadId
        }
    }
}
