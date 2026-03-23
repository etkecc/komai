// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import cc.etke.komai 1.0

Popup {
    id: snackbar

    // Workaround palettes not inheriting for popups
    palette: timelineRoot.palette

    property Item contentAreaItem: null
    property Item avoidBottomItem: null
    property var messages: []
    property string currentMessage: ""
    property real slideOffset: 0
    readonly property int queuedMessageCount: Math.max(0, messages.length - 1)
    readonly property real placementLeftInset: Math.max(Komai.paddingLarge, 16)
    readonly property real placementRightInset: Math.max(Komai.paddingMedium, 10)
    readonly property real verticalMargin: Math.max(Komai.paddingLarge, 20)
    readonly property real composerGap: Math.max(Komai.paddingMedium, 12)
    readonly property real minimumWidth: 280
    readonly property real maximumWidthRatio: 0.8
    readonly property real cardContentInset: 2 * Komai.paddingMedium + 6
    readonly property real titleWidth: Math.ceil(titleMetrics.advanceWidth || 0)
    readonly property real bodyWidth: Math.ceil(bodyMetrics.advanceWidth || 0)
    readonly property real queuedWidth: queuedChip.visible ? queuedChip.implicitWidth + Komai.paddingMedium : 0
    readonly property rect contentRect: contentAreaRectInOverlay()
    readonly property rect avoidRect: avoidRectInOverlay()
    readonly property real maximumContentWidth: Math.max(minimumWidth,
                                                         Math.floor(contentRect.width * maximumWidthRatio))
    readonly property real placementAvailableWidth: Math.max(minimumWidth,
                                                             Math.min(maximumContentWidth,
                                                                      contentRect.width - placementLeftInset - placementRightInset))
    readonly property real idealTextWidth: Math.max(titleWidth, Math.min(bodyWidth, maximumContentWidth - cardContentInset))
    readonly property real idealWidth: Math.max(minimumWidth,
                                                Math.min(maximumContentWidth,
                                                         cardContentInset + idealTextWidth + queuedWidth))

    function itemRectInOverlay(item) {
        if (item && parent) {
            const topLeft = item.mapToItem(parent, 0, 0);
            return Qt.rect(topLeft.x, topLeft.y, item.width, item.height);
        }

        return Qt.rect(0,
                       0,
                       parent ? parent.width : maximumContentWidth,
                       parent ? parent.height : 0);
    }

    function contentAreaRectInOverlay() {
        return itemRectInOverlay(contentAreaItem || timelineRoot);
    }

    function avoidRectInOverlay() {
        if (!avoidBottomItem)
            return Qt.rect(0, 0, 0, 0);

        return itemRectInOverlay(avoidBottomItem);
    }

    function showNotification(msg) {
        messages.push(msg);
        currentMessage = messages[0];
        if (!visible) {
            open();
            dismissTimer.start();
        }
    }

    Timer {
        id: dismissTimer
        interval: 10000
        onTriggered: snackbar.close()
    }

    onAboutToHide: {
        messages.shift();
    }
    onClosed: {
        if (messages.length > 0) {
            currentMessage = messages[0];
            open();
            dismissTimer.restart();
        }
    }

    parent: Overlay.overlay
    closePolicy: Popup.NoAutoClose
    opacity: 0
    implicitHeight: contentItem ? contentItem.implicitHeight : 0
    height: implicitHeight
    padding: 0
    width: Math.min(idealWidth, placementAvailableWidth)
    x: {
        const left = contentRect.x + placementLeftInset;
        const right = contentRect.x + contentRect.width - placementRightInset - width;
        return Math.round(Math.max(left, right));
    }
    y: {
        const top = contentRect.y + verticalMargin;
        const popupHeight = height;
        const bottom = contentRect.y + contentRect.height - verticalMargin - popupHeight;
        const preferredBottom = avoidRect.height > 0
            ? avoidRect.y - composerGap - popupHeight
            : bottom;
        return Math.round(Math.max(top, Math.min(preferredBottom, bottom)));
    }

    TextMetrics {
        id: titleMetrics

        font: titleLabel.font
        text: qsTr("Notification")
    }
    TextMetrics {
        id: bodyMetrics

        font: snackbarMessage.font
        text: snackbar.currentMessage
    }

    contentItem: Item {
        implicitWidth: snackbar.width
        implicitHeight: snackbarCard.implicitHeight + shadowRect.anchors.topMargin
        transform: Translate { y: snackbar.slideOffset }

        Rectangle {
            id: shadowRect

            anchors.fill: snackbarCard
            anchors.topMargin: 6
            anchors.leftMargin: 2
            anchors.rightMargin: -2
            anchors.bottomMargin: -6
            radius: snackbarCard.radius + 2
            color: Qt.rgba(0, 0, 0, palette.window.hslLightness < 0.5 ? 0.32 : 0.14)
        }

        Rectangle {
            id: snackbarCard

            width: snackbar.width
            implicitHeight: snackbarLayout.implicitHeight + 2 * Komai.paddingMedium
            radius: Komai.paddingMedium + 2
            color: palette.window
            border.width: 1
            border.color: Qt.tint(Komai.theme.separator, Qt.rgba(palette.highlight.r,
                                                                 palette.highlight.g,
                                                                 palette.highlight.b,
                                                                 0.18))

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                radius: 2
                color: palette.highlight
            }

            RowLayout {
                id: snackbarLayout

                anchors.fill: parent
                anchors.margins: Komai.paddingMedium
                anchors.leftMargin: Komai.paddingMedium + 6
                spacing: Komai.paddingMedium

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        id: titleLabel
                        color: palette.text
                        font.bold: true
                        font.pointSize: Settings.uiFontSizePt
                        text: qsTr("Notification")
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        id: snackbarMessage
                        color: palette.text
                        text: snackbar.currentMessage
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pointSize: Settings.uiFontSizePt
                    }
                }

                Rectangle {
                    id: queuedChip

                    Layout.alignment: Qt.AlignTop
                    visible: snackbar.queuedMessageCount > 0
                    radius: Math.round(height / 2)
                    color: palette.alternateBase
                    border.width: 1
                    border.color: Komai.theme.separator
                    implicitWidth: queuedLabel.implicitWidth + Komai.paddingMedium
                    implicitHeight: queuedLabel.implicitHeight + Komai.paddingSmall

                    Label {
                        id: queuedLabel

                        anchors.centerIn: parent
                        color: palette.buttonText
                        font.pointSize: Math.max(Settings.uiFontSizePt - 1, 8)
                        text: qsTr("+%1").arg(snackbar.queuedMessageCount)
                    }
                }
            }
        }
    }

    background: Rectangle {
        color: "transparent"
    }

    enter: Transition {
        NumberAnimation {
            target: snackbar
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 150
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: snackbar
            property: "slideOffset"
            from: 18
            to: 0
            duration: 180
            easing.type: Easing.OutCubic
        }
    }
    exit: Transition {
        NumberAnimation {
            target: snackbar
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 120
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: snackbar
            property: "slideOffset"
            from: 0
            to: 12
            duration: 120
            easing.type: Easing.InCubic
        }
    }
}
