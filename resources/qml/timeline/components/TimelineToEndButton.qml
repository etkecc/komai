// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Control {
    id: toEndButton

    required property var chatList
    required property var scrollbarItem
    property int fullWidth: Math.round(jumpButton.implicitWidth)
    property int fullHeight: Math.round(jumpButton.implicitHeight)
    readonly property color actionButtonColor: palette.buttonText
    readonly property color actionButtonHoverBackgroundColor: palette.dark

    QtObject {
        id: toolbarStyle

        readonly property color actionButtonColor: toEndButton.actionButtonColor
        readonly property color actionButtonActiveColor: palette.brightText
        readonly property color actionButtonHoverBackgroundColor: toEndButton.actionButtonHoverBackgroundColor
        readonly property int actionButtonIconSize: 24
        readonly property int itemHorizontalPadding: Komai.paddingMedium
        readonly property int itemVerticalPadding: Komai.paddingMedium
    }

    clip: true
    height: fullHeight
    padding: 0
    leftInset: 0
    rightInset: 0
    topInset: 0
    bottomInset: 0
    width: 0

    background: TimelineFloatingActionBarBackground {
        barColor: palette.alternateBase
        barRadius: Komai.paddingSmall
        barBorderColor: Komai.theme.separator
        barBorderWidth: 1
    }

    states: [
        State {
            name: ""

            PropertyChanges {
                toEndButton.width: 0
            }
        },
        State {
            name: "shown"
            when: !chatList.atYEnd

            PropertyChanges {
                toEndButton.width: toEndButton.fullWidth
            }
        }
    ]

    transitions: Transition {
        from: ""
        reversible: true
        to: "shown"

        SequentialAnimation {
            PauseAnimation {
                duration: 500
            }
            PropertyAnimation {
                duration: 200
                easing.type: Easing.InOutQuad
                properties: "width"
                target: toEndButton
            }
        }
    }

    contentItem: Item {
        implicitWidth: jumpButton.implicitWidth
        implicitHeight: jumpButton.implicitHeight

        MessageActionsToolbarButton {
            id: jumpButton

            anchors.centerIn: parent
            toolbarRef: toolbarStyle
            image: ":/icons/icons/ui/download.svg"
            labelText: qsTr("Jump to latest")
            toolTipText: qsTr("Jump to latest")

            onClicked: function () {
                chatList.keepPinnedToBottom = true;
                chatList.positionViewAtBeginning();
                TimelineManager.focusMessageInput();
                chatList.updateLastScroll();
            }
        }
    }

    anchors {
        bottom: parent.bottom
        bottomMargin: Komai.paddingMedium + (fullWidth - width) / 2
        right: scrollbarItem.left
        rightMargin: Komai.paddingMedium + (fullWidth - width) / 2
    }
}
