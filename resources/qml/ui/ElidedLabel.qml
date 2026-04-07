// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai 1.0

Label {
    id: root

    property alias elideWidth: metrics.elideWidth
    property alias fullText: metrics.text
    property int fullTextWidth: Math.ceil(metrics.advanceWidth)

    color: palette.text
    elide: Text.ElideRight
    maximumLineCount: 1
    text: (textFormat == Text.PlainText) ? metrics.elidedText : TimelineManager.escapeEmoji(metrics.elidedText)
    textFormat: Text.PlainText

    TextMetrics {
        id: metrics

        elide: Text.ElideRight
        font: root.font
    }
}
