// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: tabDelegate

    HoverHandler {
        id: tabHoverHandler
    }

    required property int index
    required property string roomId
    required property string roomName
    required property var tabController

    readonly property bool isActive: roomId === Rooms.currentRoomId
    readonly property bool isHovered: tabHoverHandler.hovered || closeArea.containsMouse
    readonly property bool isLastTab: index === tabController.tabs.count - 1

    // Attention state (re-evaluated when attentionRevision changes).
    readonly property int _attRev: tabController.attentionRevision
    readonly property int _roomRow: {
        var r = _attRev;
        return Rooms.roomidToIndex(roomId);
    }
    readonly property bool hasUnread: {
        var r = _attRev;
        return _roomRow >= 0
            && !!Rooms.data(Rooms.index(_roomRow, 0), tabController.roleHasUnreadMessages);
    }
    readonly property bool hasLoudNotification: {
        var r = _attRev;
        return _roomRow >= 0
            && !!Rooms.data(Rooms.index(_roomRow, 0), tabController.roleHasLoudNotification);
    }
    readonly property bool hasDraft: {
        var r = _attRev;
        return _roomRow >= 0
            && !!Rooms.data(Rooms.index(_roomRow, 0), tabController.roleHasDraft);
    }
    readonly property bool isLowPriority: {
        var r = _attRev;
        if (_roomRow < 0)
            return false;
        var tags = Rooms.data(Rooms.index(_roomRow, 0), tabController.roleTags);
        return !!tags && !!tags.indexOf && tags.indexOf("m.lowpriority") !== -1;
    }
    readonly property bool emphasizeUnread: hasUnread
        && (!isLowPriority || hasLoudNotification
            || Communities.currentFilterId === "tag:m.lowpriority")
    readonly property bool emphasizeDraft: hasDraft && !emphasizeUnread

    // Live room name from model (falls back to ListModel value).
    readonly property string displayName: {
        var r = _attRev;
        if (_roomRow < 0)
            return roomName;
        var name = Rooms.data(Rooms.index(_roomRow, 0), tabController.roleRoomName);
        return name || roomName;
    }

    // Live avatar URL from model.
    readonly property string avatarUrl: {
        var r = _attRev;
        if (_roomRow < 0)
            return "";
        return Rooms.data(Rooms.index(_roomRow, 0), tabController.roleAvatarUrl) || "";
    }

    // Text color adapts to highlight/hover state.
    readonly property color textColor: isActive ? palette.text
        : isHovered ? palette.brightText
        : palette.buttonText

    // Avatar size relative to font size (roughly 1.2x line height).
    readonly property int avatarSizePx: Math.round(Komai.fontPixelSize * 1.4)

    // Close button size (~2x the original 18px).
    readonly property int closeBtnSize: Math.round(Komai.fontPixelSize * 1.6)

    // The effective opaque background (composited for the close-button backdrop).
    // tabDelegate.color may be semi-transparent, so we need a fully opaque version
    // for the gradient/solid that hides text behind the close button.
    readonly property color opaqueBackgroundColor: {
        if (isActive) {
            var a = 0.12;
            return Qt.rgba(
                palette.highlight.r * a + palette.window.r * (1 - a),
                palette.highlight.g * a + palette.window.g * (1 - a),
                palette.highlight.b * a + palette.window.b * (1 - a),
                1.0);
        }
        if (isHovered)
            return palette.dark;
        return palette.window;
    }

    width: 180
    height: parent ? parent.height : 32
    color: isActive ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.12)
                    : (isHovered ? palette.dark : "transparent")

    // Main click area (under the visual content so close button can steal clicks).
    MouseArea {
        id: tabMouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        hoverEnabled: true

        onClicked: function(mouse) {
            if (mouse.button === Qt.MiddleButton) {
                tabController.closeTab(tabDelegate.roomId);
                return;
            }
            tabController.switchToTab(tabDelegate.index);
        }
    }

    KomaiToolTip {
        anchorItem: tabDelegate
        text: {
            var label = tabDelegate.displayName;
            if (tabDelegate.index < 9)
                label += "  [Alt+" + (tabDelegate.index + 1) + "]";
            return label;
        }
        delay: Komai.tooltipDelay
        requestedVisible: tabDelegate.isHovered
    }

    // Left attention bar (always reserves space; transparent when inactive).
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Komai.paddingSmall / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: parent.height - Komai.paddingMedium * 2
        radius: 1.5
        color: (tabDelegate.emphasizeUnread || tabDelegate.emphasizeDraft)
            ? (tabDelegate.hasLoudNotification ? Komai.theme.red
                : tabDelegate.emphasizeDraft ? Komai.theme.attention
                : palette.highlight)
            : "transparent"
    }

    // Tab content: avatar + room name (fills the full width; no space reserved for close button).
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium + Komai.paddingSmall
        anchors.rightMargin: Komai.paddingSmall
        spacing: Komai.paddingSmall

        Avatar {
            Layout.preferredWidth: tabDelegate.avatarSizePx
            Layout.preferredHeight: tabDelegate.avatarSizePx
            displayName: tabDelegate.displayName
            url: tabDelegate.avatarUrl.replace("mxc://", "image://MxcImage/")
            roomid: tabDelegate.roomId
            enabled: false
        }

        Text {
            Layout.fillWidth: true
            text: tabDelegate.displayName
            elide: Text.ElideRight
            font.bold: tabDelegate.emphasizeUnread
            font.pixelSize: Komai.fontPixelSize
            color: tabDelegate.textColor
            verticalAlignment: Text.AlignVCenter
        }
    }

    // Close button overlay (Chrome-style): floats on top of text at the right edge.
    // A gradient fades text out so the button doesn't cut characters abruptly.
    Item {
        id: closeOverlay

        anchors.right: parent.right
        anchors.rightMargin: Komai.paddingSmall
        anchors.verticalCenter: parent.verticalCenter
        width: tabDelegate.closeBtnSize + tabDelegate.closeBtnSize // button + fade zone
        height: parent.height
        visible: true

        // Gradient that fades from transparent to the opaque background.
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: tabDelegate.closeBtnSize
            height: parent.height
            gradient: Gradient {
                orientation: Gradient.Horizontal

                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: tabDelegate.opaqueBackgroundColor }
            }
        }

        // Solid opaque background behind the button so text is fully hidden.
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: tabDelegate.closeBtnSize
            height: parent.height
            color: tabDelegate.opaqueBackgroundColor
        }

        // The button itself.
        Rectangle {
            id: closeBtn

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: tabDelegate.closeBtnSize
            height: tabDelegate.closeBtnSize
            radius: tabDelegate.closeBtnSize / 2
            color: closeArea.containsMouse
                ? Qt.rgba(tabDelegate.textColor.r, tabDelegate.textColor.g, tabDelegate.textColor.b, 0.2)
                : "transparent"

            Text {
                anchors.centerIn: parent
                text: "\u00D7"
                font.pixelSize: Math.round(tabDelegate.closeBtnSize * 0.7)
                color: tabDelegate.textColor
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }

            MouseArea {
                id: closeArea

                anchors.fill: parent
                hoverEnabled: true

                onClicked: tabController.closeTab(tabDelegate.roomId)
            }
        }
    }

    // Active tab indicator (bottom accent line).
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        color: palette.highlight
        visible: tabDelegate.isActive
    }

    // Right-side tab separator (hidden for the last tab).
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: Komai.paddingMedium
        anchors.bottomMargin: Komai.paddingMedium
        width: 1
        color: tabDelegate.isLastTab ? "transparent" : Komai.theme.separator
    }
}
