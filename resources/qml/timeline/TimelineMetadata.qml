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
    property var contentPalette: null
    readonly property int colorRevision: TimelineManager.colorRevision
    readonly property bool hasContentPalette: contentPalette !== null && contentPalette !== undefined
    readonly property color effectiveBaseColor: {
        const _revision = colorRevision;
        if (hasContentPalette && contentPalette.base !== undefined && contentPalette.base !== null)
            return contentPalette.base;
        if (Komai.colors && Komai.colors.base !== undefined)
            return Komai.colors.base;
        return palette.base;
    }
    readonly property color effectiveTextColor: {
        const _revision = colorRevision;
        if (hasContentPalette && contentPalette.text !== undefined && contentPalette.text !== null)
            return contentPalette.text;
        if (Komai.colors && Komai.colors.text !== undefined)
            return Komai.colors.text;
        return palette.text;
    }
    readonly property color effectiveSecondaryTextColor: {
        const _revision = colorRevision;
        if (hasContentPalette && contentPalette.buttonText !== undefined && contentPalette.buttonText !== null)
            return contentPalette.buttonText;
        if (Komai.colors && Komai.colors.buttonText !== undefined)
            return Komai.colors.buttonText;
        return palette.buttonText;
    }
    readonly property color effectiveInactiveTextColor: {
        const _revision = colorRevision;
        if (hasContentPalette)
            return effectiveSecondaryTextColor;
        if (Komai.inactiveColors && Komai.inactiveColors.text !== undefined)
            return Komai.inactiveColors.text;
        return palette.inactive.text;
    }
    readonly property color effectiveHighlightColor: {
        const _revision = colorRevision;
        if (hasContentPalette && contentPalette.highlight !== undefined && contentPalette.highlight !== null)
            return contentPalette.highlight;
        if (Komai.colors && Komai.colors.highlight !== undefined)
            return Komai.colors.highlight;
        return palette.highlight;
    }

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
        color: effectiveInactiveTextColor
        font.pointSize: Settings.uiFontSizePt * parent.scaling
        text: metadata.timestamp.toLocaleTimeString(Locale.ShortFormat)
        visible: !metadata.forceTrailingTimestampLayout

        HoverHandler {
            id: ma

        }

        KomaiToolTip {
            anchorItem: ts
            anchorX: ts.width / 2
            anchorY: 0
            text: Qt.formatDateTime(metadata.timestamp, Qt.DefaultLocaleLongDate)
            delay: Komai.tooltipDelay
            requestedVisible: ma.hovered
        }
    }
    ImageButton {
        id: actionToggleBtnTrailingLeading

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        toolTipDelay: 0
        toolTipText: qsTr("Message actions")
        toolTipVisible: hovered && !metadata.actionBarActive
        buttonTextColor: metadata.actionBarActive ? effectiveHighlightColor : Qt.rgba(effectiveInactiveTextColor.r, effectiveInactiveTextColor.g, effectiveInactiveTextColor.b, 0.35)
        highlightColor: effectiveHighlightColor
        changeColorOnHover: true
        image: ":/icons/icons/ui/textbox-more.svg"
        visible: metadata.forceTrailingTimestampLayout
            && metadata.leadingActionInTrailingLayout
            && Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

        onClicked: metadata.actionToggled()
    }
    Label {
        id: tsTrailingLeading

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredWidth: implicitWidth
        color: effectiveInactiveTextColor
        font.pointSize: Settings.uiFontSizePt * parent.scaling
        text: metadata.timestamp.toLocaleTimeString(Locale.ShortFormat)
        visible: metadata.forceTrailingTimestampLayout && metadata.leadingActionInTrailingLayout

        HoverHandler {
            id: maTrailingLeading
        }

        KomaiToolTip {
            anchorItem: tsTrailingLeading
            anchorX: tsTrailingLeading.width / 2
            anchorY: 0
            text: Qt.formatDateTime(metadata.timestamp, Qt.DefaultLocaleLongDate)
            delay: Komai.tooltipDelay
            requestedVisible: maTrailingLeading.hovered
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
        id: editedMarker

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.indicatorSize
        Layout.preferredWidth: parent.indicatorSize
        source: "image://colorimage/:/icons/icons/ui/edit.svg?" + ((metadata.eventId == metadata.roomEditEventId) ? effectiveHighlightColor : effectiveSecondaryTextColor)
        sourceSize.height: parent.indicatorSize
        sourceSize.width: parent.indicatorSize
        visible: !metadata.forceTrailingTimestampLayout && (metadata.isEdited || metadata.eventId == metadata.roomEditEventId)
        HoverHandler {
            id: editHovered

        }

        KomaiToolTip {
            anchorItem: editedMarker
            anchorX: editedMarker.width / 2
            anchorY: 0
            text: qsTr("Edited")
            delay: Komai.tooltipDelay
            requestedVisible: editHovered.hovered
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
        toolTipDelay: 0
        toolTipText: qsTr("Message actions")
        toolTipVisible: hovered && !metadata.actionBarActive
        buttonTextColor: metadata.actionBarActive ? effectiveHighlightColor : Qt.rgba(effectiveInactiveTextColor.r, effectiveInactiveTextColor.g, effectiveInactiveTextColor.b, 0.35)
        highlightColor: effectiveHighlightColor
        changeColorOnHover: true
        image: ":/icons/icons/ui/textbox-more.svg"
        visible: !metadata.forceTrailingTimestampLayout
            && Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

        onClicked: metadata.actionToggled()
    }
    ImageButton {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        toolTipText: qsTr("Reply in this thread")
        toolTipVisible: hovered
        buttonTextColor: {
            const _revision = colorRevision;
            return TimelineManager.userColor(metadata.threadId, effectiveBaseColor);
        }
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
        id: editedMarkerTrailing

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.indicatorSize
        Layout.preferredWidth: parent.indicatorSize
        source: "image://colorimage/:/icons/icons/ui/edit.svg?" + ((metadata.eventId == metadata.roomEditEventId) ? effectiveHighlightColor : effectiveSecondaryTextColor)
        sourceSize.height: parent.indicatorSize
        sourceSize.width: parent.indicatorSize
        visible: metadata.forceTrailingTimestampLayout && (metadata.isEdited || metadata.eventId == metadata.roomEditEventId)
        HoverHandler {
            id: editHoveredTrailing

        }

        KomaiToolTip {
            anchorItem: editedMarkerTrailing
            anchorX: editedMarkerTrailing.width / 2
            anchorY: 0
            text: qsTr("Edited")
            delay: Komai.tooltipDelay
            requestedVisible: editHoveredTrailing.hovered
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
        toolTipText: qsTr("Reply in this thread")
        toolTipVisible: hovered
        buttonTextColor: {
            const _revision = colorRevision;
            return TimelineManager.userColor(metadata.threadId, effectiveBaseColor);
        }
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
        color: effectiveInactiveTextColor
        font.pointSize: Settings.uiFontSizePt * parent.scaling
        text: metadata.timestamp.toLocaleTimeString(Locale.ShortFormat)
        visible: metadata.forceTrailingTimestampLayout && !metadata.leadingActionInTrailingLayout

        HoverHandler {
            id: maTrailing

        }

        KomaiToolTip {
            anchorItem: tsTrailing
            anchorX: tsTrailing.width / 2
            anchorY: 0
            text: Qt.formatDateTime(metadata.timestamp, Qt.DefaultLocaleLongDate)
            delay: Komai.tooltipDelay
            requestedVisible: maTrailing.hovered
        }
    }
    ImageButton {
        id: actionToggleBtnTrailing

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        toolTipDelay: 0
        toolTipText: qsTr("Message actions")
        toolTipVisible: hovered && !metadata.actionBarActive
        buttonTextColor: metadata.actionBarActive ? effectiveHighlightColor : Qt.rgba(effectiveInactiveTextColor.r, effectiveInactiveTextColor.g, effectiveInactiveTextColor.b, 0.35)
        highlightColor: effectiveHighlightColor
        changeColorOnHover: true
        image: ":/icons/icons/ui/textbox-more.svg"
        visible: metadata.forceTrailingTimestampLayout
            && !metadata.leadingActionInTrailingLayout
            && Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

        onClicked: metadata.actionToggled()
    }
}
