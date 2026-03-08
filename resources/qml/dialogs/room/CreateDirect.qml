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
    property bool otherUserHasE2ee: profile ? profile.deviceList.rowCount() > 0 : true

    title: qsTr("New direct chat")
    titleIcon: ":/icons/icons/ui/person.svg"
    initialFocusItem: userID
    overlayDialogMinWidth: 720

    MatrixTextField {
        id: userID

        // Extract the homeserver from the logged-in user's ID (e.g. "@me:example.com" -> "example.com")
        readonly property string localHomeserver: {
            var uid = Settings.userId;
            var colonIdx = uid.indexOf(":");
            return colonIdx >= 0 ? uid.substring(colonIdx + 1) : "";
        }

        // Normalize input: allow short forms like "baibot" or "@baibot" for same-homeserver users
        function normalizedMxid(input) {
            var t = input.trim();
            if (t.length === 0)
                return "";
            // Ensure @ prefix
            if (t.charAt(0) !== '@')
                t = "@" + t;
            // Append local homeserver if no server part
            if (t.indexOf(":") < 0 && localHomeserver.length > 0)
                t = t + ":" + localHomeserver;
            return t;
        }

        property string resolvedMxid: normalizedMxid(text)
        property bool isValidMxid: resolvedMxid.match("@.+?:.{3,}")

        Layout.fillWidth: true
        label: qsTr("User to invite")
        placeholderText: qsTr("@user:example.com")
        onTextChanged: {
            var mxid = normalizedMxid(text);
            if (mxid.match("@.+?:.{3,}"))
                profile = TimelineManager.getGlobalUserProfile(mxid);
            else
                profile = null;
        }
    }

    Item {
        Layout.fillWidth: true
        implicitHeight: userPreviewRow.implicitHeight
        visible: createDirectRoot.profile !== null

        HoverHandler { id: userPreviewHover; blocking: false; cursorShape: Qt.PointingHandCursor }
        Rectangle { anchors.fill: userPreviewRow; color: palette.window; radius: Komai.paddingMedium; visible: userPreviewHover.hovered; z: -1 }
        TapHandler { onTapped: TimelineManager.openGlobalUserProfile(userID.resolvedMxid) }

        GridLayout {
            id: userPreviewRow
            width: parent.width
            rows: 2
            columns: 2
            rowSpacing: Komai.paddingSmall
            columnSpacing: Komai.paddingMedium

            Avatar {
                Layout.rowSpan: 2
                Layout.preferredWidth: Komai.avatarSize
                Layout.preferredHeight: Komai.avatarSize
                Layout.alignment: Qt.AlignLeft
                Layout.leftMargin: Komai.paddingMedium
                userid: profile ? profile.userid : ""
                url: profile ? profile.avatarUrl.replace("mxc://", "image://MxcImage/") : null
                displayName: profile ? profile.displayName : ""
                enabled: false
            }

            Label {
                Layout.fillWidth: true
                text: profile ? profile.displayName : ""
                color: TimelineManager.userColor(userID.resolvedMxid, palette.window)
                font.pointSize: Settings.uiFontSizePt
            }

            Label {
                Layout.fillWidth: true
                text: userID.resolvedMxid
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 0.9
            }
        }
    }

    // Encryption
    Item {
        Layout.fillWidth: true
        implicitHeight: encryptionRowContent.implicitHeight
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
        enabled: userID.isValidMxid && createDirectRoot.profile
        onClicked: {
            createDirectRoot.profile.startChat(encryption.checked);
            createDirectRoot.close();
        }
    }
}
