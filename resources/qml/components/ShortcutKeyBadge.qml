// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import cc.etke.komai 1.0

Item {
    id: root

    property string text: ""
    property string toolTipText: qsTr("Press these keyboard keys to trigger this action.")
    property bool highlighted: false
    property bool showKeyboardIcon: false
    property bool liveModifierHighlight: false
    property color keyTextColor: palette.buttonText
    readonly property real iconSize: Math.round(Settings.uiFontSizePt * 1.5)
    readonly property var keyTokens: text === "" ? [] : text.split("+")
    readonly property bool altModifierHeld: liveModifierHighlight && MainWindow.altPressed

    implicitWidth: contentRow.implicitWidth
    implicitHeight: contentRow.implicitHeight
    visible: text !== ""

    KomaiToolTip {
        anchorItem: root
        anchorX: root.width / 2
        anchorY: root.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: root.toolTipText
        delay: Komai.tooltipDelay
        requestedVisible: root.visible && badgeHover.hovered && root.toolTipText !== ""
    }

    function isHeldModifierToken(token) {
        return altModifierHeld && token.toLowerCase() === "alt";
    }

    function tokenBackgroundColor(token) {
        if (isHeldModifierToken(token))
            return palette.highlight;
        if (highlighted)
            return Qt.tint(palette.dark, Qt.rgba(1, 1, 1, 0.15));
        return Qt.tint(palette.window, Qt.rgba(0, 0, 0, 0.05));
    }

    function tokenBorderColor(token) {
        if (isHeldModifierToken(token))
            return Qt.rgba(palette.brightText.r, palette.brightText.g, palette.brightText.b, 0.5);
        if (highlighted)
            return Qt.rgba(keyTextColor.r, keyTextColor.g, keyTextColor.b, 0.5);
        return palette.mid;
    }

    function tokenTextColor(token) {
        return isHeldModifierToken(token) ? palette.brightText : keyTextColor;
    }

    HoverHandler {
        id: badgeHover
    }

    RowLayout {
        id: contentRow

        anchors.fill: parent
        spacing: Komai.paddingSmall

        Image {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: root.iconSize
            Layout.preferredHeight: root.iconSize
            fillMode: Image.PreserveAspectFit
            source: root.showKeyboardIcon ? "image://colorimage/:/icons/icons/ui/keyboard-shortcut.svg?" + root.keyTextColor : ""
            sourceSize.width: width * Screen.devicePixelRatio
            sourceSize.height: height * Screen.devicePixelRatio
            visible: root.showKeyboardIcon
        }

        Repeater {
            model: root.keyTokens

            delegate: RowLayout {
                required property string modelData
                required property int index

                spacing: Komai.paddingSmall

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: tokenLabel.implicitWidth + Komai.paddingMedium * 1.5
                    implicitHeight: tokenLabel.implicitHeight + Komai.paddingSmall * 1.5
                    radius: 6
                    color: root.tokenBackgroundColor(modelData)
                    border.width: 1
                    border.color: root.tokenBorderColor(modelData)

                    Label {
                        id: tokenLabel

                        anchors.centerIn: parent
                        text: modelData
                        color: root.tokenTextColor(modelData)
                        font.bold: true
                        font.pointSize: Settings.uiFontSizePt * 0.8
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignVCenter
                    text: "+"
                    color: root.keyTextColor
                    visible: index < root.keyTokens.length - 1
                }
            }
        }
    }
}
