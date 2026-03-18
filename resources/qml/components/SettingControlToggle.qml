// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import "../ui"

Item {
    id: root

    required property bool value
    property color textColor: palette.buttonText

    signal toggledValue(bool value)

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    ToggleButton {
        id: button

        enabled: root.enabled
        checked: root.value
        textColor: root.textColor
        onToggled: {
            root.toggledValue(checked);
        }
    }
}
