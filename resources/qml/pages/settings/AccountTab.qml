// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../ui" as UI
import cc.etke.komai

Item {
    id: root
    property bool collapsed: false

    Loader {
        anchors.fill: parent
        sourceComponent: Settings.hasActiveSession ? accountSettingsView : signedOutView
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

            ScrollView {
                id: scrollView
                anchors.fill: parent
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    id: scrollContent
                    width: scrollView.availableWidth
                    spacing: Komai.paddingSmall

                    property real contentMaxWidth: Settings.uiLayoutContentMaxWidthEffectivePx > 0 ? Settings.uiLayoutContentMaxWidthEffectivePx : Number.POSITIVE_INFINITY
                    property real sideMargin: Math.max(Komai.paddingLarge, (scrollView.availableWidth - contentMaxWidth) / 2)

                    Item { Layout.preferredHeight: Komai.paddingMedium }

                    // ── Profile section ──────────────────────────────────────────

                    Components.SettingsSection {
                        label: qsTr("Profile")
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                    }

                    // Avatar row
                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
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
                                Layout.preferredHeight: 72
                                Layout.preferredWidth: 72
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
                                Layout.rightMargin: Komai.paddingMedium
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                elide: Text.ElideRight
                                Layout.maximumWidth: scrollView.availableWidth * 0.5
                            }
                        }
                    }

                    // Homeserver row (read-only)
                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        implicitHeight: homeserverRowContent.implicitHeight

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
                                Layout.rightMargin: Komai.paddingMedium
                                Layout.topMargin: Komai.paddingMedium
                                Layout.bottomMargin: Komai.paddingMedium
                                elide: Text.ElideRight
                                Layout.maximumWidth: scrollView.availableWidth * 0.5
                            }
                        }
                    }

                    // ── This device (session) section ────────────────────────────

                    Components.SettingsSection {
                        label: qsTr("This device (session)")
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingLarge
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                    }

                    // Current device card
                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: scrollContent.sideMargin
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

                                readonly property real controlWidth: Math.min(500, Math.max(240, width - Komai.paddingLarge * 2))

                                // Header: [device ID] [Copy] ... [Logout]
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: currentDeviceHeaderRow.implicitHeight + 1
                                    color: palette.window

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
                                            Layout.topMargin: Komai.paddingMedium
                                            Layout.bottomMargin: Komai.paddingMedium
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
                                            Layout.topMargin: Komai.paddingMedium
                                            Layout.bottomMargin: Komai.paddingMedium
                                            elide: Text.ElideRight
                                        }

                                        Components.ImageButton {
                                            id: copyDeviceIdBtn
                                            property bool copied: false
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                            ToolTip.visible: hovered
                                            ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
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
                                            text: qsTr("Logout")
                                            icon.source: "qrc:/icons/icons/ui/power-off.svg"
                                            Layout.rightMargin: Komai.paddingSmall
                                            onClicked: Komai.openLogoutDialog()
                                        }
                                    }
                                }

                                // Body: Name (editable, auto-persist)
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Komai.paddingMedium
                                    Layout.leftMargin: Komai.paddingMedium
                                    Layout.rightMargin: Komai.paddingMedium
                                    spacing: Komai.paddingSmall

                                    Label {
                                        text: qsTr("Name")
                                        color: palette.text
                                        font.pointSize: 1.1 * Settings.uiFontSizePt
                                        Layout.fillWidth: true
                                    }

                                    Components.KomaiTextField {
                                        id: currentDeviceNameField

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

                                    // Spacer to align with rows that have copy buttons
                                    Item { Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                                }

                                // Body: Device fingerprint
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Komai.paddingSmall
                                    Layout.leftMargin: Komai.paddingMedium
                                    Layout.rightMargin: Komai.paddingMedium
                                    spacing: Komai.paddingSmall

                                    Label {
                                        text: qsTr("Device fingerprint")
                                        color: palette.text
                                        font.pointSize: 1.1 * Settings.uiFontSizePt
                                        Layout.fillWidth: true
                                    }

                                    Components.KomaiTextField {
                                        id: fingerprintField
                                        text: UserSettingsModel.deviceFingerprint()
                                        readOnly: true
                                        font.pointSize: Settings.uiFontSizePt
                                        Layout.preferredWidth: currentDeviceCardContent.controlWidth
                                        Layout.maximumWidth: currentDeviceCardContent.controlWidth
                                    }

                                    Components.ImageButton {
                                        id: copyFingerprintBtn
                                        property bool copied: false
                                        Layout.preferredWidth: 24
                                        Layout.preferredHeight: 24
                                        image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                        ToolTip.visible: hovered
                                        ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                                        onClicked: {
                                            Clipboard.text = UserSettingsModel.deviceFingerprint();
                                            copied = true;
                                            copyFingerprintTimer.start();
                                        }
                                        Timer {
                                            id: copyFingerprintTimer
                                            interval: 2000
                                            onTriggered: copyFingerprintBtn.copied = false
                                        }
                                    }
                                }

                                // Body: Access token
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Komai.paddingSmall
                                    Layout.leftMargin: Komai.paddingMedium
                                    Layout.rightMargin: Komai.paddingMedium

                                    Label {
                                        text: qsTr("Access token")
                                        color: palette.text
                                        font.pointSize: 1.1 * Settings.uiFontSizePt
                                        Layout.fillWidth: true
                                    }

                                    Loader {
                                        id: accessTokenLoader
                                        Layout.preferredWidth: currentDeviceCardContent.controlWidth
                                        Layout.maximumWidth: currentDeviceCardContent.controlWidth
                                        property bool revealed: false
                                        sourceComponent: revealed ? revealedTokenComponent : hiddenTokenComponent
                                    }

                                    Component {
                                        id: hiddenTokenComponent
                                        Item {
                                            implicitHeight: revealButton.implicitHeight
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
                                        ToolTip.visible: hovered
                                        ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
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

                                // Access token warning
                                Text {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Komai.paddingMedium
                                    Layout.rightMargin: Komai.paddingMedium
                                    Layout.bottomMargin: Komai.paddingMedium
                                    text: qsTr("Your access token gives full access to your account. Keep it private!")
                                    color: Komai.theme.attention
                                    font.pointSize: Settings.uiFontSizePt
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignRight
                                    Layout.alignment: Qt.AlignRight
                                    Layout.preferredWidth: currentDeviceCardContent.controlWidth
                                    Layout.maximumWidth: currentDeviceCardContent.controlWidth
                                }
                            }
                        }
                    }

                    // ── Other devices (sessions) section ─────────────────────────

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingLarge
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        visible: accountView.otherDevicesCount > 0

                        Item { Layout.fillWidth: true }

                        Components.KomaiButton {
                            text: accountView.otherDevicesExpanded
                                ? qsTr("Hide other devices")
                                : qsTr("Show all (%1) devices").arg(accountView.otherDevicesCount)
                            icon.source: accountView.otherDevicesExpanded
                                ? "qrc:/icons/icons/ui/chevron-circle-up.svg"
                                : "qrc:/icons/icons/ui/chevron-circle-down.svg"
                            onClicked: accountView.otherDevicesExpanded = !accountView.otherDevicesExpanded
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // Other devices section heading with Refresh button
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                        spacing: Komai.paddingSmall
                        visible: accountView.otherDevicesExpanded

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
                    }

                    // Loading spinner
                    UI.Spinner {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: Komai.paddingMedium
                        running: accountView.otherDevicesExpanded && accountView.profile && accountView.profile.isLoading
                        visible: accountView.otherDevicesExpanded && accountView.profile && accountView.profile.isLoading
                        foreground: palette.mid
                    }

                    // Other devices list
                    Repeater {
                        id: otherDevicesRepeater
                        model: accountView.profile ? accountView.profile.deviceList : null

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

                                    readonly property real controlWidth: Math.min(500, Math.max(240, width - Komai.paddingLarge * 2))
                                    spacing: 0

                                    // Header: [shield] [device ID] [Copy] ... [Logout]
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: deviceHeaderRow.implicitHeight + 1
                                        color: palette.window

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: 1
                                            color: Komai.theme.separator
                                        }

                                        RowLayout {
                                            id: deviceHeaderRow
                                            width: parent.width
                                            spacing: Komai.paddingSmall

                                            Item { Layout.preferredWidth: Komai.paddingSmall }

                                            // Verification status badge
                                            Rectangle {
                                                Layout.alignment: Qt.AlignVCenter
                                                Layout.topMargin: Komai.paddingMedium
                                                Layout.bottomMargin: Komai.paddingMedium
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
                                                text: deviceDelegate.deviceId
                                                font.bold: true
                                                color: palette.text
                                                Layout.alignment: Qt.AlignVCenter
                                                Layout.topMargin: Komai.paddingMedium
                                                Layout.bottomMargin: Komai.paddingMedium
                                                elide: Text.ElideRight
                                            }

                                            Components.ImageButton {
                                                id: copyOtherDeviceIdBtn
                                                property bool copied: false
                                                Layout.preferredWidth: 24
                                                Layout.preferredHeight: 24
                                                image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                                ToolTip.visible: hovered
                                                ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
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

                                            Item { Layout.fillWidth: true }

                                            Components.KomaiButton {
                                                text: qsTr("Logout")
                                                icon.source: "qrc:/icons/icons/ui/power-off.svg"
                                                Layout.rightMargin: Komai.paddingSmall
                                                onClicked: {
                                                    if (accountView.profile)
                                                        accountView.profile.signOutDevice(deviceDelegate.deviceId);
                                                }
                                            }
                                        }
                                    }

                                    // Body: Name (editable, auto-persist)
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Komai.paddingMedium
                                        Layout.leftMargin: Komai.paddingMedium
                                        Layout.rightMargin: Komai.paddingMedium
                                        spacing: Komai.paddingSmall

                                        Label {
                                            text: qsTr("Name")
                                            color: palette.text
                                            font.pointSize: 1.1 * Settings.uiFontSizePt
                                            Layout.fillWidth: true
                                        }

                                        Components.KomaiTextField {
                                            id: deviceNameEditField

                                            property string lastSubmitted: ""
                                            property string serverValue: deviceDelegate.deviceName
                                            onServerValueChanged: {
                                                if (text !== serverValue && lastSubmitted === "")
                                                    text = serverValue;
                                                lastSubmitted = "";
                                            }

                                            text: serverValue
                                            font.pointSize: Settings.uiFontSizePt
                                            Layout.preferredWidth: deviceDelegateContent.controlWidth
                                            Layout.maximumWidth: deviceDelegateContent.controlWidth

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

                                    // Body: IP address with copy
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Komai.paddingSmall
                                        Layout.leftMargin: Komai.paddingMedium
                                        Layout.rightMargin: Komai.paddingMedium
                                        Layout.bottomMargin: Komai.paddingMedium
                                        spacing: Komai.paddingSmall

                                        Label {
                                            text: qsTr("IP address")
                                            color: palette.text
                                            font.pointSize: 1.1 * Settings.uiFontSizePt
                                            Layout.fillWidth: true
                                        }

                                        Components.KomaiTextField {
                                            id: deviceIpField
                                            text: deviceDelegate.lastIp ? deviceDelegate.lastIp : "???"
                                            readOnly: true
                                            font.pointSize: Settings.uiFontSizePt
                                            Layout.preferredWidth: deviceDelegateContent.controlWidth
                                            Layout.maximumWidth: deviceDelegateContent.controlWidth
                                        }

                                        Components.ImageButton {
                                            id: copyIpBtn
                                            property bool copied: false
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                            ToolTip.visible: hovered
                                            ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
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

                                    // Footer: Last seen timestamp
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: lastSeenLabel.implicitHeight + Komai.paddingSmall * 2 + 1
                                        color: palette.window

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
                                            text: qsTr("Last seen: %1").arg(new Date(deviceDelegate.lastTs).toLocaleString(Locale.ShortFormat))
                                            color: palette.buttonText
                                            font.pointSize: 0.9 * Settings.uiFontSizePt
                                        }
                                    }
                                }
                            }
                        }
                    }

                    LocalCacheSection {
                        Layout.leftMargin: scrollContent.sideMargin
                        Layout.rightMargin: scrollContent.sideMargin
                    }

                    // Bottom spacer
                    Item { Layout.preferredHeight: Komai.paddingLarge }
                }
            }
        }
    }

    Component {
        id: signedOutView
        Flickable {
            anchors.fill: parent
            contentWidth: width
            contentHeight: container.implicitHeight + Komai.paddingLarge * 2
            clip: true

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
}
