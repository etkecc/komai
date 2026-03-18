// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import cc.etke.komai

OverlayDialog {
    id: roomDirectoryRoot

    property Item appRoot: null

    // 0 = Mine (homeserver), 1 = MRS, 2 = Another one
    property int serverMode: 0
    property string customServer: ""
    property bool autoSelectionDone: false

    // MRS room count stub — will be populated via MRS API later
    property int mrsRoomCount: -1

    readonly property bool mrsEnabled: Settings.networkMrsEnabled
    readonly property string mrsServerName: Settings.networkMrsServerName

    readonly property string localHomeserver: {
        var uid = Settings.userId;
        var colonIdx = uid.indexOf(":");
        return colonIdx >= 0 ? uid.substring(colonIdx + 1) : "";
    }

    function roomCountBadgeText(estimate) {
        if (estimate < 0)
            return "";
        if (estimate >= 1000000)
            return Math.round(estimate / 1000000) + "M";
        if (estimate >= 1000)
            return Math.round(estimate / 1000) + "k";
        return String(estimate);
    }

    title: qsTr("Explore Public Rooms")
    titleIcon: ":/icons/icons/ui/globe-search.svg"
    overlayViewport: appRoot
    overlayDialogMinWidth: 640
    overlayDialogMaxWidthRatio: 0.85

    width: {
        var vpW = overlayDialogViewport ? overlayDialogViewport.width : 760;
        var pad = Komai.paddingLarge;
        return Math.min(Math.max(240, vpW - pad * 2), Math.max(240, Math.floor(vpW * 0.85)));
    }

    onOpened: {
        if (!autoSelectionDone) {
            switchServer(0);
        }
        roomSearch.forceActiveFocus();
    }

    function switchServer(mode) {
        serverMode = mode;
        if (mode === 0) {
            publicRooms.setMatrixServer("");
            roomSearch.forceActiveFocus();
        } else if (mode === 1) {
            publicRooms.setMatrixServer(mrsServerName);
            roomSearch.forceActiveFocus();
        } else if (mode === 2) {
            publicRooms.setMatrixServer(customServer);
            customServerField.forceActiveFocus();
        }
    }

    Connections {
        target: publicRooms
        function onHasResultsChanged() {
            if (!publicRooms.hasResults
                && publicRooms.reachedEndOfPagination
                && serverMode === 0
                && !autoSelectionDone) {
                // Homeserver returned empty — fall back to MRS if enabled
                if (mrsEnabled) {
                    serverSegment.currentIndex = 1;
                    switchServer(1);
                }
            }
            if (publicRooms.hasResults && serverMode === 0) {
                autoSelectionDone = true;
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
        currentIndex: roomDirectoryRoot.serverMode
        model: {
            var segments = [
                { text: qsTr("Mine (%1)").arg(roomDirectoryRoot.localHomeserver), value: 0 }
            ];
            if (roomDirectoryRoot.mrsEnabled) {
                segments.push({ text: roomDirectoryRoot.mrsServerName, value: 1 });
            }
            segments.push({ text: qsTr("Another one"), value: 2 });
            return segments;
        }
        onActivated: function(index) {
            var value = serverSegment.model[index].value;
            roomDirectoryRoot.switchServer(value);
        }
    }

    // Room count badges row
    RowLayout {
        Layout.fillWidth: true
        spacing: 0

        Item { Layout.fillWidth: true }

        // Mine badge
        Rectangle {
            visible: publicRooms.totalRoomCountEstimate >= 0 && roomDirectoryRoot.serverMode === 0
            implicitHeight: mineBadgeLabel.implicitHeight + Komai.paddingSmall
            implicitWidth: Math.max(mineBadgeLabel.implicitWidth + Komai.paddingSmall * 2, implicitHeight)
            radius: height / 2
            color: palette.highlight
            Layout.alignment: Qt.AlignHCenter

            Label {
                id: mineBadgeLabel
                anchors.centerIn: parent
                font.bold: true
                font.pixelSize: Komai.fontPixelSize * 0.75
                color: palette.brightText
                text: roomDirectoryRoot.roomCountBadgeText(publicRooms.totalRoomCountEstimate)
            }
        }

        // MRS badge
        Rectangle {
            visible: roomDirectoryRoot.mrsEnabled && (
                (roomDirectoryRoot.serverMode === 1 && publicRooms.totalRoomCountEstimate >= 0)
                || roomDirectoryRoot.mrsRoomCount >= 0)
            implicitHeight: mrsBadgeLabel.implicitHeight + Komai.paddingSmall
            implicitWidth: Math.max(mrsBadgeLabel.implicitWidth + Komai.paddingSmall * 2, implicitHeight)
            radius: height / 2
            color: palette.highlight
            Layout.alignment: Qt.AlignHCenter

            Label {
                id: mrsBadgeLabel
                anchors.centerIn: parent
                font.bold: true
                font.pixelSize: Komai.fontPixelSize * 0.75
                color: palette.brightText
                text: {
                    if (roomDirectoryRoot.serverMode === 1 && publicRooms.totalRoomCountEstimate >= 0)
                        return roomDirectoryRoot.roomCountBadgeText(publicRooms.totalRoomCountEstimate);
                    if (roomDirectoryRoot.mrsRoomCount >= 0)
                        return roomDirectoryRoot.roomCountBadgeText(roomDirectoryRoot.mrsRoomCount);
                    return "";
                }
            }
        }

        // Custom badge
        Rectangle {
            visible: roomDirectoryRoot.serverMode === 2 && publicRooms.totalRoomCountEstimate >= 0
            implicitHeight: customBadgeLabel.implicitHeight + Komai.paddingSmall
            implicitWidth: Math.max(customBadgeLabel.implicitWidth + Komai.paddingSmall * 2, implicitHeight)
            radius: height / 2
            color: palette.highlight
            Layout.alignment: Qt.AlignHCenter

            Label {
                id: customBadgeLabel
                anchors.centerIn: parent
                font.bold: true
                font.pixelSize: Komai.fontPixelSize * 0.75
                color: palette.brightText
                text: roomDirectoryRoot.roomCountBadgeText(publicRooms.totalRoomCountEstimate)
            }
        }

        Item { Layout.fillWidth: true }
    }

    // -- Custom server card (visible when "Another one" is selected) --

    Item {
        visible: roomDirectoryRoot.serverMode === 2
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
                        if (text.trim().length > 0)
                            serverSuggestions.model = publicRooms.knownServers(text.trim());
                        else
                            serverSuggestions.model = publicRooms.knownServers("");
                    }

                    Timer {
                        id: customServerTimer

                        interval: 350
                        onTriggered: {
                            if (roomDirectoryRoot.serverMode === 2)
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

        visible: roomDirectoryRoot.serverMode === 2 && count > 0
            && !(count === 1 && model.length === 1
                 && model[0].toLowerCase() === customServerField.text.trim().toLowerCase())
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        Layout.preferredHeight: Math.min(contentHeight, 150)
        clip: true
        model: []

        delegate: AbstractButton {
            id: suggestionDelegate

            required property string modelData
            required property int index

            width: ListView.view.width
            implicitHeight: suggestionLabel.implicitHeight + Komai.paddingSmall * 2
            hoverEnabled: true

            readonly property bool activeState: hovered || pressed

            background: Rectangle {
                radius: Komai.paddingSmall
                color: suggestionDelegate.activeState ? palette.dark : "transparent"
            }

            contentItem: Label {
                id: suggestionLabel

                leftPadding: Komai.paddingMedium
                text: suggestionDelegate.modelData
                color: suggestionDelegate.activeState ? palette.brightText : palette.text
                font.pointSize: Settings.uiFontSizePt * 0.9
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                customServerField.text = modelData;
                roomDirectoryRoot.customServer = modelData;
                publicRooms.setMatrixServer(modelData);
                serverSuggestions.model = [];
            }

            KomaiCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
        }
    }

    // -- Search rooms section --

    SettingsSection {
        label: qsTr("Search rooms")
        Layout.fillWidth: true
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        // Animated Komai logo (search status indicator)
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
                interval: 200
            }

            Layout.preferredHeight: 32
            Layout.preferredWidth: 32
            Layout.alignment: Qt.AlignVCenter

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

                    property int badgeSize: Math.round(32 * 0.55)
                    property int iconSize: Math.round(badgeSize * 0.69)

                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.bottomMargin: -2
                    anchors.rightMargin: -2
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
                                from: 0; to: 3
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
                                from: 3; to: -3
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
                                from: -3; to: 0
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

                    NumberAnimation {
                        target: searchEffect
                        property: "saturation"
                        from: -1.0; to: 0.0
                        duration: 800; easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        target: searchEffect
                        property: "saturation"
                        from: 0.0; to: -1.0
                        duration: 800; easing.type: Easing.InOutQuad
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

        KomaiTextField {
            id: roomSearch

            Layout.fillWidth: true
            placeholderText: qsTr("Search for public rooms")
            onTextChanged: searchTimer.restart()

            Timer {
                id: searchTimer

                interval: 350
                onTriggered: publicRooms.setSearchTerm(roomSearch.text)
            }
        }
    }

    // -- Results --

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 400

        ListView {
            id: roomDirView

            anchors.fill: parent
            model: publicRooms
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                id: resultsScrollbar

                readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
                policy: {
                    switch (scrollbarPolicy) {
                    case Settings.ScrollbarPolicy.Always:
                        return ScrollBar.AlwaysOn;
                    case Settings.ScrollbarPolicy.Never:
                        return ScrollBar.AlwaysOff;
                    case Settings.ScrollbarPolicy.WhenNeeded:
                    default:
                        return ScrollBar.AsNeeded;
                    }
                }
            }

            delegate: AbstractButton {
                id: roomDelegate

                required property string name
                required property string roomid
                required property string avatarUrl
                required property string topic
                required property int numMembers
                required property bool canJoin
                required property int index

                width: ListView.view.width
                implicitHeight: delegateRow.implicitHeight + Komai.paddingSmall * 2
                hoverEnabled: true

                readonly property bool activeState: hovered || pressed

                background: Rectangle {
                    radius: Komai.paddingMedium
                    color: roomDelegate.activeState ? palette.dark : "transparent"
                }

                contentItem: RowLayout {
                    id: delegateRow

                    spacing: Komai.paddingMedium

                    Avatar {
                        Layout.preferredWidth: Komai.listIconSize
                        Layout.preferredHeight: Komai.listIconSize
                        Layout.alignment: Qt.AlignVCenter
                        Layout.leftMargin: Komai.paddingSmall
                        url: roomDelegate.avatarUrl.replace("mxc://", "image://MxcImage/")
                        roomid: roomDelegate.roomid
                        displayName: roomDelegate.name
                        enabled: false
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                Layout.fillWidth: true
                                text: roomDelegate.name || roomDelegate.roomid
                                color: roomDelegate.activeState ? palette.brightText : palette.text
                                font.pointSize: Settings.uiFontSizePt
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Label {
                                text: roomDelegate.numMembers.toString()
                                color: roomDelegate.activeState ? palette.brightText : palette.buttonText
                                font.pointSize: Settings.uiFontSizePt * 0.85
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: roomDelegate.topic
                            color: roomDelegate.activeState ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt * 0.9
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }
                    }

                    KomaiButton {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.rightMargin: Komai.paddingSmall
                        text: roomDelegate.canJoin ? qsTr("Join") : qsTr("Open")
                        enabled: roomDelegate.roomid !== ""
                        onClicked: {
                            if (roomDelegate.canJoin) {
                                publicRooms.joinRoom(roomDelegate.index);
                            } else {
                                Rooms.setCurrentRoom(roomDelegate.roomid);
                                roomDirectoryRoot.close();
                            }
                        }
                    }
                }

                KomaiCursorShape {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
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
                visible: roomDirView.count === 0
                    && !publicRooms.loadingMoreRooms
                    && publicRooms.reachedEndOfPagination
                    && publicRooms.errorString.length === 0
                text: qsTr("No rooms found")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 0.9
            }

            // Loading footer
            footer: Item {
                width: ListView.view ? ListView.view.width : 0
                visible: !publicRooms.reachedEndOfPagination && publicRooms.loadingMoreRooms
                height: visible ? loadingSpinner.height + Komai.paddingLarge * 2 : 0

                Spinner {
                    id: loadingSpinner

                    anchors.centerIn: parent
                    running: visible
                    foreground: palette.mid
                }
            }
        }
    }
}
