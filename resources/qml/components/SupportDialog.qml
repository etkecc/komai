// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

OverlayDialog {
    id: root

    readonly property string komaiUrl: "https://komai.chat/?utm_source=komai&amp;utm_medium=app&amp;utm_campaign=support"
    readonly property string freeSoftwareUrl: "https://www.gnu.org/philosophy/free-sw.html"
    readonly property string licenseUrl: "https://github.com/etkecc/komai/blob/main/LICENSES/GPL-3.0-or-later.txt"
    readonly property string claUrl: "https://en.wikipedia.org/wiki/Contributor_License_Agreement"

    title: qsTr("Sponsor Komai")
    titleIcon: ":/icons/icons/ui/heart.svg"
    titleIconColor: Komai.theme.error

    component ActionButton: AbstractButton {
        id: actionBtn

        required property string labelText
        required property string descriptionText
        required property string iconSource
        property color iconColor: actionBtn.actionTextColor
        property bool rawIcon: false
        property bool forceHighlight: false

        Layout.fillWidth: true
        implicitHeight: contentColumn.implicitHeight + topPadding + bottomPadding
        leftPadding: Komai.paddingMedium
        rightPadding: Komai.paddingMedium
        topPadding: Komai.paddingMedium
        bottomPadding: Komai.paddingMedium
        hoverEnabled: true
        activeFocusOnTab: true
        focusPolicy: Qt.StrongFocus

        readonly property bool activeState: hovered || pressed || activeFocus
        readonly property color actionTextColor: activeState ? palette.brightText : (forceHighlight ? palette.highlightedText : palette.text)
        readonly property color descriptionColor: activeState ? palette.brightText : (forceHighlight ? palette.highlightedText : palette.buttonText)
        readonly property real actionIconSize: Math.round(Settings.uiFontSizePt * 2)

        contentItem: RowLayout {
            id: contentColumn

            spacing: Komai.paddingMedium

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: actionBtn.actionIconSize
                Layout.preferredHeight: actionBtn.actionIconSize
                fillMode: Image.PreserveAspectFit
                source: actionBtn.iconSource !== ""
                    ? (actionBtn.rawIcon ? ("qrc" + actionBtn.iconSource) : ("image://colorimage/" + actionBtn.iconSource + "?" + actionBtn.iconColor))
                    : ""
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: actionBtn.labelText
                    color: actionBtn.actionTextColor
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: actionBtn.descriptionText
                    color: actionBtn.descriptionColor
                    font.pointSize: Settings.uiFontSizePt * 0.9
                    wrapMode: Text.WordWrap
                    visible: text !== ""
                }
            }
        }

        background: Rectangle {
            radius: Komai.paddingMedium
            color: actionBtn.activeState ? palette.dark : (actionBtn.forceHighlight ? palette.highlight : palette.window)
        }

        KomaiCursorShape {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
        }
    }

    // Introduction text
    Label {
        Layout.fillWidth: true
        textFormat: Text.RichText
        wrapMode: Text.WordWrap
        color: palette.text
        text: "<style>a { color: " + palette.highlight + "; }</style>" +
              qsTr("<a href=\"%1\">Komai</a> is fully <a href=\"%2\">Free Software</a> (<a href=\"%3\">GPL-3.0-or-later</a>) with no <a href=\"%4\">CLA</a> and no gatekeeping.")
              .arg(root.komaiUrl)
              .arg(root.freeSoftwareUrl)
              .arg(root.licenseUrl)
              .arg(root.claUrl)

        onLinkActivated: function(link) { Qt.openUrlExternally(link) }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: palette.text
        text: qsTr("You can support Komai by contributing code, design, testing, translations, helping others, or financially.")
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: palette.text
        text: qsTr("If you'd like to help financially, you can use one of the options below.")
    }

    // Monetary support section header
    Label {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingSmall
        text: qsTr("Monetary support")
        color: palette.buttonText
        font.bold: true
        font.pointSize: Settings.uiFontSizePt
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        ActionButton {
            labelText: qsTr("GitHub Sponsors")
            descriptionText: qsTr("Support via GitHub Sponsors")
            iconSource: ":/icons/icons/ui/github.svg"
            onClicked: Qt.openUrlExternally("https://github.com/sponsors/etkecc")
        }

        ActionButton {
            labelText: qsTr("Liberapay")
            descriptionText: qsTr("Support via Liberapay")
            iconSource: ":/icons/icons/ui/liberapay.svg"
            rawIcon: true
            onClicked: Qt.openUrlExternally("https://liberapay.com/etkecc")
        }
    }

    // Divider
    Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingSmall
        Layout.bottomMargin: Komai.paddingSmall
        height: 1
        color: Komai.theme.separator
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        ActionButton {
            id: sponsorToggle

            readonly property bool isSponsoring: Settings.sponsoringStatus === "sponsoring"

            forceHighlight: isSponsoring
            labelText: qsTr("I already sponsor!")
            descriptionText: isSponsoring
                ? qsTr("Click to unmark yourself as a sponsor")
                : qsTr("Mark yourself locally as a sponsor")
            iconSource: isSponsoring
                ? ":/icons/icons/ui/heart-filled.svg"
                : ":/icons/icons/ui/heart.svg"
            iconColor: Komai.theme.error
            onClicked: {
                Settings.sponsoringStatus = isSponsoring ? "visible" : "sponsoring";
                sponsorToggle.focus = false;
            }
        }

        ActionButton {
            labelText: qsTr("Hide this button")
            descriptionText: qsTr("Permanently hide the sponsor button from the interface")
            iconSource: ":/icons/icons/ui/eye-hide.svg"
            onClicked: root.showHideConfirmDialog()
        }
    }

    function showHideConfirmDialog() {
        var dialog = hideConfirmComponent.createObject(Overlay.overlay);
        dialog.open();
    }

    Component {
        id: hideConfirmComponent

        OverlayDialog {
            title: qsTr("Hide sponsor button?")

            Label {
                Layout.fillWidth: true
                color: palette.buttonText
                wrapMode: Text.WordWrap
                text: qsTr("This will permanently hide the sponsor button from this screen.")
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
                        Settings.sponsoringStatus = "hidden";
                        root.close();
                        close();
                    }
                }
            }

            initialFocusItem: hideButton
            onClosed: destroy()
        }
    }
}
