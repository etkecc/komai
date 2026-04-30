// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../../../components"
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

// Voice transcription section in Settings → Integrations.
//
// Lays out each setting as its own card (TranscriptionSettingRow), so the
// page reads the same as the model-driven settings rows on every other
// tab (responsive, hover-aware, stacks at narrow widths). The page is
// mounted via `headerContent` so it sits at the very top of the
// Integrations tab (above D-Bus / Matrix Rooms Search / Browser).
Item {
    id: root

    // Recognised by the SettingsContent deeplink dispatcher
    // (komai://settings/integrations/transcription).
    property string tagId: "transcription"

    Layout.fillWidth: true
    implicitHeight: section.implicitHeight
    implicitWidth: section.implicitWidth

    readonly property string openaiCloudUrl: "https://api.openai.com/v1"

    // Default the hosting type to "OpenAI cloud" for a fresh install.
    // Otherwise the heuristic looks at the saved api_url.
    readonly property bool hostingIsOpenaiCloud: {
        var url = (Settings.integrationsTranscriptionApiUrl || "").trim().toLowerCase();
        if (url.length === 0)
            return true;
        return url === root.openaiCloudUrl;
    }

    function setHostingType(index) {
        if (index === 0) {
            // OpenAI cloud — lock the URL to the canonical OpenAI base.
            Settings.integrationsTranscriptionApiUrl = root.openaiCloudUrl;
        } else if (Settings.integrationsTranscriptionApiUrl.trim().toLowerCase() === root.openaiCloudUrl) {
            // Switching to "Other" — clear the prefilled cloud URL so the
            // user is prompted to enter their server's URL.
            Settings.integrationsTranscriptionApiUrl = "";
        }
    }

    ColumnLayout {
        id: section
        width: parent.width
        spacing: Komai.paddingSmall

        SettingsSection {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingMedium
            Layout.bottomMargin: Komai.paddingSmall
            label: qsTr("Voice transcription")
            helperText: qsTr("Most of these settings can be overridden per room in Room Info → Preferences.")
        }

        // Provider --------------------------------------------------------
        TranscriptionSettingRow {
            Layout.fillWidth: true
            label: qsTr("Provider")
            description: qsTr("Streaming (a little more realtime, but still not word-by-word) is typically not as well-supported across API servers and tends to cost more. <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/features/voice-transcription.md#-compatible-providers\">Learn more</a>.")

            SegmentedButton {
                width: parent.width
                model: [
                    { text: qsTr("OpenAI Batch (one-shot)") },
                    { text: qsTr("OpenAI Realtime (streaming)") }
                ]
                currentIndex: Settings.integrationsTranscriptionProvider === "openai_realtime" ? 1 : 0
                onActivated: index => {
                    Settings.integrationsTranscriptionProvider =
                        index === 1 ? "openai_realtime" : "openai_batch";
                }
            }
        }

        // Hosting type ----------------------------------------------------
        TranscriptionSettingRow {
            Layout.fillWidth: true
            label: qsTr("Hosting")

            SegmentedButton {
                width: parent.width
                model: [
                    { text: qsTr("OpenAI cloud") },
                    { text: qsTr("Other (OpenAI-compatible server)") }
                ]
                currentIndex: root.hostingIsOpenaiCloud ? 0 : 1
                onActivated: index => {
                    root.setHostingType(index);
                }
            }
        }

        // API base URL ----------------------------------------------------
        TranscriptionSettingRow {
            Layout.fillWidth: true
            label: qsTr("API base URL")

            KomaiTextField {
                id: apiUrlField
                width: parent.width
                text: Settings.integrationsTranscriptionApiUrl
                readOnly: root.hostingIsOpenaiCloud
                placeholderText: qsTr("Example: http://localhost:8080/v1")
                onEditingFinished: {
                    Settings.integrationsTranscriptionApiUrl = text.trim();
                }
            }
        }

        // API key ---------------------------------------------------------
        // Loaded via Transcription.loadGlobalApiKey() and saved via
        // Transcription.saveGlobalApiKey(); the value never touches
        // config.yml.
        TranscriptionSettingRow {
            id: apiKeyRow
            Layout.fillWidth: true
            label: qsTr("API key")

            property bool initialized: false
            property string lastSavedValue: ""

            Component.onCompleted: {
                lastSavedValue = Transcription.loadGlobalApiKey();
                apiKeyField.text = lastSavedValue;
                initialized = true;
            }

            RowLayout {
                width: parent.width
                spacing: Komai.paddingSmall

                KomaiTextField {
                    id: apiKeyField
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Example: sk-…")
                    onEditingFinished: {
                        if (!apiKeyRow.initialized)
                            return;
                        var trimmed = text.trim();
                        if (trimmed === apiKeyRow.lastSavedValue)
                            return;
                        if (trimmed.length === 0)
                            Transcription.clearGlobalApiKey();
                        else
                            Transcription.saveGlobalApiKey(trimmed);
                        apiKeyRow.lastSavedValue = trimmed;
                    }
                }

                ImageButton {
                    Layout.preferredWidth: Math.round(Settings.uiFontSizePt * 2)
                    Layout.preferredHeight: Math.round(Settings.uiFontSizePt * 2)
                    Layout.alignment: Qt.AlignVCenter
                    // Flip to brightText when the surrounding card is
                    // hovered (matches the LoginPage password-reveal
                    // pattern). Hover on the button itself is handled by
                    // ImageButton's own state.
                    buttonTextColor: apiKeyRow.hovered ? palette.brightText : palette.buttonText
                    image: apiKeyField.echoMode === TextInput.Password
                        ? ":/icons/icons/ui/eye-show.svg"
                        : ":/icons/icons/ui/eye-hide.svg"
                    toolTipVisible: hovered
                    toolTipText: qsTr("Show/Hide API key")
                    onClicked: {
                        apiKeyField.echoMode = apiKeyField.echoMode === TextInput.Normal
                            ? TextInput.Password
                            : TextInput.Normal;
                    }
                }
            }
        }

        // Model -----------------------------------------------------------
        TranscriptionSettingRow {
            Layout.fillWidth: true
            label: qsTr("Model")
            description: qsTr("Leave empty to use a sensible default for the selected Provider. <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/features/voice-transcription.md#-choosing-a-model\">Learn more</a>.")

            KomaiTextField {
                width: parent.width
                text: Settings.integrationsTranscriptionModel
                placeholderText: Settings.integrationsTranscriptionProvider === "openai_realtime"
                    ? qsTr("Example: gpt-4o-mini-transcribe")
                    : qsTr("Example: whisper-1")
                onEditingFinished: {
                    Settings.integrationsTranscriptionModel = text.trim();
                }
            }
        }

        // Language --------------------------------------------------------
        TranscriptionSettingRow {
            Layout.fillWidth: true
            label: qsTr("Language")
            description: qsTr("Leave empty to let the server autodetect. Otherwise, an ISO-639-1 code (e.g. en, bg, fr).")

            KomaiTextField {
                width: parent.width
                text: Settings.integrationsTranscriptionLanguage
                placeholderText: qsTr("Example: en")
                onEditingFinished: {
                    Settings.integrationsTranscriptionLanguage = text.trim();
                }
            }
        }

        // Prompt (multiline) ----------------------------------------------
        TranscriptionSettingRow {
            Layout.fillWidth: true
            label: qsTr("Prompt")
            description: qsTr("Vocabulary or style hint passed to the transcription model. Helps with names, jargon, or style preferences.")

            KomaiTextArea {
                width: parent.width
                height: Math.max(implicitHeight, 80)
                text: Settings.integrationsTranscriptionPrompt
                placeholderText: qsTr("Example: Names: Alice, Bob, Carol. Jargon: Matrix, Komai, federation.")
                wrapMode: TextEdit.Wrap
                onEditingFinished: {
                    Settings.integrationsTranscriptionPrompt = text;
                }
            }
        }
    }
}
