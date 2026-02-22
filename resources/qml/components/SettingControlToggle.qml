// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

ToggleButton {
    id: root

    required property bool value

    signal toggled(bool value)

    checked: value
    onClicked: toggled(checked)
}
