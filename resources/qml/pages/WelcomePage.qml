// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import QtQuick.Window 2.15
import im.nheko 1.0
import "../components/"
import ".."

ColumnLayout {
    Item {
        Layout.fillHeight: true
    }

    Image {
        Layout.alignment: Qt.AlignHCenter
        source: "qrc:/logos/splash.png"
        Layout.preferredHeight: 256
        Layout.preferredWidth: 256
    }

    Label {
        Layout.topMargin: Nheko.paddingLarge
        Layout.leftMargin: Nheko.paddingLarge
        Layout.rightMargin: Nheko.paddingLarge
        Layout.bottomMargin: 0
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        text: qsTr("Welcome to Komai!")
        color: palette.text
        font.pointSize: fontMetrics.font.pointSize * 2
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
    }

    Label {
        Layout.topMargin: Nheko.paddingSmall
        Layout.leftMargin: Nheko.paddingLarge
        Layout.rightMargin: Nheko.paddingLarge
        Layout.bottomMargin: Nheko.paddingLarge
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        text: Nheko.tagline
        color: palette.buttonText
        font.pointSize: fontMetrics.font.pointSize * 1.5
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
    }

    RowLayout {
        Item {
            Layout.fillWidth: true
        }
        FlatButton {
            Layout.margins: Nheko.paddingLarge
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("REGISTER")
            onClicked: {
                mainWindow.push(registerPage);
            }
        }
        FlatButton {
            Layout.margins: Nheko.paddingLarge
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("LOGIN")
            onClicked: {
                mainWindow.push(loginPage);
            }
        }

        Item {
            Layout.fillWidth: true
        }

    }

    Label {
        Layout.topMargin: Nheko.paddingLarge
        Layout.bottomMargin: Nheko.paddingSmall
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("An early touch of personality")
        color: palette.buttonText
        font.pointSize: fontMetrics.font.pointSize * 1.1
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        Layout.leftMargin: Nheko.paddingLarge
        Layout.rightMargin: Nheko.paddingLarge

        Label {
            Layout.alignment: Qt.AlignVCenter
            Layout.margins: Nheko.paddingSmall
            text: qsTr("Theme")
            color: palette.text
        }

        ComboBox {
            id: variantCombo
            model: [qsTr("Light"), qsTr("Dark"), qsTr("System")]
            currentIndex: Settings.themeVariantIndex()
            onActivated: function(index) {
                Settings.setThemeVariantByIndex(index)
            }
            implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
            wheelEnabled: activeFocus
        }

        ComboBox {
            id: themeCombo
            visible: variantCombo.currentIndex !== 2
            model: Settings.themeNamesForCurrentVariant()
            currentIndex: Settings.themeIndexInCurrentVariant()
            onActivated: function(index) {
                Settings.setThemeByVariantIndex(index)
            }
            implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
            wheelEnabled: activeFocus
        }

        Connections {
            target: Settings
            function onThemeChanged() {
                variantCombo.currentIndex = Settings.themeVariantIndex()
                themeCombo.model = Settings.themeNamesForCurrentVariant()
                themeCombo.currentIndex = Settings.themeIndexInCurrentVariant()
            }
        }

        Item {
            Layout.preferredWidth: Nheko.paddingLarge
        }

        ToggleButton {
            id: reducedMotionToggle
            Layout.alignment: Qt.AlignVCenter
            checked: Settings.reducedMotion
            onCheckedChanged: Settings.reducedMotion = checked
        }

        Label {
            Layout.alignment: Qt.AlignVCenter
            Layout.margins: Nheko.paddingSmall
            text: qsTr("Reduce animations")
            color: palette.text

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: reducedMotionToggle.toggle()
            }

            HoverHandler {
                id: hovered
            }
            ToolTip.visible: hovered.hovered
            ToolTip.text: qsTr("Komai uses animations in several places to make stuff pretty. This allows you to turn those off if they make you feel unwell.")
            ToolTip.delay: Nheko.tooltipDelay
        }
    }

    Item {
        Layout.fillHeight: true
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: Nheko.paddingLarge
        Layout.leftMargin: Nheko.paddingLarge
        Layout.rightMargin: Nheko.paddingLarge
        font.pointSize: fontMetrics.font.pointSize * 0.9
        textFormat: Text.RichText
        text: "<style>a { color: " + palette.highlight + "; }</style>" +
              "<a href=\"https://github.com/etkecc/komai\">Komai</a>" +
              " is an opinionated UI/UX polished fork of " +
              "<a href=\"https://nheko.im\">nheko</a>" +
              ", maintained by " +
              "<a href=\"https://etke.cc/\">etke.cc</a>."
        color: palette.buttonText
        onLinkActivated: function(link) { Qt.openUrlExternally(link); }

        HoverHandler {
            id: footerHover
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }
}
