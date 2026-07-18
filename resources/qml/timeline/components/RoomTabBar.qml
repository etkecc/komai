// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../../shell/components" as ShellComponents

Rectangle {
    id: tabBar

    required property var tabController

    readonly property bool mirrored: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
    readonly property int preferredTabWidth: Settings.navigationTabsPreferredWidthPx
    readonly property int minimumTabWidth: Settings.navigationTabsMinimumWidthPx

    // Shared metrics (mirroring RoomTabDelegate layout constants).
    readonly property int _avatarSizePx: Komai.iconSize
    readonly property int _actionBtnSize: Math.round(Komai.fontPixelSize * 1.6)
    readonly property int _attentionBarMargin: Math.round(Komai.paddingSmall / 2) + 4 + Komai.paddingSmall
    readonly property bool _showPinButton: Settings.navigationTabsShowPinButton === Settings.TabPinButtonVisibility.Always

    // Pre-computed avatar-only widths for pinned and unpinned tabs.
    readonly property int _avatarOnlyPinnedWidth: {
        var w = _attentionBarMargin + _avatarSizePx + _attentionBarMargin + Komai.paddingSmall;
        if (_showPinButton)
            w += _actionBtnSize + Komai.paddingSmall;
        return w;
    }
    readonly property int _avatarOnlyUnpinnedWidth: {
        var w = _attentionBarMargin + _avatarSizePx + Komai.paddingSmall + _actionBtnSize + _attentionBarMargin;
        if (_showPinButton)
            w += _actionBtnSize + Komai.paddingSmall;
        return w;
    }

    // Compute ideal label-tab width by subtracting fixed avatar-only tab
    // widths from available space, then distributing the rest among label tabs.
    readonly property int _liveTabWidth: {
        var count = tabController.tabs.count;
        if (count === 0)
            return preferredTabWidth;

        var available = tabListView.width;
        var avatarOnlyTotal = 0;
        var labelCount = 0;

        for (var i = 0; i < count; i++) {
            var tab = tabController.tabs.get(i);
            var isAvatarOnly = !!tab.roomId
                && (tab.pinned
                    ? Settings.navigationTabsPinnedTabLabel === Settings.TabLabelDisplay.AvatarOnly
                    : Settings.navigationTabsTabLabel === Settings.TabLabelDisplay.AvatarOnly);
            if (isAvatarOnly) {
                avatarOnlyTotal += tab.pinned ? _avatarOnlyPinnedWidth : _avatarOnlyUnpinnedWidth;
            } else {
                labelCount++;
            }
        }

        // All tabs are avatar-only — return preferred as a fallback (unused).
        if (labelCount === 0)
            return preferredTabWidth;

        var remainingSpace = available - avatarOnlyTotal;
        if (labelCount * preferredTabWidth <= remainingSpace)
            return preferredTabWidth;
        var shrunk = Math.floor(remainingSpace / labelCount);
        return Math.max(minimumTabWidth, shrunk);
    }

    // Stable-close: freeze tab width while the mouse is inside the tab bar
    // so that closing a tab keeps X buttons aligned.  Recalculate only when
    // the mouse leaves.
    property int _stableTabWidth: _liveTabWidth
    readonly property bool _mouseInTabBar: tabBarHover.hovered

    readonly property int _displacedAnimDuration: 150

    // Applying a new tab width while a displaced transition is running is
    // unsafe: ListView skips transitioning items during layout, so they
    // finish sliding to positions computed with the old width and end up
    // permanently overlapping their re-laid-out neighbors (clipped tabs).
    // A late forceLayout() cannot repair this (it is a no-op without dirty
    // geometry or pending model changes), so the width change itself must
    // wait until the transitions have settled.
    function _applyStableTabWidth() {
        if (_mouseInTabBar)
            return; // frozen while hovering (stable-close)
        if (displacedSettleTimer.running)
            return; // displaced transitions in flight; retried on timeout
        if (_stableTabWidth === _liveTabWidth)
            return;
        _stableTabWidth = _liveTabWidth;
        tabListView.forceLayout();
    }

    on_LiveTabWidthChanged: _applyStableTabWidth()
    on_MouseInTabBarChanged: _applyStableTabWidth()

    Timer {
        id: displacedSettleTimer

        interval: tabBar._displacedAnimDuration + 100
        onTriggered: tabBar._applyStableTabWidth()
    }

    // Tab removals and reorders start displaced transitions on the ListView;
    // hold off width changes until those have settled.
    Connections {
        target: tabBar.tabController.tabs

        function onRowsRemoved() { displacedSettleTimer.restart(); }
        function onRowsMoved() { displacedSettleTimer.restart(); }
    }

    readonly property int effectiveTabWidth: _stableTabWidth
    readonly property bool _showLeftEdgeFade: tabBar.mirrored
        ? tabListView.contentX < tabListView.maxContentX - 1
        : tabListView.contentX > tabListView.minContentX + 1
    readonly property bool _showRightEdgeFade: tabBar.mirrored
        ? tabListView.contentX > tabListView.minContentX + 1
        : tabListView.contentX < tabListView.maxContentX - 1

    implicitHeight: Komai.navigationRowHeight
    visible: tabController.tabs.count > (Settings.navigationTabsAutoHideSingle ? 1 : 0)
    color: palette.alternateBase

    HoverHandler {
        id: tabBarHover
    }

    FontMetrics {
        id: tabBarFontMetrics
    }

    // Refresh tab display state when room data changes or the model is repopulated.
    Connections {
        target: Rooms

        function onDataChanged() { tabController.attentionRevision++; }
        function onRowsInserted() { tabController.attentionRevision++; }
        function onRowsRemoved() { tabController.attentionRevision++; }
        function onModelReset() { tabController.attentionRevision++; }
        function onLayoutChanged() { tabController.attentionRevision++; }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ListView {
            id: tabListView

            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: false
            model: tabController.tabs

            readonly property bool rightToLeft: effectiveLayoutDirection === Qt.RightToLeft
            readonly property real minContentX: originX
            readonly property real maxContentX: Math.max(minContentX, originX + contentWidth - width)

            function clampedContentX(x) {
                return Math.max(minContentX, Math.min(maxContentX, x));
            }

            function scrollBy(delta) {
                contentX = clampedContentX(contentX + delta);
            }

            delegate: RoomTabDelegate {
                tabController: tabBar.tabController
                parentListView: tabListView
                tabWidth: tabBar.effectiveTabWidth
            }

            // Animate non-dragged tabs sliding into place.
            displaced: Transition {
                NumberAnimation { properties: "x"; duration: tabBar._displacedAnimDuration; easing.type: Easing.OutQuad }
            }
        }

        // Separator before the New button.
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Komai.theme.separator
        }

        // "New" tab button — reuses RoomListActionButton for consistent styling.
        ShellComponents.RoomListActionButton {
            id: newTabBtn

            buttonSize: Komai.iconSize
            iconSource: ":/icons/icons/ui/tab-add.svg"
            toolTipText: qsTr("Open a new tab [Ctrl+T]")
            labelText: qsTr("New")
            showLabel: true
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: Komai.paddingMedium
            Layout.rightMargin: Komai.paddingMedium

            onClicked: tabController.openNewTab()
        }
    }

    // Left edge fade. In RTL the logical start/end are reversed, so the
    // scroll-threshold condition swaps while the physical gradient stays put.
    Rectangle {
        visible: tabBar._showLeftEdgeFade
        x: tabListView.x
        y: tabListView.y
        width: 40
        height: tabListView.height
        z: 2

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: palette.alternateBase }
            GradientStop { position: 0.6; color: Qt.rgba(palette.alternateBase.r, palette.alternateBase.g, palette.alternateBase.b, 0.5) }
            GradientStop { position: 1.0; color: Qt.rgba(palette.alternateBase.r, palette.alternateBase.g, palette.alternateBase.b, 0) }
        }
    }

    // Right edge fade. See the left-edge fade note above.
    Rectangle {
        visible: tabBar._showRightEdgeFade
        x: tabListView.x + tabListView.width - 40
        y: tabListView.y
        width: 40
        height: tabListView.height
        z: 2

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(palette.alternateBase.r, palette.alternateBase.g, palette.alternateBase.b, 0) }
            GradientStop { position: 0.4; color: Qt.rgba(palette.alternateBase.r, palette.alternateBase.g, palette.alternateBase.b, 0.5) }
            GradientStop { position: 1.0; color: palette.alternateBase }
        }
    }

    // Scroll the active tab into view when the current room changes.
    // Uses the tab count to compute expected content width so it works
    // even when called before the ListView has laid out a newly added tab.
    function ensureActiveTabVisible() {
        var idx = tabController.findTab(Rooms.currentRoomId);
        if (idx === -1)
            return;

        var precedingWidth = 0;
        var estimatedContentWidth = 0;
        var activeWidth = 0;
        for (var i = 0; i < tabController.tabs.count; i++) {
            var width = _tabWidthAtIndex(i);
            if (i < idx)
                precedingWidth += width;
            if (i === idx)
                activeWidth = width;
            estimatedContentWidth += width;
        }

        var contentWidth = Math.max(tabListView.contentWidth, estimatedContentWidth);
        var estimatedOriginX = tabListView.originX;
        if (tabListView.rightToLeft)
            estimatedOriginX -= Math.max(0, estimatedContentWidth - tabListView.contentWidth);

        var tabLeft;
        var tabRight;
        if (tabListView.rightToLeft) {
            tabRight = estimatedOriginX + contentWidth - precedingWidth;
            tabLeft = tabRight - activeWidth;
        } else {
            tabLeft = estimatedOriginX + precedingWidth;
            tabRight = tabLeft + activeWidth;
        }

        var viewLeft = tabListView.contentX;
        var viewRight = viewLeft + tabListView.width;
        if (tabLeft >= viewLeft && tabRight <= viewRight)
            return; // already fully visible

        var minScroll = estimatedOriginX;
        var maxScroll = Math.max(minScroll, estimatedOriginX + contentWidth - tabListView.width);
        if (maxScroll <= minScroll)
            return;
        var target;
        if (tabLeft < viewLeft)
            target = tabLeft;
        else
            target = tabRight - tabListView.width;
        target = Math.max(minScroll, Math.min(target, maxScroll));
        scrollAnimation.to = target;
        scrollAnimation.restart();
    }

    function _tabWidthAtIndex(index) {
        var item = tabListView.itemAtIndex(index);
        if (item)
            return item.width;

        var tab = tabController.tabs.get(index);
        var isAvatarOnly = !!tab.roomId
            && (tab.pinned
                ? Settings.navigationTabsPinnedTabLabel === Settings.TabLabelDisplay.AvatarOnly
                : Settings.navigationTabsTabLabel === Settings.TabLabelDisplay.AvatarOnly);
        if (isAvatarOnly)
            return tab.pinned ? _avatarOnlyPinnedWidth : _avatarOnlyUnpinnedWidth;
        return effectiveTabWidth;
    }

    NumberAnimation {
        id: scrollAnimation
        target: tabListView
        property: "contentX"
        duration: 150
        easing.type: Easing.OutQuad
    }

    Connections {
        target: Rooms
        function onCurrentRoomIdChanged() { tabBar.ensureActiveTabVisible(); }
    }

    // Wheel scroll is handled by RoomTabDelegate.onWheel (MouseArea.onWheel)
    // rather than a WheelHandler here, because WheelHandler events don't
    // reach the tab bar reliably through the ListView delegate hierarchy.

    // Bottom border.
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Komai.theme.separator
    }

    // Double-click on empty tab bar space opens a new tab.
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.DragThreshold
        onDoubleTapped: tabController.openNewTab()
    }

    // Right-click on empty tab bar space shows settings shortcut.
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: function(eventPoint) {
            tabBar._openSettingsMenu(eventPoint.position.x, eventPoint.position.y);
        }
    }

    Menu {
        id: tabBarSettingsMenu

        Component.onCompleted: {
            if (tabBarSettingsMenu.popupType != undefined)
                tabBarSettingsMenu.popupType = tabBar.mirrored ? 0 : 2;
        }

        MenuItem {
            LayoutMirroring.enabled: tabBar.mirrored
            LayoutMirroring.childrenInherit: true

            text: qsTr("Settings...") // Keep short: Qt may clip/elide longer menu item text
            icon.source: "qrc:/icons/icons/ui/settings.svg"

            onTriggered: MainWindow.showUserSettingsPage(
                UserSettingsModel.TabNavigation,
                "navigation-tab-bar-section")
        }
    }

    function _openSettingsMenu(localX, localY) {
        var popupX = tabBar.mirrored
            ? localX - tabBarSettingsMenu.implicitWidth
            : localX;
        tabBarSettingsMenu.popup(tabBar, popupX, localY);
    }
}
