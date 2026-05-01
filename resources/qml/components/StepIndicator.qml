// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    // Model: array of { text: string }
    // Steps are numbered automatically (1-based).
    property var model: []

    // The index of the currently active step (0-based).
    property int currentIndex: 0

    signal activated(int index)

    readonly property int controlHeight: Math.max(36, Math.round(Settings.uiFontSizePt * 2.7))

    Layout.fillWidth: true
    implicitHeight: controlHeight
    implicitWidth: segmentRow.implicitWidth

    Rectangle {
        id: outerFrame

        anchors.fill: parent
        radius: Komai.paddingSmall
        color: palette.alternateBase
        border.color: Komai.theme.separator
        border.width: 1
        clip: true

        RowLayout {
            id: segmentRow

            anchors.fill: parent
            anchors.margins: outerFrame.border.width
            spacing: 0

            Repeater {
                model: root.model

                AbstractButton {
                    id: stepButton

                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    readonly property bool isFirst: index === 0
                    readonly property bool isLast: index === root.model.length - 1
                    readonly property bool isCurrent: root.currentIndex === index
                    readonly property bool isCompleted: index < root.currentIndex
                    readonly property bool isLocked: index > root.currentIndex
                    readonly property bool activeState: isCurrent || isCompleted || hovered || visualFocus

                    enabled: !isLocked
                    hoverEnabled: !isLocked
                    activeFocusOnTab: !isLocked
                    focusPolicy: isLocked ? Qt.NoFocus : Qt.StrongFocus

                    Accessible.role: Accessible.PageTab
                    Accessible.name: modelData && modelData.text ? modelData.text : ""
                    Accessible.checkable: true
                    Accessible.checked: isCurrent
                    Accessible.onPressAction: if (!isLocked) clicked()

                    onClicked: {
                        root.activated(index);
                    }

                    background: Rectangle {
                        radius: Komai.paddingSmall
                        color: stepButton.isCurrent
                            ? palette.highlight
                            : stepButton.isCompleted
                                ? (stepButton.down
                                    ? Qt.darker(palette.dark, 1.08)
                                    : (stepButton.hovered || stepButton.visualFocus)
                                        ? palette.dark
                                        : "transparent")
                                : stepButton.isLocked
                                    ? "transparent"
                                    : "transparent"

                        // Separator on left edge of non-first segments
                        Rectangle {
                            visible: !stepButton.isFirst
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: outerFrame.border.width
                            color: Komai.theme.separator
                            z: 1
                        }

                        // Square off inner corners so segments butt up cleanly
                        Rectangle {
                            visible: !stepButton.isFirst
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.radius
                            color: parent.color
                        }
                        Rectangle {
                            visible: !stepButton.isLast
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
                            spacing: Komai.paddingSmall
                            width: Math.min(implicitWidth, parent.width - Komai.paddingMedium * 2)

                            // Step number badge
                            Rectangle {
                                id: numberBadge

                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(numberLabel.implicitWidth + Komai.paddingSmall, height)
                                height: numberLabel.implicitHeight + Komai.paddingSmall * 0.5
                                radius: height / 2
                                color: stepButton.isCurrent
                                    ? palette.highlightedText
                                    : stepButton.isCompleted
                                        ? palette.highlight
                                        : stepButton.isLocked
                                            ? Komai.theme.separator
                                            : palette.highlight

                                Label {
                                    id: numberLabel

                                    anchors.centerIn: parent
                                    text: stepButton.isCompleted ? "\u2713" : (stepButton.index + 1)
                                    font.pointSize: Settings.uiFontSizePt * 0.85
                                    font.bold: true
                                    color: stepButton.isCurrent
                                        ? palette.highlight
                                        : stepButton.isCompleted
                                            ? palette.highlightedText
                                            : stepButton.isLocked
                                                ? palette.buttonText
                                                : palette.highlightedText
                                }
                            }

                            // Step label
                            Label {
                                id: stepLabel

                                anchors.verticalCenter: parent.verticalCenter
                                text: stepButton.modelData.text
                                color: stepButton.isCurrent
                                    ? palette.highlightedText
                                    : stepButton.isCompleted
                                        ? ((stepButton.hovered || stepButton.visualFocus) ? palette.brightText : palette.text)
                                        : stepButton.isLocked
                                            ? palette.buttonText
                                            : palette.text
                                font.pointSize: Settings.uiFontSizePt
                                font.bold: stepButton.isCurrent
                                elide: Text.ElideRight
                            }
                        }
                    }

                    KomaiCursorShape {
                        anchors.fill: parent
                        cursorShape: stepButton.isLocked ? Qt.ArrowCursor : Qt.PointingHandCursor
                    }
                }
            }
        }
    }
}
