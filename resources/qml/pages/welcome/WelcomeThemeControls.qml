// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0

ColumnLayout {
    spacing: Komai.paddingSmall

    RowLayout {
        Label {
            Layout.alignment: Qt.AlignVCenter
            Layout.margins: Komai.paddingSmall
            text: qsTr("Theme")
            color: palette.text
        }

        KomaiComboBox {
            id: variantCombo
            model: [qsTr("Light"), qsTr("Dark"), qsTr("System")]
            currentIndex: Settings.themeVariantIndex()
            onActivated: function(index) {
                Settings.setThemeVariantByIndex(index)
            }
        }

        KomaiComboBox {
            id: themeCombo
            visible: variantCombo.currentIndex !== 2
            model: Settings.themeNamesForCurrentVariant()
            currentIndex: Settings.themeIndexInCurrentVariant()
            onActivated: function(index) {
                Settings.setThemeByVariantIndex(index)
            }
        }

        Item {
            Layout.preferredWidth: Komai.paddingLarge
        }

        ToggleButton {
            id: animationsToggle
            Layout.alignment: Qt.AlignVCenter
            checked: Settings.uiMotionAnimationsEnabled
            onCheckedChanged: Settings.uiMotionAnimationsEnabled = checked
        }

        Label {
            Layout.alignment: Qt.AlignVCenter
            Layout.margins: Komai.paddingSmall
            text: qsTr("Enable animations")
            color: palette.text

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: animationsToggle.toggle()
            }

            HoverHandler {
                id: hovered
            }
            ToolTip.visible: hovered.hovered
            ToolTip.text: qsTr("Komai uses animations in several places to improve visual feedback. Disable them if they make you feel unwell.")
            ToolTip.delay: Komai.tooltipDelay
        }
    }

    RowLayout {
        Label {
            Layout.alignment: Qt.AlignVCenter
            Layout.margins: Komai.paddingSmall
            text: qsTr("Prefer emoji suggestions for")
            color: palette.text
        }

        KomaiComboBox {
            id: preferredGenderCombo
            model: [
                qsTr("Any gender"),
                qsTr("👨 Men"),
                qsTr("👩 Women")
            ]
            currentIndex: Settings.composerInputEmojiPreferredGender
            onActivated: function(index) {
                Settings.composerInputEmojiPreferredGender = index
            }
            Layout.minimumWidth: 150
            implicitContentWidthPolicy: ComboBox.WidestText
            popup.width: Math.max(width, implicitContentWidth + leftPadding + rightPadding + 24)
        }

        KomaiComboBox {
            id: preferredSkinToneCombo
            model: [
                qsTr("Any skin tone"),
                qsTr("👍🏻 Light"),
                qsTr("👍🏼 Medium-light"),
                qsTr("👍🏽 Medium"),
                qsTr("👍🏾 Medium-dark"),
                qsTr("👍🏿 Dark")
            ]
            currentIndex: Settings.composerInputEmojiPreferredSkinTone
            onActivated: function(index) {
                Settings.composerInputEmojiPreferredSkinTone = index
            }
            Layout.minimumWidth: 210
            implicitContentWidthPolicy: ComboBox.WidestText
            popup.width: Math.max(width, implicitContentWidth + leftPadding + rightPadding + 24)
        }
    }

    Connections {
        target: Settings

        function onUiThemeSlugChanged() {
            variantCombo.currentIndex = Settings.themeVariantIndex()
            themeCombo.model = Settings.themeNamesForCurrentVariant()
            themeCombo.currentIndex = Settings.themeIndexInCurrentVariant()
        }
    }
}
