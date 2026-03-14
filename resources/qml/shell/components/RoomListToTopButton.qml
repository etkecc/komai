// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

RoomListActionButton {
    id: toTopButton

    required property ListView roomList
    required property Item scrollbarItem
    property bool collapsed: false
    readonly property bool shouldShow: !collapsed && !roomList.atYBeginning
    readonly property real labeledWidth: buttonSize + Komai.paddingSmall + labelMetrics.advanceWidth

    buttonSize: Komai.barIconSize
    iconSource: ":/icons/icons/ui/upload.svg"
    toolTipText: qsTr("Scroll to top")
    labelText: qsTr("Scroll to top")
    showLabel: !collapsed && labeledWidth <= (roomList.width - roomList.reservedScrollbarWidth) / 2
    opacity: 0

    TextMetrics {
        id: labelMetrics

        font: Qt.font({
            "bold": true,
            "pointSize": Settings.uiFontSizePt
        })
        text: toTopButton.labelText
    }

    onClicked: roomList.positionViewAtBeginning()

    background: Rectangle {
        color: toTopButton.activeState ? palette.dark : palette.alternateBase
        radius: Komai.paddingSmall
        border.color: Komai.theme.separator
        border.width: 1
    }

    anchors {
        top: parent.top
        topMargin: Komai.paddingMedium
        right: scrollbarItem.left
        rightMargin: Komai.paddingMedium
    }

    states: [
        State {
            name: "shown"
            when: toTopButton.shouldShow

            PropertyChanges {
                toTopButton.opacity: 1
            }
        }
    ]

    transitions: [
        Transition {
            to: "shown"

            SequentialAnimation {
                PauseAnimation {
                    duration: 500
                }
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.InOutQuad
                    property: "opacity"
                }
            }
        },
        Transition {
            from: "shown"

            NumberAnimation {
                duration: 200
                easing.type: Easing.InOutQuad
                property: "opacity"
            }
        }
    ]
}
