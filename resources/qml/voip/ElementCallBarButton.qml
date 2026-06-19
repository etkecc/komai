// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// A flat action button for the Element Call bar, styled to match the room header
// action buttons (no border, transparent background that fills on hover, larger
// icon, bold label). The `style` property selects one of four mutually exclusive
// looks (see the Style enum): Neutral (the default), Accept (affirmative "Join",
// solid theme-success fill), Danger (destructive "End call", solid theme-error
// fill), and OnAccent (a neutral button sitting on a coloured/accent bar). Accept
// and Danger pick a black/white foreground by luminance for cross-theme contrast.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import cc.etke.komai

AbstractButton {
    id: button

    // Mutually exclusive visual variants. Neutral and OnAccent are both "no fill,
    // fills on hover" buttons; they differ only in the foreground/hover colours
    // used against a normal vs a coloured (accent) background.
    enum Style { Neutral, Accept, Danger, OnAccent }

    property string image: ""
    // Optional themed tooltip, shown on hover (e.g. for icon-only buttons).
    property string toolTipText: ""
    // One of ElementCallBarButton.Style.*. OnAccent forces a black foreground and
    // a subtle translucent hover so a neutral button reads on the fixed
    // light-green call bars across all themes (the theme palette foreground would
    // be light, and so low-contrast, on dark themes).
    property int style: ElementCallBarButton.Style.Neutral

    readonly property bool _danger: style === ElementCallBarButton.Style.Danger
    readonly property bool _accept: style === ElementCallBarButton.Style.Accept
    readonly property bool _onAccent: style === ElementCallBarButton.Style.OnAccent

    readonly property bool activeState: hovered || pressed || visualFocus
    // Match the room-header action buttons exactly: the button is iconSize tall
    // and the glyph is inset by the same density-aware padding RoomHeader uses
    // for its action buttons (wider inset in Spacious), so the glyph ends up the
    // same size as the header icons across all densities rather than oversized.
    readonly property int buttonPaddingH: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall : Komai.paddingMedium
    readonly property int glyphSize: Math.max(14, Komai.iconSize - 2 * buttonPaddingH)

    // Foreground that stays readable on whatever background we draw. For the
    // danger variant we sit on the theme error colour (which varies per theme),
    // so pick black/white by relative luminance; otherwise follow the room-header
    // convention (brightText on hover, text otherwise).
    readonly property color foreground: _danger
        ? ((0.299 * Komai.theme.error.r + 0.587 * Komai.theme.error.g
            + 0.114 * Komai.theme.error.b) > 0.55 ? "#000000" : "#ffffff")
        : _accept
            ? ((0.299 * Komai.theme.success.r + 0.587 * Komai.theme.success.g
                + 0.114 * Komai.theme.success.b) > 0.55 ? "#000000" : "#ffffff")
            : (_onAccent ? "#000000"
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
        color: button._danger
            ? (button.activeState ? Qt.darker(Komai.theme.error, 1.15) : Komai.theme.error)
            : button._accept
                ? (button.activeState ? Qt.darker(Komai.theme.success, 1.15) : Komai.theme.success)
                : button._onAccent
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

    KomaiToolTip {
        anchorItem: button
        text: button.toolTipText
        delay: 0
        requestedVisible: button.hovered && button.toolTipText.length > 0
    }
}
