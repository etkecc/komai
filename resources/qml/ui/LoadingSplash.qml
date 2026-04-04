// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    property alias headline: headlineLabel.text
    property alias detail: detailLabel.text
    property bool spinning: true

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
            running: root.spinning
        }

        Label {
            id: headlineLabel

            Layout.fillWidth: true
            color: palette.text
            font.pointSize: Settings.uiFontSizePt * 1.7
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: text.length > 0
        }

        Label {
            id: detailLabel

            Layout.fillWidth: true
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 1.05
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: text.length > 0
        }
    }
}
