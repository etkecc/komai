// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai
import "../ui/"

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: palette.window
    }

    LoadingSplash {
        anchors.fill: parent
        headline: MainWindow.startupHeadline
        detail: MainWindow.startupDetail
    }
}
