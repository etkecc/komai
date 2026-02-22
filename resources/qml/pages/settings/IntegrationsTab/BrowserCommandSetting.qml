// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Item {
    id: root

    Layout.fillWidth: true
    implicitHeight: commandSection.implicitHeight
    implicitWidth: commandSection.implicitWidth

    ColumnLayout {
        id: commandSection
        Layout.fillWidth: true
        width: parent.width
        spacing: Nheko.paddingSmall

        Label {
            Layout.fillWidth: true
            color: palette.text
            text: qsTr("Link browser command")
            textFormat: Text.AutoText
            font.pointSize: 1.1 * fontMetrics.font.pointSize
            font.weight: Font.DemiBold
        }

        Label {
            Layout.fillWidth: true
            color: palette.buttonText
            text: qsTr("Use this command to launch links; use %u where the link URL should be inserted.")
            font.pointSize: 0.9 * fontMetrics.font.pointSize
            wrapMode: Text.Wrap
        }

        TextField {
            id: browserCommandTextField
            Layout.fillWidth: true
            text: Settings.integrationsLinksBrowserCommand ? Settings.integrationsLinksBrowserCommand : ""
            selectByMouse: true
            wrapMode: TextInput.NoWrap
            function applyCommand()
            {
                Settings.integrationsLinksBrowserCommand = browserCommandTextField.text.trim();
            }
            onEditingFinished: applyCommand()
            onAccepted: applyCommand()
            onActiveFocusChanged: if (!activeFocus) applyCommand()
            Component.onDestruction: applyCommand()
            placeholderText: qsTr("brave --profile-directory=\"Profile 7\" %u")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Use %u for the URL, or leave empty to use the default browser.")
            ToolTip.delay: Nheko.tooltipDelay
        }
    }
}
