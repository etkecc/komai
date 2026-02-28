// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ImageButton {
    required property var topBarRef
    required property int column
    property int row: 1

    Layout.alignment: Qt.AlignVCenter
    Layout.column: column
    Layout.preferredHeight: topBarRef.topBarAvatarSize
    Layout.preferredWidth: topBarRef.topBarAvatarSize
    Layout.row: row
    leftPadding: topBarRef.buttonPaddingH
    rightPadding: topBarRef.buttonPaddingH
    topPadding: topBarRef.buttonPaddingV
    bottomPadding: topBarRef.buttonPaddingV
    ToolTip.visible: hovered
}
