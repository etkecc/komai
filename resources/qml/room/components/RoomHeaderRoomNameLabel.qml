// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0

Label {
    id: root

    required property string roomName

    Layout.column: 2
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    Layout.preferredWidth: 0
    Layout.row: 1
    clip: true
    color: palette.text
    elide: Text.ElideRight
    font.bold: true
    font.pointSize: Settings.uiFontSizePt * 1.1
    maximumLineCount: 2
    text: roomName
    textFormat: Text.RichText
    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
}
