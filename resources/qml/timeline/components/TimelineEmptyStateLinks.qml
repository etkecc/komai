// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    readonly property string komaiProjectLink: "<a href=\"https://github.com/etkecc/komai\">Komai</a>"
    readonly property string etkeProjectLink: "<a href=\"https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=empty_state\">etke.cc</a>"

    spacing: Komai.paddingLarge

    RowLayout {
        id: linksRow

        Layout.alignment: Qt.AlignHCenter
        spacing: Komai.paddingMedium

        KomaiButton {
            id: issueButton

            Layout.preferredWidth: linksRow.linksUniformWidth
            text: qsTr("Report an issue")
            icon.source: "qrc:/icons/icons/ui/alert.svg"
            onClicked: Qt.openUrlExternally("https://github.com/etkecc/komai/issues")
        }

        KomaiButton {
            id: donateButton

            Layout.preferredWidth: linksRow.linksUniformWidth
            visible: Settings.donationStatus !== "hidden"
            text: Settings.donationStatus === "sponsoring" ? qsTr("Sponsoring!") : qsTr("Donate")
            icon.source: Settings.donationStatus === "sponsoring"
                ? "qrc:/icons/icons/ui/heart-color.svg"
                : "qrc:/icons/icons/ui/heart.svg"
            onClicked: {
                if (Settings.donationStatus === "sponsoring")
                    sponsoringMenu.popup(donateButton);
                else
                    donateMenu.popup(donateButton);
            }
        }

        readonly property real linksUniformWidth: {
            const pad = issueButton.leftPadding + issueButton.rightPadding;
            const issueW = issueButton.contentItem.implicitWidth + pad;
            const donateW = donateButton.visible && donateButton.contentItem
                ? donateButton.contentItem.implicitWidth + pad
                : 0;
            return Math.max(issueW, donateW);
        }
    }

    Menu {
        id: sponsoringMenu

        MenuItem {
            text: qsTr("GitHub Sponsors")
            onTriggered: Qt.openUrlExternally("https://github.com/sponsors/etkecc")
        }

        MenuItem {
            text: qsTr("Liberapay")
            onTriggered: Qt.openUrlExternally("https://liberapay.com/etkecc")
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("I no longer sponsor")
            onTriggered: Settings.donationStatus = "visible"
        }

        MenuItem {
            text: qsTr("Hide")
            onTriggered: root.showHideConfirmDialog()
        }
    }

    Menu {
        id: donateMenu

        MenuItem {
            text: qsTr("GitHub Sponsors")
            onTriggered: Qt.openUrlExternally("https://github.com/sponsors/etkecc")
        }

        MenuItem {
            text: qsTr("Liberapay")
            onTriggered: Qt.openUrlExternally("https://liberapay.com/etkecc")
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("I already sponsor!")
            onTriggered: Settings.donationStatus = "sponsoring"
        }

        MenuItem {
            text: qsTr("Hide")
            onTriggered: root.showHideConfirmDialog()
        }
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        textFormat: Text.RichText
        font.pointSize: Settings.uiFontSizePt * 0.9
        color: palette.buttonText
        text: "<style>a { color: " + palette.highlight + "; }</style>" +
              qsTr("%1 is created by %2 (managed Matrix server hosting).")
              .arg(root.komaiProjectLink)
              .arg(root.etkeProjectLink)

        onLinkActivated: function(link) { Qt.openUrlExternally(link) }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    function showHideConfirmDialog() {
        var dialog = hideConfirmComponent.createObject(Overlay.overlay);
        dialog.open();
    }

    Component {
        id: hideConfirmComponent

        Dialog {
            anchors.centerIn: parent
            title: qsTr("Hide donation button?")
            modal: true
            standardButtons: Dialog.Yes | Dialog.No

            Label {
                text: qsTr("This will permanently hide the donation button from this screen.")
                wrapMode: Text.Wrap
                width: parent.width
            }

            onAccepted: Settings.donationStatus = "hidden"
            onClosed: destroy()
        }
    }
}
