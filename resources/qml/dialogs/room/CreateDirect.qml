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
    id: createDirectRoot

    property var profile: null
    property string selectedMxid: ""
    property bool otherUserHasE2ee: profile ? profile.deviceList.rowCount() > 0 : true

    title: qsTr("New direct chat")
    titleIcon: ":/icons/icons/ui/person.svg"
    initialFocusItem: userID
    overlayDialogMinWidth: 720

    onOpened: {
        userDirectory.setSearchString("");
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
    MatrixTextField {
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
        radius: Komai.paddingSmall
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

    // Search results — fixed height to prevent dialog jumping
    ListView {
        id: searchResults

        visible: !createDirectRoot.selectedMxid
        Layout.fillWidth: true
        Layout.preferredHeight: 250
        model: userDirectory
        clip: true

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
                    Layout.preferredWidth: Komai.avatarSize
                    Layout.preferredHeight: Komai.avatarSize
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
        Label {
            anchors.centerIn: parent
            visible: searchResults.count === 0 && userID.text.trim().length > 0 && !searchTimer.running && !userDirectory.searchingUsers
            text: userID.isValidMxid ? qsTr("No results found. Press Enter to use this ID directly.") : qsTr("No results found")
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 0.9
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

    // Selected user preview — UploadBox-style row: [flip avatar] [info] [remove button]
    RowLayout {
        visible: createDirectRoot.selectedMxid !== ""
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        Item {
            Layout.fillWidth: true
            implicitHeight: userPreviewGrid.implicitHeight

            HoverHandler { id: userPreviewHover; blocking: false; cursorShape: Qt.PointingHandCursor }
            Rectangle { anchors.fill: userPreviewGrid; color: palette.window; radius: Komai.paddingMedium; visible: userPreviewHover.hovered; z: -1 }
            TapHandler { onTapped: TimelineManager.openGlobalUserProfile(createDirectRoot.selectedMxid) }

            GridLayout {
                id: userPreviewGrid
                width: parent.width
                rows: 2
                columns: 2
                rowSpacing: Komai.paddingSmall
                columnSpacing: Komai.paddingMedium

                AvatarSettingsFlipButton {
                    Layout.rowSpan: 2
                    Layout.preferredWidth: Komai.avatarSize
                    Layout.preferredHeight: Komai.avatarSize
                    Layout.alignment: Qt.AlignLeft
                    Layout.leftMargin: Komai.paddingMedium
                    avatarButtonSize: Komai.avatarSize
                    avatarUserId: profile ? profile.userid : ""
                    avatarUrl: profile ? profile.avatarUrl.replace("mxc://", "image://MxcImage/") : ""
                    avatarDisplayName: profile ? profile.displayName : ""
                    badgeIconSource: ":/icons/icons/ui/person.svg"
                    toolTipText: ""
                    onLeftClicked: TimelineManager.openGlobalUserProfile(createDirectRoot.selectedMxid)
                }

                Label {
                    Layout.fillWidth: true
                    text: (profile && profile.displayName) ? profile.displayName : qsTr("Unknown display name")
                    color: (profile && profile.displayName) ? palette.text : palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                    font.italic: !(profile && profile.displayName)
                }

                Label {
                    Layout.fillWidth: true
                    text: createDirectRoot.selectedMxid
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt * 0.9
                }
            }
        }

        KomaiButton {
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Remove")
            icon.source: "qrc:/icons/icons/ui/delete.svg"
            ToolTip.delay: Komai.tooltipDelay
            ToolTip.text: qsTr("Remove selected user")
            ToolTip.visible: hovered && text === ""
            onClicked: createDirectRoot.clearSelection()
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
                font.pointSize: 0.9 * Settings.uiFontSizePt
                wrapMode: Text.Wrap
            }
        }
    }

    Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Start")
        highlighted: true
        enabled: createDirectRoot.selectedMxid !== "" && createDirectRoot.profile
        onClicked: {
            createDirectRoot.profile.startChat(encryption.checked);
            createDirectRoot.close();
        }
    }
}
