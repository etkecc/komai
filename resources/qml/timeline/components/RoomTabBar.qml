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

    readonly property int preferredTabWidth: Settings.navigationTabsPreferredWidthPx
    readonly property int minimumTabWidth: Settings.navigationTabsMinimumWidthPx

    // Compute ideal tab width from available space and tab count.
    readonly property int _liveTabWidth: {
        var count = tabController.tabs.count;
        if (count === 0)
            return preferredTabWidth;
        var available = tabListView.width;
        if (count * preferredTabWidth <= available)
            return preferredTabWidth;
        var shrunk = Math.floor(available / count);
        return Math.max(minimumTabWidth, shrunk);
    }

    // Stable-close: freeze tab width while the mouse is inside the tab bar
    // so that closing a tab keeps X buttons aligned.  Recalculate only when
    // the mouse leaves.
    property int _stableTabWidth: _liveTabWidth
    readonly property bool _mouseInTabBar: tabBarHover.hovered

    on_LiveTabWidthChanged: {
        if (!_mouseInTabBar) {
            _stableTabWidth = _liveTabWidth;
            tabListView.forceLayout();
        }
    }
    on_MouseInTabBarChanged: {
        if (!_mouseInTabBar) {
            _stableTabWidth = _liveTabWidth;
            tabListView.forceLayout();
        }
    }

    readonly property int effectiveTabWidth: _stableTabWidth

    implicitHeight: Komai.navigationRowHeight
    visible: tabController.tabs.count > 0
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
            interactive: contentWidth > width && !tabController.isDragging
            model: tabController.tabs

            delegate: RoomTabDelegate {
                tabController: tabBar.tabController
                parentListView: tabListView
                tabWidth: tabBar.effectiveTabWidth
            }

            // Animate non-dragged tabs sliding into place.
            displaced: Transition {
                NumberAnimation { properties: "x"; duration: 150; easing.type: Easing.OutQuad }
            }
        }

        // Separator before the New button.
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: Komai.paddingMedium
            Layout.bottomMargin: Komai.paddingMedium
            color: Komai.theme.separator
        }

        // "New" tab button — reuses RoomListActionButton for consistent styling.
        ShellComponents.RoomListActionButton {
            id: newTabBtn

            buttonSize: Komai.barIconSize
            iconSource: ":/icons/icons/ui/tab-add.svg"
            toolTipText: qsTr("Open a new tab [Ctrl+T]")
            labelText: qsTr("New")
            showLabel: true
            Layout.alignment: Qt.AlignVCenter

            onClicked: tabController.openNewTab()
        }
    }

    // Left edge fade (visible when tabs are scrolled past the start).
    Rectangle {
        visible: tabListView.contentX > 1
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

    // Right edge fade (visible when tabs extend past the visible area).
    Rectangle {
        visible: tabListView.contentX + tabListView.width < tabListView.contentWidth - 1
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
        var tw = effectiveTabWidth;
        var tabLeft = idx * tw;
        var tabRight = tabLeft + tw;
        var viewLeft = tabListView.contentX;
        var viewRight = viewLeft + tabListView.width;
        if (tabLeft >= viewLeft && tabRight <= viewRight)
            return; // already fully visible
        // Use the larger of actual and expected content width so that a
        // just-appended tab (not yet laid out) can still be scrolled to.
        var expectedContentWidth = tabController.tabs.count * tw;
        var maxScroll = Math.max(tabListView.contentWidth, expectedContentWidth)
                        - tabListView.width;
        if (maxScroll <= 0)
            return;
        var target;
        if (tabLeft < viewLeft)
            target = tabLeft;
        else
            target = tabRight - tabListView.width;
        target = Math.max(0, Math.min(target, maxScroll));
        scrollAnimation.to = target;
        scrollAnimation.restart();
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

    // Convert vertical mouse wheel to horizontal scroll (only when not dragging).
    // Uses WheelHandler instead of MouseArea to avoid overriding delegate cursor shapes.
    WheelHandler {
        target: null
        parent: tabListView
        enabled: !tabController.isDragging

        onWheel: function(event) {
            var delta = event.angleDelta.y || event.angleDelta.x;
            if (delta === 0)
                return;
            var maxX = Math.max(0, tabListView.contentWidth - tabListView.width);
            tabListView.contentX = Math.max(0, Math.min(maxX, tabListView.contentX - delta));
        }
    }

    // Bottom border.
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Komai.theme.separator
    }

    // Right-click on empty tab bar space shows settings shortcut.
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: tabBarSettingsMenu.popup()
    }

    Menu {
        id: tabBarSettingsMenu

        Component.onCompleted: {
            if (tabBarSettingsMenu.popupType != undefined)
                tabBarSettingsMenu.popupType = 2;
        }

        MenuItem {
            text: qsTr("Settings...") // Keep short: Qt may clip/elide longer menu item text
            icon.source: "qrc:/icons/icons/ui/settings.svg"

            onTriggered: MainWindow.showUserSettingsPage(
                UserSettingsModel.TabNavigation,
                "navigation-tab-bar-section")
        }
    }
}
