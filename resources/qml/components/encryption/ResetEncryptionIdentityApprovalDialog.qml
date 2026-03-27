// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".." as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property bool completionHandled: false
    property string approvalUrl: ""
    signal continueRequested()
    signal cancelled()

    function finishApproval()
    {
        completionHandled = true;
        continueRequested();
        close();
    }

    title: qsTr("Approve identity reset")
    titleIcon: ":/icons/icons/ui/refresh.svg"
    titleIconColor: Komai.theme.warning
    onOpened: completionHandled = false
    onClosed: {
        if (!completionHandled)
            cancelled();
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        text: qsTr("Your server requires approval in the browser before it will reset this device's encryption identity.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    Label {
        Layout.fillWidth: true
        color: palette.buttonText
        text: root.approvalUrl
        textFormat: Text.PlainText
        visible: root.approvalUrl.length > 0
        wrapMode: Text.WrapAnywhere
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        }

        Components.KomaiButton {
            text: qsTr("Open approval page")
            visible: root.approvalUrl.length > 0
            onClicked: Komai.openLink(root.approvalUrl)
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            icon.source: "qrc:/icons/icons/ui/refresh.svg"
            text: qsTr("I've approved it")
            highlighted: true

            onClicked: root.finishApproval()
        }
    }
}
