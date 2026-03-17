// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Dialog {
    default property alias inner: scroll.data
    property int useableWidth: scroll.width - scroll.ScrollBar.vertical.width
    readonly property bool darkDialogChrome: palette.window.hslLightness < 0.5
    readonly property color modalOverlayColor: Qt.rgba(0, 0, 0, darkDialogChrome ? 0.76 : 0.68)
    readonly property color dialogOutlineColor: Qt.tint(
        Komai.theme.separator,
        Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, darkDialogChrome ? 0.22 : 0.32))

    parent: Overlay.overlay
    anchors.centerIn: parent
    height: (Math.floor(parent.height / 2) - Komai.paddingLarge) * 2
    width: (Math.floor(parent.width / 2) - Komai.paddingLarge) * 2
    padding: 0
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    closePolicy: Popup.NoAutoClose

    Overlay.modal: Rectangle {
        color: modalOverlayColor
    }

    contentChildren: [
        ScrollView {
            id: scroll

            clip: true
            anchors.fill: parent
            ScrollBar.horizontal.visible: false
            ScrollBar.vertical.visible: true
        }
    ]

    background: Rectangle {
        color: palette.window
        border.color: dialogOutlineColor
        border.width: 2
        radius: Komai.paddingSmall
    }

}
