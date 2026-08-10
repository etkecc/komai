// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../../components" as Components
import "../../../ui"
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import cc.etke.komai 1.0

Item {
    id: exportTab

    property var roomSettings
    property var members
    property var room
    property var appRoot

    // 0 = plain text, 1 = HTML, 2 = JSONL
    property int formatIndex: 0
    property bool includeMetadata: true
    // -1 while no export has completed; the exported message count afterwards.
    property int exportedCount: -1
    property int undecryptableCount: 0
    property string errorText: ""
    property bool wasCancelled: false

    // Exports are per-room jobs on the ChatExport singleton; this tab tracks
    // only its own room's job, so exports for other rooms neither show here
    // nor block starting one for this room.
    readonly property string ownRoomId: roomSettings ? roomSettings.roomId : ""
    property bool exportBusy: false
    property int roomFetched: 0
    readonly property bool finished: exportedCount >= 0

    function refreshExportState() {
        exportBusy = ownRoomId.length > 0 && ChatExport.isExporting(ownRoomId);
        roomFetched = ownRoomId.length > 0 ? ChatExport.fetchedCount(ownRoomId) : 0;
    }

    onOwnRoomIdChanged: refreshExportState()
    Component.onCompleted: refreshExportState()

    // Settings-style card: rounded palette.window background behind the
    // content, so the buttonText-colored descriptions stay readable against
    // the tab's alternateBase background.
    component ExportCard: Item {
        id: card

        default property alias content: cardContent.data

        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: cardContent.implicitHeight + Komai.paddingMedium * 2

        Rectangle {
            anchors.fill: parent
            color: palette.window
            radius: Komai.paddingMedium
            z: -1
        }

        ColumnLayout {
            id: cardContent

            anchors.fill: parent
            anchors.margins: Komai.paddingMedium
            spacing: Komai.paddingSmall
        }
    }

    // e.g. "2026-08-10-1518-matrix-room-abc123-example.com-chat-export.txt";
    // same stamp-first shape as the encryption key export's suggested name.
    function suggestedFileName(extension) {
        const stamp = Qt.formatDateTime(new Date(), "yyyy-MM-dd-HHmm");
        const roomId = String(exportTab.roomSettings ? exportTab.roomSettings.roomId : "")
            .replace(/^!/, "")
            .replace(/[^A-Za-z0-9._-]/g, "-");
        return stamp + "-matrix-room-" + roomId + "-chat-export." + extension;
    }

    function openSaveDialog() {
        exportedCount = -1;
        undecryptableCount = 0;
        errorText = "";
        wasCancelled = false;
        const dialog = formatIndex === 1 ? saveHtmlDialog
                     : formatIndex === 2 ? saveJsonlDialog
                     : saveTextDialog;
        const extension = formatIndex === 1 ? "html" : formatIndex === 2 ? "jsonl" : "txt";
        dialog.selectedFile = dialog.currentFolder + "/" + suggestedFileName(extension);
        dialog.open();
    }

    function startExport(selectedFile, format) {
        ChatExport.startExport(
            exportTab.roomSettings.roomId,
            exportTab.roomSettings.plainRoomName,
            selectedFile,
            format,
            exportTab.includeMetadata);
    }

    Connections {
        function onExportStarted(roomId) {
            if (roomId === exportTab.ownRoomId)
                exportTab.refreshExportState();
        }
        function onProgressChanged(roomId, fetchedCount) {
            if (roomId === exportTab.ownRoomId)
                exportTab.roomFetched = fetchedCount;
        }
        function onExportCompleted(roomId, messageCount, utdCount) {
            if (roomId !== exportTab.ownRoomId)
                return;
            exportTab.exportedCount = messageCount;
            exportTab.undecryptableCount = utdCount;
            exportTab.errorText = "";
            exportTab.wasCancelled = false;
            exportTab.refreshExportState();
        }
        function onExportFailed(roomId, error) {
            if (roomId !== exportTab.ownRoomId)
                return;
            exportTab.errorText = error;
            exportTab.wasCancelled = false;
            exportTab.refreshExportState();
        }
        function onExportCancelled(roomId) {
            if (roomId !== exportTab.ownRoomId)
                return;
            exportTab.wasCancelled = true;
            exportTab.refreshExportState();
        }

        target: ChatExport
    }

    ScrollView {
        id: scrollView

        anchors.fill: parent
        ScrollBar.vertical.policy: Settings.uiScrollbarPolicy === Settings.ScrollbarPolicy.Always ? ScrollBar.AlwaysOn : Settings.uiScrollbarPolicy === Settings.ScrollbarPolicy.Never ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: Komai.paddingMedium

            ExportCard {
                Layout.topMargin: Komai.paddingMedium
                enabled: !exportTab.exportBusy

                Label {
                    text: qsTr("Export chat history")
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("Saves this room's entire message history, including decrypted messages, into a single file on this computer. Media is included as links, not downloaded. Depending on the room's size, this can take a while.")
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                    Layout.fillWidth: true
                }
            }

            ExportCard {
                enabled: !exportTab.exportBusy

                Label {
                    text: qsTr("Format")
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                Components.SegmentedButton {
                    Layout.fillWidth: true
                    implicitHeight: Math.max(36, Math.round(Settings.uiFontSizePt * 2.7))
                    enabled: !exportTab.exportBusy
                    model: [
                        { text: qsTr("Plain text (.txt)") },
                        { text: qsTr("Web page (.html)") },
                        { text: qsTr("Machine-readable (.jsonl)") }
                    ]
                    currentIndex: exportTab.formatIndex
                    onActivated: index => {
                        exportTab.formatIndex = index;
                    }
                }

                // TextEdit (not Label) so the embedded <a> links render in
                // the Link palette role; a plain Text hardcodes link blue.
                TextEdit {
                    id: formatHint

                    text: {
                        if (exportTab.formatIndex === 1)
                            return qsTr("A single self-contained web page, readable in any browser.");
                        if (exportTab.formatIndex === 2)
                            return qsTr("One JSON object per line (<a href=\"https://jsonlines.org/\">JSON Lines</a>), for scripts and tools like <a href=\"https://jqlang.org/\">jq</a>. Timestamps are UTC and field values are language-independent.");
                        return qsTr("A simple transcript with one message per line, readable in any text editor.");
                    }
                    wrapMode: Text.Wrap
                    textFormat: Text.AutoText
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                    readOnly: true
                    selectByMouse: true
                    Layout.fillWidth: true

                    onLinkActivated: function(link) { Qt.openUrlExternally(link) }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        cursorShape: formatHint.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }
            }

            ExportCard {
                enabled: !exportTab.exportBusy

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Komai.paddingMedium

                    Label {
                        text: qsTr("Include export details")
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        Layout.fillWidth: true
                    }

                    Components.SettingControlToggle {
                        value: exportTab.includeMetadata
                        enabled: !exportTab.exportBusy
                        onToggledValue: value => {
                            exportTab.includeMetadata = value;
                        }
                    }
                }

                Label {
                    text: qsTr("Starts the file with a short header saying which room this is, when the export was made, and by whom.")
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                    Layout.fillWidth: true
                }
            }

            // Progress block, shown beneath the (disabled) cards while an
            // export runs, so the chosen options stay visible for
            // confirmation: prominent spinner, live counter, and Cancel.
            ColumnLayout {
                visible: exportTab.exportBusy
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingLarge
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                spacing: Komai.paddingMedium

                Spinner {
                    running: exportTab.exportBusy
                    Layout.preferredHeight: 48
                    Layout.preferredWidth: 48
                    Layout.alignment: Qt.AlignHCenter
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Exporting… fetched %n message(s) so far.", "", exportTab.roomFetched)
                    textFormat: Text.PlainText
                    color: palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("The export keeps running if you close this dialog.")
                    textFormat: Text.PlainText
                    color: palette.text
                    font.pointSize: Settings.uiFontSizePt
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Components.KomaiButton {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Cancel export")
                    onClicked: ChatExport.cancel(exportTab.ownRoomId)
                }
            }

            // Status line for the finished/failed/cancelled states. Always
            // reserves one line of height so the buttons don't shift.
            Label {
                visible: !exportTab.exportBusy
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                color: {
                    if (exportTab.errorText.length > 0)
                        return Komai.theme.error;
                    if (exportTab.finished)
                        return Komai.theme.success;
                    return palette.text;
                }
                text: {
                    if (exportTab.errorText.length > 0)
                        return qsTr("Export failed: %1").arg(exportTab.errorText);
                    if (exportTab.finished) {
                        let status = qsTr("Exported %n message(s).", "", exportTab.exportedCount);
                        if (exportTab.undecryptableCount > 0)
                            status += " " + qsTr("%n message(s) could not be decrypted.", "", exportTab.undecryptableCount);
                        return status;
                    }
                    if (exportTab.wasCancelled)
                        return qsTr("Export cancelled.");
                    return "";
                }
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }

            RowLayout {
                visible: !exportTab.exportBusy
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                spacing: Komai.paddingSmall

                Item {
                    Layout.fillWidth: true
                }

                Components.KomaiButton {
                    icon.source: "qrc:/icons/icons/ui/download.svg"
                    text: qsTr("Export…")
                    highlighted: true
                    enabled: !!exportTab.roomSettings
                    onClicked: exportTab.openSaveDialog()
                }
            }
        }
    }

    // One dialog per format: static defaultSuffix/nameFilters are required
    // for the pre-seeded suggested filename to survive the XDG portal
    // (dynamic per-format bindings on a shared dialog lose the filename).
    Dialogs.FileDialog {
        id: saveTextDialog

        fileMode: Dialogs.FileDialog.SaveFile
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        defaultSuffix: "txt"
        nameFilters: [qsTr("Text files (*.txt)"), qsTr("All files (*)")]
        title: qsTr("Export chat history to file")
        onAccepted: exportTab.startExport(selectedFile, ChatExport.Format.PlainText)
    }

    Dialogs.FileDialog {
        id: saveHtmlDialog

        fileMode: Dialogs.FileDialog.SaveFile
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        defaultSuffix: "html"
        nameFilters: [qsTr("Web pages (*.html)"), qsTr("All files (*)")]
        title: qsTr("Export chat history to file")
        onAccepted: exportTab.startExport(selectedFile, ChatExport.Format.Html)
    }

    Dialogs.FileDialog {
        id: saveJsonlDialog

        fileMode: Dialogs.FileDialog.SaveFile
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        defaultSuffix: "jsonl"
        nameFilters: [qsTr("JSON Lines files (*.jsonl)"), qsTr("All files (*)")]
        title: qsTr("Export chat history to file")
        onAccepted: exportTab.startExport(selectedFile, ChatExport.Format.JsonLines)
    }
}
