// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../../ui"

ScrollView {
    id: widgets

    required property var room
    required property string roomId
    readonly property bool layoutVisible: !!room && room.widgetLinks.length > 0 && !Settings.hiddenWidgets.includes(roomId)

    Layout.column: 1
    Layout.columnSpan: 8
    Layout.fillWidth: true
    Layout.minimumHeight: 0
    Layout.preferredHeight: layoutVisible ? Math.min(contentHeight, Komai.avatarSize * 1.5) : 0
    Layout.maximumHeight: layoutVisible ? Komai.avatarSize * 1.5 : 0
    Layout.row: 4
    ScrollBar.horizontal.visible: false
    clip: true
    visible: layoutVisible
    contentWidth: availableWidth

    ListView {
        model: room ? room.widgetLinks : undefined
        spacing: Komai.paddingSmall

        delegate: MatrixText {
            width: widgets.width
            required property var modelData

            color: palette.text
            text: modelData
        }
    }
}
