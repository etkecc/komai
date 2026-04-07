// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai 1.0

Switch {
    id: toggleButton

    implicitWidth: indicatorRow.implicitWidth
    implicitHeight: indicatorRow.implicitHeight
    property int cursor: Qt.PointingHandCursor
    property color textColor: palette.buttonText
    state: checked ? "on" : "off"

    indicator: Row {
        id: indicatorRow

        spacing: 4
        y: parent.height / 2 - height / 2

        Text {
            id: offLabel

            text: qsTr("OFF")
            font.pointSize: 0.9 * Settings.uiFontSizePt
            color: toggleButton.checked ? toggleButton.textColor : palette.highlight
            anchors.verticalCenter: parent.verticalCenter
        }

        Item {
            id: indicatorItem

            implicitHeight: 24
            implicitWidth: 48

            Rectangle {
                id: track

                color: Qt.rgba(border.color.r, border.color.g, border.color.b, 0.6)
                height: parent.height * 0.6
                radius: height / 2
                width: parent.width - height
                x: radius
                y: parent.height / 2 - height / 2
            }
            Rectangle {
                id: handle

                border.color: palette.buttonText
                color: palette.light
                height: width
                radius: width / 2
                width: parent.height * 0.9
                y: parent.height / 2 - height / 2
            }
        }

        Text {
            id: onLabel

            text: qsTr("ON")
            font.pointSize: 0.9 * Settings.uiFontSizePt
            color: toggleButton.checked ? palette.highlight : toggleButton.textColor
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    states: [
        State {
            name: "off"

            PropertyChanges {
                track.border.color: palette.buttonText
            }
            PropertyChanges {
                handle.x: 0
            }
        },
        State {
            name: "on"

            PropertyChanges {
                track.border.color: palette.highlight
            }
            PropertyChanges {
                handle.x: indicatorItem.width - handle.width
            }
        }
    ]
    transitions: [
        Transition {
            reversible: true
            to: "off"

            ParallelAnimation {
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.InOutQuad
                    property: "x"
                    target: handle
                }
                ColorAnimation {
                    duration: 200
                    properties: "color,border.color"
                    target: track
                }
            }
        }
    ]

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: toggleButton.enabled ? toggleButton.cursor : Qt.ArrowCursor
    }
}
