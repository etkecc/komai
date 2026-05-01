// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

// Three rows on the Welcome page that let the user personalise the basics
// before they have an account. Each row uses the same structure — a label
// pinned to the left and the controls right-aligned — so dropdowns of
// different widths still line up visually.
//
// Theme + Animations stay grouped because they're both "look & feel"
// adjustments and the user already gets a 4th row (Language) below.
ColumnLayout {
    id: root

    // Width-limited so the row contents don't sprawl across wider windows;
    // the parent OnboardingScrollPage is capped at 760, which leaves a
    // little air on each side for visual breathing room.
    readonly property int rowWidth: 640
    readonly property int labelWidth: 200

    spacing: Komai.paddingSmall

    component PersonalityRow: RowLayout {
        Layout.preferredWidth: root.rowWidth
        Layout.alignment: Qt.AlignHCenter
        spacing: Komai.paddingMedium
    }

    component PersonalityLabel: Label {
        Layout.preferredWidth: root.labelWidth
        Layout.alignment: Qt.AlignVCenter
        color: palette.text
    }

    PersonalityRow {
        PersonalityLabel { text: qsTr("Theme") }

        Item { Layout.fillWidth: true }

        SegmentedButton {
            id: variantSegment
            Layout.alignment: Qt.AlignVCenter
            currentIndex: Settings.themeVariantIndex()
            model: [
                { text: qsTr("Light") },
                { text: qsTr("Dark") }
            ]
            onActivated: function(index) {
                Settings.setThemeVariantByIndex(index)
            }
        }

        KomaiComboBox {
            id: themeCombo
            Layout.alignment: Qt.AlignVCenter
            model: Settings.themeNamesForCurrentVariant()
            currentIndex: Settings.themeIndexInCurrentVariant()
            onActivated: function(index) {
                Settings.setThemeByVariantIndex(index)
            }
        }
    }

    PersonalityRow {
        PersonalityLabel {
            id: animationsLabel
            text: qsTr("Enable animations")

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: animationsToggle.toggle()
            }

            HoverHandler {
                id: animationsHovered
            }

            KomaiToolTip {
                anchorItem: animationsLabel
                anchorX: animationsLabel.width / 2
                anchorY: 0
                text: qsTr("Komai uses animations in several places to improve visual feedback. Disable them if they make you feel unwell.")
                delay: Komai.tooltipDelay
                requestedVisible: animationsHovered.hovered
            }
        }

        Item { Layout.fillWidth: true }

        ToggleButton {
            id: animationsToggle
            Layout.alignment: Qt.AlignVCenter
            checked: Settings.uiMotionAnimationsEnabled
            onCheckedChanged: Settings.uiMotionAnimationsEnabled = checked
        }
    }

    PersonalityRow {
        PersonalityLabel { text: qsTr("Language") }

        Item { Layout.fillWidth: true }

        KomaiSearchableComboBox {
            id: languageCombo
            Layout.alignment: Qt.AlignVCenter
            Layout.minimumWidth: 220
            model: Settings.languageDropdownLabels()
            currentIndex: Settings.languageDropdownIndex()
            onActivated: function(index) {
                Settings.setLanguageByDropdownIndex(index)
            }

            Connections {
                target: Settings
                function onUiLanguageChanged() {
                    // Qt.callLater so the rebuild lands AFTER MainApplication's
                    // same-signal slot has swapped translators. Without this the
                    // "Use system" entry rebuilds against the previous language.
                    Qt.callLater(() => {
                        languageCombo.model = Settings.languageDropdownLabels()
                        languageCombo.currentIndex = Settings.languageDropdownIndex()
                    })
                }
            }
        }
    }

    PersonalityRow {
        PersonalityLabel { text: qsTr("Prefer emoji suggestions for") }

        Item { Layout.fillWidth: true }

        KomaiComboBox {
            id: preferredGenderCombo
            Layout.alignment: Qt.AlignVCenter
            Layout.minimumWidth: 150
            model: [
                qsTr("Any gender"),
                qsTr("👨 Men"),
                qsTr("👩 Women")
            ]
            currentIndex: Settings.composerInputEmojiPreferredGender
            onActivated: function(index) {
                Settings.composerInputEmojiPreferredGender = index
            }
            implicitContentWidthPolicy: ComboBox.WidestText
            popup.width: Math.max(width, implicitContentWidth + leftPadding + rightPadding + 24)
        }

        KomaiComboBox {
            id: preferredSkinToneCombo
            Layout.alignment: Qt.AlignVCenter
            Layout.minimumWidth: 210
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
            implicitContentWidthPolicy: ComboBox.WidestText
            popup.width: Math.max(width, implicitContentWidth + leftPadding + rightPadding + 24)
        }
    }

    Connections {
        target: Settings

        function onUiThemeSlugChanged() {
            variantSegment.currentIndex = Settings.themeVariantIndex()
            themeCombo.model = Settings.themeNamesForCurrentVariant()
            themeCombo.currentIndex = Settings.themeIndexInCurrentVariant()
        }
    }
}
