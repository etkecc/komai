// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Item {
    id: root

    property string fallbackText: ""
    property string senderName: ""
    property string body: ""
    property real elideWidth: width
    property color textColor: palette.buttonText
    property real fontPointSize: Settings.uiFontSizePt * 0.95

    readonly property bool mirrored: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
    readonly property bool structured: senderName.length > 0 && body.length > 0
    readonly property string displayText: structured
        ? (mirrored ? body + " :" + senderName : senderName + ": " + body)
        : fallbackText

    baselineOffset: previewLabel.y + previewLabel.baselineOffset
    clip: true
    implicitHeight: previewLabel.implicitHeight

    Label {
        id: previewLabel

        anchors.fill: parent
        color: root.textColor
        elide: root.mirrored ? Text.ElideLeft : Text.ElideRight
        font.pointSize: root.fontPointSize
        horizontalAlignment: root.mirrored ? Text.AlignRight : Text.AlignLeft
        LayoutMirroring.enabled: false
        maximumLineCount: 1
        text: metrics.elidedText
        textFormat: Text.PlainText
    }
    TextMetrics {
        id: metrics

        elide: root.mirrored ? Text.ElideLeft : Text.ElideRight
        elideWidth: root.elideWidth
        font: previewLabel.font
        text: root.displayText
    }
}
