// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../"
import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

ItemDelegate {
    id: communityItem

    required property int avatarSize
    required property bool collapsed
    required property var communityContextMenu
    required property var fontMetrics
    required property var model
    required property var scrollbar

    property color backgroundColor: palette.window
    property color bubbleBackground: palette.highlight
    property color bubbleText: palette.highlightedText
    property color importantText: palette.text
    property color unimportantText: palette.buttonText

    ToolTip.delay: Nheko.tooltipDelay
    ToolTip.text: model.tooltip
    ToolTip.visible: hovered && collapsed
    height: Nheko.navigationRowHeight
    state: "normal"
    width: ListView.view.width - ((scrollbar.interactive && scrollbar.visible && scrollbar.parent) ? scrollbar.width : 0)

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
            when: (communityItem.hovered || model.hidden) && !(Communities.currentTagId === model.id)

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
            when: Communities.currentTagId == model.id

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

    onClicked: Communities.setCurrentTagId(model.id)
    onPressAndHold: communityContextMenu.show(communityItem, model.id, model.hidden, model.muted)

    Item {
        anchors.fill: parent

        TapHandler {
            id: rth
            acceptedButtons: Qt.RightButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: communityContextMenu.show(rth, model.id, model.hidden, model.muted)
        }
    }
    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: Nheko.paddingMedium + (collapsed ? 0 : (fontMetrics.lineSpacing * model.depth))
        anchors.margins: Nheko.paddingMedium
        spacing: Nheko.paddingMedium

        ImageButton {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: fontMetrics.lineSpacing
            Layout.preferredWidth: fontMetrics.lineSpacing
            ToolTip.delay: Nheko.tooltipDelay
            ToolTip.text: model.collapsed ? qsTr("Expand") : qsTr("Collapse")
            ToolTip.visible: hovered
            hoverEnabled: true
            image: model.collapsed ? ":/icons/icons/ui/collapsed.svg" : ":/icons/icons/ui/expanded.svg"
            visible: !collapsed && model.collapsible

            onClicked: model.collapsed = !model.collapsed
        }
        Item {
            Layout.preferredWidth: fontMetrics.lineSpacing
            visible: !collapsed && !model.collapsible && Communities.containsSubspaces
        }
        Avatar {
            id: avatar

            Layout.alignment: Qt.AlignVCenter
            color: communityItem.backgroundColor
            displayName: model.displayName
            enabled: false
            Layout.preferredHeight: avatarSize
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
            Layout.preferredWidth: avatarSize

            NotificationBubble {
                anchors.bottom: avatar.bottom
                anchors.margins: -Nheko.paddingSmall
                anchors.right: avatar.right
                bubbleBackgroundColor: communityItem.bubbleBackground
                bubbleTextColor: communityItem.bubbleText
                font.pixelSize: fontMetrics.font.pixelSize * 0.6
                hasLoudNotification: model.hasLoudNotification
                mayBeVisible: collapsed && !model.muted && Settings.sidebarsRoomListShowCommunityCounts
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
            Layout.leftMargin: Nheko.paddingSmall
            bubbleBackgroundColor: communityItem.bubbleBackground
            bubbleTextColor: communityItem.bubbleText
            hasLoudNotification: model.hasLoudNotification
            mayBeVisible: !collapsed && !model.muted && Settings.sidebarsRoomListShowCommunityCounts
            notificationCount: model.unreadMessages
        }
    }
}
