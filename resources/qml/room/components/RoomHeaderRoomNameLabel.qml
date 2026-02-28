// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import im.nheko 1.0

Label {
    id: root

    required property string roomName

    Layout.column: 2
    Layout.fillWidth: true
    Layout.row: 1
    color: palette.text
    elide: Text.ElideRight
    font.bold: true
    font.pointSize: Settings.uiFontSizePt * 1.1
    maximumLineCount: 1
    text: roomName
    textFormat: Text.RichText
}
