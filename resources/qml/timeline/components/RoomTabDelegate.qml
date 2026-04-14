// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
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
    required property bool pinned
    required property var tabController
    required property var parentListView
    required property int tabWidth

    readonly property bool isEmptyTab: !roomId
    readonly property bool isActive: isEmptyTab ? !Rooms.currentRoomId : roomId === Rooms.currentRoomId

    // Shake animation (triggered when controller bumps shakeEmptyTabRevision).
    Connections {
        target: tabController
        enabled: tabDelegate.isEmptyTab

        function onShakeEmptyTabRevisionChanged() {
            shakeAnimation.stop();
            tabDelegate.x = tabDelegate.x; // reset to current layout position
            shakeAnimation.start();
        }
    }

    SequentialAnimation {
        id: shakeAnimation

        property real baseX: tabDelegate.x

        NumberAnimation { target: tabDelegate; property: "x"; to: shakeAnimation.baseX + 4; duration: 40 }
        NumberAnimation { target: tabDelegate; property: "x"; to: shakeAnimation.baseX - 4; duration: 40 }
        NumberAnimation { target: tabDelegate; property: "x"; to: shakeAnimation.baseX + 2; duration: 40 }
        NumberAnimation { target: tabDelegate; property: "x"; to: shakeAnimation.baseX; duration: 40 }

        onStarted: baseX = tabDelegate.x
    }
    readonly property bool isHovered: tabHoverHandler.hovered || closeArea.containsMouse || pinArea.containsMouse
    readonly property bool isLastTab: index === tabController.tabs.count - 1

    // Attention state (re-evaluated when attentionRevision changes).
    // Uses unfilteredRoomData so tabs stay correct even when the room is
    // hidden by the current community/space filter.
    readonly property int _attRev: tabController.attentionRevision
    readonly property bool hasUnread: {
        var r = _attRev;
        return !!Rooms.unfilteredRoomData(roomId, tabController.roleHasUnreadMessages);
    }
    readonly property bool hasLoudNotification: {
        var r = _attRev;
        return !!Rooms.unfilteredRoomData(roomId, tabController.roleHasLoudNotification);
    }
    readonly property bool hasDraft: {
        var r = _attRev;
        return !!Rooms.unfilteredRoomData(roomId, tabController.roleHasDraft);
    }
    readonly property bool isLowPriority: {
        var r = _attRev;
        var tags = Rooms.unfilteredRoomData(roomId, tabController.roleTags);
        return !!tags && !!tags.indexOf && tags.indexOf("m.lowpriority") !== -1;
    }
    readonly property bool emphasizeUnread: hasUnread
        && (!isLowPriority || hasLoudNotification
            || Communities.currentFilterId === "tag:m.lowpriority")
    readonly property bool emphasizeDraft: hasDraft && !emphasizeUnread

    // Live room name from model (falls back to ListModel value).
    readonly property string displayName: {
        if (isEmptyTab)
            return qsTr("New Tab");
        var r = _attRev;
        var name = Rooms.unfilteredRoomData(roomId, tabController.roleRoomName);
        return name || roomName;
    }

    // Live avatar URL from model.
    readonly property string avatarUrl: {
        var r = _attRev;
        return Rooms.unfilteredRoomData(roomId, tabController.roleAvatarUrl) || "";
    }

    // Draft activity base color (blended highlight + attention), matching the room list.
    readonly property color draftActivityBase: Qt.rgba(
        (Komai.theme.attention.r + palette.highlight.r) / 2,
        (Komai.theme.attention.g + palette.highlight.g) / 2,
        (Komai.theme.attention.b + palette.highlight.b) / 2, 1)

    // Text color adapts to active/hover/activity state.
    readonly property color textColor: isActive ? palette.brightText
        : isHovered ? palette.text
        : palette.buttonText

    // Avatar size relative to font size (roughly 1.2x line height).
    readonly property int avatarSizePx: Math.round(Komai.fontPixelSize * 1.4)

    // Close/pin button size.
    readonly property int actionBtnSize: Math.round(Komai.fontPixelSize * 1.6)

    // Pin icon image size (inside the button area).
    readonly property int pinIconSize: Math.round(actionBtnSize * 0.6)

    // The effective opaque background (composited for the close-button backdrop).
    // Must match the actual visual background so the gradient hides text cleanly.
    readonly property color opaqueBackgroundColor: {
        if (isActive) {
            var d = palette.dark;
            return Qt.rgba(
                d.r * 0.85 + palette.window.r * 0.15,
                d.g * 0.85 + palette.window.g * 0.15,
                d.b * 0.85 + palette.window.b * 0.15, 1);
        }
        if (isHovered) {
            var d2 = palette.dark;
            return Qt.rgba(
                d2.r * 0.30 + palette.window.r * 0.70,
                d2.g * 0.30 + palette.window.g * 0.70,
                d2.b * 0.30 + palette.window.b * 0.70, 1);
        }
        if (emphasizeDraft) {
            var a = Komai.theme.attention;
            return Qt.rgba(
                a.r * 0.12 + palette.window.r * 0.88,
                a.g * 0.12 + palette.window.g * 0.88,
                a.b * 0.12 + palette.window.b * 0.88, 1);
        }
        if (emphasizeUnread) {
            var h = palette.highlight;
            return Qt.rgba(
                h.r * 0.15 + palette.window.r * 0.85,
                h.g * 0.15 + palette.window.g * 0.85,
                h.b * 0.15 + palette.window.b * 0.85, 1);
        }
        if (pinned)
            return palette.alternateBase;
        return palette.window;
    }

    // Count of closeable (non-pinned) tabs other than this one.
    readonly property int closeableOtherCount: {
        var r = _attRev; // rebind on model changes
        var count = 0;
        for (var i = 0; i < tabController.tabs.count; i++) {
            if (i !== index && !tabController.tabs.get(i).pinned)
                count++;
        }
        return count;
    }

    // Count of closeable (non-pinned) tabs to the right.
    readonly property int closeableRightCount: {
        var r = _attRev;
        var count = 0;
        for (var i = index + 1; i < tabController.tabs.count; i++) {
            if (!tabController.tabs.get(i).pinned)
                count++;
        }
        return count;
    }

    // Whether this tab is in avatar-only display mode.
    readonly property bool isAvatarOnly: !isEmptyTab
        && (pinned
            ? Settings.navigationTabsPinnedTabLabel === Settings.TabLabelDisplay.AvatarOnly
            : Settings.navigationTabsTabLabel === Settings.TabLabelDisplay.AvatarOnly)

    // Intrinsic width for avatar-only tabs.
    // Close button is part of the RowLayout flow in this mode, so the
    // RowLayout sizing handles it automatically via implicitWidth.
    readonly property int _attentionBarMargin: Math.round(Komai.paddingSmall / 2) + 4 + Komai.paddingSmall

    width: {
        if (!isAvatarOnly)
            return tabWidth;
        var w = _attentionBarMargin; // left margin (attention bar zone)
        if (pinBtn.showPin)
            w += actionBtnSize + Komai.paddingSmall; // pin button
        w += avatarSizePx;
        if (!pinned)
            w += Komai.paddingSmall + actionBtnSize; // inline close button
        w += _attentionBarMargin; // right margin (mirrors attention bar zone)
        if (pinned)
            w += Komai.paddingSmall; // extra space for pin indicator overhang
        return w;
    }
    height: parent ? parent.height : 32
    color: {
        if (isActive)
            return Qt.rgba(palette.dark.r, palette.dark.g, palette.dark.b, 0.85);
        if (isHovered)
            return Qt.rgba(palette.dark.r, palette.dark.g, palette.dark.b, 0.30);
        if (emphasizeDraft)
            return Qt.rgba(Komai.theme.attention.r, Komai.theme.attention.g, Komai.theme.attention.b, 0.12);
        if (emphasizeUnread)
            return Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.15);
        if (pinned)
            return palette.alternateBase;
        return "transparent";
    }

    // Drag state tracked per-delegate.
    property real _dragStartX: 0
    property bool _dragPending: false
    property bool _dragJustFinished: false
    readonly property bool _isDraggedTab: tabController.isDragging && tabController._dragRoomId === roomId

    // Main click area (under the visual content so close/pin buttons can steal clicks).
    MouseArea {
        id: tabMouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        hoverEnabled: true
        preventStealing: tabDelegate._dragPending || tabController.isDragging

        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && !tabDelegate.isEmptyTab) {
                tabDelegate._dragStartX = mouse.x;
                tabDelegate._dragPending = true;

            }
        }

        onPositionChanged: function(mouse) {
            if (tabDelegate._dragPending) {
                if (Math.abs(mouse.x - tabDelegate._dragStartX) > 5) {

                    tabDelegate._dragPending = false;
                    tabController.beginDrag(tabDelegate.roomId);
                }
                return;
            }
            if (tabController.isDragging) {
                var listPos = tabDelegate.mapToItem(tabDelegate.parentListView, mouse.x, 0);
                var contentX = listPos.x + tabDelegate.parentListView.contentX;
                var targetIndex = Math.floor(contentX / tabDelegate.tabWidth);
                targetIndex = Math.max(0, Math.min(targetIndex, tabController.tabs.count - 1));

                tabController.updateDragPosition(targetIndex);
            }
        }

        onReleased: function(mouse) {

            tabDelegate._dragPending = false;
            if (tabController.isDragging) {
                tabController.commitDrag();
                tabDelegate._dragJustFinished = true;
                return;
            }
        }

        onClicked: function(mouse) {
            if (tabDelegate._dragJustFinished) {
                tabDelegate._dragJustFinished = false;
                return;
            }
            if (mouse.button === Qt.RightButton) {
                _openContextMenu();
                return;
            }
            if (mouse.button === Qt.MiddleButton) {
                if (!tabDelegate.pinned)
                    tabController.closeTab(tabDelegate.roomId);
                return;
            }
            tabController.switchToTab(tabDelegate.index);
        }
    }

    Menu {
        id: tabContextMenu

        Component.onCompleted: {
            if (tabContextMenu.popupType != undefined)
                tabContextMenu.popupType = 2;
        }
    }

    Component {
        id: menuItemPinToggle

        MenuItem {
            text: tabDelegate.pinned ? qsTr("Unpin Tab") : qsTr("Pin Tab")
            icon.source: tabDelegate.pinned
                ? "qrc:/icons/icons/ui/pin-off.svg"
                : "qrc:/icons/icons/ui/pin.svg"

            onTriggered: {
                if (tabDelegate.pinned)
                    tabController.unpinTab(tabDelegate.roomId);
                else
                    tabController.pinTab(tabDelegate.roomId);
            }
        }
    }

    Component {
        id: menuItemCloseTab

        MenuItem {
            text: tabDelegate.isActive ? qsTr("Close Tab [Ctrl+W]") : qsTr("Close Tab")

            onTriggered: tabController.closeTab(tabDelegate.roomId)
        }
    }

    Component {
        id: menuItemCloseOther

        MenuItem {
            text: qsTr("Close Other Tabs")

            onTriggered: tabController.closeOtherTabs(tabDelegate.roomId)
        }
    }

    Component {
        id: menuItemCloseRight

        MenuItem {
            text: qsTr("Close Tabs to the Right")

            onTriggered: tabController.closeTabsToTheRight(tabDelegate.roomId)
        }
    }

    Component {
        id: menuItemCloseUnpinned

        MenuItem {
            text: qsTr("Close Unpinned Tabs")

            onTriggered: tabController.closeUnpinnedTabs()
        }
    }

    Component {
        id: menuSeparatorComponent

        MenuSeparator {}
    }

    function _openContextMenu() {
        // Clear previous items.
        while (tabContextMenu.count > 0)
            tabContextMenu.takeItem(0).destroy();

        if (!isEmptyTab) {
            tabContextMenu.addItem(menuItemPinToggle.createObject(tabContextMenu));
            tabContextMenu.addItem(menuSeparatorComponent.createObject(tabContextMenu));
        }
        if (closeableOtherCount > 0)
            tabContextMenu.addItem(menuItemCloseOther.createObject(tabContextMenu));
        if (closeableRightCount > 0)
            tabContextMenu.addItem(menuItemCloseRight.createObject(tabContextMenu));
        if (closeableOtherCount > 0 || !tabDelegate.pinned)
            tabContextMenu.addItem(menuItemCloseUnpinned.createObject(tabContextMenu));
        tabContextMenu.addItem(menuItemCloseTab.createObject(tabContextMenu));

        tabContextMenu.popup();
    }

    KomaiToolTip {
        anchorItem: tabDelegate
        text: {
            if (closeArea.containsMouse) {
                var closeLabel = qsTr("Close %1").arg(tabDelegate.displayName);
                if (tabDelegate.isActive)
                    closeLabel += "  [Ctrl+W]";
                return closeLabel;
            }
            if (pinArea.containsMouse) {
                return tabDelegate.pinned ? qsTr("Unpin Tab") : qsTr("Pin Tab");
            }
            var prefix = "";
            if (tabDelegate.hasUnread && tabDelegate.hasDraft)
                prefix = qsTr("(Unread & Draft) ");
            else if (tabDelegate.hasUnread)
                prefix = qsTr("(Unread) ");
            else if (tabDelegate.hasDraft)
                prefix = qsTr("(Draft) ");

            var label = prefix + tabDelegate.displayName;

            if (tabDelegate.pinned)
                label += " " + qsTr("(Pinned)");
            if (tabDelegate.index < 9)
                label += "  [Alt+" + (tabDelegate.index + 1) + "]";
            return label;
        }
        delay: Komai.tooltipDelay
        requestedVisible: tabDelegate.isHovered && !tabContextMenu.visible
    }

    // Left attention bar (always reserves space; transparent when inactive).
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Komai.paddingSmall / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 4
        height: parent.height - Komai.paddingMedium * 2
        radius: 2
        color: (tabDelegate.emphasizeUnread || tabDelegate.emphasizeDraft)
            ? (tabDelegate.hasLoudNotification ? Komai.theme.red
                : tabDelegate.emphasizeDraft ? Komai.theme.attention
                : palette.highlight)
            : "transparent"
    }


    // Tab content: pin icon + avatar + room name.
    // Left margin accounts for the attention bar (3px wide at paddingSmall/2 offset)
    // so content never overlaps the indicator regardless of pin button visibility.
    RowLayout {
        id: tabContentRow
        anchors.fill: parent
        anchors.leftMargin: Math.round(Komai.paddingSmall / 2) + 4 + Komai.paddingSmall
        anchors.rightMargin: Komai.paddingSmall
        spacing: Komai.paddingSmall

        // Pin toggle button (leftmost, before avatar).
        // Hidden for empty tabs and when the setting is "never".
        Rectangle {
            id: pinBtn

            readonly property bool showPin: !tabDelegate.isEmptyTab
                && Settings.navigationTabsShowPinButton === Settings.TabPinButtonVisibility.Always

            Layout.preferredWidth: showPin ? tabDelegate.actionBtnSize : 0
            Layout.preferredHeight: showPin ? tabDelegate.actionBtnSize : 0
            visible: showPin
            radius: tabDelegate.actionBtnSize / 2
            color: pinArea.containsMouse
                ? Qt.rgba(tabDelegate.textColor.r, tabDelegate.textColor.g, tabDelegate.textColor.b, 0.2)
                : "transparent"

            Image {
                anchors.centerIn: parent
                width: tabDelegate.pinIconSize
                height: tabDelegate.pinIconSize
                source: tabDelegate.pinned
                    ? "image://colorimage/:/icons/icons/ui/pin-filled.svg?" + palette.highlight
                    : "image://colorimage/:/icons/icons/ui/pin.svg?" + tabDelegate.textColor
                sourceSize: Qt.size(tabDelegate.pinIconSize, tabDelegate.pinIconSize)
            }

            MouseArea {
                id: pinArea

                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    if (tabDelegate.pinned)
                        tabController.unpinTab(tabDelegate.roomId);
                    else
                        tabController.pinTab(tabDelegate.roomId);
                }
            }
        }

        Image {
            Layout.preferredWidth: tabDelegate.avatarSizePx
            Layout.preferredHeight: tabDelegate.avatarSizePx
            source: "qrc:/logos/komai.svg"
            sourceSize: Qt.size(tabDelegate.avatarSizePx, tabDelegate.avatarSizePx)
            visible: tabDelegate.isEmptyTab
        }

        Item {
            Layout.preferredWidth: tabDelegate.avatarSizePx
            Layout.preferredHeight: tabDelegate.avatarSizePx
            visible: !tabDelegate.isEmptyTab

            Avatar {
                anchors.fill: parent
                displayName: tabDelegate.displayName
                url: tabDelegate.avatarUrl.replace("mxc://", "image://MxcImage/")
                roomid: tabDelegate.roomId
                enabled: false
            }

            Image {
                readonly property int badgeSize: Math.round(tabDelegate.avatarSizePx * 0.65)
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: -Math.round(badgeSize * 0.65)
                anchors.rightMargin: -Math.round(badgeSize * 0.60)
                width: badgeSize
                height: badgeSize
                visible: tabDelegate.pinned
                source: "image://colorimage/:/icons/icons/ui/pin-filled.svg?" + palette.highlight
                sourceSize: Qt.size(badgeSize, badgeSize)
            }
        }

        Text {
            readonly property bool labelVisible: tabDelegate.isEmptyTab
                || (tabDelegate.pinned
                    ? Settings.navigationTabsPinnedTabLabel !== Settings.TabLabelDisplay.AvatarOnly
                    : Settings.navigationTabsTabLabel !== Settings.TabLabelDisplay.AvatarOnly)

            Layout.fillWidth: labelVisible
            Layout.preferredWidth: labelVisible ? -1 : 0
            visible: labelVisible
            text: tabDelegate.displayName
            elide: Text.ElideRight
            font.bold: tabDelegate.emphasizeUnread
            font.pixelSize: Komai.fontPixelSize
            color: tabDelegate.textColor
            verticalAlignment: Text.AlignVCenter
        }

        // Inline close button for avatar-only mode (in-flow, not overlaid).
        Rectangle {
            id: inlineCloseBtn

            Layout.preferredWidth: tabDelegate.actionBtnSize
            Layout.preferredHeight: tabDelegate.actionBtnSize
            visible: tabDelegate.isAvatarOnly && !tabDelegate.pinned
            radius: tabDelegate.actionBtnSize / 2
            color: inlineCloseArea.containsMouse
                ? Qt.rgba(tabDelegate.textColor.r, tabDelegate.textColor.g, tabDelegate.textColor.b, 0.2)
                : "transparent"

            Text {
                anchors.centerIn: parent
                text: "\u00D7"
                font.pixelSize: Math.round(tabDelegate.actionBtnSize * 0.7)
                color: tabDelegate.textColor
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }

            MouseArea {
                id: inlineCloseArea

                anchors.fill: parent
                hoverEnabled: true

                onClicked: tabController.closeTab(tabDelegate.roomId)
            }
        }
    }

    // Close button overlay (Chrome-style): floats on top of text at the right edge.
    // Hidden for pinned tabs. In avatar-only mode the fade zone is skipped since
    // there is no text to hide behind the gradient.
    Item {
        id: closeOverlay

        anchors.right: parent.right
        anchors.rightMargin: Komai.paddingSmall
        anchors.verticalCenter: parent.verticalCenter
        width: tabDelegate.isAvatarOnly
            ? tabDelegate.actionBtnSize
            : tabDelegate.actionBtnSize + tabDelegate.actionBtnSize
        height: parent.height
        visible: !tabDelegate.pinned && !tabDelegate.isAvatarOnly

        // Gradient that fades from transparent to the opaque background.
        // Hidden in avatar-only mode (no text to fade).
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: tabDelegate.actionBtnSize
            height: parent.height
            visible: !tabDelegate.isAvatarOnly
            gradient: Gradient {
                orientation: Gradient.Horizontal

                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: tabDelegate.opaqueBackgroundColor }
            }
        }

        // Solid opaque background behind the button so text is fully hidden.
        // Hidden in avatar-only mode.
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: tabDelegate.actionBtnSize
            height: parent.height
            visible: !tabDelegate.isAvatarOnly
            color: tabDelegate.opaqueBackgroundColor
        }

        // The button itself.
        Rectangle {
            id: closeBtn

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: tabDelegate.actionBtnSize
            height: tabDelegate.actionBtnSize
            radius: tabDelegate.actionBtnSize / 2
            color: closeArea.containsMouse
                ? Qt.rgba(tabDelegate.textColor.r, tabDelegate.textColor.g, tabDelegate.textColor.b, 0.2)
                : "transparent"

            Text {
                anchors.centerIn: parent
                text: "\u00D7"
                font.pixelSize: Math.round(tabDelegate.actionBtnSize * 0.7)
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
    // Height is 3px because the tab bar's 1px bottom border overlaps the
    // lowest pixel, resulting in a visually consistent 2px accent.
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 3
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
