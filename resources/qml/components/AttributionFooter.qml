// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: root

    readonly property string komaiProjectLink: "<a href=\"https://github.com/etkecc/komai\">Komai</a>"
    readonly property string etkeProjectLink: "<a href=\"https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=attribution\">etke.cc</a>"

    implicitHeight: Komai.navigationRowHeight
    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    color: palette.alternateBase

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Komai.theme.separator
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingMedium
        spacing: Komai.paddingMedium

        Image {
            id: footerLogo

            readonly property real logoSize: footerText.implicitHeight * 1.5

            Layout.preferredWidth: logoSize
            Layout.preferredHeight: logoSize
            Layout.alignment: Qt.AlignVCenter
            source: "qrc:/logos/komai.svg"
            sourceSize: Qt.size(logoSize * 2, logoSize * 2)

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/etkecc/komai")
            }
        }

        Text {
            id: footerText

            Layout.fillWidth: true
            textFormat: Text.RichText
            font.pointSize: Settings.uiFontSizePt
            color: palette.buttonText
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideRight
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

        KomaiButton {
            id: donateButton

            readonly property string heartIcon: Settings.donationStatus === "sponsoring"
                ? "qrc:/icons/icons/ui/heart-filled.svg" : "qrc:/icons/icons/ui/heart.svg"

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

        KomaiButton {
            text: qsTr("Report an issue")
            icon.source: "qrc:/icons/icons/ui/alert.svg"
            onClicked: Qt.openUrlExternally("https://github.com/etkecc/komai/issues")
        }
    }

    Menu {
        id: sponsoringMenu

        Component.onCompleted: {
            if (sponsoringMenu.popupType != undefined)
                sponsoringMenu.popupType = 2;
        }

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

        Component.onCompleted: {
            if (donateMenu.popupType != undefined)
                donateMenu.popupType = 2;
        }

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
