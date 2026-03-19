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
    property Item hoveredBadge: null

    signal activated(int index)

    readonly property int controlHeight: Math.max(46, Math.round(Settings.uiFontSizePt * 3.4))

    Layout.fillWidth: true
    implicitHeight: controlHeight
    implicitWidth: segmentRow.implicitWidth

    // Outer container with rounded corners, clipping everything inside
    Rectangle {
        id: outerFrame

        anchors.fill: parent
        radius: Komai.paddingSmall
        color: "transparent"
        clip: true

        RowLayout {
            id: segmentRow

            anchors.fill: parent
            spacing: 0

            Repeater {
                model: root.model

                Item {
                    id: segmentWrapper

                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    readonly property bool isFirst: index === 0
                    readonly property bool isLast: index === root.model.length - 1

                    // Thin divider before this segment (not on the first)
                    Rectangle {
                        visible: !segmentWrapper.isFirst
                            && !segmentButton.isSelected
                            && !(root.currentIndex === segmentWrapper.index - 1)
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        height: parent.height * 0.5
                        width: 1
                        color: Komai.theme.separator
                        z: 1
                    }

                    AbstractButton {
                        id: segmentButton

                        anchors.fill: parent

                        readonly property bool isSelected: root.currentIndex === segmentWrapper.index
                        readonly property bool activeState: hovered || visualFocus

                        hoverEnabled: true
                        activeFocusOnTab: true
                        focusPolicy: Qt.StrongFocus

                        onClicked: {
                            root.currentIndex = segmentWrapper.index;
                            root.activated(segmentWrapper.index);
                        }

                        background: Rectangle {
                            color: segmentButton.isSelected
                                ? palette.highlight
                                : segmentButton.down
                                    ? Qt.darker(palette.dark, 1.08)
                                    : segmentButton.activeState
                                        ? palette.dark
                                        : palette.window
                            // Round only outer corners
                            radius: Komai.paddingSmall

                            Rectangle {
                                visible: !segmentWrapper.isFirst
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: parent.radius
                                color: parent.color
                            }

                            Rectangle {
                                visible: !segmentWrapper.isLast
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: parent.radius
                                color: parent.color
                            }
                        }

                        contentItem: Item {
                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: Komai.paddingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: Komai.paddingMedium
                                width: Math.min(implicitWidth, parent.width - Komai.paddingMedium * 2)

                                Label {
                                    id: segmentLabel

                                    anchors.verticalCenter: parent.verticalCenter
                                    text: segmentWrapper.modelData.text
                                    color: segmentButton.isSelected
                                        ? palette.highlightedText
                                        : (segmentButton.activeState || segmentButton.down)
                                            ? palette.brightText
                                            : palette.text
                                    font.pointSize: Settings.uiFontSizePt
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                // Inline badge (optional — model entry may have a "badge" string)
                                Rectangle {
                                    id: badgeRect

                                    visible: !!segmentWrapper.modelData.badge
                                    anchors.verticalCenter: parent.verticalCenter
                                implicitHeight: badgeLabel.implicitHeight + Komai.paddingSmall * 0.5
                                implicitWidth: Math.max(badgeLabel.implicitWidth + Komai.paddingSmall * 1.5, implicitHeight)
                                radius: height / 8
                                color: segmentButton.isSelected ? palette.highlightedText : palette.highlight

                                Label {
                                    id: badgeLabel
                                    anchors.centerIn: parent
                                    font.bold: true
                                    font.pointSize: Settings.uiFontSizePt * 0.8
                                    color: segmentButton.isSelected ? palette.highlight : palette.highlightedText
                                    text: segmentWrapper.modelData.badge || ""
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                    onContainsMouseChanged: {
                                        if (containsMouse)
                                            root.hoveredBadge = badgeRect;
                                        else if (root.hoveredBadge === badgeRect)
                                            root.hoveredBadge = null;
                                    }
                                }
                            }
                            }
                        }

                        KomaiCursorShape {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                }
            }
        }
    }
}
