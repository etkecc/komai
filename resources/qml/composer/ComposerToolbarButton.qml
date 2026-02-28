// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ImageButton {
    Layout.margins: 8
    Layout.preferredHeight: 32
    Layout.preferredWidth: 32
    hoverEnabled: true
    ToolTip.visible: hovered
}
