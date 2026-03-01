// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    spacing: 16

    Image {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: Komai.timelineLogoSize
        Layout.preferredHeight: Komai.timelineLogoSize
        source: "qrc:/logos/komai.svg"
        sourceSize.height: Komai.timelineLogoSize * 2
        sourceSize.width: Komai.timelineLogoSize * 2
        fillMode: Image.PreserveAspectFit
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        font.pointSize: Settings.uiFontSizePt * 1.85
        text: qsTr("No room open")
        color: palette.text
    }
}
