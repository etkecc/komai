// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../ui/"

Item {
    id: root

    readonly property string headline: MainWindow.startupHeadline
    readonly property string detail: MainWindow.startupDetail

    Rectangle {
        anchors.fill: parent
        color: palette.window
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - Komai.paddingLarge * 2, 560)
        height: Math.min(parent.height - Komai.paddingLarge * 2, 360)
        radius: Komai.paddingLarge * 3
        color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.06)
        border.width: 1
        border.color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.12)
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Komai.paddingLarge * 4, 520)
        spacing: Komai.paddingLarge

        Spinner {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: Math.max(Komai.timelineLogoSize, 120)
            Layout.preferredWidth: Layout.preferredHeight
            foreground: palette.highlight
        }

        Label {
            Layout.fillWidth: true
            color: palette.text
            font.pointSize: Settings.uiFontSizePt * 1.7
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            text: root.headline
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 1.05
            horizontalAlignment: Text.AlignHCenter
            text: root.detail
            wrapMode: Text.Wrap
        }
    }
}
