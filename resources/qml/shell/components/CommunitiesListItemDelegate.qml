// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ItemDelegate {
    id: communityItem

    required property int avatarSize
    required property bool collapsed
    required property var communityContextMenu
    required property var model
    required property real scrollbarReservedWidth
    readonly property real baseFontPixelSize: Komai.fontPixelSize
    readonly property real depthAvatarSize: Math.max(avatarSize * 0.5, Math.round(avatarSize * Math.pow(0.85, model.depth)))
    readonly property real lineSpacing: Math.max(1, Math.round(baseFontPixelSize * 1.2))

    property color backgroundColor: palette.window
    property color bubbleBackground: palette.highlight
    property color bubbleText: palette.highlightedText
    property color importantText: palette.text
    property color unimportantText: palette.buttonText

    ToolTip.delay: Komai.tooltipDelay
    ToolTip.text: model.tooltip
    ToolTip.visible: hovered && collapsed
    height: Komai.navigationRowHeight
    state: "normal"
    width: ListView.view.width - scrollbarReservedWidth

    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    background: Rectangle {
        color: communityItem.backgroundColor
    }
    states: [
        State {
            name: "highlight"
            when: (communityItem.hovered || (model.hidden ?? false)) && !(Communities.currentFilterId === model.id)

            PropertyChanges {
                communityItem {
                    backgroundColor: palette.dark
                    bubbleBackground: palette.highlight
                    bubbleText: palette.highlightedText
                    importantText: palette.brightText
                    unimportantText: palette.brightText
                }
            }
        },
        State {
            name: "selected"
            when: Communities.currentFilterId == model.id

            PropertyChanges {
                communityItem {
                    backgroundColor: palette.highlight
                    bubbleBackground: palette.highlightedText
                    bubbleText: palette.highlight
                    importantText: palette.highlightedText
                    unimportantText: palette.highlightedText
                }
            }
        }
    ]

    onClicked: Communities.setCurrentFilterId(model.id)
    onPressAndHold: communityContextMenu?.show(communityItem, model.id, model.hidden, model.badgesHidden, model.displayName)

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: communityItem.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    Item {
        anchors.fill: parent

        TapHandler {
            id: rth
            acceptedButtons: Qt.RightButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: communityContextMenu?.show(communityItem, model.id, model.hidden, model.badgesHidden, model.displayName)
        }
    }
    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium + (collapsed ? 0 : (lineSpacing * model.depth))
        anchors.rightMargin: Komai.paddingMedium + Komai.paddingSmall
        anchors.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall / 2 : Komai.paddingMedium
        anchors.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall / 2 : Komai.paddingMedium
        spacing: Komai.paddingMedium

        ImageButton {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: lineSpacing
            Layout.preferredWidth: lineSpacing
            ToolTip.delay: Komai.tooltipDelay
            ToolTip.text: model.collapsed ? qsTr("Expand") : qsTr("Collapse")
            ToolTip.visible: hovered
            hoverEnabled: true
            image: model.collapsed ? ":/icons/icons/ui/collapsed.svg" : ":/icons/icons/ui/expanded.svg"
            visible: !collapsed && model.collapsible

            onClicked: model.collapsed = !model.collapsed
        }
        Item {
            Layout.preferredWidth: lineSpacing
            visible: !collapsed && !model.collapsible && Communities.containsSubspaces
        }
        Avatar {
            id: avatar

            Layout.alignment: Qt.AlignVCenter
            color: communityItem.backgroundColor
            displayName: model.displayName
            enabled: false
            Layout.preferredHeight: depthAvatarSize
            roomid: model.id
            textColor: model.avatarUrl?.startsWith(":/") == true ? communityItem.unimportantText : communityItem.importantText
            url: {
                if (model.avatarUrl?.startsWith("mxc://") == true)
                    return model.avatarUrl.replace("mxc://", "image://MxcImage/");
                else if ((model.avatarUrl?.length ?? 0) > 0)
                    return model.avatarUrl;
                else
                    return "";
            }
            Layout.preferredWidth: depthAvatarSize

            NotificationBubble {
                anchors.bottom: avatar.bottom
                anchors.margins: -Komai.paddingSmall
                anchors.right: avatar.right
                bubbleBackgroundColor: communityItem.bubbleBackground
                bubbleTextColor: communityItem.bubbleText
                font.pixelSize: baseFontPixelSize * 0.6
                hasLoudNotification: model.hasLoudNotification
                mayBeVisible: collapsed && !model.badgesHidden && Settings.sidebarsRoomListShowCommunityCounts
                notificationCount: model.unreadMessages
            }
        }
        ElidedLabel {
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            color: communityItem.importantText
            elideWidth: width
            fullText: model.displayName
            textFormat: Text.PlainText
            visible: !collapsed
        }
        Item {
            Layout.fillWidth: true
        }
        NotificationBubble {
            Layout.alignment: Qt.AlignRight
            Layout.leftMargin: Komai.paddingSmall
            bubbleBackgroundColor: communityItem.bubbleBackground
            bubbleTextColor: communityItem.bubbleText
            hasLoudNotification: model.hasLoudNotification
            mayBeVisible: !collapsed && !model.badgesHidden && Settings.sidebarsRoomListShowCommunityCounts
            notificationCount: model.unreadMessages
        }
    }
}
