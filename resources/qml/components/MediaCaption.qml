// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

// Selectable caption text for media events (m.image / m.video / m.audio /
// m.file). Renders `formattedBody` as HTML when present, otherwise falls back
// to the plain `body`. Hovering a link shows a Komai-styled tooltip with the
// puny-link form, mirroring the TextMessage caption pattern. Owners set
// width / Layout.fillWidth themselves; this component is invisible while it
// has no content.
TextEdit {
    id: caption

    property string body: ""
    property string formattedBody: ""

    readonly property bool useFormattedCaption: formattedBody.length > 0
    readonly property bool hasContent: useFormattedCaption || body.length > 0

    property point hoverPoint: Qt.point(0, 0)

    visible: hasContent
    readOnly: true
    selectByMouse: true
    selectionColor: palette.highlight
    selectedTextColor: palette.highlightedText
    wrapMode: TextEdit.Wrap
    textFormat: useFormattedCaption ? TextEdit.RichText : TextEdit.PlainText
    text: useFormattedCaption ? formattedBody : body
    color: palette.text

    onLinkActivated: (link) => Komai.openLink(link)

    HoverHandler {
        cursorShape: caption.hoveredLink.length > 0
            ? Qt.PointingHandCursor
            : Qt.IBeamCursor
        onPointChanged: if (hovered)
            caption.hoverPoint = Qt.point(point.position.x, point.position.y)
    }

    Loader {
        active: caption.hoveredLink.length > 0
        sourceComponent: Component {
            Item {
                TextMetrics {
                    id: linkMetrics
                    text: Komai.punyLink(caption.hoveredLink)
                }
                KomaiToolTip {
                    anchorItem: caption
                    anchorX: caption.hoverPoint.x
                    anchorY: caption.hoverPoint.y
                    gapX: Komai.paddingMedium
                    gapY: Komai.paddingMedium
                    text: linkMetrics.text
                    requestedVisible: caption.hoveredLink.length > 0
                    width: Math.min(linkMetrics.advanceWidth + leftPadding + rightPadding, 500)
                }
            }
        }
    }
}
