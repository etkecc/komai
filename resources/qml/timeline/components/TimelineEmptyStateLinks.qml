// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
            id: donateButton

            readonly property string heartIcon: Settings.donationStatus === "sponsoring"
                ? "qrc:/icons/icons/ui/heart-filled.svg" : "qrc:/icons/icons/ui/heart.svg"

            Layout.preferredWidth: linksRow.linksUniformWidth
            visible: Settings.donationStatus !== "hidden"
            text: Settings.donationStatus === "sponsoring" ? qsTr("Donating!") : qsTr("Donate")
            icon.source: "image://colorimage/:" + heartIcon.substring(4) + "?" + Komai.theme.error
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
            text: qsTr("I no longer donate")
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
            text: qsTr("I already donate!")
            onTriggered: Settings.donationStatus = "sponsoring"
        }

        MenuItem {
            text: qsTr("Hide")
            onTriggered: root.showHideConfirmDialog()
        }
    }

    function showHideConfirmDialog() {
        var dialog = hideConfirmComponent.createObject(Overlay.overlay);
        dialog.open();
    }

    Component {
        id: hideConfirmComponent

        OverlayDialog {
            title: qsTr("Hide donation button?")

            Label {
                Layout.fillWidth: true
                color: palette.buttonText
                wrapMode: Text.WordWrap
                text: qsTr("This will permanently hide the donation button from this screen.")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Komai.paddingMedium

                KomaiButton {
                    text: qsTr("Cancel")
                    onClicked: close()
                }

                Item {
                    Layout.fillWidth: true
                }

                KomaiButton {
                    id: hideButton

                    text: qsTr("Hide")
                    highlighted: true
                    onClicked: {
                        Settings.donationStatus = "hidden";
                        close();
                    }
                }
            }

            initialFocusItem: hideButton
            onClosed: destroy()
        }
    }
}
