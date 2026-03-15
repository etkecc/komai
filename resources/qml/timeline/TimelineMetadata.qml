// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

RowLayout {
    id: metadata

    property int iconSize: Math.floor(fontMetrics.ascent * scaling)
    property int rawButtonSize: Math.round(iconSize * buttonScale)
    property int buttonSize: (rawButtonSize % 2 === 0) ? rawButtonSize : (rawButtonSize + 1)
    property int rawIndicatorSize: Math.round(iconSize * 1.5)
    property int indicatorSize: (rawIndicatorSize % 2 === 0) ? rawIndicatorSize : (rawIndicatorSize + 1)
    required property double scaling
    property double buttonScale: 2
    required property bool isSender
    // Plain style uses fixed metadata order:
    // [icons/buttons ...][timestamp][message actions button].
    property bool forceTrailingTimestampLayout: false
    // In trailing layout, allow placing the actions toggle before timestamp/icons.
    property bool leadingActionInTrailingLayout: false
    property bool actionBarActive: false
    readonly property Item actionToggleButton: forceTrailingTimestampLayout
        ? (leadingActionInTrailingLayout ? actionToggleBtnTrailingLeading : actionToggleBtnTrailing)
        : actionToggleBtn

    signal actionToggled()

    layoutDirection: metadata.forceTrailingTimestampLayout
        ? Qt.LeftToRight
        : (metadata.isSender ? Qt.RightToLeft : Qt.LeftToRight)

    required property string eventId
    required property int status
    required property int trustlevel
    required property bool isEdited
    required property bool isEncrypted
    required property bool isStateEvent
    required property string threadId
    required property date timestamp
    required property var room
    readonly property string roomEditEventId: room ? room.edit : ""
    readonly property bool roomIsEncrypted: room ? room.isEncrypted : false

    spacing: 2

    Label {
        id: ts

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredWidth: implicitWidth
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: Qt.formatDateTime(metadata.timestamp, Qt.DefaultLocaleLongDate)
        ToolTip.visible: ma.hovered
        color: palette.inactive.text
        font.pointSize: Settings.uiFontSizePt * parent.scaling
        text: metadata.timestamp.toLocaleTimeString(Locale.ShortFormat)
        visible: !metadata.forceTrailingTimestampLayout

        HoverHandler {
            id: ma

        }
    }
    ImageButton {
        id: actionToggleBtnTrailingLeading

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: qsTr("Message actions")
        ToolTip.visible: hovered && !metadata.actionBarActive
        buttonTextColor: metadata.actionBarActive ? palette.highlight : Qt.rgba(palette.inactive.text.r, palette.inactive.text.g, palette.inactive.text.b, 0.35)
        highlightColor: palette.highlight
        changeColorOnHover: true
        image: ":/icons/icons/ui/options-circle.svg"
        visible: metadata.forceTrailingTimestampLayout
            && metadata.leadingActionInTrailingLayout
            && Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

        onClicked: metadata.actionToggled()
    }
    Label {
        id: tsTrailingLeading

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredWidth: implicitWidth
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: Qt.formatDateTime(metadata.timestamp, Qt.DefaultLocaleLongDate)
        ToolTip.visible: maTrailingLeading.hovered
        color: palette.inactive.text
        font.pointSize: Settings.uiFontSizePt * parent.scaling
        text: metadata.timestamp.toLocaleTimeString(Locale.ShortFormat)
        visible: metadata.forceTrailingTimestampLayout && metadata.leadingActionInTrailingLayout

        HoverHandler {
            id: maTrailingLeading
        }
    }

    StatusIndicator {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.indicatorSize
        Layout.preferredWidth: parent.indicatorSize
        visible: !metadata.forceTrailingTimestampLayout && !metadata.isStateEvent && metadata.status != MtxEvent.Empty
        eventId: metadata.eventId
        status: metadata.status
    }
    Image {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.indicatorSize
        Layout.preferredWidth: parent.indicatorSize
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: qsTr("Edited")
        ToolTip.visible: editHovered.hovered
        source: "image://colorimage/:/icons/icons/ui/edit.svg?" + ((metadata.eventId == metadata.roomEditEventId) ? palette.highlight : palette.buttonText)
        sourceSize.height: parent.indicatorSize
        sourceSize.width: parent.indicatorSize
        visible: !metadata.forceTrailingTimestampLayout && (metadata.isEdited || metadata.eventId == metadata.roomEditEventId)
        HoverHandler {
            id: editHovered

        }
    }
    EncryptionIndicator {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        encrypted: metadata.isEncrypted
        sourceSize.height: parent.buttonSize
        sourceSize.width: parent.buttonSize
        trust: metadata.trustlevel
        visible: !metadata.forceTrailingTimestampLayout && metadata.roomIsEncrypted
    }
    ImageButton {
        id: actionToggleBtn

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: qsTr("Message actions")
        ToolTip.visible: hovered && !metadata.actionBarActive
        buttonTextColor: metadata.actionBarActive ? palette.highlight : Qt.rgba(palette.inactive.text.r, palette.inactive.text.g, palette.inactive.text.b, 0.35)
        highlightColor: palette.highlight
        changeColorOnHover: true
        image: ":/icons/icons/ui/options-circle.svg"
        visible: !metadata.forceTrailingTimestampLayout
            && Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

        onClicked: metadata.actionToggled()
    }
    ImageButton {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: qsTr("Reply in this thread")
        ToolTip.visible: hovered
        buttonTextColor: TimelineManager.userColor(metadata.threadId, palette.base)
        image: ":/icons/icons/ui/thread.svg"
        visible: !metadata.forceTrailingTimestampLayout && metadata.threadId

        onClicked: {
            if (metadata.room)
                metadata.room.thread = threadId
        }
    }

    StatusIndicator {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.indicatorSize
        Layout.preferredWidth: parent.indicatorSize
        visible: metadata.forceTrailingTimestampLayout && !metadata.isStateEvent && metadata.status != MtxEvent.Empty
        eventId: metadata.eventId
        status: metadata.status
    }
    Image {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.indicatorSize
        Layout.preferredWidth: parent.indicatorSize
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: qsTr("Edited")
        ToolTip.visible: editHoveredTrailing.hovered
        source: "image://colorimage/:/icons/icons/ui/edit.svg?" + ((metadata.eventId == metadata.roomEditEventId) ? palette.highlight : palette.buttonText)
        sourceSize.height: parent.indicatorSize
        sourceSize.width: parent.indicatorSize
        visible: metadata.forceTrailingTimestampLayout && (metadata.isEdited || metadata.eventId == metadata.roomEditEventId)
        HoverHandler {
            id: editHoveredTrailing

        }
    }
    EncryptionIndicator {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        encrypted: metadata.isEncrypted
        sourceSize.height: parent.buttonSize
        sourceSize.width: parent.buttonSize
        trust: metadata.trustlevel
        visible: metadata.forceTrailingTimestampLayout && metadata.roomIsEncrypted
    }
    ImageButton {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: qsTr("Reply in this thread")
        ToolTip.visible: hovered
        buttonTextColor: TimelineManager.userColor(metadata.threadId, palette.base)
        image: ":/icons/icons/ui/thread.svg"
        visible: metadata.forceTrailingTimestampLayout && metadata.threadId

        onClicked: {
            if (metadata.room)
                metadata.room.thread = threadId
        }
    }
    Label {
        id: tsTrailing

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredWidth: implicitWidth
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: Qt.formatDateTime(metadata.timestamp, Qt.DefaultLocaleLongDate)
        ToolTip.visible: maTrailing.hovered
        color: palette.inactive.text
        font.pointSize: Settings.uiFontSizePt * parent.scaling
        text: metadata.timestamp.toLocaleTimeString(Locale.ShortFormat)
        visible: metadata.forceTrailingTimestampLayout && !metadata.leadingActionInTrailingLayout

        HoverHandler {
            id: maTrailing

        }
    }
    ImageButton {
        id: actionToggleBtnTrailing

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        ToolTip.delay: Komai.tooltipDelay
        ToolTip.text: qsTr("Message actions")
        ToolTip.visible: hovered && !metadata.actionBarActive
        buttonTextColor: metadata.actionBarActive ? palette.highlight : Qt.rgba(palette.inactive.text.r, palette.inactive.text.g, palette.inactive.text.b, 0.35)
        highlightColor: palette.highlight
        changeColorOnHover: true
        image: ":/icons/icons/ui/options-circle.svg"
        visible: metadata.forceTrailingTimestampLayout
            && !metadata.leadingActionInTrailingLayout
            && Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

        onClicked: metadata.actionToggled()
    }
}
