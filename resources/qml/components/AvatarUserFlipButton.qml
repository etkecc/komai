// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

MouseArea {
    id: control

    property int avatarButtonSize: Komai.iconSize
    readonly property int effectiveButtonSize: Math.max(1, Math.round(avatarButtonSize) - (Math.round(avatarButtonSize) % 2))
    property bool motionEnabled: Settings.uiMotionAnimationsEnabled
    property string avatarDisplayName: ""
    property string avatarUrl: ""
    property string avatarUserId: ""
    property string avatarRoomId: ""
    property string toolTipText: ""
    property string badgeIconSource: ":/icons/icons/ui/person.svg"
    property bool cleanFront: false
    property bool suppressHoverUntilExit: false
    property real toolTipAnchorX: width / 2
    readonly property string resolvedToolTipText: toolTipText.length > 0
        ? toolTipText
        : (avatarDisplayName + (avatarUserId.length > 0 ? ("\n" + avatarUserId) : ""))
    readonly property bool hoverActive: containsMouse && !suppressHoverUntilExit
    property real flipAngle: (motionEnabled && hoverActive) ? 180 : 0
    readonly property bool activeState: hoverActive || pressed
    readonly property color iconBackgroundColor: activeState ? palette.dark : palette.window
    readonly property color iconColor: activeState ? palette.brightText : palette.buttonText

    signal leftClicked()
    signal rightClicked()

    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    acceptedButtons: Qt.LeftButton | Qt.RightButton

    KomaiToolTip {
        anchorItem: control
        anchorX: control.toolTipAnchorX
        anchorY: control.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: control.resolvedToolTipText
        delay: 0
        requestedVisible: control.hoverActive && control.resolvedToolTipText.length > 0
    }

    onClicked: function (mouse) {
        suppressHoverUntilExit = true;
        if (mouse.button === Qt.RightButton)
            control.rightClicked();
        else
            control.leftClicked();
    }
    onPositionChanged: function (mouse) {
        toolTipAnchorX = mouse.x;
        if (suppressHoverUntilExit && !pressed)
            suppressHoverUntilExit = false;
    }
    onExited: suppressHoverUntilExit = false

    Behavior on flipAngle {
        enabled: control.motionEnabled

        NumberAnimation {
            duration: 240
            easing.type: Easing.InOutQuad
        }
    }

    Item {
        id: iconFrame

        width: control.effectiveButtonSize
        height: control.effectiveButtonSize
        x: Math.floor((control.width - width) / 2)
        y: Math.floor((control.height - height) / 2)
    }

    Item {
        id: card

        anchors.fill: iconFrame
        layer.enabled: true

        transform: Rotation {
            origin.x: card.width / 2
            origin.y: card.height / 2
            axis {
                x: 0
                y: 1
                z: 0
            }
            angle: control.flipAngle
        }

        Item {
            id: frontFace

            anchors.fill: parent
            visible: control.flipAngle < 90

            Avatar {
                id: frontAvatar

                anchors.centerIn: parent
                width: control.effectiveButtonSize
                height: control.effectiveButtonSize
                displayName: control.avatarDisplayName
                url: control.avatarUrl
                userid: control.avatarUserId
                roomid: control.avatarRoomId
                fallbackBorderColor: palette.highlight
                enabled: false
            }

            Rectangle {
                id: frontBadge

                property int badgeSize: Math.round(control.effectiveButtonSize * 0.44)
                property int iconSize: Math.round(badgeSize * 0.69)

                visible: !control.cleanFront
                anchors.bottom: frontAvatar.bottom
                anchors.left: frontAvatar.left
                anchors.bottomMargin: -2
                anchors.leftMargin: -2
                width: badgeSize
                height: badgeSize
                radius: Math.round(badgeSize * 0.25)
                color: control.iconBackgroundColor

                Image {
                    anchors.centerIn: parent
                    source: "image://colorimage/" + control.badgeIconSource + "?" + control.iconColor
                    sourceSize.width: parent.iconSize
                    sourceSize.height: parent.iconSize
                    width: parent.iconSize
                    height: parent.iconSize
                }
            }
        }

        Item {
            id: backFace

            anchors.fill: parent
            visible: control.flipAngle >= 90

            transform: Rotation {
                origin.x: backFace.width / 2
                origin.y: backFace.height / 2
                axis {
                    x: 0
                    y: 1
                    z: 0
                }
                angle: 180
            }

            Rectangle {
                id: backCard

                anchors.centerIn: parent
                width: control.effectiveButtonSize
                height: control.effectiveButtonSize
                radius: Math.round(control.effectiveButtonSize * 0.26)
                color: control.iconBackgroundColor
                border.width: 1
                border.color: palette.dark

                Image {
                    property int personSize: Math.round(control.effectiveButtonSize * 0.62)

                    anchors.centerIn: parent
                    source: "image://colorimage/" + control.badgeIconSource + "?" + control.iconColor
                    sourceSize.width: personSize
                    sourceSize.height: personSize
                    width: personSize
                    height: personSize
                }
            }

            Rectangle {
                id: backBadge

                property int badgeSize: Math.round(control.effectiveButtonSize * 0.44)
                property real badgeAvatarScale: badgeSize > 0 ? (Math.round(badgeSize * 0.78) / control.effectiveButtonSize) : 1

                anchors.bottom: backCard.bottom
                anchors.left: backCard.left
                anchors.bottomMargin: -2
                anchors.leftMargin: -2
                width: badgeSize
                height: badgeSize
                radius: Math.round(badgeSize * 0.22)
                color: palette.window

                Avatar {
                    anchors.centerIn: parent
                    width: control.effectiveButtonSize
                    height: control.effectiveButtonSize
                    scale: backBadge.badgeAvatarScale
                    displayName: control.avatarDisplayName
                    url: control.avatarUrl
                    userid: control.avatarUserId
                    roomid: control.avatarRoomId
                    fallbackBorderColor: palette.highlight
                    enabled: false
                }
            }
        }
    }
}
