// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    property var model: []
    property int currentIndex: 0

    signal activated(int index)

    readonly property int controlHeight: Math.max(40, Math.round(Settings.uiFontSizePt * 2.9))

    Layout.fillWidth: true
    implicitHeight: controlHeight
    implicitWidth: segmentRow.implicitWidth

    RowLayout {
        id: segmentRow

        anchors.fill: parent
        spacing: 0

        Repeater {
            model: root.model

            AbstractButton {
                id: segmentButton

                required property var modelData
                required property int index

                readonly property bool isFirst: index === 0
                readonly property bool isLast: index === root.model.length - 1
                readonly property bool isSelected: root.currentIndex === index
                readonly property bool activeState: hovered || visualFocus

                hoverEnabled: true
                activeFocusOnTab: true
                focusPolicy: Qt.StrongFocus
                Layout.fillWidth: true
                Layout.preferredHeight: root.controlHeight

                onClicked: {
                    root.currentIndex = index;
                    root.activated(index);
                }

                background: Item {
                    // Full-radius rectangle (clipped by container for per-corner rounding)
                    Rectangle {
                        id: bgRect

                        anchors.fill: parent
                        // Extend into neighbor to overlap border
                        anchors.leftMargin: segmentButton.isFirst ? 0 : -1
                        color: segmentButton.isSelected
                            ? palette.highlight
                            : segmentButton.down
                                ? Qt.darker(palette.dark, 1.08)
                                : segmentButton.activeState
                                    ? palette.dark
                                    : palette.window
                        border.color: segmentButton.activeFocus ? palette.highlight : Komai.theme.separator
                        border.width: segmentButton.activeFocus ? 2 : 1
                        radius: Komai.paddingSmall
                    }

                    // Mask to remove rounding on inner edges
                    Rectangle {
                        visible: !segmentButton.isFirst
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Komai.paddingSmall + 1
                        color: bgRect.color
                        border.color: bgRect.border.color
                        border.width: bgRect.border.width

                        // Cover left border seam
                        Rectangle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: bgRect.border.width + 1
                            color: bgRect.color
                        }
                    }

                    Rectangle {
                        visible: !segmentButton.isLast
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Komai.paddingSmall + 1
                        color: bgRect.color
                        border.color: bgRect.border.color
                        border.width: bgRect.border.width

                        // Cover right border seam
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: bgRect.border.width + 1
                            color: bgRect.color
                        }
                    }
                }

                contentItem: Label {
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: segmentButton.modelData.text
                    color: segmentButton.isSelected
                        ? palette.brightText
                        : (segmentButton.activeState || segmentButton.down)
                            ? palette.brightText
                            : palette.text
                    font.pointSize: Settings.uiFontSizePt
                    font.bold: true
                    elide: Text.ElideRight
                }

                KomaiCursorShape {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }
}
