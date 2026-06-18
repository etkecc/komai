// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// A flat action button for the Element Call bar, styled to match the room header
// action buttons (no border, transparent background that fills on hover, larger
// icon, bold label). Set `danger: true` for the destructive "End call" variant
// (theme error colour) or `accept: true` for the affirmative "Join" variant
// (theme success colour); both fill solid with a luminance-picked readable
// foreground.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import cc.etke.komai

AbstractButton {
    id: button

    property string image: ""
    property bool danger: false
    // The affirmative variant (e.g. "Join"): a solid theme-success fill.
    property bool accept: false
    // Set when the button sits on a coloured (accent) bar, e.g. the green call
    // bars: force a black foreground and a subtle translucent hover so it reads
    // on the fixed light-green background across all themes (the theme palette
    // foreground would be light, and so low-contrast, on dark themes).
    property bool onAccent: false

    readonly property bool activeState: hovered || pressed || visualFocus
    // Match the room-header action buttons: the button is iconSize tall and the
    // glyph is a touch smaller than that (not the full size, which looked
    // oversized).
    readonly property int buttonPaddingH: Komai.paddingSmall
    readonly property int glyphSize: Math.max(14, Komai.iconSize - 2 * buttonPaddingH)

    // Foreground that stays readable on whatever background we draw. For the
    // danger variant we sit on the theme error colour (which varies per theme),
    // so pick black/white by relative luminance; otherwise follow the room-header
    // convention (brightText on hover, text otherwise).
    readonly property color foreground: danger
        ? ((0.299 * Komai.theme.error.r + 0.587 * Komai.theme.error.g
            + 0.114 * Komai.theme.error.b) > 0.55 ? "#000000" : "#ffffff")
        : accept
            ? ((0.299 * Komai.theme.success.r + 0.587 * Komai.theme.success.g
                + 0.114 * Komai.theme.success.b) > 0.55 ? "#000000" : "#ffffff")
            : (onAccent ? "#000000"
                : (activeState ? palette.brightText : palette.text))

    font.pointSize: Settings.uiFontSizePt
    font.bold: true
    hoverEnabled: true
    opacity: enabled ? 1 : 0.5

    Layout.alignment: Qt.AlignVCenter
    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: 0
    bottomPadding: 0
    implicitHeight: Komai.iconSize
    implicitWidth: contentRow.implicitWidth + leftPadding + rightPadding

    background: Rectangle {
        radius: Komai.paddingSmall
        color: button.danger
            ? (button.activeState ? Qt.darker(Komai.theme.error, 1.15) : Komai.theme.error)
            : button.accept
                ? (button.activeState ? Qt.darker(Komai.theme.success, 1.15) : Komai.theme.success)
                : button.onAccent
                    ? (button.activeState ? Qt.rgba(0, 0, 0, 0.12) : "transparent")
                    : (button.activeState ? palette.dark : "transparent")
    }

    contentItem: RowLayout {
        id: contentRow

        anchors.fill: parent
        anchors.leftMargin: button.leftPadding
        anchors.rightMargin: button.rightPadding
        spacing: Komai.paddingSmall

        Image {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: button.glyphSize
            Layout.preferredWidth: button.glyphSize
            visible: button.image !== ""
            source: button.image !== ""
                ? ("image://colorimage/" + button.image + "?" + button.foreground)
                : ""
            sourceSize.height: button.glyphSize
            sourceSize.width: button.glyphSize
            fillMode: Image.PreserveAspectFit
        }

        Label {
            Layout.alignment: Qt.AlignVCenter
            visible: button.text !== ""
            text: button.text
            color: button.foreground
            font: button.font
        }
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: button.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
