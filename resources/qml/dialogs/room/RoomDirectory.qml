// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import cc.etke.komai

OverlayDialog {
    id: roomDirectoryRoot

    property Item appRoot: null

    readonly property int largeRoomThreshold: 2000
    readonly property int veryLargeRoomThreshold: 10000

    // Per-tab room size filter combo index: 0 = up to large, 1 = up to very large, 2 = any
    property var sizeFilterPerTab: ({
        [RoomDirectory.ServerMode.Mine]: RoomDirectory.SizeFilter.Any,
        [RoomDirectory.ServerMode.MRS]: RoomDirectory.SizeFilter.UpToLarge,
        [RoomDirectory.ServerMode.Custom]: RoomDirectory.SizeFilter.Any
    })

    function defaultSizeFilterForMode(mode) {
        return mode === RoomDirectory.ServerMode.MRS
            ? RoomDirectory.SizeFilter.UpToLarge
            : RoomDirectory.SizeFilter.Any;
    }

    function sizeFilterValueForIndex(index) {
        switch (index) {
        case RoomDirectory.SizeFilter.UpToLarge: return largeRoomThreshold;
        case RoomDirectory.SizeFilter.UpToVeryLarge: return veryLargeRoomThreshold;
        case RoomDirectory.SizeFilter.Any: return 0;
        default: return 0;
        }
    }

    function applySizeFilter(comboIndex) {
        roomSizeFilter.currentIndex = comboIndex;
        publicRooms.maxMemberFilter = sizeFilterValueForIndex(comboIndex);
    }

    // Per-tab room type filter index: 0 = all, 1 = rooms, 2 = spaces
    property var typeFilterPerTab: ({
        [RoomDirectory.ServerMode.Mine]: RoomDirectory.TypeFilter.All,
        [RoomDirectory.ServerMode.MRS]: RoomDirectory.TypeFilter.All,
        [RoomDirectory.ServerMode.Custom]: RoomDirectory.TypeFilter.All
    })

    function typeFilterValueForIndex(index) {
        switch (index) {
        case RoomDirectory.TypeFilter.Rooms: return "room";
        case RoomDirectory.TypeFilter.Spaces: return "space";
        default: return "";
        }
    }

    function roomSizeWarning(memberCount) {
        if (memberCount >= veryLargeRoomThreshold)
            return qsTr("This room is extremely large. You should probably stay away from it unless you have a very powerful server. Joining may take a very long time.");
        if (memberCount >= largeRoomThreshold)
            return qsTr("This room is large. Joining may take a long time and increase resource usage on your server.");
        return "";
    }

    property Item hoveredMemberBadge: null
    property string hoveredMemberBadgeText: ""

    KomaiToolTip {
        parent: roomDirectoryRoot.contentItem.parent
        anchorItem: roomDirectoryRoot.hoveredMemberBadge
        anchorX: roomDirectoryRoot.hoveredMemberBadge ? roomDirectoryRoot.hoveredMemberBadge.width / 2 : 0
        anchorY: 0
        gapY: Komai.paddingMedium
        preferBelow: false
        text: roomDirectoryRoot.hoveredMemberBadgeText
        delay: 0
        requestedVisible: roomDirectoryRoot.hoveredMemberBadge !== null
    }

    JoinLargeRoomDialog {
        id: joinConfirmDialog

        overlayViewport: roomDirectoryRoot.appRoot

        onConfirmed: function(index) {
            publicRooms.joinRoom(index);
        }
    }

    enum ServerMode { Mine, MRS, Custom }
    enum SizeFilter { UpToLarge, UpToVeryLarge, Any }
    enum TypeFilter { All, Rooms, Spaces }
    property int serverMode: RoomDirectory.ServerMode.Mine
    property string customServer: ""
    property bool autoSelectionDone: false

    // Cached room counts per server mode (totalRoomCountEstimate is shared/volatile)
    property int homeserverRoomCount: -1
    property int customServerRoomCount: -1

    readonly property bool mrsEnabled: Settings.networkMrsEnabled
    readonly property string mrsServerName: Settings.networkMrsServerName

    onMrsServerNameChanged: {
        // Server changed — clear stale data and re-fetch
        publicRooms.fetchMrsRoomCount(mrsEnabled ? mrsServerName : "");
        if (serverMode === RoomDirectory.ServerMode.MRS)
            publicRooms.setMatrixServer(mrsServerName);
    }

    onMrsEnabledChanged: {
        if (!mrsEnabled)
            publicRooms.fetchMrsRoomCount("");  // clears count
    }

    readonly property string localHomeserver: {
        var uid = Settings.userId;
        var colonIdx = uid.indexOf(":");
        return colonIdx >= 0 ? uid.substring(colonIdx + 1) : "";
    }

    function roomCountBadgeText(estimate) {
        if (estimate < 0)
            return "";
        return estimate.toLocaleString();
    }

    title: qsTr("Explore Public Rooms")
    titleIcon: ":/icons/icons/ui/globe-search.svg"
    overlayViewport: appRoot
    overlayDialogMinWidth: 640
    overlayDialogMaxWidthRatio: 0.85

    readonly property int dialogViewportWidth: overlayDialogViewport ? overlayDialogViewport.width : 760
    readonly property int dialogViewportHeight: overlayDialogViewport ? overlayDialogViewport.height : 600

    width: Math.min(
        Math.max(240, dialogViewportWidth - Komai.paddingLarge * 2),
        Math.max(240, Math.floor(dialogViewportWidth * overlayDialogMaxWidthRatio))
    )
    height: Math.min(implicitHeight, dialogViewportHeight - Komai.paddingLarge * 2)
    x: Math.round((dialogViewportWidth - width) / 2)
    y: Math.max(Komai.paddingLarge, Math.round((dialogViewportHeight - height) / 2))

    onOpened: {
        if (!autoSelectionDone) {
            switchServer(RoomDirectory.ServerMode.Mine);
        }
        if (mrsEnabled) {
            publicRooms.fetchMrsRoomCount(mrsServerName);
        }
        roomSearch.forceActiveFocus();
    }

    function switchServer(mode) {
        // Save current tab's filters before switching
        if (serverMode !== mode) {
            sizeFilterPerTab[serverMode] = roomSizeFilter.currentIndex;
            typeFilterPerTab[serverMode] = roomTypeFilter.currentIndex;
        }

        serverMode = mode;

        // Restore new tab's filters
        applySizeFilter(sizeFilterPerTab[mode] ?? defaultSizeFilterForMode(mode));
        var typeIdx = typeFilterPerTab[mode] ?? RoomDirectory.TypeFilter.All;
        roomTypeFilter.currentIndex = typeIdx;
        publicRooms.roomTypeFilter = typeFilterValueForIndex(typeIdx);

        // Clear language filter when leaving MRS tab
        if (mode !== RoomDirectory.ServerMode.MRS && publicRooms.mrsLanguageFilter !== "") {
            publicRooms.mrsLanguageFilter = "";
            languageFilter.currentIndex = 0;
        }
        // Show loading indicator immediately
        searchLoadingHoldTimer.start();

        if (mode === RoomDirectory.ServerMode.Mine) {
            publicRooms.setMatrixServer("");
            roomSearch.forceActiveFocus();
        } else if (mode === RoomDirectory.ServerMode.MRS) {
            publicRooms.setMatrixServer(mrsServerName);
            roomSearch.forceActiveFocus();
        } else if (mode === RoomDirectory.ServerMode.Custom) {
            if (customServer.trim().length > 0) {
                publicRooms.setMatrixServer(customServer);
            } else {
                publicRooms.clearResults();
            }
            customServerField.forceActiveFocus();
        }
    }

    Connections {
        target: publicRooms
        function onHasResultsChanged() {
            if (!publicRooms.hasResults
                && publicRooms.reachedEndOfPagination
                && serverMode === RoomDirectory.ServerMode.Mine
                && !autoSelectionDone) {
                // Homeserver returned empty — fall back to MRS if enabled
                if (mrsEnabled) {
                    serverSegment.currentIndex = RoomDirectory.ServerMode.MRS;
                    switchServer(RoomDirectory.ServerMode.MRS);
                }
            }
            if (publicRooms.hasResults && serverMode === RoomDirectory.ServerMode.Mine) {
                autoSelectionDone = true;
            }
        }
        function onTotalRoomCountEstimateChanged() {
            if (publicRooms.totalRoomCountEstimate >= 0) {
                if (serverMode === RoomDirectory.ServerMode.Mine)
                    homeserverRoomCount = publicRooms.totalRoomCountEstimate;
                else if (serverMode === RoomDirectory.ServerMode.Custom && customServer.trim().length > 0)
                    customServerRoomCount = publicRooms.totalRoomCountEstimate;
            }
        }
    }

    // -- Server to explore section --

    SettingsSection {
        label: qsTr("Server to explore")
        Layout.fillWidth: true
    }

    SegmentedButton {
        id: serverSegment

        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: Math.max(46, Math.round(Settings.uiFontSizePt * 3.4))
        currentIndex: roomDirectoryRoot.serverMode
        model: {
            var mineBadge = roomDirectoryRoot.homeserverRoomCount >= 0
                ? roomDirectoryRoot.roomCountBadgeText(roomDirectoryRoot.homeserverRoomCount)
                : "";
            var segments = [
                { text: qsTr("Mine (%1)").arg(roomDirectoryRoot.localHomeserver), value: RoomDirectory.ServerMode.Mine, badge: mineBadge }
            ];
            if (roomDirectoryRoot.mrsEnabled) {
                var mrsBadge = publicRooms.mrsRoomCount >= 0
                    ? roomDirectoryRoot.roomCountBadgeText(publicRooms.mrsRoomCount)
                    : "";
                segments.push({ text: roomDirectoryRoot.mrsServerName, value: RoomDirectory.ServerMode.MRS, badge: mrsBadge });
            }
            var customBadge = (roomDirectoryRoot.customServer.trim().length > 0 && roomDirectoryRoot.customServerRoomCount >= 0)
                ? roomDirectoryRoot.roomCountBadgeText(roomDirectoryRoot.customServerRoomCount)
                : "";
            segments.push({ text: qsTr("Another server"), value: RoomDirectory.ServerMode.Custom, badge: customBadge });
            return segments;
        }
        onActivated: function(index) {
            var value = serverSegment.model[index].value;
            roomDirectoryRoot.switchServer(value);
        }
    }

    KomaiToolTip {
        id: badgeTooltip

        property Item badge: serverSegment.hoveredBadge

        parent: roomDirectoryRoot.contentItem.parent
        anchorItem: badge
        anchorX: badge ? badge.width / 2 : 0
        anchorY: badge ? badge.height : 0
        gapY: Komai.paddingMedium
        preferBelow: false
        text: qsTr("Number of known public rooms in this server's directory")
        delay: 0
        requestedVisible: badge !== null
    }

    // -- Custom server card (visible when "Another one" is selected) --

    Item {
        visible: roomDirectoryRoot.serverMode === RoomDirectory.ServerMode.Custom
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: customServerCardContent.implicitHeight

        HoverHandler { id: customServerHover; blocking: false }
        Rectangle {
            anchors.fill: customServerCardContent
            color: customServerHover.hovered ? palette.dark : palette.window
            radius: Komai.paddingMedium
            z: -1
        }

        ColumnLayout {
            id: customServerCardContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium

                Label {
                    text: qsTr("Custom server")
                    color: customServerHover.hovered ? palette.brightText : palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                KomaiTextField {
                    id: customServerField

                    text: roomDirectoryRoot.customServer
                    placeholderText: qsTr("example.com")
                    Layout.preferredWidth: Math.max(200, parent.width * 0.5)
                    onTextChanged: {
                        roomDirectoryRoot.customServer = text;
                        customServerTimer.restart();
                        if (text.trim().length > 0) {
                            serverSuggestions.model = publicRooms.knownServers(text.trim());
                        } else {
                            serverSuggestions.model = publicRooms.knownServers("");
                            roomDirectoryRoot.customServerRoomCount = -1;
                        }
                    }

                    Timer {
                        id: customServerTimer

                        interval: 350
                        onTriggered: {
                            if (roomDirectoryRoot.serverMode === RoomDirectory.ServerMode.Custom)
                                publicRooms.setMatrixServer(customServerField.text.trim());
                        }
                    }
                }
            }
        }
    }

    // Server suggestions (below card)
    ListView {
        id: serverSuggestions

        visible: roomDirectoryRoot.serverMode === RoomDirectory.ServerMode.Custom && count > 0
            && !(count === 1 && model.length === 1
                 && model[0].toLowerCase() === customServerField.text.trim().toLowerCase())
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        Layout.preferredHeight: Math.min(contentHeight, 200)
        clip: true
        spacing: 2
        model: []

        delegate: Item {
            id: suggestionDelegate

            required property string modelData
            required property int index

            width: ListView.view.width
            implicitHeight: suggestionRow.implicitHeight + Komai.paddingSmall * 2

            readonly property bool activeState: suggestionHover.hovered

            HoverHandler {
                id: suggestionHover
            }

            Rectangle {
                anchors.fill: parent
                radius: Komai.paddingMedium
                color: suggestionDelegate.activeState ? palette.dark : palette.window
            }

            RowLayout {
                id: suggestionRow

                anchors.fill: parent
                anchors.leftMargin: Komai.paddingMedium
                anchors.rightMargin: Komai.paddingMedium
                spacing: Komai.paddingMedium

                Avatar {
                    Layout.preferredWidth: Komai.listIconSize
                    Layout.preferredHeight: Komai.listIconSize
                    Layout.alignment: Qt.AlignVCenter
                    displayName: suggestionDelegate.modelData
                    roomid: "!" + suggestionDelegate.modelData + ":server"
                    enabled: false
                }

                Label {
                    Layout.fillWidth: true
                    text: suggestionDelegate.modelData
                    color: suggestionDelegate.activeState ? palette.brightText : palette.text
                    font.pointSize: Settings.uiFontSizePt
                    font.bold: true
                    elide: Text.ElideRight
                }

                KomaiButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Choose")
                    highlighted: true
                    onClicked: {
                        var server = suggestionDelegate.modelData;
                        customServerField.text = server;
                        roomDirectoryRoot.customServer = server;
                        publicRooms.setMatrixServer(server);
                        serverSuggestions.model = [];
                    }
                }
            }
        }
    }

    // -- Search rooms section --

    SettingsSection {
        label: qsTr("Filtering")
        Layout.fillWidth: true
    }

    // -- Room type filter card --

    Item {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: roomTypeCardContent.implicitHeight

        HoverHandler { id: roomTypeCardHover; blocking: false }
        Rectangle {
            anchors.fill: roomTypeCardContent
            color: roomTypeCardHover.hovered ? palette.dark : palette.window
            radius: Komai.paddingMedium
            z: -1
        }

        RowLayout {
            id: roomTypeCardContent
            width: parent.width
            spacing: Komai.paddingMedium

            Label {
                text: qsTr("Type")
                color: roomTypeCardHover.hovered ? palette.brightText : palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
            }

            SegmentedButton {
                id: roomTypeFilter

                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.maximumWidth: Math.min(350, parent.width * 0.5)
                model: [
                    { text: qsTr("All") },
                    { text: qsTr("Rooms") },
                    { text: qsTr("Spaces") }
                ]
                currentIndex: roomDirectoryRoot.typeFilterPerTab[roomDirectoryRoot.serverMode]
                    ?? RoomDirectory.TypeFilter.All
                onActivated: function(index) {
                    roomDirectoryRoot.typeFilterPerTab[roomDirectoryRoot.serverMode] = index;
                    publicRooms.roomTypeFilter = roomDirectoryRoot.typeFilterValueForIndex(index);
                    if (roomDirectoryRoot.serverMode === RoomDirectory.ServerMode.Custom
                        && roomDirectoryRoot.customServer.trim().length === 0) {
                        publicRooms.clearResults();
                    }
                }
            }
        }
    }

    // -- Room size filter card --

    Item {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: roomSizeCardContent.implicitHeight

        HoverHandler { id: roomSizeCardHover; blocking: false }
        Rectangle {
            anchors.fill: roomSizeCardContent
            color: roomSizeCardHover.hovered ? palette.dark : palette.window
            radius: Komai.paddingMedium
            z: -1
        }

        RowLayout {
            id: roomSizeCardContent
            width: parent.width
            spacing: Komai.paddingMedium

            Label {
                text: qsTr("Size")
                color: roomSizeCardHover.hovered ? palette.brightText : palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
            }

            KomaiComboBox {
                id: roomSizeFilter

                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.preferredWidth: Math.max(implicitWidth, Math.min(350, parent.width * 0.5))
                model: [
                    qsTr("Up to large (≤ %1 members)").arg(roomDirectoryRoot.largeRoomThreshold.toLocaleString()),
                    qsTr("Up to very large (≤ %1 members)").arg(roomDirectoryRoot.veryLargeRoomThreshold.toLocaleString()),
                    qsTr("Any")
                ]
                currentIndex: roomDirectoryRoot.sizeFilterPerTab[roomDirectoryRoot.serverMode]
                    ?? roomDirectoryRoot.defaultSizeFilterForMode(roomDirectoryRoot.serverMode)
                onActivated: function(index) {
                    roomDirectoryRoot.sizeFilterPerTab[roomDirectoryRoot.serverMode] = index;
                    publicRooms.maxMemberFilter = roomDirectoryRoot.sizeFilterValueForIndex(index);
                    // Don't auto-fetch if on Custom tab with no server entered
                    if (roomDirectoryRoot.serverMode === RoomDirectory.ServerMode.Custom
                        && roomDirectoryRoot.customServer.trim().length === 0) {
                        publicRooms.clearResults();
                    }
                }
            }
        }
    }

    // -- Keyword search card --

    Item {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: searchCardContent.implicitHeight

        HoverHandler { id: searchCardHover; blocking: false }
        Rectangle {
            anchors.fill: searchCardContent
            color: searchCardHover.hovered ? palette.dark : palette.window
            radius: Komai.paddingMedium
            z: -1
        }

        ColumnLayout {
            id: searchCardContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: Komai.paddingMedium

                Label {
                    text: qsTr("Keyword")
                    color: searchCardHover.hovered ? palette.brightText : palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                    Layout.leftMargin: Komai.paddingMedium
                    Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                    Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                }

                KomaiTextField {
                    id: roomSearch

                    Layout.preferredWidth: Math.max(implicitWidth, Math.min(350, parent.width * 0.5))
                    Layout.rightMargin: Komai.paddingMedium
                    Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                    Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                    placeholderText: qsTr("e.g. Matrix, food, coffee, tech")
                    onTextChanged: searchTimer.restart()

                    Timer {
                        id: searchTimer

                        interval: 350
                        onTriggered: publicRooms.setSearchTerm(roomSearch.text)
                    }
                }
            }

            // Quick keyword presets (MRS only)
            Flow {
                visible: roomDirectoryRoot.serverMode === RoomDirectory.ServerMode.MRS
                Layout.maximumWidth: parent.width - Komai.paddingMedium * 2
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                spacing: Komai.paddingSmall

                Label {
                    text: qsTr("Quick presets:")
                    color: searchCardHover.hovered ? palette.brightText : palette.buttonText
                    font.pointSize: Settings.uiFontSizePt * 0.8
                    height: quickPresetRepeater.count > 0 ? quickPresetRepeater.itemAt(0).height : implicitHeight
                    verticalAlignment: Text.AlignVCenter
                }

                Repeater {
                    id: quickPresetRepeater
                    model: ["Matrix Hosting", "FOSS", "mastodon", "Android", "food", "coffee", "linux", "gaming", "minecraft", "social", "tech", "travel", "photography"]

                    KomaiButton {
                        required property string modelData
                        text: modelData
                        highlighted: roomSearch.text.toLowerCase() === modelData.toLowerCase()
                        font.pointSize: Settings.uiFontSizePt * 0.8
                        topPadding: Komai.paddingSmall * 0.5
                        bottomPadding: Komai.paddingSmall * 0.5
                        leftPadding: Komai.paddingSmall
                        rightPadding: Komai.paddingSmall
                        onClicked: {
                            roomSearch.text = modelData;
                        }
                    }
                }
            }
        }
    }

    // -- Language filter card (MRS only) --

    Item {
        visible: roomDirectoryRoot.serverMode === RoomDirectory.ServerMode.MRS
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: languageCardContent.implicitHeight

        HoverHandler { id: languageCardHover; blocking: false }
        Rectangle {
            anchors.fill: languageCardContent
            color: languageCardHover.hovered ? palette.dark : palette.window
            radius: Komai.paddingMedium
            z: -1
        }

        RowLayout {
            id: languageCardContent
            width: parent.width
            spacing: Komai.paddingMedium

            Label {
                text: qsTr("Language")
                color: languageCardHover.hovered ? palette.brightText : palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
            }

            KomaiSearchableComboBox {
                id: languageFilter

                Layout.preferredWidth: Math.max(implicitWidth, Math.min(350, parent.width * 0.5))
                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                Layout.bottomMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                model: {
                    var langs = publicRooms.availableLanguages();
                    langs.unshift(qsTr("Any language"));
                    return langs;
                }
                currentIndex: 0
                onActivated: function(index) {
                    languageFilter.currentIndex = index;
                    if (index === 0) {
                        publicRooms.mrsLanguageFilter = "";
                    } else {
                        var entry = languageFilter.model[index];
                        var match = entry.match(/\(([a-z]{2})\)$/);
                        publicRooms.mrsLanguageFilter = match ? match[1] : "";
                    }
                }
            }
        }
    }

    // -- Results section --

    SettingsSection {
        label: qsTr("Rooms & spaces")
        Layout.fillWidth: true
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: Math.max(
            160,
            roomDirectoryRoot.dialogViewportHeight - overlayDialogChromeHeight - Komai.paddingLarge * 2
        )

        ListView {
            id: roomDirView

            anchors.fill: parent
            readonly property bool hasVerticalOverflow: contentHeight > height
            readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
            readonly property bool scrollbarVisible: {
                switch (scrollbarPolicy) {
                case Settings.ScrollbarPolicy.Always:
                    return true;
                case Settings.ScrollbarPolicy.Never:
                    return false;
                case Settings.ScrollbarPolicy.WhenNeeded:
                default:
                    return hasVerticalOverflow;
                }
            }
            readonly property real reservedScrollbarWidth: scrollbarVisible
                ? Math.max(resultsScrollbar.width, resultsScrollbar.implicitWidth) + Komai.paddingSmall
                : 0

            rightMargin: reservedScrollbarWidth
            model: publicRooms
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                id: resultsScrollbar

                policy: roomDirView.scrollbarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            delegate: Item {
                id: roomDelegate

                required property string name
                required property string roomid
                required property string avatarUrl
                required property string topic
                required property int numMembers
                required property bool canJoin
                required property bool isSpace
                required property string alias
                required property int index

                width: ListView.view.width - roomDirView.rightMargin
                implicitHeight: delegateRow.implicitHeight + Komai.paddingSmall * 2

                readonly property bool activeState: delegateHover.hovered
                readonly property color actionTextColor: activeState ? palette.brightText : palette.text

                HoverHandler {
                    id: delegateHover
                }

                Rectangle {
                    anchors.fill: parent
                    radius: Komai.paddingMedium
                    color: roomDelegate.activeState ? palette.dark : palette.window
                }

                RowLayout {
                    id: delegateRow

                    anchors.fill: parent
                    anchors.leftMargin: Komai.paddingMedium
                    anchors.rightMargin: Komai.paddingMedium
                    spacing: Komai.paddingMedium

                    Avatar {
                        Layout.preferredWidth: Math.round(Komai.listIconSize * 1.3)
                        Layout.preferredHeight: Math.round(Komai.listIconSize * 1.3)
                        Layout.alignment: Qt.AlignVCenter
                        url: roomDelegate.avatarUrl.replace("mxc://", "image://MxcImage/")
                        roomid: roomDelegate.roomid
                        displayName: roomDelegate.name
                        enabled: false
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: nameRow.implicitHeight

                            Row {
                                id: nameRow
                                width: parent.width
                                spacing: Komai.paddingMedium
                                clip: true

                                TextEdit {
                                    width: Math.min(implicitWidth, nameRow.width - (spaceBadgeRect.visible ? spaceBadgeRect.width + nameRow.spacing : 0))
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: roomDelegate.name || roomDelegate.roomid || qsTr("(unnamed room)")
                                    color: roomDelegate.actionTextColor
                                    font.pointSize: Settings.uiFontSizePt * 1.1
                                    font.bold: true
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.NoWrap
                                }

                                Rectangle {
                                    id: spaceBadgeRect
                                    visible: roomDelegate.isSpace
                                    anchors.verticalCenter: parent.verticalCenter
                                    implicitWidth: spaceBadgeLabel.implicitWidth + Komai.paddingSmall * 2
                                    implicitHeight: spaceBadgeLabel.implicitHeight + Komai.paddingSmall * 0.5
                                    radius: Komai.paddingSmall
                                    color: Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.15)
                                    border.color: Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.4)
                                    border.width: 1

                                    Label {
                                        id: spaceBadgeLabel
                                        anchors.centerIn: parent
                                        text: qsTr("Space")
                                        color: palette.text
                                        font.pointSize: Settings.uiFontSizePt * 0.8
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: roomDelegate.alias.length > 0
                            spacing: Komai.paddingSmall

                            TextEdit {
                                text: roomDelegate.alias
                                color: roomDelegate.activeState ? palette.brightText : palette.buttonText
                                font.pointSize: Settings.uiFontSizePt
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.NoWrap
                            }

                            ImageButton {
                                id: copyAliasBtn

                                property bool copied: false

                                Layout.preferredWidth: Math.round(Settings.uiFontSizePt * 1.6)
                                Layout.preferredHeight: Layout.preferredWidth
                                Layout.alignment: Qt.AlignVCenter
                                buttonTextColor: roomDelegate.activeState ? palette.brightText : palette.buttonText
                                image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                hoverEnabled: true
                                toolTipVisible: hovered
                                toolTipText: copied ? qsTr("Copied!") : qsTr("Copy room address")
                                onClicked: {
                                    Clipboard.text = roomDelegate.alias;
                                    copied = true;
                                    copyAliasTimer.restart();
                                }

                                Timer {
                                    id: copyAliasTimer
                                    interval: 2000
                                    onTriggered: copyAliasBtn.copied = false
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        TextEdit {
                            Layout.fillWidth: true
                            Layout.maximumHeight: Math.ceil(font.pixelSize * 2.8)
                            visible: roomDelegate.topic.length > 0
                            text: roomDelegate.topic
                            textFormat: TextEdit.RichText
                            color: roomDelegate.activeState ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WordWrap
                            clip: true
                            onLinkActivated: function(link) { Komai.openLink(link); }

                            KomaiCursorShape {
                                anchors.fill: parent
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
                            }
                        }
                    }

                    Rectangle {
                        id: memberBadge

                        readonly property bool isLargeRoom: roomDelegate.numMembers >= roomDirectoryRoot.largeRoomThreshold
                        readonly property bool isVeryLargeRoom: roomDelegate.numMembers >= roomDirectoryRoot.veryLargeRoomThreshold
                        readonly property string warningText: roomDirectoryRoot.roomSizeWarning(roomDelegate.numMembers)
                        readonly property color badgeColor: isVeryLargeRoom
                            ? Komai.theme.error
                            : isLargeRoom ? Komai.theme.warning : palette.text
                        readonly property int badgeIconSize: Math.max(14, Math.round(Settings.uiFontSizePt * 1.5))

                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: memberBadgeRow.implicitWidth + Komai.paddingSmall * 2
                        implicitHeight: memberBadgeRow.implicitHeight + Komai.paddingSmall * 0.5
                        radius: Komai.paddingSmall
                        color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
                        border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
                        border.width: 1

                        Row {
                            id: memberBadgeRow
                            anchors.centerIn: parent
                            spacing: Komai.paddingSmall * 0.5

                            Image {
                                anchors.verticalCenter: parent.verticalCenter
                                width: memberBadge.badgeIconSize
                                height: memberBadge.badgeIconSize
                                sourceSize.width: width
                                sourceSize.height: height
                                source: "image://colorimage/:/icons/icons/ui/people.svg?" + memberBadge.badgeColor
                            }

                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: roomDelegate.numMembers.toLocaleString()
                                color: memberBadge.badgeColor
                                font.pointSize: Settings.uiFontSizePt * 0.8
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                            onContainsMouseChanged: {
                                if (containsMouse) {
                                    roomDirectoryRoot.hoveredMemberBadge = memberBadge;
                                    roomDirectoryRoot.hoveredMemberBadgeText = memberBadge.isLargeRoom
                                        ? memberBadge.warningText
                                        : qsTr("There are %1 members in this room").arg(roomDelegate.numMembers.toLocaleString());
                                } else if (roomDirectoryRoot.hoveredMemberBadge === memberBadge) {
                                    roomDirectoryRoot.hoveredMemberBadge = null;
                                }
                            }
                        }
                    }

                    KomaiButton {
                        Layout.alignment: Qt.AlignVCenter
                        text: roomDelegate.canJoin ? qsTr("Join") : qsTr("Open")
                        highlighted: roomDelegate.canJoin
                        enabled: roomDelegate.roomid !== ""
                        onClicked: {
                            if (!roomDelegate.canJoin) {
                                Rooms.setCurrentRoom(roomDelegate.roomid);
                                roomDirectoryRoot.close();
                            } else if (roomDelegate.numMembers >= roomDirectoryRoot.largeRoomThreshold) {
                                joinConfirmDialog.roomName = roomDelegate.name || roomDelegate.roomid;
                                joinConfirmDialog.roomIndex = roomDelegate.index;
                                joinConfirmDialog.memberCount = roomDelegate.numMembers;
                                joinConfirmDialog.warningText = roomDirectoryRoot.roomSizeWarning(roomDelegate.numMembers);
                                joinConfirmDialog.open();
                            } else {
                                publicRooms.joinRoom(roomDelegate.index);
                            }
                        }
                    }
                }

            }

            // Error overlay
            Label {
                anchors.centerIn: parent
                width: parent.width - Komai.paddingLarge * 2
                visible: publicRooms.errorString.length > 0
                text: publicRooms.errorString
                color: Komai.theme.error
                font.pointSize: Settings.uiFontSizePt
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            // Empty state
            Label {
                anchors.centerIn: parent
                width: parent.width - Komai.paddingLarge * 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: roomDirView.count === 0
                    && !publicRooms.loadingMoreRooms
                    && publicRooms.reachedEndOfPagination
                    && publicRooms.errorString.length === 0
                text: qsTr("Nothing found.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 1.5
            }

            // Filter hint
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: Math.round(parent.height / 2) + Komai.paddingLarge
                width: parent.width - Komai.paddingLarge * 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: roomDirView.count === 0
                    && !publicRooms.loadingMoreRooms
                    && publicRooms.reachedEndOfPagination
                    && publicRooms.errorString.length === 0
                    && publicRooms.maxMemberFilter > 0
                text: qsTr("The room size filter may be hiding results. Try a larger size or \"Any\".")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
            }

            // Custom server hint
            Label {
                anchors.centerIn: parent
                width: parent.width - Komai.paddingLarge * 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: roomDirView.count === 0
                    && !publicRooms.loadingMoreRooms
                    && !publicRooms.reachedEndOfPagination
                    && roomDirectoryRoot.serverMode === RoomDirectory.ServerMode.Custom
                    && roomDirectoryRoot.customServer.trim().length === 0
                text: qsTr("Enter a server address above to explore its public rooms.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 1.5
            }

        }

        // Search progress overlay — centered over results
        Item {
            id: searchStatusIcon

            property bool _rawLoading: publicRooms.loadingMoreRooms
            property bool isLoading: _rawLoading || searchLoadingHoldTimer.running

            on_RawLoadingChanged: {
                if (_rawLoading)
                    searchLoadingHoldTimer.stop();
                else
                    searchLoadingHoldTimer.start();
            }

            Timer {
                id: searchLoadingHoldTimer
                interval: 600
            }

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Math.max(Komai.paddingLarge, (parent.height - height) / 3)
            width: 160
            height: 160
            visible: isLoading
            z: 10

            Item {
                id: searchIconContent

                anchors.fill: parent
                visible: false
                layer.enabled: true

                Image {
                    anchors.fill: parent
                    source: "qrc:/logos/komai.svg"
                    sourceSize.width: width * 2
                    sourceSize.height: height * 2
                    fillMode: Image.PreserveAspectFit
                }

                Rectangle {
                    id: searchBadge

                    property int badgeSize: Math.round(parent.width * 0.55)
                    property int iconSize: Math.round(badgeSize * 0.69)

                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.bottomMargin: -4
                    anchors.rightMargin: -4
                    width: badgeSize
                    height: badgeSize
                    radius: Math.round(badgeSize * 0.25)
                    color: palette.alternateBase

                    transform: Translate { id: badgeTranslate; x: 0 }

                    SequentialAnimation {
                        loops: Animation.Infinite
                        running: searchStatusIcon.isLoading && Settings.uiMotionAnimationsEnabled

                        ParallelAnimation {
                            NumberAnimation {
                                target: badgeTranslate; property: "x"
                                from: 0; to: 5
                                duration: 300; easing.type: Easing.InOutQuad
                            }
                            NumberAnimation {
                                target: searchBadge; property: "scale"
                                from: 1.0; to: 1.3
                                duration: 300; easing.type: Easing.InOutQuad
                            }
                        }
                        ParallelAnimation {
                            NumberAnimation {
                                target: badgeTranslate; property: "x"
                                from: 5; to: -5
                                duration: 600; easing.type: Easing.InOutQuad
                            }
                            NumberAnimation {
                                target: searchBadge; property: "scale"
                                from: 1.3; to: 1.3
                                duration: 600
                            }
                        }
                        ParallelAnimation {
                            NumberAnimation {
                                target: badgeTranslate; property: "x"
                                from: -5; to: 0
                                duration: 300; easing.type: Easing.InOutQuad
                            }
                            NumberAnimation {
                                target: searchBadge; property: "scale"
                                from: 1.3; to: 1.0
                                duration: 300; easing.type: Easing.InOutQuad
                            }
                        }

                        onRunningChanged: {
                            if (!running) {
                                badgeTranslate.x = 0;
                                searchBadge.scale = 1.0;
                            }
                        }
                    }

                    Image {
                        anchors.centerIn: parent
                        source: "image://colorimage/:/icons/icons/ui/search.svg?" + (searchStatusIcon.isLoading ? palette.highlight : palette.text)
                        sourceSize.width: searchBadge.iconSize
                        sourceSize.height: searchBadge.iconSize
                        width: searchBadge.iconSize
                        height: searchBadge.iconSize
                    }
                }
            }

            MultiEffect {
                id: searchEffect

                anchors.fill: parent
                source: searchIconContent
                saturation: searchStatusIcon.isLoading && !Settings.uiMotionAnimationsEnabled ? 0.0 : -1.0

                SequentialAnimation {
                    loops: Animation.Infinite
                    running: searchStatusIcon.isLoading && Settings.uiMotionAnimationsEnabled

                    PropertyAction {
                        target: searchEffect
                        property: "saturation"
                        value: -0.4
                    }
                    NumberAnimation {
                        target: searchEffect
                        property: "saturation"
                        from: -0.4; to: 0.0
                        duration: 150; easing.type: Easing.OutQuad
                    }
                    NumberAnimation {
                        target: searchEffect
                        property: "saturation"
                        from: 0.0; to: -1.0
                        duration: 500; easing.type: Easing.InQuad
                    }

                    onRunningChanged: {
                        if (!running) {
                            searchEffect.saturation = Qt.binding(function() {
                                return (searchStatusIcon.isLoading && !Settings.uiMotionAnimationsEnabled) ? 0.0 : -1.0;
                            });
                        }
                    }
                }
            }
        }
    }
}
