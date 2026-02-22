// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
// qmllint disable unused-imports
import QtQuick.Controls as Controls
// qmllint enable unused-imports

Item {
    id: root

    required property bool value

    signal toggledValue(bool value)

    onEnabledChanged: button.enabled = enabled

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    Controls.ToggleButton {
        id: button

        checked: root.value
        onToggled: {
            root.value = checked;
            root.toggledValue(checked);
        }
    }

    Component.onCompleted: button.enabled = enabled
}
