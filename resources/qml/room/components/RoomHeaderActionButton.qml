// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai
import "../../components"

AbstractButton {
    id: button

    required property var topBarRef
    required property int column
    property int row: 1
    property string image: ""
    property string labelText: ""
    property bool showLabel: false
    property bool alwaysShowToolTip: false
    property string toolTipText: labelText
    property real toolTipAnchorX: width / 2
    readonly property bool hasLabel: showLabel && labelText.length > 0
    readonly property int iconSize: Math.max(14, topBarRef.topBarAvatarSize - 2 * topBarRef.buttonPaddingH)
    readonly property bool activeState: hovered || pressed || visualFocus
    readonly property color actionTextColor: activeState ? palette.brightText : palette.buttonText
    readonly property color actionLabelColor: activeState ? palette.brightText : palette.text

    function isActivationKey(event) {
        if (!event)
            return false;

        const modifiers = Number(event.modifiers);
        if ((modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) !== 0)
            return false;

        return event.key === Qt.Key_Return
            || event.key === Qt.Key_Enter
            || event.key === Qt.Key_Space;
    }

    Layout.alignment: Qt.AlignVCenter
    Layout.column: column
    Layout.preferredHeight: topBarRef.topBarAvatarSize
    Layout.preferredWidth: implicitWidth
    Layout.row: row
    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    font.pointSize: Settings.uiFontSizePt
    hoverEnabled: true
    Keys.priority: Keys.BeforeItem
    Keys.onPressed: event => {
        if (!button.enabled || !button.isActivationKey(event))
            return;

        button.clicked();
        event.accepted = true;
    }
    leftPadding: topBarRef.buttonPaddingH
    rightPadding: topBarRef.buttonPaddingH
    topPadding: topBarRef.buttonPaddingV
    bottomPadding: topBarRef.buttonPaddingV
    implicitHeight: topBarRef.topBarAvatarSize
    implicitWidth: topBarRef.topBarAvatarSize + (hasLabel ? (Komai.paddingSmall + actionLabel.implicitWidth) : 0)

    TextMetrics {
        id: toolTipMetrics

        font: button.font
        text: button.toolTipText
    }

    KomaiToolTip {
        id: actionToolTip

        anchorItem: button
        anchorX: button.toolTipAnchorX
        anchorY: button.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: button.toolTipText
        delay: 0
        requestedVisible: button.hovered && button.toolTipText.length > 0 && (button.alwaysShowToolTip || !button.hasLabel)
        width: Math.min(toolTipMetrics.advanceWidth + leftPadding + rightPadding,
                        (button.Window.window ? button.Window.window.width : 500) * 0.5)
    }

    HoverHandler {
        onPointChanged: if (hovered)
            button.toolTipAnchorX = point.position.x
    }

    background: Rectangle {
        radius: Komai.paddingSmall
        color: button.activeState ? palette.dark : "transparent"
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: button.leftPadding
        anchors.rightMargin: button.rightPadding
        anchors.topMargin: button.topPadding
        anchors.bottomMargin: button.bottomPadding
        spacing: Komai.paddingSmall

        Image {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: button.iconSize
            Layout.preferredWidth: button.iconSize
            source: button.image !== "" ? ("image://colorimage/" + button.image + "?" + button.actionTextColor) : ""
            sourceSize.height: button.iconSize
            sourceSize.width: button.iconSize
        }
        Label {
            id: actionLabel

            Layout.alignment: Qt.AlignVCenter
            color: button.actionLabelColor
            font.bold: true
            text: button.labelText
            visible: button.hasLabel
        }
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
}
