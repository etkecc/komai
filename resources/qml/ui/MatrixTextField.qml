// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import cc.etke.komai 1.0
import "../components"

ColumnLayout {
    id: c

    property color backgroundColor: palette.base
    property alias color: labelC.color
    property alias echoMode: input.echoMode
    property alias font: input.font
    property var hasClear: false
    property alias label: labelC.text
    property alias placeholderText: input.placeholderText
    property alias selectByMouse: input.selectByMouse
    property alias text: input.text
    property alias textPadding: input.padding
    property real radius: 0
    property string toolTipText: ""
    property bool toolTipVisible: hover.hovered && toolTipText.length > 0
    property int toolTipDelay: Komai.tooltipDelay

    signal accepted
    signal editingFinished
    signal textEdited

    function clear() {
        input.clear();
    }
    function forceActiveFocus() {
        input.forceActiveFocus();
    }

    spacing: 0

    KomaiToolTip {
        anchorItem: c
        anchorX: hover.point.position.x
        anchorY: c.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: c.toolTipText
        delay: c.toolTipDelay
        requestedVisible: c.toolTipVisible
    }

    onTextChanged: timer.restart()

    Timer {
        id: timer

        interval: 350

        onTriggered: editingFinished()
    }
    Item {
        Layout.bottomMargin: Komai.paddingSmall
        Layout.fillWidth: true
        Layout.margins: input.padding
        Layout.preferredHeight: labelC.contentHeight
        visible: labelC.text
        z: 1

        Label {
            id: labelC

            color: palette.text
            enabled: false
            font.letterSpacing: input.font.pixelSize * 0.02
            font.pixelSize: input.font.pixelSize
            font.weight: Font.DemiBold
            state: labelC.text && (input.activeFocus == true || input.text) ? "focused" : ""
            width: parent.width
            y: contentHeight + input.padding + Komai.paddingSmall

            states: State {
                name: "focused"

                PropertyChanges {
                    labelC.y: 0
                }
                PropertyChanges {
                    input.opacity: 1
                }
            }
            transitions: Transition {
                from: ""
                reversible: true
                to: "focused"

                NumberAnimation {
                    alwaysRunToEnd: true
                    duration: 210
                    easing.type: Easing.InCubic
                    properties: "y"
                    target: labelC
                }
                NumberAnimation {
                    alwaysRunToEnd: true
                    duration: 210
                    easing.type: Easing.InCubic
                    properties: "opacity"
                    target: input
                }
            }
        }
    }
    TextField {
        id: input

        Layout.fillWidth: true
        color: labelC.color
        focus: true
        opacity: labelC.text ? 0 : 1

        background: Rectangle {
            id: backgroundRect

            color: labelC.text ? "transparent" : backgroundColor
            radius: c.radius
            border.color: c.radius > 0 ? (input.activeFocus ? palette.text : palette.highlight) : "transparent"
            border.width: c.radius > 0 ? 1 : 0
        }

        onAccepted: c.accepted()
        onEditingFinished: c.editingFinished()
        onTextEdited: c.textEdited()

        ImageButton {
            id: clearText

            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            image: ":/icons/icons/ui/round-remove-button.svg"
            visible: c.hasClear && searchField.text !== ''

            onClicked: {
                searchField.clear();
                topBar.searchString = "";
            }

            anchors {
                bottom: parent.bottom
                right: parent.right
                rightMargin: Komai.paddingSmall
                top: parent.top
            }
        }
    }
    Rectangle {
        id: blueBar

        Layout.fillWidth: true
        color: palette.highlight
        Layout.preferredHeight: 1
        visible: c.radius === 0

        Rectangle {
            id: blackBar

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            color: palette.text
            height: parent.height * 2
            width: 0

            states: State {
                name: "focused"
                when: input.activeFocus == true

                PropertyChanges {
                    blackBar.width: blueBar.width
                }
            }
            transitions: Transition {
                from: ""
                reversible: true
                to: "focused"

                NumberAnimation {
                    alwaysRunToEnd: true
                    duration: 310
                    easing.type: Easing.InCubic
                    properties: "width"
                    target: blackBar
                }
            }
        }
    }
    HoverHandler {
        id: hover

        enabled: c.toolTipText !== ""
    }
}
