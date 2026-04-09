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
    property var previewData: ({})
    property var roomModelOverride: null

    required property string eventId
    property var room: null
    readonly property var effectiveRoomContext: room
        ? room
        : (roomModelOverride || ((previewData && previewData.room) ? previewData.room : null))
    readonly property string previewFirstLine: String((previewData && previewData.redactedFirst) || "")
    readonly property string previewSecondLine: String((previewData && previewData.redactedSecond) || "")
    readonly property var redactedPair: {
        if (previewFirstLine.length > 0 || previewSecondLine.length > 0) {
            return {
                "first": previewFirstLine,
                "second": previewSecondLine
            };
        }

        if (effectiveRoomContext && typeof effectiveRoomContext.formatRedactedEvent === "function")
            return effectiveRoomContext.formatRedactedEvent(msgRoot.eventId);

        return {
            "first": qsTr("Deleted message"),
            "second": ""
        };
    }

    contentItem: RowLayout {
        id: redactedLayout
        spacing: Komai.paddingSmall

        Image {
            id: trashImg
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            Layout.preferredWidth: fontMetrics.font.pixelSize
            Layout.preferredHeight: fontMetrics.font.pixelSize
            sourceSize.width: fontMetrics.font.pixelSize
            sourceSize.height: fontMetrics.font.pixelSize
            source: "image://colorimage/:/icons/icons/ui/delete.svg?" + palette.text
        }
        Label {
            id: redactedLabel
            Layout.margins: 0
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            Layout.maximumWidth: implicitWidth + 1
            Layout.fillWidth: true
            text: msgRoot.redactedPair["first"]
            color: palette.text
            wrapMode: Label.WordWrap

            HoverHandler {
                id: hh
            }

            KomaiToolTip {
                anchorItem: redactedLabel
                anchorX: redactedLabel.width / 2
                anchorY: 0
                text: msgRoot.redactedPair["second"]
                requestedVisible: hh.hovered && msgRoot.redactedPair["second"].length > 0
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
