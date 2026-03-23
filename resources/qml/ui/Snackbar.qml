// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
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
    property bool interactionLocked: false
    property real slideOffset: 0
    property real timeoutProgress: 0
    property point dismissToolTipPoint: Qt.point(0, 0)
    readonly property int queuedMessageCount: Math.max(0, messages.length - 1)
    readonly property real placementLeftInset: Math.max(Komai.paddingLarge, 16)
    readonly property real placementRightInset: Math.max(Komai.paddingMedium, 10)
    readonly property real verticalMargin: Math.max(Komai.paddingLarge, 20)
    readonly property real composerGap: Math.max(Komai.paddingMedium, 12)
    readonly property real minimumWidth: 280
    readonly property real maximumWidthRatio: 0.8
    readonly property real cardContentInset: 2 * Komai.paddingMedium
    readonly property int dismissButtonSize: Math.max(22, Math.round(Settings.uiFontSizePt * 1.7))
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
            startLifetime();
        }
    }

    function startLifetime() {
        interactionLocked = false;
        timeoutProgress = 0;
        progressAnimation.stop();
        progressAnimation.start();
        dismissTimer.restart();
    }

    function lockCurrent() {
        if (interactionLocked)
            return;

        interactionLocked = true;
        progressAnimation.stop();
        dismissTimer.stop();
    }

    function dismissCurrent() {
        progressAnimation.stop();
        dismissTimer.stop();
        close();
    }

    Timer {
        id: dismissTimer
        interval: 10000
        onTriggered: snackbar.close()
    }
    NumberAnimation {
        id: progressAnimation

        target: snackbar
        property: "timeoutProgress"
        from: 0
        to: 1
        duration: dismissTimer.interval
        easing.type: Easing.Linear
    }

    onAboutToHide: {
        messages.shift();
    }
    onClosed: {
        if (messages.length > 0) {
            currentMessage = messages[0];
            open();
            startLifetime();
        } else {
            interactionLocked = false;
            timeoutProgress = 0;
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
    TextMetrics {
        id: dismissToolTipMetrics

        font: dismissButton.font
        text: qsTr("Dismiss this message")
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
            implicitHeight: snackbarCardLayout.implicitHeight + 2 * Komai.paddingMedium
            radius: Komai.paddingMedium + 2
            color: palette.window
            border.width: 2
            border.color: Qt.tint(Komai.theme.separator, Qt.rgba(palette.highlight.r,
                                                                 palette.highlight.g,
                                                                 palette.highlight.b,
                                                                 0.28))

            HoverHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onHoveredChanged: if (hovered)
                    snackbar.lockCurrent()
            }

            ColumnLayout {
                id: snackbarCardLayout

                anchors.fill: parent
                anchors.margins: Komai.paddingMedium
                spacing: Komai.paddingSmall + 2

                RowLayout {
                    id: snackbarLayout

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
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

                        KomaiTextArea {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            id: snackbarMessage
                            activeFocusOnPress: true
                            background: null
                            color: palette.text
                            cursorVisible: activeFocus
                            font.pointSize: Settings.uiFontSizePt
                            leftPadding: 0
                            readOnly: true
                            rightPadding: 0
                            selectByMouse: true
                            selectedTextColor: palette.highlightedText
                            selectionColor: Qt.rgba(palette.highlight.r,
                                                    palette.highlight.g,
                                                    palette.highlight.b,
                                                    0.85)
                            text: snackbar.currentMessage
                            textFormat: Text.PlainText
                            topPadding: 0
                            wrapMode: Text.Wrap
                            onActiveFocusChanged: if (activeFocus)
                                snackbar.lockCurrent()
                            onSelectedTextChanged: if (selectedText.length > 0)
                                snackbar.lockCurrent()
                        }
                    }

                    Rectangle {
                        id: queuedChip

                        Layout.alignment: Qt.AlignTop
                        color: "transparent"
                        implicitWidth: sideControls.implicitWidth
                        implicitHeight: sideControls.implicitHeight

                        RowLayout {
                            id: sideControls

                            anchors.fill: parent
                            spacing: Komai.paddingSmall

                            Rectangle {
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

                            ImageButton {
                                id: dismissButton

                                Layout.alignment: Qt.AlignTop
                                Layout.preferredWidth: snackbar.dismissButtonSize
                                Layout.preferredHeight: snackbar.dismissButtonSize
                                Layout.minimumWidth: snackbar.dismissButtonSize
                                Layout.minimumHeight: snackbar.dismissButtonSize
                                buttonTextColor: palette.buttonText
                                highlightColor: palette.text
                                hoverEnabled: true
                                image: ":/icons/icons/ui/dismiss.svg"
                                ripple: false
                                onClicked: snackbar.dismissCurrent()

                                HoverHandler {
                                    id: dismissButtonHover

                                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                    onHoveredChanged: if (hovered && snackbar.parent)
                                        snackbar.dismissToolTipPoint = dismissButton.mapToItem(snackbar.parent,
                                                                                                 dismissButton.width / 2,
                                                                                                 0)
                                    onPointChanged: if (snackbar.parent)
                                        snackbar.dismissToolTipPoint = dismissButton.mapToItem(snackbar.parent,
                                                                                                 point.position.x,
                                                                                                 point.position.y)
                                }
                            }

                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.minimumWidth: 0
                    implicitHeight: 4
                    radius: 2
                    color: palette.alternateBase

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * snackbar.timeoutProgress
                        radius: parent.radius
                        color: palette.highlight
                    }
                }
            }

            KomaiToolTip {
                parent: snackbar.parent
                anchorX: snackbar.dismissToolTipPoint.x
                anchorY: snackbar.dismissToolTipPoint.y
                gapX: Math.max(Komai.paddingSmall + 2, 8)
                gapY: Math.max(Komai.paddingMedium * 2, 14)
                preferRight: false
                text: qsTr("Dismiss this message")
                delay: 0
                requestedVisible: dismissButtonHover.hovered
                width: Math.min(dismissToolTipMetrics.advanceWidth + leftPadding + rightPadding,
                                (snackbar.parent ? snackbar.parent.width : 500) * 0.5)
                z: 10000
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
