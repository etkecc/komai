// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

OverlayDialog {
    id: inviteDialogRoot

    property InviteesModel invitees
    readonly property int selectedCount: invitees ? invitees.count : 0

    title: invitees && invitees.roomName.length > 0
        ? qsTr("Invite users to %1").arg(invitees.roomName)
        : qsTr("Invite users")
    titleIcon: ":/icons/icons/ui/plus-circle.svg"
    initialFocusItem: inviteeEntry
    overlayDialogMinWidth: 760

    onOpened: {
        userDirectory.setSearchString("");
    }
    onClosed: {
        inviteeEntry.clear();
        userDirectory.setSearchString("");
    }

    function resetSearch()
    {
        inviteeEntry.clear();
        userDirectory.setSearchString("");
        inviteeEntry.forceActiveFocus();
    }

    function addInvite(mxid, displayName, avatarUrl)
    {
        const trimmedMxid = (mxid || "").trim();
        if (!trimmedMxid.match("@.+?:.{3,}")) {
            console.log("invalid mxid: " + trimmedMxid);
            return false;
        }
        if (invitees.containsUser(trimmedMxid))
            return false;

        invitees.addUser(trimmedMxid, displayName || "", avatarUrl || "");
        resetSearch();
        return true;
    }

    function addCurrentInvite()
    {
        if (inviteeEntry.isValidMxid)
            return addInvite(inviteeEntry.resolvedMxid, "", "");

        if (searchResults.count > 0) {
            const firstResult = searchResults.itemAtIndex(0);
            if (firstResult && firstResult.enabled)
                return addInvite(firstResult.userIdText,
                                 firstResult.displayNameText,
                                 firstResult.avatarUrlText);
        }

        return false;
    }

    function cleanUpAndClose() {
        addCurrentInvite();
        if (invitees.count === 0)
            return;
        invitees.accept();
        close();
    }

    Shortcut {
        sequence: "Ctrl+Enter"
        onActivated: inviteDialogRoot.cleanUpAndClose()
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Selected users")
        color: palette.text
        font.bold: true
        font.pointSize: Settings.uiFontSizePt * 1.1
    }

    ListView {
        id: selectedInvitees

        Layout.fillWidth: true
        Layout.preferredHeight: inviteDialogRoot.selectedCount > 0
            ? Math.min(220,
                       Math.max(Komai.listIconSize + Komai.paddingMedium * 2,
                                selectedInvitees.contentHeight + Komai.paddingSmall * 2))
            : 0
        visible: inviteDialogRoot.selectedCount > 0
        clip: true
        model: inviteDialogRoot.invitees
        spacing: Komai.paddingSmall

        delegate: Rectangle {
            width: ListView.view.width
            implicitHeight: selectedInviteeRow.implicitHeight + Komai.paddingSmall * 2
            color: palette.window
            radius: Komai.paddingMedium
            border.color: Komai.theme.separator
            border.width: 1

            RowLayout {
                id: selectedInviteeRow

                anchors.fill: parent
                anchors.margins: Komai.paddingSmall
                spacing: Komai.paddingMedium

                AvatarUserFlipButton {
                    Layout.preferredWidth: Komai.listIconSize
                    Layout.preferredHeight: Komai.listIconSize
                    Layout.alignment: Qt.AlignVCenter
                    avatarButtonSize: Komai.listIconSize
                    cleanFront: true
                    avatarDisplayName: model.displayName
                    avatarUrl: (model.avatarUrl || "").replace("mxc://", "image://MxcImage/")
                    avatarUserId: model.mxid
                    badgeIconSource: ":/icons/icons/ui/person.svg"
                    onLeftClicked: TimelineManager.openGlobalUserProfile(model.mxid)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        text: model.displayName || qsTr("Unknown display name")
                        color: model.displayName ? palette.text : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: !model.displayName
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: model.mxid
                        color: palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 0.9
                        elide: Text.ElideRight
                    }
                }

                KomaiButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Remove")
                    icon.source: "qrc:/icons/icons/ui/delete.svg"
                    onClicked: inviteDialogRoot.invitees.removeUser(model.mxid)
                }
            }
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? emptySelectedState.implicitHeight + Komai.paddingSmall * 2 : 0
        visible: inviteDialogRoot.selectedCount === 0

        Label {
            id: emptySelectedState

            anchors.centerIn: parent
            width: parent.width - Komai.paddingLarge * 2
            text: qsTr("No one is selected yet.")
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 0.95
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingSmall
        text: qsTr("Search")
        color: palette.text
        font.bold: true
        font.pointSize: Settings.uiFontSizePt * 1.1
    }

    KomaiTextField {
        id: inviteeEntry

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
        font.pixelSize: Math.ceil(Komai.fontPixelSize * 1.2)
        onAccepted: inviteDialogRoot.addCurrentInvite()
        Keys.onShortcutOverride: function(event) { event.accepted = ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier)); }
        Keys.onPressed: function(event) {
            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier))
                inviteDialogRoot.cleanUpAndClose();
        }
        onTextChanged: {
            if (text.trim().length === 0)
                userDirectory.setSearchString("");
            else
                searchTimer.restart();
        }

        Timer {
            id: searchTimer

            interval: 350
            onTriggered: {
                if (inviteeEntry.text.trim().length > 0)
                    userDirectory.setSearchString(inviteeEntry.text.trim());
            }
        }
    }

    // Direct MXID invite card — shown when input is a valid MXID
    AbstractButton {
        id: directInviteCard

        readonly property bool activeState: hovered || pressed
        readonly property bool alreadyAdded: inviteDialogRoot.selectedCount > 0
            && inviteDialogRoot.invitees.containsUser(inviteeEntry.resolvedMxid)
        property string resolvedDisplayName: ""
        property string resolvedAvatarUrl: ""
        property string resolvedMxid: ""

        function resetResolved() {
            resolvedDisplayName = "";
            resolvedAvatarUrl = "";
            resolvedMxid = "";
        }

        Layout.fillWidth: true
        visible: inviteeEntry.isValidMxid && !alreadyAdded
        implicitHeight: directInviteRow.implicitHeight + Komai.paddingSmall * 2
        onClicked: inviteDialogRoot.addInvite(inviteeEntry.resolvedMxid,
            directInviteCard.resolvedDisplayName, directInviteCard.resolvedAvatarUrl)

        Connections {
            target: inviteeEntry
            function onResolvedMxidChanged() {
                directInviteCard.resetResolved();
                if (inviteeEntry.isValidMxid)
                    userDirectory.resolveUser(inviteeEntry.resolvedMxid);
            }
        }

        Connections {
            target: userDirectory
            function onUserResolved(mxid, displayName, avatarUrl) {
                if (mxid === inviteeEntry.resolvedMxid) {
                    directInviteCard.resolvedMxid = mxid;
                    directInviteCard.resolvedDisplayName = displayName;
                    directInviteCard.resolvedAvatarUrl = avatarUrl;
                }
            }
        }

        background: Rectangle {
            radius: Komai.paddingMedium
            color: directInviteCard.activeState ? palette.dark : palette.window
            border.color: Komai.theme.separator
            border.width: 1
        }

        contentItem: RowLayout {
            id: directInviteRow

            spacing: Komai.paddingMedium

            Avatar {
                Layout.preferredWidth: Komai.listIconSize
                Layout.preferredHeight: Komai.listIconSize
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: Komai.paddingMedium
                userid: inviteeEntry.resolvedMxid
                displayName: directInviteCard.resolvedDisplayName
                url: (directInviteCard.resolvedAvatarUrl || "").replace("mxc://", "image://MxcImage/")
                enabled: false
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Komai.paddingSmall

                Label {
                    Layout.fillWidth: true
                    text: directInviteCard.resolvedDisplayName || qsTr("Invite directly")
                    color: directInviteCard.activeState ? palette.brightText : palette.text
                    font.pointSize: Settings.uiFontSizePt
                    font.italic: !directInviteCard.resolvedDisplayName
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: inviteeEntry.resolvedMxid
                    color: directInviteCard.activeState ? palette.brightText : palette.buttonText
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
                    + (directInviteCard.activeState ? palette.brightText : palette.buttonText)
            }
        }

        KomaiCursorShape {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
        }
    }

    ListView {
        id: searchResults

        Layout.fillWidth: true
        Layout.preferredHeight: 280
        model: userDirectory
        clip: true

        delegate: AbstractButton {
            id: resultDelegate

            readonly property bool activeState: hovered || pressed
            readonly property bool alreadySelected: inviteDialogRoot.selectedCount > 0
                && inviteDialogRoot.invitees.containsUser(model.userid)
            property string userIdText: model.userid
            property string displayNameText: model.displayName
            property string avatarUrlText: model.avatarUrl

            width: ListView.view.width
            implicitHeight: resultRow.implicitHeight + Komai.paddingSmall * 2
            hoverEnabled: !alreadySelected
            enabled: !alreadySelected
            opacity: alreadySelected ? 0.7 : 1
            onClicked: inviteDialogRoot.addInvite(userIdText, displayNameText, avatarUrlText)

            background: Rectangle {
                radius: Komai.paddingMedium
                color: resultDelegate.alreadySelected
                    ? palette.window
                    : resultDelegate.activeState ? palette.dark : "transparent"
                border.color: resultDelegate.alreadySelected ? Komai.theme.separator : "transparent"
                border.width: resultDelegate.alreadySelected ? 1 : 0
            }

            contentItem: RowLayout {
                id: resultRow

                spacing: Komai.paddingMedium

                Avatar {
                    Layout.preferredWidth: Komai.listIconSize
                    Layout.preferredHeight: Komai.listIconSize
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: Komai.paddingMedium
                    userid: resultDelegate.userIdText
                    url: (resultDelegate.avatarUrlText || "").replace("mxc://", "image://MxcImage/")
                    displayName: resultDelegate.displayNameText
                    enabled: false
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        text: resultDelegate.displayNameText || qsTr("Unknown display name")
                        color: resultDelegate.activeState
                            ? palette.brightText
                            : resultDelegate.displayNameText ? palette.text : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: !resultDelegate.displayNameText
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: resultDelegate.userIdText
                        color: resultDelegate.activeState ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 0.9
                        elide: Text.ElideRight
                    }
                }

                Image {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: visible ? 18 : 0
                    Layout.preferredHeight: visible ? 18 : 0
                    Layout.rightMargin: Komai.paddingMedium
                    visible: resultDelegate.alreadySelected
                    fillMode: Image.PreserveAspectFit
                    source: visible
                        ? "image://colorimage/:/icons/icons/ui/double-checkmark.svg?" + palette.buttonText
                        : ""
                }
            }

            KomaiCursorShape {
                anchors.fill: parent
                cursorShape: resultDelegate.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }

        Label {
            anchors.centerIn: parent
            visible: searchResults.count === 0 && inviteeEntry.text.trim().length === 0
            text: qsTr("Type a search query. Results will appear here.")
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 0.9
        }

        Column {
            anchors.centerIn: parent
            spacing: Komai.paddingSmall
            visible: searchResults.count === 0
                && inviteeEntry.text.trim().length > 0
                && !searchTimer.running
                && !userDirectory.searchingUsers

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("No matching users found.")
                color: palette.buttonText
                font.pointSize: 1.1 * Settings.uiFontSizePt
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: inviteeEntry.isValidMxid
                text: qsTr("Use the suggestion above to invite by Matrix ID.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
            }
        }

        Spinner {
            anchors.centerIn: parent
            height: 48
            running: searchResults.count === 0
                && inviteeEntry.text.trim().length > 0
                && (searchTimer.running || userDirectory.searchingUsers)
            visible: running
        }
    }

    KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Invite")
        highlighted: true
        enabled: invitees.count > 0
        onClicked: inviteDialogRoot.cleanUpAndClose()
    }
}
