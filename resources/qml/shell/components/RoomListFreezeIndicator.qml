// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai
import "../../components"

Control {
    id: root

    required property ListView roomList
    required property bool suppressed
    readonly property Item footerItem: roomList.footerItem
    readonly property bool indicatorHovered: hoverHandler.hovered
    property int buttonSize: Komai.barIconSize
    property int buttonPaddingH: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    readonly property int iconSize: Math.max(14, buttonSize - 2 * buttonPaddingH)
    readonly property color iconColor: hoverHandler.hovered ? palette.brightText : palette.buttonText
    readonly property real footerTopInViewport: (!footerItem || !footerItem.visible || footerItem.height <= 0)
        ? roomList.height
        : footerItem.y - roomList.contentY
    readonly property real bottomBoundaryY: Math.max(0, Math.min(roomList.height, footerTopInViewport))
    readonly property real toolTipAnchorX: x + width / 2
    readonly property real toolTipAnchorY: y

    parent: roomList
    z: 2
    visible: opacity > 0
    opacity: 0
    implicitHeight: buttonSize
    implicitWidth: buttonSize
    activeFocusOnTab: false
    focusPolicy: Qt.NoFocus

    x: parent.width - width - (Komai.paddingMedium + roomList.reservedScrollbarWidth)
    y: Math.max(Komai.paddingMedium, bottomBoundaryY - height - Komai.paddingMedium)

    KomaiToolTip {
        anchorItem: root.roomList
        anchorX: root.toolTipAnchorX
        anchorY: root.toolTipAnchorY
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        preferRight: false
        text: qsTr("Live updates are paused while you interact with the room list.")
        delay: 0
        requestedVisible: hoverHandler.hovered
    }

    HoverHandler {
        id: hoverHandler

        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
    }

    Rectangle {
        anchors.fill: parent
        color: hoverHandler.hovered ? root.palette.dark : root.palette.alternateBase
        radius: Komai.paddingSmall
        border.color: Komai.theme.separator
        border.width: 1
    }

    Image {
        anchors.centerIn: parent
        height: root.iconSize
        width: root.iconSize
        source: "image://colorimage/:/icons/icons/ui/pause-circle.svg?" + root.iconColor
        sourceSize.height: root.iconSize
        sourceSize.width: root.iconSize
    }

    states: [
        State {
            name: "shown"
            when: root.suppressed

            PropertyChanges {
                root.opacity: 1
            }
        }
    ]

    transitions: [
        Transition {
            to: "shown"

            NumberAnimation {
                duration: 150
                easing.type: Easing.InOutQuad
                property: "opacity"
            }
        },
        Transition {
            from: "shown"

            NumberAnimation {
                duration: 150
                easing.type: Easing.InOutQuad
                property: "opacity"
            }
        }
    ]
}
