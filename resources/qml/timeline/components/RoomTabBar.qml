// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: tabBar

    required property var tabController

    readonly property int tabWidth: 180

    implicitHeight: Math.max(28, Math.round(fontMetrics.height * 2.2))
    visible: tabController.tabs.count > 0
    color: palette.window

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

    ListView {
        id: tabListView

        anchors.fill: parent
        orientation: Qt.Horizontal
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width && !tabController.isDragging
        model: tabController.tabs

        delegate: RoomTabDelegate {
            tabController: tabBar.tabController
            parentListView: tabListView
        }

        // Animate non-dragged tabs sliding into place.
        displaced: Transition {
            NumberAnimation { properties: "x"; duration: 150; easing.type: Easing.OutQuad }
        }
    }

    // Convert vertical mouse wheel to horizontal scroll (only when not dragging).
    MouseArea {
        anchors.fill: tabListView
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true
        enabled: !tabController.isDragging

        onWheel: function(wheel) {
            var delta = wheel.angleDelta.y || wheel.angleDelta.x;
            if (delta === 0)
                return;
            var maxX = Math.max(0, tabListView.contentWidth - tabListView.width);
            tabListView.contentX = Math.max(0, Math.min(maxX, tabListView.contentX - delta));
            wheel.accepted = true;
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
}
