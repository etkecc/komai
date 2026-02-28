// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

ImageButton {
    id: root

    required property int buttonSize
    required property string toolTipText
    required property string iconSource
    property bool rippleEnabled: true

    ToolTip.delay: Nheko.tooltipDelay
    ToolTip.text: toolTipText
    ToolTip.visible: hovered
    Layout.preferredHeight: buttonSize
    Layout.preferredWidth: buttonSize
    hoverEnabled: true
    image: iconSource
    ripple: rippleEnabled
}
