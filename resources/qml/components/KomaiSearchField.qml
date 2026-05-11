// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window
import cc.etke.komai

// A KomaiTextField with an inline "clear" button on its trailing edge,
// styled like the KomaiSpinBox up/down indicators: a distinct cell that
// sits inside the field's frame, separated from the text area by a thin
// line. The button only appears while the field holds text.
//
// LTR: the cell sits on the right; RTL: on the left. The cell and its
// separator are positioned with explicit x values rather than anchors so
// they stay correct regardless of any inherited LayoutMirroring.
KomaiTextField {
    id: control

    // Emitted right after the field is emptied via clear() (the inline
    // button or the Escape key).
    signal cleared()

    // Empties the field if it holds text, emitting cleared(). Returns true
    // when it actually cleared something, false when the field was already
    // empty — handy for deciding whether to swallow an Escape key press.
    function clear() {
        if (text.length === 0)
            return false;
        text = "";
        cleared();
        return true;
    }

    // Escape clears the query; once the field is empty it lets the key
    // propagate (so an outer handler can, say, close the dialog).
    Keys.onEscapePressed: function (event) {
        event.accepted = control.clear();
    }

    readonly property bool _rtl: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
    readonly property int _clearButtonWidth: 22 + Komai.paddingSmall * 2
    readonly property bool _clearButtonShown: enabled && text.length > 0
    // The trailing-edge slot is reserved unconditionally so typed text
    // never slides under the button and the field width doesn't jump when
    // the button appears (mirrors how KomaiSpinBox reserves room for its
    // +/- indicators).
    readonly property int _trailingPadding: _clearButtonWidth + Komai.paddingSmall
    readonly property int _normalPadding: Komai.paddingMedium + 2

    leftPadding: _rtl ? _trailingPadding : _normalPadding
    rightPadding: _rtl ? _normalPadding : _trailingPadding

    // Honour the layout direction for the placeholder and typed text.
    // Without an explicit alignment an empty field shows its (Latin)
    // placeholder left-flush even in an RTL UI; setting it explicitly lets
    // LayoutMirroring flip the *effective* alignment to the right.
    horizontalAlignment: Text.AlignLeft

    Rectangle {
        id: clearButton

        readonly property int outerBorderWidth: control.activeFocus ? 2 : 1
        // Match the field frame's corner radius (KomaiTextField uses
        // Komai.paddingSmall) so the cell's trailing corners nest cleanly
        // inside it instead of poking past the rounded border.
        readonly property real frameRadius: Komai.paddingSmall
        // Hover/press lift, matching RoomHeaderActionButton's active state.
        readonly property bool active: clearMouseArea.containsMouse || clearMouseArea.pressed

        visible: control._clearButtonShown
        x: control._rtl
            ? outerBorderWidth
            : control.width - outerBorderWidth - width
        y: outerBorderWidth
        width: control._clearButtonWidth
        height: Math.max(1, control.height - outerBorderWidth * 2)
        radius: frameRadius
        color: active ? control.palette.dark : control.palette.alternateBase

        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Clear")
        Accessible.onPressAction: {
            control.clear();
            control.forceActiveFocus();
        }

        KomaiToolTip {
            anchorItem: clearButton
            text: qsTr("Clear")
            requestedVisible: clearMouseArea.containsMouse
        }

        // radius rounds all four corners, but only the trailing pair
        // should stay rounded; square off the leading corners (the side
        // meeting the text area) by overpainting that strip. This keeps
        // the result working on the Qt 6.5 floor — per-corner Rectangle
        // radii would need Qt 6.7.
        Rectangle {
            x: control._rtl ? parent.width - width : 0
            y: 0
            width: parent.radius
            height: parent.height
            color: parent.color
        }

        // Separator on the side facing the text area: the cell's left edge
        // in LTR, its right edge in RTL. Drawn on top of the squaring strip.
        Rectangle {
            x: control._rtl ? parent.width - width : 0
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: Komai.theme.separator
        }

        Image {
            anchors.centerIn: parent
            width: 14
            height: 14
            sourceSize.width: width * Screen.devicePixelRatio
            sourceSize.height: height * Screen.devicePixelRatio
            fillMode: Image.PreserveAspectFit
            source: "image://colorimage/:/icons/icons/ui/dismiss.svg?"
                    + (clearButton.active ? control.palette.brightText
                       : control.enabled ? control.palette.text
                       : control.palette.buttonText)
        }

        MouseArea {
            id: clearMouseArea

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                control.clear();
                control.forceActiveFocus();
            }
        }
    }
}
