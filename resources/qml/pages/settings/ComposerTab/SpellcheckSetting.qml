// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../../../components"
import "../IntegrationsTab"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

// Spellcheck section in Settings → Composer (mounted as the tab's footer
// content). Master on/off toggle plus one card per dictionary Komai found —
// the bundled en_US, anything under the per-user dictionary dir, and the
// system Hunspell locations (incl. Flatpak `org.freedesktop.Platform.Hunspell.*`
// extensions). Edits go through SpellCheckEngine, which persists them into the
// regular per-profile config.yml.
Item {
    id: root

    // Recognized by the SettingsContent deeplink dispatcher
    // (komai://settings/composer/spellcheck).
    property string tagId: "spellcheck"

    property var dictModel: []

    Layout.fillWidth: true
    implicitHeight: section.implicitHeight
    implicitWidth: section.implicitWidth

    function refresh() {
        root.dictModel = SpellCheckEngine.availableDictionaries();
    }

    Component.onCompleted: root.refresh()

    Connections {
        target: SpellCheckEngine
        function onConfigChanged() {
            root.refresh();
        }
    }

    ColumnLayout {
        id: section
        width: root.width
        spacing: Komai.paddingSmall

        SettingsSection {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingLarge
            Layout.bottomMargin: Komai.paddingSmall
            label: qsTr("Spellcheck")
        }

        TranscriptionSettingRow {
            id: masterRow

            Layout.fillWidth: true
            label: qsTr("Enable spell checking")
            description: qsTr("Underlines misspelled words (those not recognized by any of the enabled dictionaries).")

            SettingControlToggle {
                anchors.left: masterRow.useStackedLayout ? parent.left : undefined
                anchors.right: masterRow.useStackedLayout ? undefined : parent.right
                value: SpellCheckEngine.enabled
                textColor: masterRow.hovered ? palette.brightText : palette.buttonText
                onToggledValue: function (v) {
                    SpellCheckEngine.enabled = v;
                }
            }
        }

        Repeater {
            model: root.dictModel

            delegate: TranscriptionSettingRow {
                id: dictRow

                required property var modelData

                Layout.fillWidth: true
                visible: SpellCheckEngine.enabled
                //: Settings-list row label for the bundled dictionary that
                //: ships with Komai (as opposed to one installed from the
                //: system or a manual drop-in). %1 is the dictionary's display
                //: name ("English / United States", "Bulgarian / Bulgaria", or
                //: just "Esperanto" for locale codes without a territory).
                label: dictRow.modelData.builtin
                    ? qsTr("%1 (built-in)").arg(dictRow.modelData.name)
                    : dictRow.modelData.name

                SettingControlToggle {
                    anchors.left: dictRow.useStackedLayout ? parent.left : undefined
                    anchors.right: dictRow.useStackedLayout ? undefined : parent.right
                    value: dictRow.modelData.enabled
                    textColor: dictRow.hovered ? palette.brightText : palette.buttonText
                    onToggledValue: function (v) {
                        SpellCheckEngine.setLanguageEnabled(dictRow.modelData.code, v);
                    }
                }
            }
        }

        TextEdit {
            Layout.fillWidth: true
            Layout.leftMargin: Komai.paddingSmall
            Layout.rightMargin: Komai.paddingSmall
            Layout.topMargin: Komai.paddingSmall
            Layout.bottomMargin: Komai.paddingMedium
            visible: SpellCheckEngine.enabled
            color: palette.buttonText
            text: qsTr("Don't see a language? You need to install a Hunspell dictionary. See the <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/features/spellcheck.md\">spell checking documentation</a>. English (US) is built in.")
            textFormat: Text.RichText
            font.pointSize: Settings.uiFontSizePt
            wrapMode: Text.Wrap
            readOnly: true
            selectByMouse: true
            onLinkActivated: function (link) {
                Qt.openUrlExternally(link);
            }
        }
    }
}
