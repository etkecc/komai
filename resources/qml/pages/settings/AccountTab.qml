// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/SettingsRows"
import "../../components/encryption" as Encryption
import "../../ui" as UI
import cc.etke.komai

Item {
    id: root
    property bool collapsed: false

    // AccountTab is fully custom QML, so it doesn't go through SettingsContent's
    // model-row search. We do two things here:
    //   1. Mirror the tab-level empty-state: if search is active and no Account
    //      keyword matches, hide everything and show "no matches".
    //   2. Per-section gating: each scrollContent child binds its `visible` to
    //      sectionVisible("<id>"), so a query like "device" narrows the tab
    //      to the device-related sections.
    //
    // The explicit `var _ = UserSettingsModel.searchQuery` reads in both
    // helpers are not no-ops: Q_INVOKABLE methods don't notify QML, so the
    // visible bindings only re-evaluate when something they read directly
    // changes. Reading searchQuery makes it the binding's tracked dep, which
    // is the property that fires NOTIFY on every keystroke in the search
    // field — without it, going from "abc" to "device" wouldn't refresh
    // section visibility.
    readonly property string _searchQuery: UserSettingsModel.searchQuery ?? ""
    readonly property bool hasActiveQuery: _searchQuery.length > 0
    readonly property bool searchHidesEverything: {
        var _ = root._searchQuery;
        return root.hasActiveQuery && !UserSettingsModel.tabHasCustomMatches(UserSettingsModel.TabAccount);
    }
    function sectionVisible(sectionId) {
        var _ = root._searchQuery;
        return UserSettingsModel.customSectionMatches(UserSettingsModel.TabAccount, sectionId);
    }
    // Sections after the first one in scrollContent set
    // `Layout.topMargin: paddingLarge` to separate themselves from the
    // previous section. When search hides every section ahead of them,
    // that margin sits at the top of the tab and looks like an unwanted
    // gap. This helper returns true only if at least one preceding
    // section is currently visible — so the call sites can drop the
    // margin to zero when this section turns out to be the first one
    // actually rendered.
    //
    // The "otherDevices" entry is included even though its real
    // visibility also depends on `accountView.otherDevicesCount > 0` —
    // accountView lives inside the Loader and isn't directly reachable
    // from here. Worst case: if only otherDevices keyword-matches but
    // count is 0, a later section gets paddingLarge above it instead of
    // 0. Rare and not visually loud.
    readonly property var _sectionOrder: ["profile", "thisDevice", "otherDevices", "users", "localCache"]
    function anyEarlierSectionVisible(sectionId) {
        var idx = root._sectionOrder.indexOf(sectionId);
        if (idx <= 0)
            return false;
        for (var i = 0; i < idx; i++) {
            if (root.sectionVisible(root._sectionOrder[i]))
                return true;
        }
        return false;
    }

    Loader {
        anchors.fill: parent
        active: !root.searchHidesEverything
        visible: active
        sourceComponent: Settings.hasActiveSession ? accountSettingsView : signedOutView
    }

    Label {
        anchors.centerIn: parent
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        width: Math.min(parent.width - Komai.paddingLarge * 2, 480)
        color: palette.buttonText
        font.pointSize: Settings.uiFontSizePt
        text: qsTranslate("UserSettingsModel", "No settings in this tab match your search.")
        visible: root.searchHidesEverything
    }

    Component {
        id: accountSettingsView

        Item {
            id: accountView

            property var profile: Komai.currentUser
            property string currentDeviceName: ""
            property int otherDevicesCount: 0
            property bool otherDevicesExpanded: false

            function lookupCurrentDeviceName() {
                if (!profile || !profile.deviceList)
                    return;
                var roleDeviceId = 0;   // DeviceInfoModel::DeviceId
                var roleDeviceName = 1; // DeviceInfoModel::DeviceName
                for (var i = 0; i < profile.deviceList.rowCount(); i++) {
                    var idx = profile.deviceList.index(i, 0);
                    if (profile.deviceList.data(idx, roleDeviceId) === Settings.deviceId) {
                        currentDeviceName = profile.deviceList.data(idx, roleDeviceName) || "";
                        return;
                    }
                }
            }

            function updateOtherDevicesCount() {
                if (!profile || !profile.deviceList) {
                    otherDevicesCount = 0;
                    otherDevicesExpanded = false;
                    return;
                }

                var roleDeviceId = 0; // DeviceInfoModel::DeviceId
                var count = 0;
                for (var i = 0; i < profile.deviceList.rowCount(); i++) {
                    var idx = profile.deviceList.index(i, 0);
                    if (profile.deviceList.data(idx, roleDeviceId) !== Settings.deviceId)
                        count++;
                }

                otherDevicesCount = count;
                if (count === 0)
                    otherDevicesExpanded = false;
            }

            Component.onCompleted: {
                Komai.updateUserProfile();
                accountView.updateOtherDevicesCount();
            }

            Connections {
                target: Komai
                function onProfileChanged() {
                    accountView.profile = Komai.currentUser;
                    accountView.currentDeviceName = "";
                    accountView.otherDevicesExpanded = false;
                    accountView.lookupCurrentDeviceName();
                    accountView.updateOtherDevicesCount();
                }
            }

            Connections {
                target: accountView.profile ? accountView.profile : null
                function onDevicesChanged() {
                    accountView.lookupCurrentDeviceName();
                    accountView.updateOtherDevicesCount();
                }
            }

            Flickable {
                id: scrollView
                property real availableWidth: width
                anchors.left: parent.left
                anchors.right: scrollBar.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                contentWidth: width
                contentHeight: scrollContent.height + Komai.paddingLarge
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Components.FlickableWheelBooster { flickable: scrollView }

                ColumnLayout {
                    id: scrollContent
                width: scrollView.width
                    spacing: Komai.paddingSmall

                    property real sideMargin: Komai.paddingMedium

                    Item { Layout.preferredHeight: Komai.paddingMedium }

                    // ── Profile section ──────────────────────────────────────────

                    Components.SettingsSection {
                        label: qsTr("Profile")
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        visible: root.sectionVisible("profile")
                    }

                    // Avatar row
                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
                        visible: root.sectionVisible("profile")
                        Layout.rightMargin: scrollContent.sideMargin
                        implicitHeight: avatarRowContent.implicitHeight

                        HoverHandler { id: avatarRowHover; blocking: false }
                        Rectangle {
                            anchors.fill: avatarRowContent
                            color: avatarRowHover.hovered ? palette.dark : palette.window
                            radius: Komai.paddingMedium
                            z: -1
                        }

                        RowLayout {
                            id: avatarRowContent
                            width: parent.width
                            spacing: Komai.paddingMedium

                            Layout.topMargin: Komai.paddingMedium
                            Layout.bottomMargin: Komai.paddingMedium

                            Item {
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                Layout.leftMargin: Komai.paddingMedium
                                Layout.preferredWidth: avatarLabel.implicitWidth
                                Layout.preferredHeight: avatarLabel.implicitHeight

                                Label {
                                    id: avatarLabel
                                    text: qsTr("Avatar")
                                    color: avatarRowHover.hovered ? palette.brightText : palette.text
                                    font.pointSize: 1.1 * Settings.uiFontSizePt
                                }
                            }

                            Components.SyncedToMatrixBadge {
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Item { Layout.fillWidth: true }

                            Components.KomaiButton {
                                text: qsTr("Change")
                                icon.source: "qrc:/icons/icons/ui/edit.svg"
                                visible: accountView.profile
                                onClicked: accountView.profile.changeAvatar()
                            }

                            Components.KomaiButton {
                                text: qsTr("Remove")
                                icon.source: "qrc:/icons/icons/ui/delete.svg"
                                visible: accountView.profile && accountView.profile.avatarUrl !== ""
                                onClicked: confirmRemoveAvatarDialog.open()
                            }

                            Components.Avatar {
                                url: accountView.profile ? accountView.profile.avatarUrl.replace("mxc://", "image://MxcImage/") : ""
                                displayName: accountView.profile ? accountView.profile.displayName : ""
                                userid: accountView.profile ? accountView.profile.userid : ""
                                Layout.preferredHeight: Komai.iconSize
                                Layout.preferredWidth: Komai.iconSize
                                Layout.rightMargin: Komai.paddingMedium
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                enabled: false
                            }
                        }

                        Components.OverlayDialog {
                            id: confirmRemoveAvatarDialog
                            title: qsTr("Remove avatar")
                            titleIcon: ":/icons/icons/ui/delete.svg"

                            Label {
                                Layout.fillWidth: true
                                color: palette.text
                                wrapMode: Text.WordWrap
                                text: qsTr("Are you sure you want to remove your avatar?")
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Komai.paddingMedium

                                Components.KomaiButton {
                                    text: qsTr("Cancel")
                                    onClicked: confirmRemoveAvatarDialog.close()
                                }

                                Item { Layout.fillWidth: true }

                                Components.KomaiButton {
                                    text: qsTr("Remove")
                                    highlighted: true
                                    onClicked: {
                                        if (accountView.profile)
                                            accountView.profile.removeAvatar();
                                        confirmRemoveAvatarDialog.close();
                                    }
                                }
                            }
                        }
                    }

                    // Display name row
                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        implicitHeight: displayNameRowContent.implicitHeight
                        visible: root.sectionVisible("profile")

                        HoverHandler { id: displayNameRowHover; blocking: false }
                        Rectangle {
                            anchors.fill: displayNameRowContent
                            color: displayNameRowHover.hovered ? palette.dark : palette.window
                            radius: Komai.paddingMedium
                            z: -1
                        }

                        RowLayout {
                            id: displayNameRowContent
                            width: parent.width
                            spacing: Komai.paddingMedium

                            Item {
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                Layout.leftMargin: Komai.paddingMedium
                                Layout.preferredWidth: displayNameLabel.implicitWidth
                                Layout.preferredHeight: displayNameLabel.implicitHeight

                                Label {
                                    id: displayNameLabel
                                    text: qsTr("Display name")
                                    color: displayNameRowHover.hovered ? palette.brightText : palette.text
                                    font.pointSize: 1.1 * Settings.uiFontSizePt
                                }
                            }

                            Components.SyncedToMatrixBadge {
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Item { Layout.fillWidth: true }

                            Components.KomaiTextField {
                                id: displayNameField

                                property string lastSubmitted: ""
                                property string serverValue: accountView.profile ? accountView.profile.displayName : ""
                                onServerValueChanged: {
                                    if (text !== serverValue && lastSubmitted === "")
                                        text = serverValue;
                                    lastSubmitted = "";
                                }

                                text: serverValue
                                Layout.preferredWidth: scrollView.availableWidth * 0.4
                                Layout.minimumWidth: 150
                                Layout.rightMargin: Komai.paddingMedium
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium

                                function applyName() {
                                    var val = text.trim();
                                    if (accountView.profile && val !== serverValue && val !== lastSubmitted && val.length > 0) {
                                        lastSubmitted = val;
                                        accountView.profile.changeUsername(val);
                                    }
                                }

                                onEditingFinished: applyName()
                                onActiveFocusChanged: if (!activeFocus) applyName()
                            }
                        }
                    }

                    // User ID row (read-only)
                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        implicitHeight: userIdRowContent.implicitHeight
                        visible: root.sectionVisible("profile")

                        HoverHandler { id: userIdRowHover; blocking: false }
                        Rectangle {
                            anchors.fill: userIdRowContent
                            color: userIdRowHover.hovered ? palette.dark : palette.window
                            radius: Komai.paddingMedium
                            z: -1
                        }

                        RowLayout {
                            id: userIdRowContent
                            width: parent.width
                            spacing: Komai.paddingMedium

                            Label {
                                text: qsTr("User ID")
                                color: userIdRowHover.hovered ? palette.brightText : palette.text
                                font.pointSize: 1.1 * Settings.uiFontSizePt
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                Layout.leftMargin: Komai.paddingMedium
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: Settings.userId
                                color: userIdRowHover.hovered ? palette.brightText : palette.buttonText
                                font.pointSize: Settings.uiFontSizePt
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                elide: Text.ElideRight
                                Layout.maximumWidth: scrollView.availableWidth * 0.5
                            }

                            Components.ImageButton {
                                id: copyUserIdBtn

                                property bool copied: false

                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                Layout.rightMargin: Komai.paddingMedium
                                buttonTextColor: userIdRowHover.hovered ? palette.brightText : palette.buttonText
                                image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                hoverEnabled: true
                                toolTipVisible: hovered
                                toolTipText: copied ? qsTr("Copied!") : qsTr("Copy user ID")
                                onClicked: {
                                    Clipboard.text = Settings.userId;
                                    copied = true;
                                    copyUserIdTimer.restart();
                                }

                                Timer {
                                    id: copyUserIdTimer
                                    interval: 2000
                                    onTriggered: copyUserIdBtn.copied = false
                                }
                            }
                        }
                    }

                    // Homeserver row (read-only)
                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        implicitHeight: homeserverRowContent.implicitHeight
                        visible: root.sectionVisible("profile")

                        HoverHandler { id: homeserverRowHover; blocking: false }
                        Rectangle {
                            anchors.fill: homeserverRowContent
                            color: homeserverRowHover.hovered ? palette.dark : palette.window
                            radius: Komai.paddingMedium
                            z: -1
                        }

                        RowLayout {
                            id: homeserverRowContent
                            width: parent.width
                            spacing: Komai.paddingMedium

                            Label {
                                text: qsTr("Homeserver")
                                color: homeserverRowHover.hovered ? palette.brightText : palette.text
                                font.pointSize: 1.1 * Settings.uiFontSizePt
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                Layout.leftMargin: Komai.paddingMedium
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: Settings.homeserver
                                color: homeserverRowHover.hovered ? palette.brightText : palette.buttonText
                                font.pointSize: Settings.uiFontSizePt
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                elide: Text.ElideRight
                                Layout.maximumWidth: scrollView.availableWidth * 0.5
                            }

                            Components.ImageButton {
                                id: copyHomeserverBtn

                                property bool copied: false

                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                Layout.rightMargin: Komai.paddingMedium
                                buttonTextColor: homeserverRowHover.hovered ? palette.brightText : palette.buttonText
                                image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                hoverEnabled: true
                                toolTipVisible: hovered
                                toolTipText: copied ? qsTr("Copied!") : qsTr("Copy homeserver")
                                onClicked: {
                                    Clipboard.text = Settings.homeserver;
                                    copied = true;
                                    copyHomeserverTimer.restart();
                                }

                                Timer {
                                    id: copyHomeserverTimer
                                    interval: 2000
                                    onTriggered: copyHomeserverBtn.copied = false
                                }
                            }
                        }
                    }

                    // ── This device (session) section ────────────────────────────

                    Components.SettingsSection {
                        label: qsTr("This device (session)")
                        Layout.fillWidth: true
                        Layout.topMargin: root.anyEarlierSectionVisible("thisDevice") ? Komai.paddingLarge : 0
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        visible: root.sectionVisible("thisDevice")
                    }

                    // Current device card
                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
                        visible: root.sectionVisible("thisDevice")
                        Layout.rightMargin: scrollContent.sideMargin
                        implicitHeight: currentDeviceCard.implicitHeight

                        Rectangle {
                            id: currentDeviceCard
                            width: parent.width
                            implicitHeight: currentDeviceCardContent.implicitHeight
                            color: palette.window
                            radius: Komai.paddingMedium
                            border.width: 1
                            border.color: Komai.theme.separator
                            clip: true

                            ColumnLayout {
                                id: currentDeviceCardContent
                                width: parent.width
                                spacing: 0

                                readonly property bool useStackedLayout: width < Komai.settingRowStackBreakpoint
                                readonly property real controlWidth: useStackedLayout
                                    ? Math.max(0, width - Komai.paddingMedium * 2)
                                    : Math.min(500, Math.max(240, width - Komai.paddingLarge * 2))

                                // Header: [device ID] [Copy] ... [Logout]
                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: currentDeviceHeaderRow.implicitHeight + 1

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 1
                                        color: Komai.theme.separator
                                    }

                                    RowLayout {
                                        id: currentDeviceHeaderRow
                                        width: parent.width
                                        spacing: Komai.paddingSmall

                                        Item { Layout.preferredWidth: Komai.paddingSmall }

                                        // "This device" verification badge
                                        Rectangle {
                                            Layout.alignment: Qt.AlignVCenter
                                            Layout.topMargin: Komai.paddingMedium + 2
                                            Layout.bottomMargin: Komai.paddingMedium + 2
                                            implicitWidth: currentDeviceBadgeRow.implicitWidth + Komai.paddingSmall * 2
                                            implicitHeight: currentDeviceBadgeRow.implicitHeight + Komai.paddingSmall
                                            radius: Komai.paddingSmall
                                            color: Qt.rgba(Komai.theme.success.r, Komai.theme.success.g, Komai.theme.success.b, 0.15)
                                            border.color: Komai.theme.success
                                            border.width: 1

                                            RowLayout {
                                                id: currentDeviceBadgeRow
                                                anchors.centerIn: parent
                                                spacing: Komai.paddingSmall

                                                Image {
                                                    readonly property int badgeIconSize: Math.max(8, Math.round(Settings.uiFontSizePt * 0.9))
                                                    Layout.preferredHeight: badgeIconSize
                                                    Layout.preferredWidth: badgeIconSize
                                                    sourceSize.height: height
                                                    sourceSize.width: width
                                                    source: "image://colorimage/:/icons/icons/ui/checkmark.svg?" + Komai.theme.success
                                                }

                                                Label {
                                                    text: qsTr("This device")
                                                    color: Komai.theme.success
                                                    font.pointSize: Math.floor(Settings.uiFontSizePt * 0.85)
                                                }
                                            }
                                        }

                                        Label {
                                            text: Settings.deviceId
                                            font.bold: true
                                            color: palette.text
                                            Layout.alignment: Qt.AlignVCenter
                                            Layout.topMargin: Komai.paddingMedium + 2
                                            Layout.bottomMargin: Komai.paddingMedium + 2
                                            elide: Text.ElideRight
                                        }

                                        Components.ImageButton {
                                            id: copyDeviceIdBtn
                                            property bool copied: false
                                            Layout.topMargin: Komai.paddingMedium + 2
                                            Layout.bottomMargin: Komai.paddingMedium + 2
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                            toolTipVisible: hovered
                                            toolTipText: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                                            onClicked: {
                                                Clipboard.text = Settings.deviceId;
                                                copied = true;
                                                copyDeviceIdTimer.start();
                                            }
                                            Timer {
                                                id: copyDeviceIdTimer
                                                interval: 2000
                                                onTriggered: copyDeviceIdBtn.copied = false
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Components.KomaiButton {
                                            text: qsTr("Sign out")
                                            icon.source: "qrc:/icons/icons/ui/power-off.svg"
                                            Layout.topMargin: Komai.paddingMedium + 2
                                            Layout.bottomMargin: Komai.paddingMedium + 2
                                            Layout.rightMargin: Komai.paddingSmall
                                            onClicked: Komai.openLogoutDialog()
                                        }
                                    }
                                }

                                // Body: Name (editable, auto-persist)
                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Komai.paddingMedium
                                    Layout.leftMargin: Komai.paddingMedium
                                    Layout.rightMargin: Komai.paddingMedium
                                    columns: currentDeviceCardContent.useStackedLayout ? 1 : 2
                                    rowSpacing: Komai.paddingSmall
                                    columnSpacing: Komai.paddingSmall

                                    RowLayout {
                                        Layout.row: 0
                                        Layout.column: 0
                                        Layout.fillWidth: !currentDeviceCardContent.useStackedLayout
                                        spacing: Komai.paddingSmall

                                        Label {
                                            text: qsTr("Name")
                                            color: palette.text
                                            font.pointSize: 1.1 * Settings.uiFontSizePt
                                        }

                                        Components.SyncedToMatrixBadge {
                                            Layout.alignment: Qt.AlignVCenter
                                        }

                                        Item {
                                            Layout.fillWidth: !currentDeviceCardContent.useStackedLayout
                                        }
                                    }

                                    Components.KomaiTextField {
                                        id: currentDeviceNameField

                                        Layout.row: currentDeviceCardContent.useStackedLayout ? 1 : 0
                                        Layout.column: currentDeviceCardContent.useStackedLayout ? 0 : 1
                                        Layout.alignment: currentDeviceCardContent.useStackedLayout
                                            ? Qt.AlignLeft
                                            : Qt.AlignRight

                                        property string lastSubmitted: ""
                                        property string serverValue: accountView.currentDeviceName
                                        onServerValueChanged: {
                                            if (text !== serverValue && lastSubmitted === "")
                                                text = serverValue;
                                            lastSubmitted = "";
                                        }

                                        text: serverValue
                                        font.pointSize: Settings.uiFontSizePt
                                        Layout.preferredWidth: currentDeviceCardContent.controlWidth
                                        Layout.maximumWidth: currentDeviceCardContent.controlWidth

                                        function applyName() {
                                            var val = text.trim();
                                            if (accountView.profile && val !== serverValue && val !== lastSubmitted && val.length > 0) {
                                                lastSubmitted = val;
                                                accountView.profile.changeDeviceName(Settings.deviceId, val);
                                            }
                                        }

                                        onEditingFinished: applyName()
                                        onActiveFocusChanged: if (!activeFocus) applyName()
                                    }

                                }


                                // Body: Access token
                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Komai.paddingSmall
                                    Layout.leftMargin: Komai.paddingMedium
                                    Layout.rightMargin: Komai.paddingMedium
                                    columns: currentDeviceCardContent.useStackedLayout ? 1 : 2
                                    rowSpacing: Komai.paddingSmall
                                    columnSpacing: Komai.paddingSmall

                                    Label {
                                        Layout.row: 0
                                        Layout.column: 0
                                        text: qsTr("Access token")
                                        color: palette.text
                                        font.pointSize: 1.1 * Settings.uiFontSizePt
                                        Layout.fillWidth: !currentDeviceCardContent.useStackedLayout
                                    }

                                    RowLayout {
                                        Layout.row: currentDeviceCardContent.useStackedLayout ? 1 : 0
                                        Layout.column: currentDeviceCardContent.useStackedLayout ? 0 : 1
                                        Layout.fillWidth: currentDeviceCardContent.useStackedLayout
                                        Layout.alignment: currentDeviceCardContent.useStackedLayout
                                            ? Qt.AlignLeft
                                            : Qt.AlignRight
                                        Layout.preferredWidth: currentDeviceCardContent.controlWidth
                                        Layout.maximumWidth: currentDeviceCardContent.controlWidth
                                        spacing: Komai.paddingSmall

                                        Loader {
                                            id: accessTokenLoader
                                            Layout.fillWidth: true
                                            property bool revealed: false
                                            sourceComponent: revealed ? revealedTokenComponent : hiddenTokenComponent
                                        }

                                        Component {
                                            id: hiddenTokenComponent
                                            Item {
                                                implicitHeight: revealButton.implicitHeight
                                                implicitWidth: revealButton.implicitWidth
                                                Components.KomaiButton {
                                                    id: revealButton
                                                    anchors.right: parent.right
                                                    font.pointSize: Settings.uiFontSizePt
                                                    icon.source: "qrc:/icons/icons/ui/eye-show.svg"
                                                    text: qsTr("Click to reveal")
                                                    onClicked: accessTokenLoader.revealed = true
                                                }
                                            }
                                        }

                                        Component {
                                            id: revealedTokenComponent
                                            Components.KomaiTextField {
                                                text: Settings.accessToken
                                                readOnly: true
                                                font.pointSize: Settings.uiFontSizePt
                                            }
                                        }

                                        Components.ImageButton {
                                            id: copyTokenBtn
                                            visible: accessTokenLoader.revealed
                                            property bool copied: false
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                            toolTipVisible: hovered
                                            toolTipText: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                                            onClicked: {
                                                Clipboard.text = Settings.accessToken;
                                                copied = true;
                                                copyTokenTimer.start();
                                            }

                                            Timer {
                                                id: copyTokenTimer
                                                interval: 2000
                                                onTriggered: copyTokenBtn.copied = false
                                            }
                                        }
                                    }
                                }

                                // Access token warning
                                Text {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Komai.paddingMedium
                                    Layout.rightMargin: Komai.paddingMedium
                                    Layout.topMargin: 2
                                    text: qsTr("Access tokens grant full account access. Keep private!")
                                    color: Komai.theme.attention
                                    font.pointSize: Settings.uiFontSizePt
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: currentDeviceCardContent.useStackedLayout
                                        ? Text.AlignLeft
                                        : Text.AlignRight
                                }

                                // Body: Encryption keys export/import
                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Komai.paddingMedium
                                    Layout.leftMargin: Komai.paddingMedium
                                    Layout.rightMargin: Komai.paddingMedium
                                    columns: currentDeviceCardContent.useStackedLayout ? 1 : 2
                                    rowSpacing: Komai.paddingSmall
                                    columnSpacing: Komai.paddingSmall

                                    Label {
                                        Layout.row: 0
                                        Layout.column: 0
                                        text: qsTr("Encryption keys")
                                        color: palette.text
                                        font.pointSize: 1.1 * Settings.uiFontSizePt
                                        Layout.fillWidth: !currentDeviceCardContent.useStackedLayout
                                    }

                                    RowLayout {
                                        Layout.row: currentDeviceCardContent.useStackedLayout ? 1 : 0
                                        Layout.column: currentDeviceCardContent.useStackedLayout ? 0 : 1
                                        Layout.alignment: currentDeviceCardContent.useStackedLayout
                                            ? Qt.AlignLeft
                                            : Qt.AlignRight
                                        spacing: Komai.paddingSmall

                                        Components.KomaiButton {
                                            text: qsTr("Export…")
                                            icon.source: "qrc:/icons/icons/ui/download.svg"
                                            onClicked: exportEncryptionKeysDialog.open()
                                        }

                                        Components.KomaiButton {
                                            text: qsTr("Import…")
                                            icon.source: "qrc:/icons/icons/ui/upload.svg"
                                            onClicked: importEncryptionKeysDialog.open()
                                        }
                                    }
                                }

                                // Encryption keys hint
                                Text {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Komai.paddingMedium
                                    Layout.rightMargin: Komai.paddingMedium
                                    Layout.topMargin: 2
                                    Layout.bottomMargin: Komai.paddingMedium
                                    text: qsTr("Manual export/import of message decryption keys. Usually not needed when online key backup is enabled.")
                                    color: palette.buttonText
                                    font.pointSize: Settings.uiFontSizePt
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: currentDeviceCardContent.useStackedLayout
                                        ? Text.AlignLeft
                                        : Text.AlignRight
                                }
                            }
                        }
                    }

                    // ── Other devices (sessions) section ─────────────────────────

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: root.anyEarlierSectionVisible("otherDevices") ? Komai.paddingLarge : 0
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        spacing: Komai.paddingSmall
                        visible: accountView.otherDevicesCount > 0 && root.sectionVisible("otherDevices")

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Komai.paddingSmall

                            Label {
                                text: qsTr("Other devices (sessions)")
                                color: palette.text
                                font.pointSize: 1.1 * Settings.uiFontSizePt
                                font.capitalization: Font.AllUppercase
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Item { Layout.fillWidth: true }

                            Components.KomaiButton {
                                id: refreshDevicesButton

                                property bool refreshed: false

                                text: refreshed ? qsTr("Refreshed") : qsTr("Refresh")
                                icon.source: refreshed ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/refresh.svg"
                                onClicked: {
                                    if (accountView.profile)
                                        accountView.profile.refreshDevices();
                                    refreshed = true;
                                    refreshFeedbackTimer.restart();
                                }

                                Timer {
                                    id: refreshFeedbackTimer
                                    interval: 1200
                                    repeat: false
                                    onTriggered: refreshDevicesButton.refreshed = false
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Komai.paddingLarge

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.right: parent.right
                                color: palette.buttonText
                                height: 1
                            }
                        }

                        Components.KomaiActionRowButton {
                            labelText: accountView.otherDevicesExpanded
                                ? qsTr("Hide other devices")
                                : qsTr("Show all (%1) devices").arg(accountView.otherDevicesCount)
                            iconSource: accountView.otherDevicesExpanded
                                ? ":/icons/icons/ui/chevron-circle-up.svg"
                                : ":/icons/icons/ui/chevron-circle-down.svg"
                            onClicked: accountView.otherDevicesExpanded = !accountView.otherDevicesExpanded
                        }
                    }

                    // Loading spinner
                    UI.Spinner {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: Komai.paddingMedium
                        running: accountView.otherDevicesExpanded && accountView.profile && accountView.profile.isLoading
                        visible: accountView.otherDevicesExpanded && accountView.profile && accountView.profile.isLoading && root.sectionVisible("otherDevices")
                        foreground: palette.mid
                    }

                    // Other devices list
                    Repeater {
                        id: otherDevicesRepeater
                        // Suppress the model entirely when this section is hidden by
                        // the search filter — Repeater delegates are parented to the
                        // outer scrollContent and don't have their own visibility
                        // collapse, so we drop them by clearing the model.
                        model: root.sectionVisible("otherDevices")
                            ? (accountView.profile ? accountView.profile.deviceList : null)
                            : null

                        delegate: Item {
                            id: deviceDelegate
                            required property int verificationStatus
                            required property string deviceId
                            required property string deviceName
                            required property string lastIp
                            required property var lastTs

                            // Skip current device
                            visible: accountView.otherDevicesExpanded && deviceId !== Settings.deviceId
                            Layout.fillWidth: true
                            Layout.leftMargin: scrollContent.sideMargin
                            Layout.rightMargin: scrollContent.sideMargin
                            Layout.topMargin: Komai.paddingLarge
                            implicitHeight: visible ? deviceCard.implicitHeight : 0

                            Rectangle {
                                id: deviceCard
                                width: parent.width
                                implicitHeight: deviceDelegateContent.implicitHeight
                                color: palette.window
                                radius: Komai.paddingMedium
                                border.width: 1
                                border.color: Komai.theme.separator
                                clip: true

                                ColumnLayout {
                                    id: deviceDelegateContent
                                    width: parent.width

                                    readonly property bool useStackedLayout: width < Komai.settingRowStackBreakpoint
                                    readonly property real controlWidth: useStackedLayout
                                        ? Math.max(0, width - Komai.paddingMedium * 2)
                                        : Math.min(500, Math.max(240, width - Komai.paddingLarge * 2))
                                    spacing: 0

                                    // Header: [shield] [device ID] [Copy] ... [Logout]
                                    Item {
                                        Layout.fillWidth: true
                                        implicitHeight: deviceHeaderRow.implicitHeight + 1

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: 1
                                            color: Komai.theme.separator
                                        }

                                        GridLayout {
                                            id: deviceHeaderRow
                                            width: parent.width
                                            columns: deviceDelegateContent.useStackedLayout ? 1 : 2
                                            rowSpacing: 0
                                            columnSpacing: Komai.paddingSmall

                                            // Identity cluster: [badge] [deviceId] [copy]
                                            RowLayout {
                                                Layout.row: 0
                                                Layout.column: 0
                                                Layout.fillWidth: !deviceDelegateContent.useStackedLayout
                                                Layout.leftMargin: Komai.paddingSmall
                                                Layout.minimumWidth: 0
                                                spacing: Komai.paddingSmall

                                                // Verification status badge
                                                Rectangle {
                                                    Layout.alignment: Qt.AlignVCenter
                                                    Layout.topMargin: Komai.paddingMedium + 2
                                                    Layout.bottomMargin: Komai.paddingMedium + 2
                                                    visible: deviceDelegate.verificationStatus != VerificationStatus.NOT_APPLICABLE
                                                    implicitWidth: otherDeviceBadgeRow.implicitWidth + Komai.paddingSmall * 2
                                                    implicitHeight: otherDeviceBadgeRow.implicitHeight + Komai.paddingSmall
                                                    radius: Komai.paddingSmall
                                                    color: {
                                                        switch (deviceDelegate.verificationStatus) {
                                                        case VerificationStatus.VERIFIED:
                                                        case VerificationStatus.SELF:
                                                            return Qt.rgba(Komai.theme.success.r, Komai.theme.success.g, Komai.theme.success.b, 0.15);
                                                        case VerificationStatus.UNVERIFIED:
                                                            return Qt.rgba(Komai.theme.warning.r, Komai.theme.warning.g, Komai.theme.warning.b, 0.15);
                                                        default:
                                                            return Qt.rgba(Komai.theme.error.r, Komai.theme.error.g, Komai.theme.error.b, 0.15);
                                                        }
                                                    }
                                                    border.color: {
                                                        switch (deviceDelegate.verificationStatus) {
                                                        case VerificationStatus.VERIFIED:
                                                        case VerificationStatus.SELF:
                                                            return Komai.theme.success;
                                                        case VerificationStatus.UNVERIFIED:
                                                            return Komai.theme.warning;
                                                        default:
                                                            return Komai.theme.error;
                                                        }
                                                    }
                                                    border.width: 1

                                                    RowLayout {
                                                        id: otherDeviceBadgeRow
                                                        anchors.centerIn: parent
                                                        spacing: Komai.paddingSmall

                                                        Image {
                                                            readonly property int badgeIconSize: Math.max(8, Math.round(Settings.uiFontSizePt * 0.9))
                                                            Layout.preferredHeight: badgeIconSize
                                                            Layout.preferredWidth: badgeIconSize
                                                            sourceSize.height: height
                                                            sourceSize.width: width
                                                            source: {
                                                                switch (deviceDelegate.verificationStatus) {
                                                                case VerificationStatus.VERIFIED:
                                                                    return "image://colorimage/:/icons/icons/ui/shield-regular-checkmark.svg?" + Komai.theme.success;
                                                                case VerificationStatus.UNVERIFIED:
                                                                    return "image://colorimage/:/icons/icons/ui/shield-regular-exclamation-mark.svg?" + Komai.theme.warning;
                                                                case VerificationStatus.SELF:
                                                                    return "image://colorimage/:/icons/icons/ui/checkmark.svg?" + Komai.theme.success;
                                                                default:
                                                                    return "image://colorimage/:/icons/icons/ui/shield-regular-cross.svg?" + Komai.theme.error;
                                                                }
                                                            }
                                                        }

                                                        Label {
                                                            text: {
                                                                switch (deviceDelegate.verificationStatus) {
                                                                case VerificationStatus.VERIFIED:
                                                                    return qsTr("Verified");
                                                                case VerificationStatus.UNVERIFIED:
                                                                    return qsTr("Unverified");
                                                                case VerificationStatus.SELF:
                                                                    return qsTr("This device");
                                                                default:
                                                                    return qsTr("Blocked");
                                                                }
                                                            }
                                                            color: {
                                                                switch (deviceDelegate.verificationStatus) {
                                                                case VerificationStatus.VERIFIED:
                                                                case VerificationStatus.SELF:
                                                                    return Komai.theme.success;
                                                                case VerificationStatus.UNVERIFIED:
                                                                    return Komai.theme.warning;
                                                                default:
                                                                    return Komai.theme.error;
                                                                }
                                                            }
                                                            font.pointSize: Math.floor(Settings.uiFontSizePt * 0.85)
                                                        }
                                                    }
                                                }

                                                Label {
                                                    Layout.fillWidth: true
                                                    Layout.minimumWidth: 0
                                                    text: deviceDelegate.deviceId
                                                    font.bold: true
                                                    color: palette.text
                                                    Layout.alignment: Qt.AlignVCenter
                                                    Layout.topMargin: Komai.paddingMedium + 2
                                                    Layout.bottomMargin: Komai.paddingMedium + 2
                                                    elide: Text.ElideRight
                                                }

                                                Components.ImageButton {
                                                    id: copyOtherDeviceIdBtn
                                                    property bool copied: false
                                                    Layout.topMargin: Komai.paddingMedium + 2
                                                    Layout.bottomMargin: Komai.paddingMedium + 2
                                                    Layout.preferredWidth: 24
                                                    Layout.preferredHeight: 24
                                                    image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                                    toolTipVisible: hovered
                                                    toolTipText: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                                                    onClicked: {
                                                        Clipboard.text = deviceDelegate.deviceId;
                                                        copied = true;
                                                        copyOtherDeviceIdTimer.start();
                                                    }
                                                    Timer {
                                                        id: copyOtherDeviceIdTimer
                                                        interval: 2000
                                                        onTriggered: copyOtherDeviceIdBtn.copied = false
                                                    }
                                                }
                                            }

                                            // Action cluster: [Verify | Unverify] [Block] [Sign out]
                                            Flow {
                                                Layout.row: deviceDelegateContent.useStackedLayout ? 1 : 0
                                                Layout.column: deviceDelegateContent.useStackedLayout ? 0 : 1
                                                Layout.fillWidth: deviceDelegateContent.useStackedLayout
                                                Layout.alignment: deviceDelegateContent.useStackedLayout
                                                    ? Qt.AlignLeft | Qt.AlignVCenter
                                                    : Qt.AlignRight | Qt.AlignVCenter
                                                Layout.leftMargin: deviceDelegateContent.useStackedLayout ? Komai.paddingSmall : 0
                                                Layout.rightMargin: Komai.paddingSmall
                                                Layout.topMargin: deviceDelegateContent.useStackedLayout ? 0 : Komai.paddingMedium + 2
                                                Layout.bottomMargin: Komai.paddingMedium + 2
                                                spacing: Komai.paddingSmall

                                                Components.KomaiButton {
                                                    visible: deviceDelegate.verificationStatus === VerificationStatus.UNVERIFIED
                                                    text: qsTr("Verify")
                                                    icon.source: "qrc:/icons/icons/ui/shield-regular-checkmark.svg"
                                                    onClicked: {
                                                        if (accountView.profile)
                                                            accountView.profile.verify(deviceDelegate.deviceId);
                                                    }
                                                }

                                                Components.KomaiButton {
                                                    visible: deviceDelegate.verificationStatus === VerificationStatus.VERIFIED
                                                    text: qsTr("Unverify")
                                                    icon.source: "qrc:/icons/icons/ui/shield-regular-exclamation-mark.svg"
                                                    onClicked: {
                                                        if (accountView.profile)
                                                            accountView.profile.unverify(deviceDelegate.deviceId);
                                                    }
                                                }

                                                Components.KomaiButton {
                                                    text: deviceDelegate.verificationStatus === VerificationStatus.BLOCKED
                                                        ? qsTr("Unblock")
                                                        : qsTr("Block")
                                                    icon.source: deviceDelegate.verificationStatus === VerificationStatus.BLOCKED
                                                        ? "qrc:/icons/icons/ui/shield-regular-exclamation-mark.svg"
                                                        : "qrc:/icons/icons/ui/shield-regular-cross.svg"
                                                    onClicked: {
                                                        if (!accountView.profile)
                                                            return;

                                                        if (deviceDelegate.verificationStatus === VerificationStatus.BLOCKED)
                                                            accountView.profile.unblockDevice(deviceDelegate.deviceId);
                                                        else
                                                            accountView.profile.blockDevice(deviceDelegate.deviceId);
                                                    }
                                                }

                                                Components.KomaiButton {
                                                    text: qsTr("Sign out")
                                                    icon.source: "qrc:/icons/icons/ui/power-off.svg"
                                                    onClicked: {
                                                        if (accountView.profile)
                                                            accountView.profile.signOutDevice(deviceDelegate.deviceId);
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Body: Name (editable, auto-persist)
                                    GridLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Komai.paddingMedium
                                        Layout.leftMargin: Komai.paddingMedium
                                        Layout.rightMargin: Komai.paddingMedium
                                        columns: deviceDelegateContent.useStackedLayout ? 1 : 2
                                        rowSpacing: Komai.paddingSmall
                                        columnSpacing: Komai.paddingSmall

                                        RowLayout {
                                            Layout.row: 0
                                            Layout.column: 0
                                            Layout.fillWidth: !deviceDelegateContent.useStackedLayout
                                            spacing: Komai.paddingSmall

                                            Label {
                                                text: qsTr("Name")
                                                color: palette.text
                                                font.pointSize: 1.1 * Settings.uiFontSizePt
                                            }

                                            Components.SyncedToMatrixBadge {
                                                Layout.alignment: Qt.AlignVCenter
                                            }

                                            Item {
                                                Layout.fillWidth: !deviceDelegateContent.useStackedLayout
                                            }
                                        }

                                        RowLayout {
                                            Layout.row: deviceDelegateContent.useStackedLayout ? 1 : 0
                                            Layout.column: deviceDelegateContent.useStackedLayout ? 0 : 1
                                            Layout.alignment: deviceDelegateContent.useStackedLayout
                                                ? Qt.AlignLeft
                                                : Qt.AlignRight
                                            Layout.preferredWidth: deviceDelegateContent.controlWidth
                                            Layout.maximumWidth: deviceDelegateContent.controlWidth
                                            spacing: Komai.paddingSmall

                                            Components.KomaiTextField {
                                                id: deviceNameEditField

                                                Layout.fillWidth: true

                                                property string lastSubmitted: ""
                                                property string serverValue: deviceDelegate.deviceName
                                                onServerValueChanged: {
                                                    if (text !== serverValue && lastSubmitted === "")
                                                        text = serverValue;
                                                    lastSubmitted = "";
                                                }

                                                text: serverValue
                                                font.pointSize: Settings.uiFontSizePt

                                                function applyName() {
                                                    var val = text.trim();
                                                    if (accountView.profile && val !== serverValue && val !== lastSubmitted && val.length > 0) {
                                                        lastSubmitted = val;
                                                        accountView.profile.changeDeviceName(deviceDelegate.deviceId, val);
                                                    }
                                                }

                                                onEditingFinished: applyName()
                                                onActiveFocusChanged: if (!activeFocus) applyName()
                                            }

                                            // Spacer to align with IP address row's copy button
                                            Item { Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                                        }
                                    }

                                    // Body: IP address with copy
                                    GridLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Komai.paddingSmall
                                        Layout.leftMargin: Komai.paddingMedium
                                        Layout.rightMargin: Komai.paddingMedium
                                        Layout.bottomMargin: Komai.paddingMedium
                                        columns: deviceDelegateContent.useStackedLayout ? 1 : 2
                                        rowSpacing: Komai.paddingSmall
                                        columnSpacing: Komai.paddingSmall

                                        Label {
                                            Layout.row: 0
                                            Layout.column: 0
                                            text: qsTr("IP address")
                                            color: palette.text
                                            font.pointSize: 1.1 * Settings.uiFontSizePt
                                            Layout.fillWidth: !deviceDelegateContent.useStackedLayout
                                        }

                                        RowLayout {
                                            Layout.row: deviceDelegateContent.useStackedLayout ? 1 : 0
                                            Layout.column: deviceDelegateContent.useStackedLayout ? 0 : 1
                                            Layout.alignment: deviceDelegateContent.useStackedLayout
                                                ? Qt.AlignLeft
                                                : Qt.AlignRight
                                            Layout.preferredWidth: deviceDelegateContent.controlWidth
                                            Layout.maximumWidth: deviceDelegateContent.controlWidth
                                            spacing: Komai.paddingSmall

                                            Components.KomaiTextField {
                                                id: deviceIpField
                                                Layout.fillWidth: true
                                                text: deviceDelegate.lastIp ? deviceDelegate.lastIp : "???"
                                                readOnly: true
                                                font.pointSize: Settings.uiFontSizePt
                                            }

                                            Components.ImageButton {
                                                id: copyIpBtn
                                                property bool copied: false
                                                Layout.preferredWidth: 24
                                                Layout.preferredHeight: 24
                                                image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                                toolTipVisible: hovered
                                                toolTipText: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                                                onClicked: {
                                                    Clipboard.text = deviceIpField.text;
                                                    copied = true;
                                                    copyIpTimer.start();
                                                }

                                                Timer {
                                                    id: copyIpTimer
                                                    interval: 2000
                                                    onTriggered: copyIpBtn.copied = false
                                                }
                                            }
                                        }
                                    }

                                    // Footer: Last seen timestamp
                                    Item {
                                        Layout.fillWidth: true
                                        implicitHeight: lastSeenLabel.implicitHeight + Komai.paddingSmall * 2 + 1

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            height: 1
                                            color: Komai.theme.separator
                                        }

                                        Label {
                                            id: lastSeenLabel
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.leftMargin: Komai.paddingMedium
                                            anchors.rightMargin: Komai.paddingMedium
                                            text: deviceDelegate.lastTs > 0 ? qsTr("Last seen: %1").arg(new Date(deviceDelegate.lastTs).toLocaleString(Locale.ShortFormat)) : qsTr("Last seen: Unknown")
                                            color: palette.buttonText
                                            font.pointSize: Settings.uiFontSizePt
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ── Users section ────────────────────────────────────────

                    Components.SettingsSection {
                        label: qsTr("Users")
                        Layout.fillWidth: true
                        Layout.topMargin: root.anyEarlierSectionVisible("users") ? Komai.paddingLarge : 0
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        visible: root.sectionVisible("users")
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        visible: root.sectionVisible("users")
                        implicitHeight: ignoredUsersRowContent.implicitHeight
                        HoverHandler { id: ignoredUsersRowHover; blocking: false }
                        Rectangle { anchors.fill: ignoredUsersRowContent; color: ignoredUsersRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
                        RowLayout {
                            id: ignoredUsersRowContent
                            width: parent.width
                            spacing: Komai.paddingMedium

                            Label {
                                text: qsTr("Ignored users")
                                color: ignoredUsersRowHover.hovered ? palette.brightText : palette.text
                                font.pointSize: 1.1 * Settings.uiFontSizePt
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                Layout.leftMargin: Komai.paddingMedium
                            }

                            Components.SyncedToMatrixBadge {
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Item { Layout.fillWidth: true }

                            SettingRowIgnoredUsers {
                                Layout.rightMargin: Komai.paddingMedium
                            }
                        }
                    }

                    LocalCacheSection {
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        Layout.topMargin: root.anyEarlierSectionVisible("localCache") ? Komai.paddingLarge : 0
                        visible: root.sectionVisible("localCache")
                    }

                    // Bottom spacer
                    Item { Layout.preferredHeight: Komai.paddingLarge }
                }
            }

            ScrollBar {
                id: scrollBar
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                policy: ScrollBar.AlwaysOn
                size: scrollView.contentHeight > 0 ? scrollView.height / scrollView.contentHeight : 1
                position: scrollView.visibleArea.yPosition
                visible: scrollView.contentHeight > 0
                onPositionChanged: {
                    if (active)
                        scrollView.contentY = position * scrollView.contentHeight
                }
            }
        }
    }

    Component {
        id: signedOutView
        Flickable {
            id: signedOutFlickable
            anchors.fill: parent
            contentWidth: width
            contentHeight: container.implicitHeight + Komai.paddingLarge * 2
            clip: true

            Components.FlickableWheelBooster { flickable: signedOutFlickable }

            ColumnLayout {
                id: container
                y: Komai.paddingLarge
                width: Math.max(0, parent.width - Komai.paddingLarge * 2)
                x: Komai.paddingLarge
                spacing: Komai.paddingMedium

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Account")
                    font.bold: true
                    font.pointSize: 1.2 * Settings.uiFontSizePt
                    color: palette.text
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("You are not logged in yet, so account details are unavailable.")
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                }
            }
        }
    }

    Encryption.ExportEncryptionKeysDialog {
        id: exportEncryptionKeysDialog
    }

    Encryption.ImportEncryptionKeysDialog {
        id: importEncryptionKeysDialog
    }
}
