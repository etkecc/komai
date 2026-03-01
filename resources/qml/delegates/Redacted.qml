// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Control {
    id: msgRoot

    property int metadataWidth: 0
    property bool fitsMetadata: false //parent.width - redactedLayout.width > metadataWidth + 4

    required property string eventId
    required property Room room

    contentItem: RowLayout {
        id: redactedLayout
        spacing: Komai.paddingSmall

        Image {
            id: trashImg
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            Layout.preferredWidth: fontMetrics.font.pixelSize
            Layout.preferredHeight: fontMetrics.font.pixelSize
            source: "image://colorimage/:/icons/icons/ui/delete.svg?" + palette.text
        }
        Label {
            id: redactedLabel
            Layout.margins: 0
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            Layout.maximumWidth: implicitWidth + 1
            Layout.fillWidth: true
            property var redactedPair: msgRoot.room.formatRedactedEvent(msgRoot.eventId)
            text: redactedPair["first"]
            color: palette.text
            wrapMode: Label.WordWrap

            ToolTip.text: redactedPair["second"]
            ToolTip.visible: hh.hovered
            HoverHandler {
                id: hh
            }
        }
    }

    padding: Komai.paddingSmall

    Layout.maximumWidth: redactedLayout.Layout.maximumWidth + padding * 2

    background: Rectangle {
        color: palette.alternateBase
        radius: fontMetrics.lineSpacing / 2 + 2 * Komai.paddingSmall
    }
}
