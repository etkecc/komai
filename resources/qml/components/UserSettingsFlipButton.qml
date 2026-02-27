// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".."
import QtQuick
import QtQuick.Controls
import im.nheko

MouseArea {
    id: control

    property var profile: Nheko.currentUser
    property int avatarButtonSize: Nheko.barIconSize
    readonly property int effectiveButtonSize: Math.max(1, Math.round(avatarButtonSize) - (Math.round(avatarButtonSize) % 2))
    property bool motionEnabled: Settings.uiMotionAnimationsEnabled
    property real flipAngle: 0

    signal leftClicked()
    signal rightClicked()

    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    acceptedButtons: Qt.LeftButton | Qt.RightButton

    ToolTip.delay: Nheko.tooltipDelay
    ToolTip.text: (profile ? profile.displayName : "") + "\n" + (profile ? profile.userid : "")
    ToolTip.visible: containsMouse

    onClicked: function(mouse) {
        if (mouse.button === Qt.RightButton)
            control.rightClicked();
        else
            control.leftClicked();
    }

    onContainsMouseChanged: updateFlipAngle()
    onMotionEnabledChanged: {
        if (!motionEnabled) {
            flipAngle = 0;
            return;
        }
        updateFlipAngle();
    }

    Component.onCompleted: updateFlipAngle()

    function updateFlipAngle() {
        flipAngle = (motionEnabled && containsMouse) ? 180 : 0;
    }

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
                displayName: control.profile ? control.profile.displayName : ""
                url: (control.profile ? control.profile.avatarUrl : "").replace("mxc://", "image://MxcImage/")
                userid: control.profile ? control.profile.userid : ""
                fallbackBorderColor: palette.highlight
                enabled: false
            }

            Rectangle {
                id: frontBadge

                property int badgeSize: Math.round(control.effectiveButtonSize * 0.44)
                property int iconSize: Math.round(badgeSize * 0.69)

                anchors.bottom: frontAvatar.bottom
                anchors.left: frontAvatar.left
                anchors.bottomMargin: -2
                anchors.leftMargin: -2
                width: badgeSize
                height: badgeSize
                radius: Math.round(badgeSize * 0.25)
                color: palette.window

                Image {
                    anchors.centerIn: parent
                    source: "image://colorimage/:/icons/icons/ui/settings.svg?" + palette.text
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
                color: palette.window
                border.width: 1
                border.color: palette.dark

                Image {
                    property int cogSize: Math.round(control.effectiveButtonSize * 0.62)

                    anchors.centerIn: parent
                    source: "image://colorimage/:/icons/icons/ui/settings.svg?" + palette.text
                    sourceSize.width: cogSize
                    sourceSize.height: cogSize
                    width: cogSize
                    height: cogSize
                }
            }

            Rectangle {
                id: backBadge

                property int badgeSize: Math.round(control.effectiveButtonSize * 0.44)
                property int avatarSize: Math.round(badgeSize * 0.78)

                anchors.bottom: backCard.bottom
                anchors.right: backCard.right
                anchors.bottomMargin: -2
                anchors.rightMargin: -2
                width: badgeSize
                height: badgeSize
                radius: Math.round(badgeSize * 0.22)
                color: palette.window

                Avatar {
                    anchors.centerIn: parent
                    width: backBadge.avatarSize
                    height: backBadge.avatarSize
                    displayName: control.profile ? control.profile.displayName : ""
                    url: (control.profile ? control.profile.avatarUrl : "").replace("mxc://", "image://MxcImage/")
                    userid: control.profile ? control.profile.userid : ""
                    fallbackBorderColor: palette.highlight
                    enabled: false
                }
            }
        }
    }
}
