// SPDX-FileCopyrightText: Nheko Contributors
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
    id: createDirectRoot

    property var profile: null
    property string selectedMxid: ""
    property bool otherUserHasE2ee: profile ? profile.deviceList.rowCount() > 0 : true
    property string initialSearchText: ""

    title: qsTr("New direct chat")
    titleIcon: ":/icons/icons/ui/person.svg"
    initialFocusItem: userID
    overlayDialogMinWidth: 720

    onOpened: {
        if (initialSearchText.length > 0) {
            userID.text = initialSearchText;
            searchTimer.stop();
            userDirectory.setSearchString(initialSearchText);
            initialSearchText = "";
        } else {
            userDirectory.setSearchString("");
        }
    }
    onClosed: {
        userID.clear();
        clearSelection();
        userDirectory.setSearchString("");
    }

    function selectUser(mxid, displayName, avatarUrl) {
        selectedMxid = mxid;
        profile = TimelineManager.getGlobalUserProfile(mxid);
    }

    function clearSelection() {
        selectedMxid = "";
        profile = null;
        userID.forceActiveFocus();
        if (userID.text.trim().length > 0)
            userDirectory.setSearchString(userID.text.trim());
    }

    // Search field — large, rounded, like ForwardCompleter
    KomaiTextField {
        id: userID

        readonly property string localHomeserver: {
            var uid = Settings.userId;
            var colonIdx = uid.indexOf(":");
            return colonIdx >= 0 ? uid.substring(colonIdx + 1) : "";
        }

        function normalizedMxid(input) {
            var t = input.trim();
            if (t.length === 0)
                return "";
            if (t.charAt(0) !== '@')
                t = "@" + t;
            if (t.indexOf(":") < 0 && localHomeserver.length > 0)
                t = t + ":" + localHomeserver;
            return t;
        }

        property string resolvedMxid: normalizedMxid(text)
        property bool isValidMxid: resolvedMxid.match("@.+?:.{3,}")

        visible: !createDirectRoot.selectedMxid
        Layout.fillWidth: true
        placeholderText: qsTr("Search by name or @user:example.com")
        font.pixelSize: Math.ceil(Komai.fontPixelSize * 1.2)
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
                if (userID.text.trim().length > 0)
                    userDirectory.setSearchString(userID.text.trim());
            }
        }
    }

    // Direct MXID card — shown when input is a valid MXID
    AbstractButton {
        id: directCard

        readonly property bool activeState: hovered || pressed
        property string resolvedDisplayName: ""
        property string resolvedAvatarUrl: ""

        function resetResolved() {
            resolvedDisplayName = "";
            resolvedAvatarUrl = "";
        }

        visible: !createDirectRoot.selectedMxid && userID.isValidMxid
        Layout.fillWidth: true
        implicitHeight: directRow.implicitHeight + Komai.paddingSmall * 2
        onClicked: createDirectRoot.selectUser(userID.resolvedMxid,
            directCard.resolvedDisplayName, directCard.resolvedAvatarUrl)

        Connections {
            target: userID
            function onResolvedMxidChanged() {
                directCard.resetResolved();
                if (userID.isValidMxid)
                    userDirectory.resolveUser(userID.resolvedMxid);
            }
        }

        Connections {
            target: userDirectory
            function onUserResolved(mxid, displayName, avatarUrl) {
                if (mxid === userID.resolvedMxid) {
                    directCard.resolvedDisplayName = displayName;
                    directCard.resolvedAvatarUrl = avatarUrl;
                }
            }
        }

        background: Rectangle {
            radius: Komai.paddingMedium
            color: directCard.activeState ? palette.dark : palette.window
            border.color: Komai.theme.separator
            border.width: 1
        }

        contentItem: RowLayout {
            id: directRow

            spacing: Komai.paddingMedium

            Avatar {
                Layout.preferredWidth: Komai.iconSize
                Layout.preferredHeight: Komai.iconSize
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: Komai.paddingMedium
                userid: userID.resolvedMxid
                displayName: directCard.resolvedDisplayName
                url: (directCard.resolvedAvatarUrl || "").replace("mxc://", "image://MxcImage/")
                enabled: false
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Komai.paddingSmall

                Label {
                    Layout.fillWidth: true
                    text: directCard.resolvedDisplayName || qsTr("Start chat directly")
                    color: directCard.activeState ? palette.brightText : palette.text
                    font.pointSize: Settings.uiFontSizePt
                    font.italic: !directCard.resolvedDisplayName
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: userID.resolvedMxid
                    color: directCard.activeState ? palette.brightText : palette.buttonText
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
                    + (directCard.activeState ? palette.brightText : palette.buttonText)
            }
        }

        KomaiCursorShape {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
        }
    }

    // Search results — fixed height to prevent dialog jumping
    ListView {
        id: searchResults

        visible: !createDirectRoot.selectedMxid
        Layout.fillWidth: true
        Layout.preferredHeight: 250
        model: userDirectory
        clip: true

        FlickableWheelBooster { flickable: searchResults }

        delegate: AbstractButton {
            id: resultDelegate

            width: ListView.view.width
            implicitHeight: resultRow.implicitHeight + Komai.paddingSmall * 2
            hoverEnabled: true
            onClicked: createDirectRoot.selectUser(model.userid, model.displayName, model.avatarUrl)

            readonly property bool activeState: hovered || pressed

            background: Rectangle {
                radius: Komai.paddingMedium
                color: resultDelegate.activeState ? palette.dark : "transparent"
            }

            contentItem: RowLayout {
                id: resultRow
                spacing: Komai.paddingMedium

                Avatar {
                    Layout.preferredWidth: Komai.iconSize
                    Layout.preferredHeight: Komai.iconSize
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: Komai.paddingMedium
                    userid: model.userid
                    url: (model.avatarUrl || "").replace("mxc://", "image://MxcImage/")
                    displayName: model.displayName
                    enabled: false
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        text: model.displayName || qsTr("Unknown display name")
                        color: resultDelegate.activeState ? palette.brightText : (model.displayName ? palette.text : palette.buttonText)
                        font.pointSize: Settings.uiFontSizePt
                        font.italic: !model.displayName
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: model.userid
                        color: resultDelegate.activeState ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 0.9
                        elide: Text.ElideRight
                    }
                }
            }

            KomaiCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
        }

        // Empty state: prompt when no query entered
        Label {
            anchors.centerIn: parent
            visible: searchResults.count === 0 && userID.text.trim().length === 0
            text: qsTr("Type a search query. Results will appear here.")
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 0.9
        }

        // No results (only shown when search is fully complete)
        Column {
            anchors.centerIn: parent
            spacing: Komai.paddingSmall
            visible: searchResults.count === 0 && userID.text.trim().length > 0 && !searchTimer.running && !userDirectory.searchingUsers

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("No matching users found.")
                color: palette.buttonText
                font.pointSize: 1.1 * Settings.uiFontSizePt
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: userID.isValidMxid
                text: qsTr("Use the suggestion above to start a chat by Matrix ID.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
            }
        }

        // Pulsing Komai logo spinner while searching
        Spinner {
            anchors.centerIn: parent
            height: 48
            running: searchResults.count === 0 && userID.text.trim().length > 0 && (searchTimer.running || userDirectory.searchingUsers)
            visible: running
        }
    }

    // Enter key: select first result or use typed MXID directly
    Connections {
        target: userID
        function onAccepted() {
            if (userID.isValidMxid) {
                createDirectRoot.selectUser(userID.resolvedMxid, "", "");
            } else if (searchResults.count > 0) {
                var item = searchResults.itemAtIndex(0);
                if (item)
                    createDirectRoot.selectUser(item.userid, "", "");
            }
        }
    }

    Rectangle {
        visible: createDirectRoot.selectedMxid !== ""
        Layout.fillWidth: true
        implicitHeight: selectedUserRow.implicitHeight + Komai.paddingSmall * 2
        color: palette.window
        radius: Komai.paddingMedium
        border.color: Komai.theme.separator
        border.width: 1

        RowLayout {
            id: selectedUserRow

            anchors.fill: parent
            anchors.margins: Komai.paddingSmall
            spacing: Komai.paddingMedium

            AvatarUserFlipButton {
                Layout.preferredWidth: Komai.iconSize
                Layout.preferredHeight: Komai.iconSize
                Layout.alignment: Qt.AlignVCenter
                avatarButtonSize: Komai.iconSize
                cleanFront: true
                avatarUserId: profile ? profile.userid : ""
                avatarUrl: profile ? profile.avatarUrl.replace("mxc://", "image://MxcImage/") : ""
                avatarDisplayName: profile ? profile.displayName : ""
                onLeftClicked: TimelineManager.openGlobalUserProfile(createDirectRoot.selectedMxid)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Komai.paddingSmall

                Label {
                    Layout.fillWidth: true
                    text: (profile && profile.displayName) ? profile.displayName : qsTr("Unknown display name")
                    color: (profile && profile.displayName) ? palette.text : palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                    font.italic: !(profile && profile.displayName)
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: createDirectRoot.selectedMxid
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt * 0.9
                    elide: Text.ElideRight
                }
            }

            KomaiButton {
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Remove")
                icon.source: "qrc:/icons/icons/ui/delete.svg"
                toolTipText: qsTr("Remove selected user")
                toolTipVisible: hovered && text === ""
                onClicked: createDirectRoot.clearSelection()
            }
        }
    }

    // Encryption
    Item {
        Layout.fillWidth: true
        implicitHeight: encryptionRowContent.implicitHeight
        visible: createDirectRoot.selectedMxid !== ""
        HoverHandler { id: encryptionRowHover; blocking: false }
        Rectangle { anchors.fill: encryptionRowContent; color: palette.window; radius: Komai.paddingMedium; visible: encryptionRowHover.hovered; z: -1 }
        ColumnLayout {
            id: encryptionRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingSmall

                Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignLeft
                    text: qsTr("Encryption")
                    color: palette.text
                }

                ToggleButton {
                    id: encryption

                    Layout.alignment: Qt.AlignRight
                    checked: createDirectRoot.otherUserHasE2ee
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                text: qsTr("End-to-end encryption protects messages so only you and the recipient can read them.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.Wrap
            }
        }
    }

    KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Create")
        highlighted: true
        enabled: createDirectRoot.selectedMxid !== "" && createDirectRoot.profile
        onClicked: {
            createDirectRoot.profile.startChat(encryption.checked);
            createDirectRoot.close();
        }
    }
}
