// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    spacing: Komai.paddingLarge

    RowLayout {
        id: linksRow

        Layout.alignment: Qt.AlignHCenter
        spacing: Komai.paddingMedium

        KomaiButton {
            id: issueButton

            Layout.preferredWidth: linksRow.linksUniformWidth
            text: qsTr("Report an issue")
            icon.source: "qrc:/icons/icons/ui/bug.svg"
            onClicked: Qt.openUrlExternally("https://github.com/etkecc/komai/issues")
        }

        KomaiButton {
            id: supportButton

            readonly property string heartIcon: Settings.sponsoringStatus === "sponsoring"
                ? "qrc:/icons/icons/ui/heart-filled.svg" : "qrc:/icons/icons/ui/heart.svg"

            Layout.preferredWidth: linksRow.linksUniformWidth
            visible: Settings.sponsoringStatus !== "hidden"
            text: Settings.sponsoringStatus === "sponsoring" ? qsTr("Sponsoring!") : qsTr("Sponsor")
            icon.source: "image://colorimage/:" + heartIcon.substring(4) + "?" + Komai.theme.error
            onClicked: supportDialog.open()
        }

        readonly property real linksUniformWidth: {
            const pad = issueButton.leftPadding + issueButton.rightPadding;
            const issueW = issueButton.contentItem.implicitWidth + pad;
            const supportW = supportButton.visible && supportButton.contentItem
                ? supportButton.contentItem.implicitWidth + pad
                : 0;
            return Math.max(issueW, supportW);
        }
    }

    Components.SupportDialog {
        id: supportDialog
    }
}
