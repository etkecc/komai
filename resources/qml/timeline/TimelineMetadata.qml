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
    property bool actionBarActive: false
    readonly property var actionToggleButton: actionToggleLoader.item ? actionToggleLoader.item : null

    signal actionToggled()

    layoutDirection: metadata.isSender ? Qt.RightToLeft : Qt.LeftToRight

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
    readonly property bool canOpenThreadNavigation: !!room
        && threadId !== ""
        && room.supportsThreadNavigation !== false
        && room.thread !== undefined

    spacing: 2

    Label {
        id: ts

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredWidth: implicitWidth
        color: effectiveInactiveTextColor
        font.pointSize: Settings.uiFontSizePt * parent.scaling
        text: metadata.timestamp.toLocaleTimeString(Locale.ShortFormat)

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

    StatusIndicator {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.indicatorSize
        Layout.preferredWidth: parent.indicatorSize
        visible: !metadata.isStateEvent && metadata.status != MtxEvent.Empty
        eventId: metadata.eventId
        status: metadata.status
    }
    Loader {
        active: metadata.isEdited || metadata.eventId == metadata.roomEditEventId
        sourceComponent: Component {
            Image {
                id: editedMarker

                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Layout.preferredHeight: metadata.indicatorSize
                Layout.preferredWidth: metadata.indicatorSize
                source: "image://colorimage/:/icons/icons/ui/edit.svg?" + ((metadata.eventId == metadata.roomEditEventId) ? effectiveHighlightColor : effectiveSecondaryTextColor)
                sourceSize.height: metadata.indicatorSize
                sourceSize.width: metadata.indicatorSize
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
        visible: metadata.roomIsEncrypted
    }
    Loader {
        id: actionToggleLoader
        active: Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton
        sourceComponent: Component {
            ImageButton {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Layout.preferredHeight: metadata.buttonSize
                Layout.preferredWidth: metadata.buttonSize
                toolTipDelay: 0
                toolTipText: qsTr("Message actions")
                toolTipVisible: hovered && !metadata.actionBarActive
                buttonTextColor: metadata.actionBarActive ? effectiveHighlightColor : Qt.rgba(effectiveInactiveTextColor.r, effectiveInactiveTextColor.g, effectiveInactiveTextColor.b, 0.35)
                highlightColor: effectiveHighlightColor
                changeColorOnHover: true
                image: ":/icons/icons/ui/textbox-more.svg"

                onClicked: metadata.actionToggled()
            }
        }
    }
    Loader {
        active: metadata.canOpenThreadNavigation
        sourceComponent: Component {
            ImageButton {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Layout.preferredHeight: metadata.buttonSize
                Layout.preferredWidth: metadata.buttonSize
                toolTipText: qsTr("Reply in this thread")
                toolTipVisible: hovered
                buttonTextColor: {
                    const _revision = colorRevision;
                    return TimelineManager.userColor(metadata.threadId, effectiveBaseColor);
                }
                image: ":/icons/icons/ui/thread.svg"

                onClicked: {
                    if (metadata.room)
                        metadata.room.thread = threadId
                }
            }
        }
    }
}
