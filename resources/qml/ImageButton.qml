// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "./ui"
import QtQuick
import QtQuick.Controls
import im.nheko // for cursor shape

AbstractButton {
    id: button

    property color buttonTextColor: palette.buttonText
    property bool changeColorOnHover: true
    property int cursor: Qt.PointingHandCursor
    property color highlightColor: palette.highlight
    property string image: undefined
    property bool ripple: true
    property bool hoverPulse: false

    focusPolicy: Qt.NoFocus
    height: 16
    width: 16

    Image {
        id: buttonImg

        // Workaround, can't get icon.source working for now...
        anchors.fill: parent
        anchors.leftMargin: button.leftPadding
        anchors.rightMargin: button.rightPadding
        anchors.topMargin: button.topPadding
        anchors.bottomMargin: button.bottomPadding
        fillMode: Image.PreserveAspectFit
        source: button.image != "" ? ("image://colorimage/" + button.image + "?" + ((button.hovered && button.changeColorOnHover) ? button.highlightColor : button.buttonTextColor)) : ""
        sourceSize.height: buttonImg.height
        sourceSize.width: buttonImg.width
    }
    NhekoCursorShape {
        id: mouseArea

        anchors.fill: parent
        cursorShape: button.cursor
    }
    Ripple {
        color: Qt.rgba(button.buttonTextColor.r, button.buttonTextColor.g, button.buttonTextColor.b, 0.5)
        enabled: button.ripple
    }
    SequentialAnimation {
        id: hoverPulseAnim

        NumberAnimation { target: button; property: "scale"; from: 1.0; to: 1.2; duration: 150; easing.type: Easing.OutQuad }
        NumberAnimation { target: button; property: "scale"; from: 1.2; to: 1.0; duration: 150; easing.type: Easing.InQuad }
    }
    onHoveredChanged: {
        if (hovered && hoverPulse && Settings.uiMotionAnimationsEnabled)
            hoverPulseAnim.start();
    }
}
