// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

AbstractButton {
    id: control

    property int cursor: Qt.PointingHandCursor
    property string labelText: ""
    property string displayLabelText: labelText
    property string iconSource: ""
    property string displayIconSource: iconSource
    property bool mirrorIcon: false
    property bool displayMirrorIcon: mirrorIcon
    property Component trailingContent: null
    property int actionIconSize: 24
    readonly property bool activeState: hovered || visualFocus
    readonly property int controlHeight: Math.max(40, Math.round(Settings.uiFontSizePt * 2.9))
    readonly property color normalBackground: palette.window
    readonly property color hoverBackground: palette.dark
    readonly property color pressedBackground: Qt.darker(palette.dark, 1.08)
    readonly property color disabledBackground: Qt.rgba(normalBackground.r,
                                                        normalBackground.g,
                                                        normalBackground.b,
                                                        0.65)
    readonly property color foregroundColor: !enabled
        ? palette.buttonText
        : ((activeState || down) ? palette.brightText : palette.text)

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

    hoverEnabled: true
    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    font.pointSize: Settings.uiFontSizePt
    spacing: Komai.paddingSmall
    padding: Komai.paddingSmall + 2
    leftPadding: Komai.paddingMedium + 2
    rightPadding: Komai.paddingMedium + 2
    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    implicitWidth: leftPadding + rightPadding + contentRow.implicitWidth
    implicitHeight: Math.max(controlHeight,
                             contentRow.implicitHeight + topPadding + bottomPadding)

    background: Rectangle {
        color: !control.enabled
            ? control.disabledBackground
            : control.down
                ? control.pressedBackground
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

            anchors.fill: parent
            spacing: control.spacing

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: control.actionIconSize
                Layout.preferredHeight: control.actionIconSize
                visible: control.displayIconSource.length > 0
                source: visible ? control.tintedIconSource(control.displayIconSource) : ""
                sourceSize.width: control.actionIconSize
                sourceSize.height: control.actionIconSize
                fillMode: Image.PreserveAspectFit
                smooth: true

                transform: Scale {
                    origin.x: control.actionIconSize / 2
                    xScale: control.displayMirrorIcon ? -1 : 1
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                Layout.minimumWidth: 0
                text: control.displayLabelText
                color: control.foregroundColor
                font.family: control.font.family
                font.pointSize: control.font.pointSize
                font.bold: true
                elide: Text.ElideRight
            }

            Loader {
                Layout.alignment: Qt.AlignVCenter
                active: control.trailingContent !== null
                sourceComponent: control.trailingContent
            }
        }
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: control.enabled ? control.cursor : Qt.ArrowCursor
    }
}
