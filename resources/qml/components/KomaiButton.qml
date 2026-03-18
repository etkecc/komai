// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import cc.etke.komai 1.0

Button {
    id: control

    property int cursor: Qt.PointingHandCursor
    property string toolTipText: ""
    property bool toolTipVisible: hovered && toolTipText.length > 0
    property int toolTipDelay: 0
    property real toolTipAnchorX: width / 2
    readonly property bool activeState: hovered || visualFocus
    readonly property int controlHeight: Math.max(36, Math.round(Settings.uiFontSizePt * 2.7))
    readonly property color normalBackground: palette.alternateBase
    readonly property color hoverBackground: palette.dark
    readonly property color highlightHoverBackground: Qt.darker(palette.highlight, 1.15)
    readonly property color pressedBackground: highlighted
        ? Qt.darker(palette.highlight, 1.12)
        : Qt.darker(palette.dark, 1.08)
    readonly property color disabledBackground: highlighted
        ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.45)
        : Qt.rgba(normalBackground.r, normalBackground.g, normalBackground.b, 0.65)
    readonly property color foregroundColor: !enabled
        ? (highlighted ? Qt.rgba(palette.highlightedText.r, palette.highlightedText.g,
                                  palette.highlightedText.b, 0.6)
                       : palette.buttonText)
        : (highlighted ? palette.highlightedText
                       : ((activeState || down) ? palette.brightText : palette.text))
    readonly property int effectiveIconSize: Math.max(14, Math.round(Settings.uiFontSizePt * 1.4))

    font.pointSize: Settings.uiFontSizePt
    implicitHeight: Math.max(controlHeight,
                             (contentItem ? contentItem.implicitHeight : 0) + topPadding + bottomPadding)
    spacing: Komai.paddingSmall
    padding: Komai.paddingSmall + 2
    leftPadding: Komai.paddingMedium + 2
    rightPadding: Komai.paddingMedium + 2
    icon.color: foregroundColor
    readonly property bool hasIconSource: {
        const src = control.icon.source;
        if (!src)
            return false;
        const raw = (typeof src.toString === "function") ? src.toString() : String(src);
        return raw.length > 0;
    }

    function tintedIconSource(source)
    {
        if (!source)
            return "";

        let resolved = (typeof source.toString === "function") ? source.toString() : String(source);
        if (!resolved || resolved.length === 0)
            return "";

        if (resolved.startsWith("image://"))
            return resolved;
        if (resolved.startsWith("qrc:/"))
            resolved = ":" + resolved.substring(4);
        return "image://colorimage/" + resolved + "?" + foregroundColor;
    }

    TextMetrics {
        id: toolTipMetrics

        font: control.font
        text: control.toolTipText
    }

    KomaiToolTip {
        anchorItem: control
        anchorX: control.toolTipAnchorX
        anchorY: control.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: control.toolTipText
        delay: control.toolTipDelay
        requestedVisible: control.toolTipVisible
        width: Math.min(toolTipMetrics.advanceWidth + leftPadding + rightPadding,
                        (control.Window.window ? control.Window.window.width : 500) * 0.5)
    }

    background: Rectangle {
        color: !control.enabled
            ? control.disabledBackground
            : control.down
                ? control.pressedBackground
                : control.highlighted
                    ? (control.activeState ? control.highlightHoverBackground
                                           : control.palette.highlight)
                    : control.activeState
                        ? control.hoverBackground
                        : control.normalBackground
        radius: Komai.paddingSmall
        border.color: control.activeFocus ? control.palette.highlight : Komai.theme.separator
        border.width: control.activeFocus ? 2 : 1
    }

    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        RowLayout {
            id: contentRow

            anchors.centerIn: parent
            spacing: control.spacing

            Image {
                id: iconImage

                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: control.effectiveIconSize
                Layout.preferredHeight: control.effectiveIconSize
                visible: control.hasIconSource &&
                    control.display !== AbstractButton.TextOnly
                source: visible ? control.tintedIconSource(control.icon.source) : ""
                sourceSize.width: control.effectiveIconSize
                sourceSize.height: control.effectiveIconSize
                fillMode: Image.PreserveAspectFit
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                visible: control.text !== "" &&
                    control.display !== AbstractButton.IconOnly
                text: control.text
                color: control.foregroundColor
                font: control.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: control.enabled ? control.cursor : Qt.ArrowCursor
    }

    HoverHandler {
        onPointChanged: if (hovered)
            control.toolTipAnchorX = point.position.x
    }

}
