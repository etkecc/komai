// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../../components" as Components
import "../../../ui"
import "../../moderation"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Item {
    id: preferencesTab

    property var roomSettings
    property var members
    property var room
    property var appRoot

    readonly property string currentRoomId: roomSettings ? roomSettings.roomId : ""

    // Bumped whenever a per-room transcription override changes, so QML
    // bindings that consult `Settings.integrationsTranscriptionOverridesForRoom`
    // / `hasIntegrationsTranscriptionOverrideForRoom` re-evaluate. The
    // Q_INVOKABLE getters don't have NOTIFY signals tied to them.
    property int transcriptionOverridesRevision: 0

    Connections {
        target: Settings
        function onIntegrationsTranscriptionOverridesByRoomChanged() {
            preferencesTab.transcriptionOverridesRevision++;
        }
    }

    readonly property string openaiCloudUrl: "https://api.openai.com/v1"

    function _resolvedTranscriptionField(fieldName) {
        // Reads the per-room override if set, otherwise the global value.
        // `transcriptionOverridesRevision` is referenced so any binding
        // calling this re-runs after a setter/clearer fires.
        var _ = preferencesTab.transcriptionOverridesRevision;
        if (preferencesTab.currentRoomId &&
            Settings.hasIntegrationsTranscriptionOverrideForRoom(preferencesTab.currentRoomId, fieldName)) {
            var overrides = Settings.integrationsTranscriptionOverridesForRoom(preferencesTab.currentRoomId);
            return overrides[fieldName] !== undefined ? overrides[fieldName] : "";
        }
        switch (fieldName) {
        case "provider": return Settings.integrationsTranscriptionProvider;
        case "api_url": return Settings.integrationsTranscriptionApiUrl;
        case "model": return Settings.integrationsTranscriptionModel;
        case "language": return Settings.integrationsTranscriptionLanguage;
        case "prompt": return Settings.integrationsTranscriptionPrompt;
        }
        return "";
    }

    function _hasTranscriptionOverride(fieldName) {
        var _ = preferencesTab.transcriptionOverridesRevision;
        if (!preferencesTab.currentRoomId) return false;
        return Settings.hasIntegrationsTranscriptionOverrideForRoom(preferencesTab.currentRoomId, fieldName);
    }

    function _providerToLabel(token) {
        if (token === "openai_realtime")
            return qsTr("OpenAI Realtime (streaming)");
        return qsTr("OpenAI Batch (one-shot)");
    }

    function _hostingToLabel(url) {
        var trimmed = (url || "").trim().toLowerCase();
        if (trimmed.length === 0 || trimmed === preferencesTab.openaiCloudUrl)
            return qsTr("OpenAI cloud");
        return qsTr("Other (OpenAI-compatible server)");
    }

    ScrollView {
        id: scrollView

        anchors.fill: parent
        ScrollBar.vertical.policy: Settings.uiScrollbarPolicy === Settings.ScrollbarPolicy.Always ? ScrollBar.AlwaysOn : Settings.uiScrollbarPolicy === Settings.ScrollbarPolicy.Never ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: Komai.paddingSmall

            // --- Message visibility section ---
            Components.SettingsSection {
                label: qsTr("Message visibility")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
            }

            // Locally hidden events
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: hiddenEventsRowContent.implicitHeight
                HoverHandler { id: hiddenEventsRowHover; blocking: false }
                Rectangle { anchors.fill: hiddenEventsRowContent; color: hiddenEventsRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                RowLayout {
                    id: hiddenEventsRowContent
                    width: parent.width

                    Label {
                        text: qsTr("Locally hidden events")
                        color: hiddenEventsRowHover.hovered ? palette.brightText : palette.text
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                    }

                    Item { Layout.fillWidth: true }

                    HiddenEventsDialog {
                        id: hiddenEventsDialog
                        roomid: preferencesTab.roomSettings ? preferencesTab.roomSettings.roomId : ""
                    }

                    Components.KomaiButton {
                        text: qsTr("Configure")
                        onClicked: hiddenEventsDialog.open()
                        Layout.rightMargin: Komai.paddingMedium
                    }
                }
            }

            // Collapse thread replies (per-room override)
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: collapseRepliesRowContent.implicitHeight
                HoverHandler { id: collapseRepliesRowHover; blocking: false }
                Rectangle { anchors.fill: collapseRepliesRowContent; color: collapseRepliesRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                ColumnLayout {
                    id: collapseRepliesRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Collapse thread replies")
                            color: collapseRepliesRowHover.hovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                        }

                        Item { Layout.fillWidth: true }

                        Components.KomaiComboBox {
                            id: collapseRepliesCombo

                            readonly property string roomId: preferencesTab.roomSettings ? preferencesTab.roomSettings.roomId : ""
                            readonly property bool globalValue: Settings.timelineThreadsCollapseReplies

                            model: [
                                qsTr("Global Default (currently: %1)").arg(globalValue ? qsTr("On") : qsTr("Off")),
                                qsTr("On"),
                                qsTr("Off")
                            ]

                            currentIndex: {
                                var override = Settings.timelineThreadsCollapseRepliesOverrideForRoom(roomId);
                                if (override !== undefined && override !== null)
                                    return override ? 1 : 2;
                                return 0;
                            }

                            onActivated: function(index) {
                                var roomId = collapseRepliesCombo.roomId;
                                if (!roomId) return;
                                if (index === 0) {
                                    Settings.removeTimelineThreadsCollapseRepliesForRoom(roomId);
                                } else {
                                    Settings.setTimelineThreadsCollapseRepliesForRoom(roomId, index === 1);
                                }
                            }
                        }
                    }

                    Label {
                        text: qsTr("Hides thread replies from the main timeline, showing only thread root messages.<br>⚠️ Per-thread unread tracking is not supported, so you may miss replies in older threads.")
                        color: collapseRepliesRowHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                        textFormat: Text.RichText
                    }
                }
            }

            // --- Voice transcription section ---
            //
            // Per-room overrides for the 5 non-secret transcription fields,
            // plus the per-room API key. Each override row uses the
            // ToggleButton pattern: off → row shows the inherited global
            // value (read-only); on → per-room editable control.
            // The composer master toggle (`composer.input.transcription.enabled`)
            // is intentionally NOT per-room.
            Components.SettingsSection {
                label: qsTr("Voice transcription")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingLarge
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
            }

            // Provider override
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: providerRowContent.implicitHeight
                HoverHandler { id: providerRowHover; blocking: false }
                Rectangle { anchors.fill: providerRowContent; color: providerRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                ColumnLayout {
                    id: providerRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Provider")
                            color: providerRowHover.hovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                        }

                        Item { Layout.fillWidth: true }

                        ToggleButton {
                            id: providerOverrideToggle
                            textColor: providerRowHover.hovered ? palette.brightText : palette.buttonText
                            checked: preferencesTab._hasTranscriptionOverride("provider")
                            onToggled: {
                                if (!preferencesTab.currentRoomId) return;
                                if (checked) {
                                    // Seed the override with the current global value.
                                    Settings.setIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId,
                                        "provider",
                                        Settings.integrationsTranscriptionProvider || "openai_batch");
                                } else {
                                    Settings.clearIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId, "provider");
                                }
                            }
                        }
                    }

                    // Inherited (read-only) line
                    Label {
                        visible: !providerOverrideToggle.checked
                        text: qsTr("Inherited: %1").arg(preferencesTab._providerToLabel(Settings.integrationsTranscriptionProvider))
                        color: providerRowHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: true
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                    }

                    // Per-room editable control
                    Components.SegmentedButton {
                        visible: providerOverrideToggle.checked
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        model: [
                            { text: qsTr("OpenAI Batch (one-shot)") },
                            { text: qsTr("OpenAI Realtime (streaming)") }
                        ]
                        currentIndex: preferencesTab._resolvedTranscriptionField("provider") === "openai_realtime" ? 1 : 0
                        onActivated: function(index) {
                            if (!preferencesTab.currentRoomId) return;
                            Settings.setIntegrationsTranscriptionOverrideForRoom(
                                preferencesTab.currentRoomId,
                                "provider",
                                index === 1 ? "openai_realtime" : "openai_batch");
                        }
                    }
                }
            }

            // Hosting + API URL override (one shared override on `api_url`)
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: apiUrlRowContent.implicitHeight
                HoverHandler { id: apiUrlRowHover; blocking: false }
                Rectangle { anchors.fill: apiUrlRowContent; color: apiUrlRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                ColumnLayout {
                    id: apiUrlRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Hosting & API URL")
                            color: apiUrlRowHover.hovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                        }

                        Item { Layout.fillWidth: true }

                        ToggleButton {
                            id: apiUrlOverrideToggle
                            textColor: apiUrlRowHover.hovered ? palette.brightText : palette.buttonText
                            checked: preferencesTab._hasTranscriptionOverride("api_url")
                            onToggled: {
                                if (!preferencesTab.currentRoomId) return;
                                if (checked) {
                                    Settings.setIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId,
                                        "api_url",
                                        Settings.integrationsTranscriptionApiUrl);
                                } else {
                                    Settings.clearIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId, "api_url");
                                }
                            }
                        }
                    }

                    Label {
                        visible: !apiUrlOverrideToggle.checked
                        text: {
                            var url = Settings.integrationsTranscriptionApiUrl;
                            var hosting = preferencesTab._hostingToLabel(url);
                            if (url && url.length > 0)
                                return qsTr("Inherited: %1 — %2").arg(hosting).arg(url);
                            return qsTr("Inherited: %1").arg(hosting);
                        }
                        color: apiUrlRowHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: true
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                    }

                    ColumnLayout {
                        visible: apiUrlOverrideToggle.checked
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        spacing: Komai.paddingSmall

                        readonly property string overrideUrl: preferencesTab._resolvedTranscriptionField("api_url")
                        readonly property bool hostingIsOpenaiCloud: {
                            var trimmed = (overrideUrl || "").trim().toLowerCase();
                            if (trimmed.length === 0)
                                return true;
                            return trimmed === preferencesTab.openaiCloudUrl;
                        }

                        Components.SegmentedButton {
                            id: hostingPicker
                            Layout.fillWidth: true
                            model: [
                                { text: qsTr("OpenAI cloud") },
                                { text: qsTr("Other (OpenAI-compatible server)") }
                            ]
                            currentIndex: parent.hostingIsOpenaiCloud ? 0 : 1
                            onActivated: function(index) {
                                if (!preferencesTab.currentRoomId) return;
                                if (index === 0) {
                                    Settings.setIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId, "api_url", preferencesTab.openaiCloudUrl);
                                } else {
                                    var current = preferencesTab._resolvedTranscriptionField("api_url") || "";
                                    if (current.trim().toLowerCase() === preferencesTab.openaiCloudUrl) {
                                        Settings.setIntegrationsTranscriptionOverrideForRoom(
                                            preferencesTab.currentRoomId, "api_url", "");
                                    }
                                }
                            }
                        }

                        Components.KomaiTextField {
                            Layout.fillWidth: true
                            text: preferencesTab._resolvedTranscriptionField("api_url")
                            readOnly: parent.hostingIsOpenaiCloud
                            placeholderText: qsTr("Example: http://localhost:8080/v1")
                            onEditingFinished: {
                                if (!preferencesTab.currentRoomId) return;
                                Settings.setIntegrationsTranscriptionOverrideForRoom(
                                    preferencesTab.currentRoomId, "api_url", text.trim());
                            }
                        }
                    }
                }
            }

            // API key override (per-room secret in keychain)
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: apiKeyRowContent.implicitHeight
                HoverHandler { id: apiKeyRowHover; blocking: false }
                Rectangle { anchors.fill: apiKeyRowContent; color: apiKeyRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                // Loaded once when the toggle goes on, so we can compare on
                // editingFinished and avoid a redundant keychain write.
                property string lastSavedRoomKey: ""
                property bool roomKeyInitialized: false
                property bool hasRoomKey: false

                function _refreshRoomKey() {
                    if (!preferencesTab.currentRoomId) {
                        roomKeyInitialized = false;
                        hasRoomKey = false;
                        return;
                    }
                    var loaded = Transcription.loadRoomApiKey(preferencesTab.currentRoomId);
                    lastSavedRoomKey = loaded;
                    hasRoomKey = loaded.length > 0;
                    roomKeyInitialized = true;
                }

                Component.onCompleted: _refreshRoomKey()

                ColumnLayout {
                    id: apiKeyRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("API key")
                            color: apiKeyRowHover.hovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                        }

                        Item { Layout.fillWidth: true }

                        ToggleButton {
                            id: apiKeyOverrideToggle
                            textColor: apiKeyRowHover.hovered ? palette.brightText : palette.buttonText
                            checked: apiKeyRowContent.parent.hasRoomKey
                            onToggled: {
                                if (!preferencesTab.currentRoomId) {
                                    checked = false;
                                    return;
                                }
                                if (!checked) {
                                    Transcription.clearRoomApiKey(preferencesTab.currentRoomId);
                                    apiKeyRowContent.parent._refreshRoomKey();
                                    apiKeyField.text = "";
                                    apiKeyField.echoMode = TextInput.Password;
                                }
                            }
                        }
                    }

                    Label {
                        visible: !apiKeyOverrideToggle.checked
                        text: Transcription.loadGlobalApiKey().length > 0
                              ? qsTr("Inherited: a global API key is configured.")
                              : qsTr("Inherited: no global API key configured.")
                        color: apiKeyRowHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: true
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                    }

                    RowLayout {
                        visible: apiKeyOverrideToggle.checked
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        spacing: Komai.paddingSmall

                        Components.KomaiTextField {
                            id: apiKeyField
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: qsTr("Example: sk-…")
                            text: apiKeyRowContent.parent.roomKeyInitialized ? apiKeyRowContent.parent.lastSavedRoomKey : ""
                            onEditingFinished: {
                                if (!preferencesTab.currentRoomId) return;
                                if (!apiKeyRowContent.parent.roomKeyInitialized) return;
                                var trimmed = text.trim();
                                if (trimmed === apiKeyRowContent.parent.lastSavedRoomKey) return;
                                if (trimmed.length === 0) {
                                    Transcription.clearRoomApiKey(preferencesTab.currentRoomId);
                                } else {
                                    Transcription.saveRoomApiKey(preferencesTab.currentRoomId, trimmed);
                                }
                                apiKeyRowContent.parent.lastSavedRoomKey = trimmed;
                                apiKeyRowContent.parent.hasRoomKey = trimmed.length > 0;
                            }
                        }

                        Components.ImageButton {
                            Layout.preferredWidth: Math.round(Settings.uiFontSizePt * 2)
                            Layout.preferredHeight: Math.round(Settings.uiFontSizePt * 2)
                            Layout.alignment: Qt.AlignVCenter
                            buttonTextColor: apiKeyRowHover.hovered ? palette.brightText : palette.buttonText
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
            }

            // Model override
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: modelRowContent.implicitHeight
                HoverHandler { id: modelRowHover; blocking: false }
                Rectangle { anchors.fill: modelRowContent; color: modelRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                ColumnLayout {
                    id: modelRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Model")
                            color: modelRowHover.hovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                        }

                        Item { Layout.fillWidth: true }

                        ToggleButton {
                            id: modelOverrideToggle
                            textColor: modelRowHover.hovered ? palette.brightText : palette.buttonText
                            checked: preferencesTab._hasTranscriptionOverride("model")
                            onToggled: {
                                if (!preferencesTab.currentRoomId) return;
                                if (checked) {
                                    Settings.setIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId, "model",
                                        Settings.integrationsTranscriptionModel);
                                } else {
                                    Settings.clearIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId, "model");
                                }
                            }
                        }
                    }

                    Label {
                        visible: !modelOverrideToggle.checked
                        text: {
                            var v = Settings.integrationsTranscriptionModel;
                            return v.length > 0
                                ? qsTr("Inherited: %1").arg(v)
                                : qsTr("Inherited: provider default");
                        }
                        color: modelRowHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: true
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                    }

                    Components.KomaiTextField {
                        visible: modelOverrideToggle.checked
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        text: preferencesTab._resolvedTranscriptionField("model")
                        placeholderText: preferencesTab._resolvedTranscriptionField("provider") === "openai_realtime"
                            ? qsTr("Example: gpt-4o-mini-transcribe")
                            : qsTr("Example: whisper-1")
                        onEditingFinished: {
                            if (!preferencesTab.currentRoomId) return;
                            Settings.setIntegrationsTranscriptionOverrideForRoom(
                                preferencesTab.currentRoomId, "model", text.trim());
                        }
                    }
                }
            }

            // Language override
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: languageRowContent.implicitHeight
                HoverHandler { id: languageRowHover; blocking: false }
                Rectangle { anchors.fill: languageRowContent; color: languageRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                ColumnLayout {
                    id: languageRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Language")
                            color: languageRowHover.hovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                        }

                        Item { Layout.fillWidth: true }

                        ToggleButton {
                            id: languageOverrideToggle
                            textColor: languageRowHover.hovered ? palette.brightText : palette.buttonText
                            checked: preferencesTab._hasTranscriptionOverride("language")
                            onToggled: {
                                if (!preferencesTab.currentRoomId) return;
                                if (checked) {
                                    Settings.setIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId, "language",
                                        Settings.integrationsTranscriptionLanguage);
                                } else {
                                    Settings.clearIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId, "language");
                                }
                            }
                        }
                    }

                    Label {
                        visible: !languageOverrideToggle.checked
                        text: {
                            var v = Settings.integrationsTranscriptionLanguage;
                            return v.length > 0
                                ? qsTr("Inherited: %1").arg(v)
                                : qsTr("Inherited: autodetect");
                        }
                        color: languageRowHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: true
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                    }

                    Components.KomaiTextField {
                        visible: languageOverrideToggle.checked
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        text: preferencesTab._resolvedTranscriptionField("language")
                        placeholderText: qsTr("Example: en (leave empty to autodetect)")
                        onEditingFinished: {
                            if (!preferencesTab.currentRoomId) return;
                            Settings.setIntegrationsTranscriptionOverrideForRoom(
                                preferencesTab.currentRoomId, "language", text.trim());
                        }
                    }
                }
            }

            // Prompt override (multiline)
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: promptRowContent.implicitHeight
                HoverHandler { id: promptRowHover; blocking: false }
                Rectangle { anchors.fill: promptRowContent; color: promptRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                ColumnLayout {
                    id: promptRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Prompt")
                            color: promptRowHover.hovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                        }

                        Item { Layout.fillWidth: true }

                        ToggleButton {
                            id: promptOverrideToggle
                            textColor: promptRowHover.hovered ? palette.brightText : palette.buttonText
                            checked: preferencesTab._hasTranscriptionOverride("prompt")
                            onToggled: {
                                if (!preferencesTab.currentRoomId) return;
                                if (checked) {
                                    Settings.setIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId, "prompt",
                                        Settings.integrationsTranscriptionPrompt);
                                } else {
                                    Settings.clearIntegrationsTranscriptionOverrideForRoom(
                                        preferencesTab.currentRoomId, "prompt");
                                }
                            }
                        }
                    }

                    Label {
                        visible: !promptOverrideToggle.checked
                        text: {
                            var v = Settings.integrationsTranscriptionPrompt;
                            return v.length > 0
                                ? qsTr("Inherited: %1").arg(v.length > 80 ? v.substring(0, 80) + "…" : v)
                                : qsTr("Inherited: no prompt");
                        }
                        color: promptRowHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: true
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                    }

                    Components.KomaiTextArea {
                        visible: promptOverrideToggle.checked
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.preferredHeight: Math.max(implicitHeight, 80)
                        text: preferencesTab._resolvedTranscriptionField("prompt")
                        placeholderText: qsTr("Example: Names: Alice, Bob, Carol. Jargon: Matrix, Komai, federation.")
                        wrapMode: TextEdit.Wrap
                        onEditingFinished: {
                            if (!preferencesTab.currentRoomId) return;
                            // Don't trim — multiline prompts may have intentional leading newlines.
                            Settings.setIntegrationsTranscriptionOverrideForRoom(
                                preferencesTab.currentRoomId, "prompt", text);
                        }
                    }
                }
            }
        }
    }
}
