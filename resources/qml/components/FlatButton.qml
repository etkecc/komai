// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import im.nheko

// FIXME(Nico): Don't use hardcoded colors.
Button {
    id: control

    property bool compact: false
    property string iconImage: ""
    property real sizeScale: compact ? 1.3 : 1.5
    property real heightScale: compact ? 1.55 : 1.70

    implicitHeight: Math.ceil(control.contentItem.implicitHeight * control.heightScale)
    implicitWidth: Math.ceil(control.contentItem.implicitWidth + control.contentItem.implicitHeight)
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.6

    MultiEffect {
        anchors.fill: control.background
        shadowHorizontalOffset: 3
        shadowVerticalOffset: 3
        shadowBlur: 8
        shadowEnabled: control.enabled
        shadowColor: "#80000000"
        source: control.background
    }

    contentItem: RowLayout {
        spacing: 0
        anchors.centerIn: parent
        Image {
            Layout.leftMargin: Nheko.paddingMedium
            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            Layout.preferredHeight: fontMetrics.font.pixelSize * control.sizeScale
            Layout.preferredWidth:  fontMetrics.font.pixelSize * control.sizeScale
            visible: !!iconImage
            source: iconImage
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: control.text
            //font: control.font
            font.capitalization: Font.AllUppercase
            font.pointSize: Math.ceil(Settings.uiFontSizePt * control.sizeScale)
            //font.capitalization: Font.AllUppercase
            color: control.enabled ? palette.light : palette.buttonText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        //height: control.contentItem.implicitHeight * 2
        //width: control.contentItem.implicitWidth * 2
        radius: height / 8
        color: control.enabled ? Qt.lighter(palette.dark, control.down ? 1.4 : (control.hovered ? 1.2 : 1)) : palette.mid
    }

}
