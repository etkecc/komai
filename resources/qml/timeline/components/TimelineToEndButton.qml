// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import im.nheko

RoundButton {
    id: toEndButton

    required property var chatList
    required property var scrollbarItem
    property int fullWidth: 40

    flat: true
    height: width
    hoverEnabled: true
    radius: width / 2
    width: 0

    background: Rectangle {
        border.color: toEndButton.hovered ? palette.highlight : palette.buttonText
        border.width: 1
        color: toEndButton.down ? palette.highlight : palette.button
        opacity: enabled ? 1 : 0.3
        radius: toEndButton.radius
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

    onClicked: function () {
        chatList.keepPinnedToBottom = true;
        chatList.positionViewAtBeginning();
        TimelineManager.focusMessageInput();
        chatList.updateLastScroll();
    }

    anchors {
        bottom: parent.bottom
        bottomMargin: Nheko.paddingMedium + (fullWidth - width) / 2
        right: scrollbarItem.left
        rightMargin: Nheko.paddingMedium + (fullWidth - width) / 2
    }
    Image {
        anchors.fill: parent
        anchors.margins: Nheko.paddingMedium
        fillMode: Image.PreserveAspectFit
        source: "image://colorimage/:/icons/icons/ui/download.svg?" + (toEndButton.down ? palette.highlightedText : palette.buttonText)
    }
}
