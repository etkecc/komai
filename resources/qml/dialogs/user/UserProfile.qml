// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui" as UI
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property var profile
    property string moderationAction: ""
    property var appRoot

    overlayViewport: appRoot
    readonly property int dialogViewportWidth: overlayDialogViewport ? overlayDialogViewport.width : 760
    readonly property int dialogViewportHeight: overlayDialogViewport ? overlayDialogViewport.height : 600

    readonly property bool isRoomProfile: !profile.isGlobalUserProfile
    readonly property bool showingModerationPrompt: moderationAction !== ""
    readonly property bool hasCustomRoomName: isRoomProfile
        && profile.globalDisplayName !== ""
        && profile.displayName !== profile.globalDisplayName
    readonly property bool hasCustomRoomAvatar: isRoomProfile
        && profile.globalAvatarUrl !== ""
        && profile.avatarUrl !== profile.globalAvatarUrl
    readonly property int copyButtonSize: Math.max(20, Math.round(Settings.uiFontSizePt * 1.6))

    width: Math.min(
        Math.max(240, dialogViewportWidth - Komai.paddingLarge * 2),
        Math.max(240, Math.floor(dialogViewportWidth * overlayDialogMaxWidthRatio))
    )
    x: Math.round((dialogViewportWidth - width) / 2)
    y: Math.max(Komai.paddingLarge, Math.round((dialogViewportHeight - height) / 2))
    title: {
        if (showingModerationPrompt) {
            return moderationAction === "kick"
                ? qsTr("Kick %1 from room?").arg(profile.userid)
                : qsTr("Ban %1 from room?").arg(profile.userid);
        }

        return isRoomProfile ? qsTr("Room member profile") : qsTr("User profile");
    }
    titleIcon: showingModerationPrompt
        ? (moderationAction === "kick"
            ? ":/icons/icons/ui/round-remove-button.svg"
            : ":/icons/icons/ui/ban.svg")
        : ":/icons/icons/ui/person.svg"

    function closeDialogSoon()
    {
        // Defer closing so the dialog is not destroyed from inside an active
        // button signal handler.
        Qt.callLater(() => root.close());
    }

    function openModerationPrompt(action)
    {
        moderationAction = action;
        moderationReasonInput.text = "";
        Qt.callLater(() => moderationReasonInput.forceActiveFocus());
    }

    function closeModerationPrompt()
    {
        moderationAction = "";
        moderationReasonInput.text = "";
    }

    function submitModerationPrompt()
    {
        const targetProfile = profile;
        const reason = moderationReasonInput.text;
        const action = moderationAction;

        closeModerationPrompt();

        if (action === "kick")
            targetProfile.kickUser(reason);
        else if (action === "ban")
            targetProfile.banUser(reason);

        closeDialogSoon();
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: {
            if (root.showingModerationPrompt)
                root.closeModerationPrompt();
            else
                root.closeDialogSoon();
        }
    }

    // Body background: alternateBase. Sections override with window/transparent as needed.
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: !root.showingModerationPrompt
        Layout.preferredHeight: root.showingModerationPrompt
            ? moderationPromptLayout.implicitHeight + Komai.paddingMedium * 2
            : (root.parent ? root.parent.height * 0.85 : 600)
        color: palette.alternateBase
        radius: Komai.paddingSmall

        ScrollView {
            id: scrollView

            anchors.fill: parent
            anchors.margins: Komai.paddingMedium
            visible: !root.showingModerationPrompt
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: scrollContent

                width: scrollView.availableWidth - (scrollView.ScrollBar.vertical.visible ? Komai.paddingSmall : 0)
                spacing: Komai.paddingSmall

            // ---- Room section (room profiles only) ----
            Components.SettingsSection {
                visible: root.isRoomProfile && profile.room
                label: qsTr("Room")
                Layout.fillWidth: true
            }

            Item {
                visible: root.isRoomProfile && profile.room
                Layout.fillWidth: true
                implicitHeight: roomRowDelegate.implicitHeight

                ItemDelegate {
                    id: roomRowDelegate
                    anchors.fill: parent
                    padding: 0
                    hoverEnabled: true
                    onClicked: {
                        if (profile.room)
                            TimelineManager.openRoomInfo(profile.room.roomId);
                    }
                    background: Rectangle {
                        color: roomRowDelegate.hovered ? palette.dark : palette.window
                        radius: Komai.paddingMedium
                    }

                    contentItem: RowLayout {
                        id: roomRowContent
                        spacing: Komai.paddingMedium

                        Components.Avatar {
                            Layout.preferredHeight: Komai.listIconSize
                            Layout.preferredWidth: Komai.listIconSize
                            Layout.leftMargin: Komai.paddingMedium
                            Layout.topMargin: Komai.paddingMedium
                            Layout.bottomMargin: Komai.paddingMedium
                            roomid: profile.room ? profile.room.roomId : ""
                            displayName: profile.room ? profile.room.plainRoomName : ""
                            url: profile.room ? profile.room.roomAvatarUrl.replace("mxc://", "image://MxcImage/") : ""
                            enabled: false
                        }

                        Components.ElidedLabel {
                            fullText: profile.room ? profile.room.plainRoomName : ""
                            color: roomRowDelegate.hovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.fillWidth: true
                            elideWidth: roomRowContent.width - Komai.paddingMedium * 4 - 48
                        }
                    }
                }

                Components.KomaiCursorShape {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                }
            }

            // ---- Profile section ----
            Components.SettingsSection {
                label: qsTr("Profile")
                Layout.fillWidth: true
            }

            // Loading spinner
            UI.Spinner {
                Layout.alignment: Qt.AlignHCenter
                running: profile.isLoading
                visible: profile.isLoading
                foreground: palette.mid
            }

            // Error toast
            Text {
                id: errorText

                color: Komai.theme.error
                visible: opacity > 0
                opacity: 0
                Layout.alignment: Qt.AlignHCenter
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                wrapMode: Text.Wrap
            }

            SequentialAnimation {
                id: hideErrorAnimation

                running: false

                PauseAnimation {
                    duration: 4000
                }

                NumberAnimation {
                    target: errorText
                    property: "opacity"
                    to: 0
                    duration: 1000
                }
            }

            Connections {
                function onDisplayError(errorMessage) {
                    errorText.text = errorMessage;
                    errorText.opacity = 1;
                    hideErrorAnimation.restart();
                }

                target: profile
            }

            // Avatar row: label | [buttons] [avatar]
            Item {
                id: avatarRowItem
                Layout.fillWidth: true
                implicitHeight: avatarRowColumn.implicitHeight

                HoverHandler { id: avatarRowHover; blocking: false }
                readonly property bool rowHovered: avatarRowHover.hovered
                Rectangle {
                    anchors.fill: avatarRowColumn
                    color: parent.rowHovered ? palette.dark : palette.window
                    radius: Komai.paddingMedium
                    z: -1
                }

                ColumnLayout {
                    id: avatarRowColumn
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        id: avatarRowContent
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        Label {
                            text: qsTr("Avatar")
                            color: avatarRowItem.rowHovered ? palette.brightText : palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: Komai.paddingMedium
                            Layout.bottomMargin: Komai.paddingMedium
                            Layout.leftMargin: Komai.paddingMedium
                        }

                        Item { Layout.fillWidth: true }

                        // Avatar action buttons (self only)
                        Components.KomaiButton {
                            visible: profile.isSelf
                            text: root.isRoomProfile ? qsTr("Change avatar for this room") : qsTr("Change")
                            icon.source: "qrc:/icons/icons/ui/edit.svg"
                            onClicked: profile.changeAvatar()
                        }

                        Components.KomaiButton {
                            visible: profile.isSelf && root.isRoomProfile && root.hasCustomRoomAvatar
                            text: qsTr("Reset to global avatar")
                            icon.source: "qrc:/icons/icons/ui/delete.svg"
                            onClicked: confirmResetRoomAvatarDialog.open()
                        }

                        Components.KomaiButton {
                            visible: profile.isSelf && !root.isRoomProfile && profile.avatarUrl !== ""
                            text: qsTr("Remove")
                            icon.source: "qrc:/icons/icons/ui/delete.svg"
                            onClicked: confirmRemoveAvatarDialog.open()
                        }

                        Components.Avatar {
                            id: avatarImage

                            url: profile.avatarUrl.replace("mxc://", "image://MxcImage/")
                            displayName: profile.displayName
                            userid: profile.userid
                            Layout.preferredHeight: Komai.listIconSize
                            Layout.preferredWidth: Komai.listIconSize
                            Layout.rightMargin: Komai.paddingMedium
                            Layout.topMargin: Komai.paddingMedium
                            Layout.bottomMargin: Komai.paddingMedium
                            onClicked: {
                                if (profile.avatarUrl !== "")
                                    TimelineManager.openMediaOverlay(null, profile.avatarUrl, "", 0, 0);
                            }
                        }
                    }

                    // Note about different global avatar (right-aligned, below avatar)
                    Label {
                        visible: root.hasCustomRoomAvatar
                        Layout.alignment: Qt.AlignRight
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        text: profile.isSelf
                            ? qsTr("You have a different global avatar.")
                            : qsTr("This user has a different global avatar.")
                        color: avatarRowItem.rowHovered ? palette.brightText : palette.buttonText
                        font.pointSize: Math.floor(Settings.uiFontSizePt * 0.85)
                        font.italic: true
                    }
                }

                Components.OverlayDialog {
                    id: confirmResetRoomAvatarDialog
                    title: qsTr("Reset avatar")
                    titleIcon: ":/icons/icons/ui/delete.svg"

                    Label {
                        Layout.fillWidth: true
                        color: palette.text
                        wrapMode: Text.WordWrap
                        text: qsTr("Are you sure you want to reset your avatar for this room to the global one?")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        Components.KomaiButton {
                            text: qsTr("Cancel")
                            onClicked: confirmResetRoomAvatarDialog.close()
                        }

                        Item { Layout.fillWidth: true }

                        Components.KomaiButton {
                            text: qsTr("Reset")
                            highlighted: true
                            onClicked: {
                                profile.removeAvatar();
                                confirmResetRoomAvatarDialog.close();
                            }
                        }
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
                                profile.removeAvatar();
                                confirmRemoveAvatarDialog.close();
                            }
                        }
                    }
                }
            }

            // Display name row: label | [field or value + copy]
            Item {
                id: displayNameRowItem
                Layout.fillWidth: true
                implicitHeight: displayNameRowContent.implicitHeight

                HoverHandler { id: displayNameRowHover; blocking: false }
                readonly property bool rowHovered: displayNameRowHover.hovered
                Rectangle {
                    anchors.fill: displayNameRowContent
                    color: parent.rowHovered ? palette.dark : palette.window
                    radius: Komai.paddingMedium
                    z: -1
                }

                RowLayout {
                    id: displayNameRowContent
                    width: parent.width
                    spacing: Komai.paddingMedium

                    Label {
                        text: qsTr("Display name")
                        color: displayNameRowItem.rowHovered ? palette.brightText : palette.text
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                    }

                    Item { Layout.fillWidth: true }

                    // Self: editable text field (auto-persist like AccountTab)
                    ColumnLayout {
                        visible: profile.isSelf
                        Layout.preferredWidth: scrollView.availableWidth * 0.4
                        Layout.maximumWidth: scrollView.availableWidth * 0.4
                        Layout.minimumWidth: 150
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        spacing: 2

                        Components.KomaiTextField {
                            id: displayNameField

                            Layout.fillWidth: true

                            property bool hasPendingSubmit: false
                            property var lastSubmitted: null
                            // For room profiles, displayName falls back to user_id when
                            // no room override is set. Treat that as empty so the field
                            // shows the placeholder instead of the raw user ID.
                            property string serverValue: {
                                var dn = profile.displayName;
                                if (root.isRoomProfile && dn === profile.userid)
                                    return "";
                                return dn;
                            }
                            onServerValueChanged: {
                                if (!hasPendingSubmit && text !== serverValue)
                                    text = serverValue;
                                hasPendingSubmit = false;
                            }

                            text: serverValue
                            placeholderText: root.isRoomProfile ? profile.globalDisplayName : ""

                            function applyName() {
                                var val = text.trim();
                                // For room profiles, clearing means "reset to global name".
                                if (root.isRoomProfile && val.length === 0)
                                    val = profile.globalDisplayName;
                                if (val === serverValue || val === lastSubmitted)
                                    return;
                                // For global profiles, empty is not allowed.
                                if (val.length === 0)
                                    return;
                                lastSubmitted = val;
                                hasPendingSubmit = true;
                                profile.changeUsername(val);
                            }

                            onEditingFinished: applyName()
                            onActiveFocusChanged: if (!activeFocus) applyName()
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.isRoomProfile
                            text: qsTr("Leave empty to use your global name: %1").arg(profile.globalDisplayName)
                            color: displayNameRowItem.rowHovered ? palette.brightText : palette.buttonText
                            font.pointSize: Math.floor(Settings.uiFontSizePt * 0.85)
                            wrapMode: Text.Wrap
                        }
                    }

                    // Others: read-only display name + copy button
                    Label {
                        visible: !profile.isSelf
                        text: profile.displayName
                        color: displayNameRowItem.rowHovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        Layout.rightMargin: copyNameBtn.visible ? 0 : Komai.paddingMedium
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        elide: Text.ElideRight
                        Layout.maximumWidth: scrollView.availableWidth * 0.5
                    }

                    Components.ImageButton {
                        id: copyNameBtn

                        property bool copied: false

                        visible: !profile.isSelf
                        Layout.preferredWidth: root.copyButtonSize
                        Layout.preferredHeight: root.copyButtonSize
                        buttonTextColor: displayNameRowItem.rowHovered ? palette.brightText : palette.buttonText
                        image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                        hoverEnabled: true
                        toolTipVisible: hovered
                        toolTipText: copied ? qsTr("Copied!") : qsTr("Copy display name")
                        Layout.rightMargin: Komai.paddingMedium
                        onClicked: {
                            Clipboard.text = profile.displayName;
                            copied = true;
                            copyNameTimer.restart();
                        }

                        Timer {
                            id: copyNameTimer
                            interval: 2000
                            onTriggered: copyNameBtn.copied = false
                        }
                    }
                }
            }

            // User ID row: label | [value + copy]
            Item {
                id: userIdRowItem
                Layout.fillWidth: true
                implicitHeight: userIdRowContent.implicitHeight

                HoverHandler { id: userIdRowHover; blocking: false }
                readonly property bool rowHovered: userIdRowHover.hovered
                Rectangle {
                    anchors.fill: userIdRowContent
                    color: parent.rowHovered ? palette.dark : palette.window
                    radius: Komai.paddingMedium
                    z: -1
                }

                RowLayout {
                    id: userIdRowContent
                    width: parent.width
                    spacing: Komai.paddingMedium

                    Label {
                        text: qsTr("User ID")
                        color: userIdRowItem.rowHovered ? palette.brightText : palette.text
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: profile.userid
                        color: userIdRowItem.rowHovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        elide: Text.ElideRight
                        Layout.maximumWidth: scrollView.availableWidth * 0.5
                    }

                    Components.ImageButton {
                        id: copyIdBtn

                        property bool copied: false

                        Layout.preferredWidth: root.copyButtonSize
                        Layout.preferredHeight: root.copyButtonSize
                        buttonTextColor: userIdRowItem.rowHovered ? palette.brightText : palette.buttonText
                        image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                        hoverEnabled: true
                        toolTipVisible: hovered
                        toolTipText: copied ? qsTr("Copied!") : qsTr("Copy user ID")
                        Layout.rightMargin: Komai.paddingMedium
                        onClicked: {
                            Clipboard.text = profile.userid;
                            copied = true;
                            copyIdTimer.restart();
                        }

                        Timer {
                            id: copyIdTimer
                            interval: 2000
                            onTriggered: copyIdBtn.copied = false
                        }
                    }
                }
            }

            // Presence status row: label | [value]
            Item {
                id: presenceRowItem
                Layout.fillWidth: true
                implicitHeight: presenceRowContent.implicitHeight
                visible: statusMsg.userStatus !== ""

                HoverHandler { id: presenceRowHover; blocking: false }
                readonly property bool rowHovered: presenceRowHover.hovered
                Rectangle {
                    anchors.fill: presenceRowContent
                    color: parent.rowHovered ? palette.dark : palette.window
                    radius: Komai.paddingMedium
                    z: -1
                }

                RowLayout {
                    id: presenceRowContent
                    width: parent.width
                    spacing: Komai.paddingMedium

                    Label {
                        text: qsTr("Status")
                        color: presenceRowItem.rowHovered ? palette.brightText : palette.text
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        id: statusMsg

                        property string userStatus: Presence.userStatus(profile.userid)

                        text: userStatus
                        color: presenceRowItem.rowHovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: true
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        elide: Text.ElideRight
                        Layout.maximumWidth: scrollView.availableWidth * 0.5

                        Connections {
                            target: Presence
                            function onPresenceChanged(id) {
                                if (id === profile.userid)
                                    statusMsg.userStatus = Presence.userStatus(profile.userid);
                            }
                        }
                    }
                }
            }

            // Verification row: label | [badge]
            Item {
                id: verificationRowItem
                Layout.fillWidth: true
                implicitHeight: verificationRowContent.implicitHeight
                visible: profile.userVerificationEnabled

                HoverHandler { id: verificationRowHover; blocking: false }
                readonly property bool rowHovered: verificationRowHover.hovered
                Rectangle {
                    anchors.fill: verificationRowContent
                    color: parent.rowHovered ? palette.dark : palette.window
                    radius: Komai.paddingMedium
                    z: -1
                }

                RowLayout {
                    id: verificationRowContent
                    width: parent.width
                    spacing: Komai.paddingMedium

                    Label {
                        text: qsTr("Verification")
                        color: verificationRowItem.rowHovered ? palette.brightText : palette.text
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                    }

                    Item { Layout.fillWidth: true }

                    // Verification status badge + help text
                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        spacing: Komai.paddingSmall

                        // Badge (opaque colors so hover background doesn't bleed through)
                        Rectangle {
                            id: verificationBadge

                            readonly property color badgeBase: {
                                switch (profile.userVerified) {
                                case Crypto.Verified:
                                    return Komai.theme.success;
                                case Crypto.TOFU:
                                    return Komai.theme.warning;
                                default:
                                    return Komai.theme.error;
                                }
                            }

                            Layout.alignment: Qt.AlignRight
                            implicitWidth: verificationBadgeRow.implicitWidth + Komai.paddingMedium * 2
                            implicitHeight: verificationBadgeRow.implicitHeight + Komai.paddingMedium
                            radius: Komai.paddingSmall
                            color: Qt.rgba(
                                palette.window.r * 0.85 + badgeBase.r * 0.15,
                                palette.window.g * 0.85 + badgeBase.g * 0.15,
                                palette.window.b * 0.85 + badgeBase.b * 0.15,
                                1.0)
                            border.color: badgeBase
                            border.width: 1

                            RowLayout {
                                id: verificationBadgeRow
                                anchors.centerIn: parent
                                spacing: Komai.paddingSmall

                                Image {
                                    readonly property int badgeIconSize: Math.max(14, Math.round(Settings.uiFontSizePt * 1.4))
                                    Layout.preferredHeight: badgeIconSize
                                    Layout.preferredWidth: badgeIconSize
                                    sourceSize.height: height
                                    sourceSize.width: width
                                    source: {
                                        switch (profile.userVerified) {
                                        case Crypto.Verified:
                                            return "image://colorimage/:/icons/icons/ui/shield-regular-checkmark.svg?" + verificationBadge.badgeBase;
                                        case Crypto.TOFU:
                                            return "image://colorimage/:/icons/icons/ui/shield-regular-exclamation-mark.svg?" + verificationBadge.badgeBase;
                                        default:
                                            return "image://colorimage/:/icons/icons/ui/shield-regular-cross.svg?" + verificationBadge.badgeBase;
                                        }
                                    }
                                }

                                Label {
                                    text: {
                                        switch (profile.userVerified) {
                                        case Crypto.Verified:
                                            return qsTr("Verified");
                                        case Crypto.TOFU:
                                            return qsTr("Implicitly trusted");
                                        default:
                                            return qsTr("Unverified");
                                        }
                                    }
                                    color: verificationBadge.badgeBase
                                    font.pointSize: Settings.uiFontSizePt
                                }
                            }
                        }

                        // Help text explaining the verification status
                        Label {
                            Layout.alignment: Qt.AlignRight
                            Layout.maximumWidth: scrollView.availableWidth * 0.5
                            visible: profile.userVerified !== Crypto.Verified
                            text: {
                                switch (profile.userVerified) {
                                case Crypto.TOFU:
                                    return qsTr("Accepted on first use, not explicitly verified.");
                                default:
                                    return qsTr("Identity keys changed or never seen. Consider verifying.");
                                }
                            }
                            color: verificationRowItem.rowHovered ? palette.brightText : palette.buttonText
                            font.pointSize: Math.floor(Settings.uiFontSizePt * 0.85)
                            font.italic: true
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            // Global display name note (when room-specific name differs)
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                visible: root.hasCustomRoomName
                text: qsTr("Global display name: %1").arg(profile.globalDisplayName)
                color: palette.buttonText
                font.pointSize: Math.floor(Settings.uiFontSizePt * 0.9)
                font.italic: true
                wrapMode: Text.Wrap
            }

            // ---- Actions section ----
            Components.SettingsSection {
                label: qsTr("Actions")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                visible: !profile.isSelf
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Komai.paddingSmall
                visible: !profile.isSelf

                // Opens an existing direct chat or creates a new one
                Components.KomaiActionRowButton {
                    labelText: qsTr("Start direct chat")
                    iconSource: ":/icons/icons/ui/person.svg"
                    onClicked: {
                        root.closeDialogSoon();
                        profile.startChat();
                    }
                }

                Components.KomaiActionRowButton {
                    visible: profile.canStartVerification && profile.userVerified !== Crypto.Verified
                    labelText: profile.userVerificationEnabled ? qsTr("Verify user") : qsTr("Verify device")
                    iconSource: ":/icons/icons/ui/shield-regular-checkmark.svg"
                    onClicked: profile.verify()
                }

                Components.KomaiActionRowButton {
                    id: ignoreBtn
                    labelText: profile.ignored ? qsTr("Unignore user") : qsTr("Ignore user")
                    iconSource: ":/icons/icons/ui/volume-off-indicator.svg"
                    onClicked: {
                        ignoreBtn.focus = false;
                        var dialog = confirmIgnoreDialogComponent.createObject(root.parent);
                        dialog.open();
                        dialog.closed.connect(function() { Qt.callLater(() => dialog.destroy()); });
                    }
                }

                Components.KomaiActionRowButton {
                    visible: root.isRoomProfile && profile.room && profile.room.permissions.canKick()
                    labelText: qsTr("Kick from room")
                    iconSource: ":/icons/icons/ui/round-remove-button.svg"
                    onClicked: root.openModerationPrompt("kick")
                }

                Components.KomaiActionRowButton {
                    visible: root.isRoomProfile && profile.room && profile.room.permissions.canBan()
                    labelText: qsTr("Ban from room")
                    iconSource: ":/icons/icons/ui/ban.svg"
                    onClicked: root.openModerationPrompt("ban")
                }
            }

            // Ignore confirmation dialog (OverlayDialog, same pattern as LeaveRoomDialog)
            Component {
                id: confirmIgnoreDialogComponent

                Components.OverlayDialog {
                    id: confirmIgnoreDialog

                    title: profile.ignored
                        ? qsTr("Unignore %1?").arg(profile.userid)
                        : qsTr("Ignore %1?").arg(profile.userid)
                    titleIcon: ":/icons/icons/ui/volume-off-indicator.svg"

                    Label {
                        Layout.fillWidth: true
                        color: palette.text
                        wrapMode: Text.WordWrap
                        text: profile.ignored
                            ? qsTr("You will see their messages again.")
                            : qsTr("After ignoring, you will no longer see their messages in any room.\nYou can unignore later via this user's profile or via Settings → Privacy → Ignored users.")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        Components.KomaiButton {
                            text: qsTr("Cancel")
                            onClicked: confirmIgnoreDialog.close()
                        }

                        Item { Layout.fillWidth: true }

                        Components.KomaiButton {
                            text: profile.ignored ? qsTr("Unignore") : qsTr("Ignore")
                            highlighted: true
                            onClicked: {
                                profile.ignored = !profile.ignored;
                                confirmIgnoreDialog.close();
                            }
                        }
                    }
                }
            }

            // ---- Rooms in common section ----
            Components.SettingsSection {
                label: qsTr("Rooms in common")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                visible: !profile.isSelf && profile.sharedRooms && profile.sharedRooms.rowCount() > 0
            }

            Repeater {
                model: (!profile.isSelf && profile.sharedRooms) ? profile.sharedRooms : null

                delegate: AbstractButton {
                    id: roomDelegate

                    required property string roomId
                    required property string roomName
                    required property string avatarUrl

                    Layout.fillWidth: true
                    implicitHeight: Komai.listIconSize + Komai.paddingSmall * 2
                    leftPadding: Komai.paddingMedium
                    rightPadding: Komai.paddingMedium
                    hoverEnabled: true
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus

                    readonly property bool activeState: hovered || pressed || activeFocus
                    readonly property color actionTextColor: activeState ? palette.brightText : palette.text

                    onClicked: {
                        root.closeDialogSoon();
                        Rooms.setCurrentRoom(roomDelegate.roomId);
                    }

                    contentItem: RowLayout {
                        spacing: Komai.paddingMedium

                        Components.Avatar {
                            id: roomAvatar

                            Layout.preferredHeight: Komai.listIconSize
                            Layout.preferredWidth: Komai.listIconSize
                            Layout.alignment: Qt.AlignVCenter
                            url: roomDelegate.avatarUrl.replace("mxc://", "image://MxcImage/")
                            roomid: roomDelegate.roomId
                            displayName: roomDelegate.roomName
                            enabled: false

                            FontMetrics {
                                id: fontMetrics
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: roomDelegate.roomName
                            color: roomDelegate.actionTextColor
                            elide: Text.ElideRight
                        }
                    }

                    background: Rectangle {
                        radius: Komai.paddingMedium
                        color: roomDelegate.activeState ? palette.dark : palette.window
                    }

                    KomaiCursorShape {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }

            // ---- Devices (sessions) section ----
            // Section heading with inline Refresh button (matching AccountTab pattern)
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                spacing: Komai.paddingSmall

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Komai.paddingSmall

                    Label {
                        text: qsTr("Devices (sessions)")
                        color: palette.text
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        font.capitalization: Font.AllUppercase
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }

                    Components.KomaiButton {
                        id: refreshBtn

                        property bool refreshed: false

                        visible: !profile.isSelf
                        text: refreshed ? qsTr("Refreshed") : qsTr("Refresh")
                        icon.source: refreshed ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/refresh.svg"
                        onClicked: {
                            profile.refreshDevices();
                            refreshed = true;
                            refreshTimer.restart();
                        }

                        Timer {
                            id: refreshTimer

                            interval: 1200
                            onTriggered: refreshBtn.refreshed = false
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

            // Self: redirect to Account settings
            Components.KomaiActionRowButton {
                visible: profile.isSelf
                labelText: qsTr("Manage")
                iconSource: ":/icons/icons/ui/person.svg"
                onClicked: {
                    root.closeDialogSoon();
                    MainWindow.showUserSettingsPage(UserSettingsModel.TabAccount);
                }
            }

            // Others: device list
            Repeater {
                id: deviceRepeater
                model: profile.isSelf ? null : profile.deviceList

                delegate: Rectangle {
                    id: deviceCard

                    required property int verificationStatus
                    required property string deviceId
                    required property string deviceName

                    Layout.fillWidth: true
                    implicitHeight: deviceContent.implicitHeight + Komai.paddingMedium * 2
                    color: palette.window
                    border.color: Komai.theme.separator
                    border.width: 1
                    radius: Komai.paddingSmall

                    ColumnLayout {
                        id: deviceContent

                        anchors.fill: parent
                        anchors.margins: Komai.paddingMedium
                        spacing: Komai.paddingSmall

                        // Top row: verification badge + device ID
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Komai.paddingSmall

                            // Verification status badge
                            Rectangle {
                                visible: deviceCard.verificationStatus !== VerificationStatus.NOT_APPLICABLE
                                implicitWidth: badgeRow.implicitWidth + Komai.paddingSmall * 2
                                implicitHeight: badgeRow.implicitHeight + Komai.paddingSmall
                                radius: Komai.paddingSmall
                                color: {
                                    switch (deviceCard.verificationStatus) {
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
                                    switch (deviceCard.verificationStatus) {
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
                                    id: badgeRow

                                    anchors.centerIn: parent
                                    spacing: Komai.paddingSmall

                                    Image {
                                        readonly property int badgeIconSize: Math.max(14, Math.round(Settings.uiFontSizePt * 1.4))
                                        Layout.preferredHeight: badgeIconSize
                                        Layout.preferredWidth: badgeIconSize
                                        sourceSize.height: height
                                        sourceSize.width: width
                                        source: {
                                            switch (deviceCard.verificationStatus) {
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
                                            switch (deviceCard.verificationStatus) {
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
                                            switch (deviceCard.verificationStatus) {
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
                                text: deviceCard.deviceId
                                font.bold: true
                                color: palette.text
                                elide: Text.ElideRight
                            }

                            Components.KomaiButton {
                                visible: deviceCard.verificationStatus === VerificationStatus.UNVERIFIED && !profile.userVerificationEnabled
                                text: qsTr("Verify")
                                icon.source: ":/icons/icons/ui/shield-regular-checkmark.svg"
                                onClicked: profile.verify(deviceCard.deviceId)
                            }

                            Components.KomaiButton {
                                visible: deviceCard.verificationStatus !== VerificationStatus.SELF
                                text: deviceCard.verificationStatus === VerificationStatus.BLOCKED
                                    ? qsTr("Unblock")
                                    : qsTr("Block")
                                icon.source: deviceCard.verificationStatus === VerificationStatus.BLOCKED
                                    ? ":/icons/icons/ui/shield-regular-exclamation-mark.svg"
                                    : ":/icons/icons/ui/shield-regular-cross.svg"
                                onClicked: {
                                    if (deviceCard.verificationStatus === VerificationStatus.BLOCKED)
                                        profile.unblockDevice(deviceCard.deviceId);
                                    else
                                        profile.blockDevice(deviceCard.deviceId);
                                }
                            }
                        }

                        // Device name (second row)
                        Label {
                            Layout.fillWidth: true
                            visible: deviceCard.deviceName !== ""
                            text: deviceCard.deviceName
                            color: palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // Empty device list state
            Label {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                visible: !profile.isSelf && deviceRepeater.count === 0
                text: qsTr("Nothing found.")
                color: palette.buttonText
                horizontalAlignment: Text.AlignHCenter
            }

            // Bottom spacer
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Komai.paddingMedium
            }
        }
        }

        ColumnLayout {
            id: moderationPromptLayout

            anchors.fill: parent
            anchors.margins: Komai.paddingMedium
            spacing: Komai.paddingMedium
            visible: root.showingModerationPrompt

            Components.KomaiTextField {
                id: moderationReasonInput

                Layout.fillWidth: true
                placeholderText: root.moderationAction === "kick"
                    ? qsTr("Add optional reason for kicking %1").arg(profile.userid)
                    : qsTr("Add optional reason for banning %1").arg(profile.userid)
                onAccepted: root.submitModerationPrompt()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Komai.paddingMedium

                Components.KomaiButton {
                    text: qsTr("Cancel")
                    onClicked: root.closeModerationPrompt()
                }

                Item {
                    Layout.fillWidth: true
                }

                Components.KomaiButton {
                    text: root.moderationAction === "kick" ? qsTr("Kick") : qsTr("Ban")
                    highlighted: true
                    onClicked: root.submitModerationPrompt()
                }
            }
        }
        }
    }
