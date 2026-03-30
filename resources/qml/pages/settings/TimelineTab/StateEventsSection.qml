// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../../../components" as Components
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    implicitHeight: content.implicitHeight
    width: parent ? parent.width : 0

    ColumnLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Komai.paddingSmall

        Components.SettingsSection {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingLarge
            Layout.bottomMargin: Komai.paddingSmall
            label: qsTr("Additional events")
            helperText: Settings.hasActiveSession
                ? qsTr("If you're feeling overwhelmed, consider disabling some of these noisy events here globally, or per-room (in Room Settings).")
                : qsTr("Available after you sign in.")
        }

        Components.HiddenEventsSettingsContent {
            Layout.fillWidth: true
            roomId: ""
            autoSave: true
            togglesEnabled: Settings.hasActiveSession
        }
    }
}
