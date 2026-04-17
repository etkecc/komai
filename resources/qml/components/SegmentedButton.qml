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

    readonly property int controlHeight: Math.max(36, Math.round(Settings.uiFontSizePt * 2.7))

    TextMetrics {
        id: labelMetrics
        font.pointSize: Settings.uiFontSizePt
        font.bold: true
    }

    TextMetrics {
        id: badgeMetrics
        font.pointSize: Settings.uiFontSizePt * 0.8
        font.bold: true
    }

    // Measured imperatively to avoid a binding loop (mutating TextMetrics.text inside a
    // binding that reads TextMetrics.advanceWidth triggers QML's self-read detection).
    property real _maxSegmentContentWidth: 0

    function _recomputeMaxSegmentWidth() {
        var maxW = 0;
        var sideMargins = Komai.paddingMedium * 2;
        var labelBadgeSpacing = Komai.paddingMedium;
        var items = root.model || [];
        for (var i = 0; i < items.length; i++) {
            var entry = items[i];
            labelMetrics.text = (entry && entry.text) ? entry.text : "";
            var w = labelMetrics.advanceWidth;
            if (entry && entry.badge) {
                badgeMetrics.text = entry.badge;
                var badgeH = badgeMetrics.boundingRect.height + Komai.paddingSmall * 0.5;
                var badgeW = Math.max(badgeMetrics.advanceWidth + Komai.paddingSmall * 1.5, badgeH);
                w += labelBadgeSpacing + badgeW;
            }
            w += sideMargins;
            if (w > maxW)
                maxW = w;
        }
        _maxSegmentContentWidth = maxW;
    }

    onModelChanged: _recomputeMaxSegmentWidth()
    Component.onCompleted: _recomputeMaxSegmentWidth()
    Connections {
        target: Settings
        function onUiFontSizePtChanged() { root._recomputeMaxSegmentWidth(); }
    }

    Layout.fillWidth: true
    implicitHeight: controlHeight
    implicitWidth: _maxSegmentContentWidth * (root.model ? root.model.length : 0) + 2

    Rectangle {
        id: outerFrame

        anchors.fill: parent
        radius: Komai.paddingSmall
        color: palette.window
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
                    id: segmentButton

                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: root._maxSegmentContentWidth
                    Layout.minimumWidth: 0

                    readonly property bool isFirst: index === 0
                    readonly property bool isLast: index === root.model.length - 1
                    readonly property bool isSelected: root.currentIndex === index
                    readonly property bool activeState: isSelected || hovered || visualFocus

                    hoverEnabled: true
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus

                    onClicked: {
                        root.currentIndex = index;
                        root.activated(index);
                    }

                    background: Rectangle {
                        radius: Komai.paddingSmall
                        color: segmentButton.isSelected
                            ? palette.highlight
                            : segmentButton.down
                                ? Qt.darker(palette.dark, 1.08)
                                : (segmentButton.hovered || segmentButton.visualFocus)
                                    ? palette.dark
                                    : "transparent"

                        // Separator on left edge of non-first segments
                        Rectangle {
                            visible: !segmentButton.isFirst
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: outerFrame.border.width
                            color: Komai.theme.separator
                            z: 1
                        }

                        // Square off inner corners so segments butt up cleanly
                        Rectangle {
                            visible: !segmentButton.isFirst
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.radius
                            color: parent.color
                        }
                        Rectangle {
                            visible: !segmentButton.isLast
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.radius
                            color: parent.color
                        }
                    }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Komai.paddingMedium
                    anchors.rightMargin: Komai.paddingMedium
                    spacing: Komai.paddingMedium

                    Label {
                        id: segmentLabel

                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        text: segmentButton.modelData.text
                        color: segmentButton.isSelected
                            ? palette.highlightedText
                            : segmentButton.activeState
                                ? palette.brightText
                                : palette.text
                        font.pointSize: Settings.uiFontSizePt
                        font.bold: true
                        elide: Text.ElideRight
                        horizontalAlignment: !!segmentButton.modelData.badge ? Text.AlignLeft : Text.AlignHCenter
                    }

                    // Inline badge (optional — model entry may have a "badge" string)
                    Rectangle {
                        id: badgeRect

                        visible: !!segmentButton.modelData.badge
                        Layout.alignment: Qt.AlignVCenter
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
                            text: segmentButton.modelData.badge || ""
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

                KomaiCursorShape {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                }

                KomaiToolTip {
                    anchorItem: segmentButton
                    anchorX: 0
                    anchorY: segmentButton.height
                    followMouse: false
                    text: segmentLabel.truncated ? (segmentButton.modelData.text || "") : ""
                    requestedVisible: segmentButton.hovered && segmentLabel.truncated
                    delay: 300
                }
            }
        }
        }
    }
}
