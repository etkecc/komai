// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: localCacheSection

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight
    width: parent ? parent.width : 0

    property var cacheInfo: ({})
    property string mediaHint: qsTr("Files can be purged freely. The cache regenerates over time.")
    property color mediaHintColor: Komai.theme.success
    readonly property real controlWidth: Math.min(500, Math.max(240, width - Komai.paddingLarge * 2))
    readonly property real directoryControlWidth: Math.min(880, Math.max(360, width - 320))
    readonly property real noteWidth: Math.min(width, directoryControlWidth + 180)
    readonly property bool showDatabaseStatusBadge: (cacheInfo.statusKind || "") !== ""
        && (cacheInfo.statusKind || "") !== "ready"
    readonly property bool showFormatRow: (cacheInfo.cacheFormat || "") !== ""
        && (cacheInfo.cacheFormat || "") !== qsTr("Current")

    function refreshLocalCacheInfo() {
        cacheInfo = Komai.localCacheInfo();
    }

    function statusTone(kind) {
        switch (kind) {
        case "ready":
            return Komai.theme.success;
        case "loading":
        case "empty":
            return Komai.theme.warning;
        case "reset_required":
        case "error":
            return Komai.theme.attention;
        default:
            return palette.buttonText;
        }
    }

    function statusDetailColor(kind) {
        switch (kind) {
        case "reset_required":
        case "error":
            return Komai.theme.attention;
        default:
            return palette.buttonText;
        }
    }

    function purgeMediaCache() {
        const error = Komai.purgeMediaCache();
        refreshLocalCacheInfo();

        if (error && error.length > 0) {
            mediaHint = error;
            mediaHintColor = Komai.theme.attention;
            purgeMediaButton.purged = false;
            return;
        }

        mediaHint = qsTr("Files can be purged freely. The cache regenerates over time.");
        mediaHintColor = Komai.theme.success;
        purgeMediaButton.purged = true;
        purgeMediaFeedbackTimer.restart();
    }

    Component.onCompleted: refreshLocalCacheInfo()

    component StatusBadge: Rectangle {
        id: statusBadge

        required property string text
        required property color tone

        readonly property color badgeTextColor: tone

        implicitWidth: badgeRow.implicitWidth + Komai.paddingSmall * 2
        implicitHeight: badgeRow.implicitHeight + Komai.paddingSmall
        radius: Komai.paddingSmall
        color: Qt.rgba(tone.r, tone.g, tone.b, 0.15)
        border.color: tone
        border.width: 1

        RowLayout {
            id: badgeRow

            anchors.centerIn: parent
            spacing: Komai.paddingSmall

            Label {
                text: statusBadge.text
                color: statusBadge.badgeTextColor
                font.pointSize: Math.floor(Settings.uiFontSizePt * 0.85)
            }
        }
    }

    component CardValueRow: Item {
        id: cardValueRow

        required property string label
        required property string value
        property color valueColor: palette.text

        Layout.fillWidth: true
        implicitHeight: row.implicitHeight

        RowLayout {
            id: row

            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Komai.paddingSmall

            Label {
                text: cardValueRow.label
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
            }

            Text {
                text: cardValueRow.value
                color: cardValueRow.valueColor
                font.pointSize: Settings.uiFontSizePt
                Layout.preferredWidth: localCacheSection.controlWidth
                Layout.maximumWidth: localCacheSection.controlWidth
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.Wrap
            }
        }
    }

    component CardLinkValueRow: Item {
        id: cardLinkValueRow

        required property string label
        required property string value
        required property string link

        Layout.fillWidth: true
        implicitHeight: row.implicitHeight

        RowLayout {
            id: row

            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Komai.paddingSmall

            Label {
                text: cardLinkValueRow.label
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
            }

            Text {
                text: "<a href=\"" + cardLinkValueRow.link + "\">" + cardLinkValueRow.value + "</a>"
                textFormat: Text.RichText
                linkColor: palette.highlight
                font.pointSize: Settings.uiFontSizePt
                Layout.preferredWidth: localCacheSection.controlWidth
                Layout.maximumWidth: localCacheSection.controlWidth
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.Wrap
                onLinkActivated: function(link) {
                    Komai.openLink(link);
                }
            }
        }
    }

    component RightAlignedNote: Item {
        id: rightAlignedNote

        required property string text
        property color textColor: palette.buttonText

        Layout.fillWidth: true
        implicitHeight: noteLabel.implicitHeight

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0

            Item {
                Layout.fillWidth: true
            }

            Text {
                id: noteLabel

                text: rightAlignedNote.text
                color: rightAlignedNote.textColor
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignRight
                textFormat: Text.RichText
                linkColor: palette.highlight
                Layout.preferredWidth: localCacheSection.noteWidth
                Layout.maximumWidth: localCacheSection.noteWidth
                onLinkActivated: function(link) {
                    Komai.openLink(link);
                }
            }
        }
    }

    Connections {
        target: Komai

        function onLocalCacheInfoChanged() {
            localCacheSection.refreshLocalCacheInfo();
        }
    }

    Connections {
        target: Settings

        function onProfileChanged() {
            localCacheSection.mediaHint = qsTr("Files can be purged freely. The cache regenerates over time.");
            localCacheSection.mediaHintColor = Komai.theme.success;
            localCacheSection.refreshLocalCacheInfo();
        }

        function onUserIdChanged() {
            localCacheSection.refreshLocalCacheInfo();
        }

        function onSessionAuthStateChanged() {
            localCacheSection.refreshLocalCacheInfo();
        }
    }

    ColumnLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Komai.paddingSmall

        Components.SettingsSection {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingLarge
            Layout.bottomMargin: Komai.paddingSmall
            label: qsTr("Local cache")
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: databaseCard.implicitHeight

            Rectangle {
                id: databaseCard

                width: parent.width
                implicitHeight: databaseCardContent.implicitHeight
                color: palette.window
                radius: Komai.paddingMedium
                border.width: 1
                border.color: Komai.theme.separator
                clip: true

                ColumnLayout {
                    id: databaseCardContent

                    width: parent.width
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        implicitHeight: databaseHeaderRow.implicitHeight + 1

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: Komai.theme.separator
                        }

                        RowLayout {
                            id: databaseHeaderRow

                            width: parent.width
                            spacing: Komai.paddingSmall

                            Item {
                                Layout.preferredWidth: Komai.paddingSmall
                            }

                            Label {
                                text: qsTr("Database")
                                color: palette.text
                                font.bold: true
                                font.pointSize: Settings.uiFontSizePt
                                Layout.alignment: Qt.AlignVCenter
                                Layout.topMargin: Komai.paddingMedium + 2
                                Layout.bottomMargin: Komai.paddingMedium + 2
                            }

                            StatusBadge {
                                visible: localCacheSection.showDatabaseStatusBadge
                                text: localCacheSection.cacheInfo.statusLabel || qsTr("Unknown")
                                tone: localCacheSection.statusTone(localCacheSection.cacheInfo.statusKind || "")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.topMargin: Komai.paddingMedium + 2
                                Layout.bottomMargin: Komai.paddingMedium + 2
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: visible ? Komai.paddingMedium : 0
                        visible: !!localCacheSection.cacheInfo.statusDetails
                        text: localCacheSection.cacheInfo.statusDetails || ""
                        color: localCacheSection.statusDetailColor(localCacheSection.cacheInfo.statusKind || "")
                        font.pointSize: Settings.uiFontSizePt
                        wrapMode: Text.Wrap
                    }

                    CardLinkValueRow {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: localCacheSection.cacheInfo.statusDetails ? Komai.paddingSmall : Komai.paddingMedium
                        label: qsTr("Backend")
                        value: localCacheSection.cacheInfo.backend || qsTr("Unavailable")
                        link: "https://en.wikipedia.org/wiki/Lightning_Memory-Mapped_Database"
                        visible: (localCacheSection.cacheInfo.backend || "") === "LMDB"
                    }

                    CardValueRow {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: localCacheSection.cacheInfo.statusDetails ? Komai.paddingSmall : Komai.paddingMedium
                        label: qsTr("Backend")
                        value: localCacheSection.cacheInfo.backend || qsTr("Unavailable")
                        visible: (localCacheSection.cacheInfo.backend || "") !== "LMDB"
                    }

                    CardValueRow {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        label: qsTr("Size")
                        value: localCacheSection.cacheInfo.databaseSizeHuman || qsTr("Unavailable")
                    }

                    CardValueRow {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        visible: localCacheSection.showFormatRow
                        label: qsTr("Format")
                        value: localCacheSection.cacheInfo.cacheFormat || qsTr("Unavailable")
                    }

                    CardValueRow {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        label: qsTr("Stores")
                        value: localCacheSection.cacheInfo.namedStoresKnown
                            ? String(localCacheSection.cacheInfo.namedStores)
                            : qsTr("Unavailable")
                    }

                    CardValueRow {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        label: qsTr("Joined rooms")
                        value: localCacheSection.cacheInfo.joinedRoomsKnown
                            ? String(localCacheSection.cacheInfo.joinedRooms)
                            : qsTr("Unavailable")
                    }

                    CardValueRow {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        label: qsTr("Invites")
                        value: localCacheSection.cacheInfo.invitesKnown
                            ? String(localCacheSection.cacheInfo.invites)
                            : qsTr("Unavailable")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        spacing: Komai.paddingSmall

                        Label {
                            text: qsTr("Directory")
                            color: palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.fillWidth: true
                        }

                        Components.KomaiTextField {
                            id: databasePathField

                            text: localCacheSection.cacheInfo.databasePath || qsTr("Unavailable until login")
                            readOnly: true
                            font.pointSize: Settings.uiFontSizePt
                            Layout.preferredWidth: localCacheSection.directoryControlWidth
                            Layout.maximumWidth: localCacheSection.directoryControlWidth
                        }

                        Components.ImageButton {
                            id: copyDatabasePathButton

                            property bool copied: false

                            enabled: !!localCacheSection.cacheInfo.databasePath
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                            ToolTip.visible: hovered
                            ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                            onClicked: {
                                Clipboard.text = localCacheSection.cacheInfo.databasePath;
                                copied = true;
                                copyDatabasePathTimer.restart();
                            }

                            Timer {
                                id: copyDatabasePathTimer
                                interval: 2000
                                repeat: false
                                onTriggered: copyDatabasePathButton.copied = false
                            }
                        }

                        Components.KomaiButton {
                            id: browseDatabaseButton
                            text: qsTr("Browse")
                            icon.source: "qrc:/icons/icons/ui/open-externally.svg"
                            enabled: localCacheSection.cacheInfo.databasePathExists === true
                            onClicked: Komai.openLocalPath(localCacheSection.cacheInfo.databasePath)
                        }
                    }

                    RightAlignedNote {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: 2
                        Layout.bottomMargin: Komai.paddingMedium
                        text: qsTr("To purge: log out &amp; log back in. <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/storage.md#database-compaction\">Compaction</a> can be done via CLI.")
                        textColor: Komai.theme.attention
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: mediaCard.implicitHeight

            Rectangle {
                id: mediaCard

                width: parent.width
                implicitHeight: mediaCardContent.implicitHeight
                color: palette.window
                radius: Komai.paddingMedium
                border.width: 1
                border.color: Komai.theme.separator
                clip: true

                ColumnLayout {
                    id: mediaCardContent

                    width: parent.width
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        implicitHeight: mediaHeaderRow.implicitHeight + 1

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: Komai.theme.separator
                        }

                        RowLayout {
                            id: mediaHeaderRow

                            width: parent.width
                            spacing: Komai.paddingSmall

                            Item {
                                Layout.preferredWidth: Komai.paddingSmall
                            }

                            Label {
                                text: qsTr("Media cache")
                                color: palette.text
                                font.bold: true
                                font.pointSize: Settings.uiFontSizePt
                                Layout.alignment: Qt.AlignVCenter
                                Layout.topMargin: Komai.paddingMedium + 2
                                Layout.bottomMargin: Komai.paddingMedium + 2
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Components.KomaiButton {
                                id: purgeMediaButton

                                property bool purged: false

                                text: purged ? qsTr("Purged") : qsTr("Purge")
                                icon.source: purged ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/delete.svg"
                                Layout.topMargin: Komai.paddingMedium + 2
                                Layout.bottomMargin: Komai.paddingMedium + 2
                                Layout.rightMargin: Komai.paddingSmall
                                onClicked: localCacheSection.purgeMediaCache()
                            }

                            Timer {
                                id: purgeMediaFeedbackTimer
                                interval: 1400
                                repeat: false
                                onTriggered: purgeMediaButton.purged = false
                            }
                        }
                    }

                    CardValueRow {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingMedium
                        label: qsTr("Size")
                        value: localCacheSection.cacheInfo.mediaCacheSizeHuman || qsTr("Unavailable")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingSmall
                        spacing: Komai.paddingSmall

                        Label {
                            text: qsTr("Directory")
                            color: palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.fillWidth: true
                        }

                        Components.KomaiTextField {
                            id: mediaPathField

                            text: localCacheSection.cacheInfo.mediaCachePath || ""
                            readOnly: true
                            font.pointSize: Settings.uiFontSizePt
                            Layout.preferredWidth: localCacheSection.directoryControlWidth
                            Layout.maximumWidth: localCacheSection.directoryControlWidth
                        }

                        Components.ImageButton {
                            id: copyMediaPathButton

                            property bool copied: false

                            enabled: !!localCacheSection.cacheInfo.mediaCachePath
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                            ToolTip.visible: hovered
                            ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                            onClicked: {
                                Clipboard.text = localCacheSection.cacheInfo.mediaCachePath;
                                copied = true;
                                copyMediaPathTimer.restart();
                            }

                            Timer {
                                id: copyMediaPathTimer
                                interval: 2000
                                repeat: false
                                onTriggered: copyMediaPathButton.copied = false
                            }
                        }

                        Components.KomaiButton {
                            id: browseMediaButton
                            text: qsTr("Browse")
                            icon.source: "qrc:/icons/icons/ui/open-externally.svg"
                            enabled: localCacheSection.cacheInfo.mediaCachePathExists === true
                            onClicked: Komai.openLocalPath(localCacheSection.cacheInfo.mediaCachePath)
                        }
                    }

                    RightAlignedNote {
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: 2
                        Layout.bottomMargin: Komai.paddingMedium
                        text: localCacheSection.mediaHint
                        textColor: localCacheSection.mediaHintColor
                    }
                }
            }
        }
    }
}
