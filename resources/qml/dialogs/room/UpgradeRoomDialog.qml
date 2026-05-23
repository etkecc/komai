// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

OverlayDialog {
    id: upgradeRoomRoot

    required property string roomId
    property string currentVersion: ""

    readonly property var roomPreview: Rooms.getRoomPreviewById(roomId)
    readonly property string roomName: roomPreview ? roomPreview.roomName : ""
    readonly property bool isSpace: !!roomPreview && roomPreview.isSpace

    // Versions come from the homeserver's `m.room_versions` capability.
    // While the capability fetch is in flight (or if it failed) we fall back
    // to a single-entry list containing the local default so the dropdown
    // still has something to render — callers can still type a numeric
    // version into `/upgraderoom` if they want a non-listed one.
    readonly property string localFallbackVersion: "12"
    readonly property bool versionsLoaded: TimelineManager.roomVersionsCapabilityLoaded
    readonly property var availableVersions: versionsLoaded
        ? TimelineManager.stableRoomVersions
        : [localFallbackVersion]
    // Same list as `availableVersions`, with the room's current version
    // tagged "(current)" so the dropdown surfaces where you're starting from
    // and lower-numbered choices read as downgrades instead of upgrades.
    readonly property var displayVersions: availableVersions.map(function(v) {
        return v === upgradeRoomRoot.currentVersion ? qsTr("%1 (current)").arg(v) : v;
    })
    readonly property string defaultNewVersion: versionsLoaded
        ? TimelineManager.defaultRoomVersion
        : localFallbackVersion

    property string chosenVersion: defaultNewVersion

    readonly property bool isDowngrade: {
        const cn = parseInt(upgradeRoomRoot.currentVersion, 10);
        const nn = parseInt(upgradeRoomRoot.chosenVersion, 10);
        return Number.isFinite(cn) && Number.isFinite(nn) && nn < cn;
    }
    readonly property bool supportsAdditionalCreators: {
        const n = parseInt(chosenVersion, 10);
        return Number.isFinite(n) && n >= 12;
    }

    // List of { mxid, displayName, avatarUrl } objects so the selected-list
    // delegates can render avatars like the InviteDialog picker does.
    property var additionalCreators: []
    readonly property int selectedCreatorCount: additionalCreators.length

    title: roomName
        ? (isSpace ? qsTr("Upgrade the %1 space?").arg(roomName)
                   : qsTr("Upgrade the %1 room?").arg(roomName))
        : (isSpace ? qsTr("Upgrade this space?") : qsTr("Upgrade this room?"))
    titleIcon: ":/icons/icons/ui/refresh.svg"
    overlayDialogMinWidth: 720

    onOpened: {
        // Make sure the version list is populated.  Initial sync usually
        // triggers this, but cover the case where the user opens the dialog
        // before that has happened.
        if (!TimelineManager.roomVersionsCapabilityLoaded)
            TimelineManager.refreshRoomVersionsCapability();
    }

    onClosed: {
        creatorEntry.clear();
        userDirectory.setSearchString("");
    }

    function creatorsContainsMxid(mxid) {
        for (let i = 0; i < additionalCreators.length; i++) {
            if (additionalCreators[i].mxid === mxid)
                return true;
        }
        return false;
    }

    function addCreator(mxid, displayName, avatarUrl) {
        const trimmed = (mxid || "").trim();
        if (!trimmed.match("@.+?:.{3,}"))
            return false;
        if (creatorsContainsMxid(trimmed))
            return false;
        const next = additionalCreators.slice();
        next.push({
            "mxid": trimmed,
            "displayName": displayName || "",
            "avatarUrl": avatarUrl || ""
        });
        additionalCreators = next;
        creatorEntry.clear();
        userDirectory.setSearchString("");
        creatorEntry.forceActiveFocus();
        return true;
    }

    function addCurrentCreatorFromSearch() {
        if (creatorEntry.isValidMxid)
            return addCreator(creatorEntry.resolvedMxid, "", "");

        if (creatorResults.count > 0) {
            const first = creatorResults.itemAtIndex(0);
            if (first && first.enabled)
                return addCreator(first.userIdText, first.displayNameText, first.avatarUrlText);
        }
        return false;
    }

    function removeCreator(mxid) {
        additionalCreators = additionalCreators.filter(function (c) {
            return c.mxid !== mxid;
        });
    }

    function selectedCreatorMxids() {
        return additionalCreators.map(function(c) { return c.mxid; });
    }

    function submit() {
        const mxids = selectedCreatorMxids();
        upgradeRoomRoot.close();
        TimelineManager.performRoomUpgrade(upgradeRoomRoot.roomId,
                                           upgradeRoomRoot.chosenVersion,
                                           mxids);
    }

    // Warning callout
    Label {
        Layout.fillWidth: true
        color: Komai.theme.attention
        wrapMode: Text.WordWrap
        text: qsTr("Upgrading replaces this room with a new one. The old room stays as a read-only archive with a pointer to the new room.")
    }

    // Current version
    Label {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingSmall
        text: qsTr("Current version")
        color: palette.text
        font.bold: true
        font.pointSize: Settings.uiFontSizePt * 1.1
    }

    Label {
        Layout.fillWidth: true
        text: upgradeRoomRoot.currentVersion || qsTr("unknown")
        color: upgradeRoomRoot.currentVersion ? palette.text : palette.buttonText
        font.italic: !upgradeRoomRoot.currentVersion
        font.pointSize: Settings.uiFontSizePt
    }

    // New version
    Label {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingSmall
        text: qsTr("New version")
        color: palette.text
        font.bold: true
        font.pointSize: Settings.uiFontSizePt * 1.1
    }

    KomaiComboBox {
        id: versionCombo

        Layout.fillWidth: true
        model: upgradeRoomRoot.displayVersions
        currentIndex: Math.max(0, upgradeRoomRoot.availableVersions.indexOf(upgradeRoomRoot.chosenVersion))
        onActivated: function(index) {
            const v = upgradeRoomRoot.availableVersions[index];
            if (v !== undefined)
                upgradeRoomRoot.chosenVersion = v;
        }
    }

    Label {
        Layout.fillWidth: true
        visible: upgradeRoomRoot.isDowngrade
        color: Komai.theme.attention
        wrapMode: Text.WordWrap
        text: qsTr("You're switching to an older room version. This removes features supported in v%1.").arg(upgradeRoomRoot.currentVersion)
    }

    // Additional creators (v12+ only)
    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingSmall
        spacing: Komai.paddingSmall
        visible: upgradeRoomRoot.supportsAdditionalCreators

        RowLayout {
            Layout.fillWidth: true
            spacing: Komai.paddingSmall

            Label {
                text: qsTr("Additional creators")
                color: palette.text
                font.bold: true
                font.pointSize: Settings.uiFontSizePt * 1.1
            }

            // Same pill style as the "Space" badge in the room directory.
            Rectangle {
                id: optionalBadge
                readonly property color badgeColor: palette.buttonText
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: optionalBadgeLabel.implicitWidth + Komai.paddingSmall * 2
                implicitHeight: optionalBadgeLabel.implicitHeight + Komai.paddingSmall * 0.5
                radius: Komai.paddingSmall
                color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
                border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
                border.width: 1

                Label {
                    id: optionalBadgeLabel
                    anchors.centerIn: parent
                    text: qsTr("Optional")
                    color: optionalBadge.badgeColor
                    font.pointSize: Settings.uiFontSizePt * 0.8
                }
            }

            Item { Layout.fillWidth: true }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: palette.buttonText
            text: qsTr("In room version 12 and newer, listed users receive infinite, immutable Creator-level power in the new room.")
        }

        // Selected creators
        ListView {
            id: selectedCreatorsList

            Layout.fillWidth: true
            Layout.preferredHeight: upgradeRoomRoot.selectedCreatorCount > 0
                ? Math.min(180,
                           Math.max(Komai.iconSize + Komai.paddingMedium * 2,
                                    selectedCreatorsList.contentHeight + Komai.paddingSmall * 2))
                : 0
            visible: upgradeRoomRoot.selectedCreatorCount > 0
            clip: true
            spacing: Komai.paddingSmall
            model: upgradeRoomRoot.additionalCreators

            delegate: Rectangle {
                id: selectedCreatorRow
                required property var modelData

                width: ListView.view.width
                implicitHeight: selectedCreatorRowContent.implicitHeight + Komai.paddingSmall * 2
                color: palette.window
                radius: Komai.paddingMedium
                border.color: Komai.theme.separator
                border.width: 1

                RowLayout {
                    id: selectedCreatorRowContent

                    anchors.fill: parent
                    anchors.margins: Komai.paddingSmall
                    spacing: Komai.paddingMedium

                    Avatar {
                        Layout.preferredWidth: Komai.iconSize
                        Layout.preferredHeight: Komai.iconSize
                        Layout.alignment: Qt.AlignVCenter
                        userid: selectedCreatorRow.modelData.mxid
                        displayName: selectedCreatorRow.modelData.displayName
                        url: (selectedCreatorRow.modelData.avatarUrl || "").replace("mxc://", "image://MxcImage/")
                        enabled: false
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall

                        Label {
                            Layout.fillWidth: true
                            text: selectedCreatorRow.modelData.displayName || qsTr("Unknown display name")
                            color: selectedCreatorRow.modelData.displayName ? palette.text : palette.buttonText
                            font.italic: !selectedCreatorRow.modelData.displayName
                            font.pointSize: Settings.uiFontSizePt
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: selectedCreatorRow.modelData.mxid
                            color: palette.buttonText
                            font.pointSize: Settings.uiFontSizePt * 0.9
                            elide: Text.ElideRight
                        }
                    }

                    KomaiButton {
                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("Remove")
                        icon.source: "qrc:/icons/icons/ui/delete.svg"
                        onClicked: upgradeRoomRoot.removeCreator(selectedCreatorRow.modelData.mxid)
                    }
                }
            }
        }

        // Search field
        KomaiTextField {
            id: creatorEntry

            readonly property string localHomeserver: {
                const uid = Settings.userId;
                const colonIdx = uid.indexOf(":");
                return colonIdx >= 0 ? uid.substring(colonIdx + 1) : "";
            }

            function normalizedMxid(input) {
                var t = input.trim();
                if (t.length === 0)
                    return "";
                if (t.charAt(0) !== "@")
                    t = "@" + t;
                if (t.indexOf(":") < 0 && localHomeserver.length > 0)
                    t = t + ":" + localHomeserver;
                return t;
            }

            property string resolvedMxid: normalizedMxid(text)
            property bool isValidMxid: resolvedMxid.match("@.+?:.{3,}")

            Layout.fillWidth: true
            placeholderText: qsTr("Search by name or @user:example.com")
            onAccepted: upgradeRoomRoot.addCurrentCreatorFromSearch()
            onTextChanged: {
                if (text.trim().length === 0)
                    userDirectory.setSearchString("");
                else
                    creatorSearchTimer.restart();
            }

            Timer {
                id: creatorSearchTimer
                interval: 350
                onTriggered: {
                    if (creatorEntry.text.trim().length > 0)
                        userDirectory.setSearchString(creatorEntry.text.trim());
                }
            }
        }

        // Direct MXID add card (shown when input is a valid MXID and not already added)
        AbstractButton {
            id: directAddCard

            readonly property bool activeState: hovered || pressed
            readonly property bool alreadyAdded: creatorEntry.isValidMxid
                && upgradeRoomRoot.creatorsContainsMxid(creatorEntry.resolvedMxid)
            property string resolvedDisplayName: ""
            property string resolvedAvatarUrl: ""

            Layout.fillWidth: true
            visible: creatorEntry.isValidMxid && !alreadyAdded
            implicitHeight: directAddRow.implicitHeight + Komai.paddingSmall * 2
            onClicked: upgradeRoomRoot.addCreator(creatorEntry.resolvedMxid,
                directAddCard.resolvedDisplayName,
                directAddCard.resolvedAvatarUrl)

            Connections {
                target: creatorEntry
                function onResolvedMxidChanged() {
                    directAddCard.resolvedDisplayName = "";
                    directAddCard.resolvedAvatarUrl = "";
                    if (creatorEntry.isValidMxid)
                        userDirectory.resolveUser(creatorEntry.resolvedMxid);
                }
            }

            Connections {
                target: userDirectory
                function onUserResolved(mxid, displayName, avatarUrl) {
                    if (mxid === creatorEntry.resolvedMxid) {
                        directAddCard.resolvedDisplayName = displayName;
                        directAddCard.resolvedAvatarUrl = avatarUrl;
                    }
                }
            }

            background: Rectangle {
                radius: Komai.paddingMedium
                color: directAddCard.activeState ? palette.dark : palette.window
                border.color: Komai.theme.separator
                border.width: 1
            }

            contentItem: RowLayout {
                id: directAddRow
                spacing: Komai.paddingMedium

                Avatar {
                    Layout.preferredWidth: Komai.iconSize
                    Layout.preferredHeight: Komai.iconSize
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: Komai.paddingMedium
                    userid: creatorEntry.resolvedMxid
                    displayName: directAddCard.resolvedDisplayName
                    url: (directAddCard.resolvedAvatarUrl || "").replace("mxc://", "image://MxcImage/")
                    enabled: false
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        text: directAddCard.resolvedDisplayName || qsTr("Add directly")
                        color: directAddCard.activeState ? palette.brightText : palette.text
                        font.italic: !directAddCard.resolvedDisplayName
                        font.pointSize: Settings.uiFontSizePt
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: creatorEntry.resolvedMxid
                        color: directAddCard.activeState ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 0.9
                        elide: Text.ElideRight
                    }
                }

                Image {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    Layout.rightMargin: Komai.paddingMedium
                    fillMode: Image.PreserveAspectFit
                    source: "image://colorimage/:/icons/icons/ui/plus-circle.svg?"
                        + (directAddCard.activeState ? palette.brightText : palette.buttonText)
                }
            }

            KomaiCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
        }

        // Live search results
        ListView {
            id: creatorResults

            Layout.fillWidth: true
            Layout.preferredHeight: 220
            model: userDirectory
            clip: true

            FlickableWheelBooster { flickable: creatorResults }

            delegate: AbstractButton {
                id: creatorResultRow

                readonly property bool activeState: hovered || pressed
                readonly property bool alreadySelected: upgradeRoomRoot.creatorsContainsMxid(model.userid)
                property string userIdText: model.userid
                property string displayNameText: model.displayName
                property string avatarUrlText: model.avatarUrl

                width: ListView.view.width
                implicitHeight: creatorResultRowContent.implicitHeight + Komai.paddingSmall * 2
                hoverEnabled: !alreadySelected
                enabled: !alreadySelected
                opacity: alreadySelected ? 0.7 : 1
                onClicked: upgradeRoomRoot.addCreator(userIdText, displayNameText, avatarUrlText)

                background: Rectangle {
                    radius: Komai.paddingMedium
                    color: creatorResultRow.alreadySelected
                        ? palette.window
                        : creatorResultRow.activeState ? palette.dark : "transparent"
                    border.color: creatorResultRow.alreadySelected ? Komai.theme.separator : "transparent"
                    border.width: creatorResultRow.alreadySelected ? 1 : 0
                }

                contentItem: RowLayout {
                    id: creatorResultRowContent
                    spacing: Komai.paddingMedium

                    Avatar {
                        Layout.preferredWidth: Komai.iconSize
                        Layout.preferredHeight: Komai.iconSize
                        Layout.alignment: Qt.AlignVCenter
                        Layout.leftMargin: Komai.paddingMedium
                        userid: creatorResultRow.userIdText
                        url: (creatorResultRow.avatarUrlText || "").replace("mxc://", "image://MxcImage/")
                        displayName: creatorResultRow.displayNameText
                        enabled: false
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall

                        Label {
                            Layout.fillWidth: true
                            text: creatorResultRow.displayNameText || qsTr("Unknown display name")
                            color: creatorResultRow.activeState
                                ? palette.brightText
                                : creatorResultRow.displayNameText ? palette.text : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            font.italic: !creatorResultRow.displayNameText
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: creatorResultRow.userIdText
                            color: creatorResultRow.activeState ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt * 0.9
                            elide: Text.ElideRight
                        }
                    }

                    Image {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: visible ? 18 : 0
                        Layout.preferredHeight: visible ? 18 : 0
                        Layout.rightMargin: Komai.paddingMedium
                        visible: creatorResultRow.alreadySelected
                        fillMode: Image.PreserveAspectFit
                        source: visible
                            ? "image://colorimage/:/icons/icons/ui/double-checkmark.svg?" + palette.buttonText
                            : ""
                    }
                }

                KomaiCursorShape {
                    anchors.fill: parent
                    cursorShape: creatorResultRow.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            Label {
                anchors.centerIn: parent
                visible: creatorResults.count === 0 && creatorEntry.text.trim().length === 0
                text: qsTr("Type a name or Matrix ID to search.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 0.9
            }

            Column {
                anchors.centerIn: parent
                spacing: Komai.paddingSmall
                visible: creatorResults.count === 0
                    && creatorEntry.text.trim().length > 0
                    && !creatorSearchTimer.running
                    && !userDirectory.searchingUsers

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("No matching users found.")
                    color: palette.buttonText
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: creatorEntry.isValidMxid
                    text: qsTr("Use the suggestion above to add by Matrix ID.")
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                }
            }

            Spinner {
                anchors.centerIn: parent
                height: 48
                running: creatorResults.count === 0
                    && creatorEntry.text.trim().length > 0
                    && (creatorSearchTimer.running || userDirectory.searchingUsers)
                visible: running
            }
        }
    }

    // Cancel / Upgrade
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingMedium
        spacing: Komai.paddingMedium

        KomaiButton {
            text: qsTr("Cancel")
            onClicked: upgradeRoomRoot.close()
        }

        Item { Layout.fillWidth: true }

        KomaiButton {
            text: qsTr("Upgrade")
            highlighted: true
            onClicked: upgradeRoomRoot.submit()
        }
    }
}
