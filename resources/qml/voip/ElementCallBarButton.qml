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

    // Mutually exclusive visual variants. Neutral, OnAccent and OnDark are all
    // "no fill, fills on hover" buttons; they differ only in the foreground/hover
    // colours used against a normal, a coloured (accent), or a dark translucent
    // background. OnDark is for the fullscreen OSD (light icons over the video).
    enum Style { Neutral, Accept, Danger, OnAccent, OnDark }

    property string image: ""
    // Optional themed tooltip, shown on hover (e.g. for icon-only buttons).
    property string toolTipText: ""
    // For buttons whose caption flips with state (e.g. "Mute"/"Unmute"): the
    // other state's caption. The label reserves the width of the wider of the
    // two so the caption can change without the button (and its neighbours)
    // shifting around.
    property string altText: ""
    // One of ElementCallBarButton.Style.*. OnAccent forces a black foreground and
    // a subtle translucent hover so a neutral button reads on the fixed
    // light-green call bars across all themes (the theme palette foreground would
    // be light, and so low-contrast, on dark themes).
    property int style: ElementCallBarButton.Style.Neutral

    readonly property bool _danger: style === ElementCallBarButton.Style.Danger
    readonly property bool _accept: style === ElementCallBarButton.Style.Accept
    readonly property bool _onAccent: style === ElementCallBarButton.Style.OnAccent
    readonly property bool _onDark: style === ElementCallBarButton.Style.OnDark

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
            : _onDark ? "#ffffff"
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
                : button._onDark
                    ? (button.activeState ? Qt.rgba(1, 1, 1, 0.18) : "transparent")
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
            // Reserve the wider of the current and alternate caption so a caption
            // that flips with state does not resize the button. Centre the text so
            // both captions sit consistently within the reserved width.
            Layout.preferredWidth: button.altText.length > 0
                ? Math.max(metricsText.advanceWidth, metricsAlt.advanceWidth)
                : implicitWidth
            horizontalAlignment: Text.AlignHCenter
            visible: button.text !== ""
            text: button.text
            color: button.foreground
            font: button.font
        }
    }

    TextMetrics {
        id: metricsText
        font: button.font
        text: button.text
    }

    TextMetrics {
        id: metricsAlt
        font: button.font
        text: button.altText
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: button.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    KomaiToolTip {
        anchorItem: button
        text: button.toolTipText
        delay: 0
        // Only for icon-only buttons; a captioned button needs no tooltip.
        requestedVisible: button.hovered && button.toolTipText.length > 0
            && button.text.length === 0
    }
}
