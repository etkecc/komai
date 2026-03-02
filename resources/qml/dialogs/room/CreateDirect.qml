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

    property var profile
    property bool otherUserHasE2ee: profile ? profile.deviceList.rowCount() > 0 : true

    title: qsTr("Create Direct Chat")
    titleIcon: ":/icons/icons/ui/plus-circle.svg"
    initialFocusItem: userID

    GridLayout {
        Layout.fillWidth: true
        rows: 2
        columns: 2
        rowSpacing: Komai.paddingSmall
        columnSpacing: Komai.paddingMedium

        Avatar {
            Layout.rowSpan: 2
            Layout.preferredWidth: Komai.avatarSize
            Layout.preferredHeight: Komai.avatarSize
            Layout.alignment: Qt.AlignLeft
            userid: profile ? profile.userid : ""
            url: profile ? profile.avatarUrl.replace("mxc://", "image://MxcImage/") : null
            displayName: profile ? profile.displayName : ""
            enabled: false
        }

        Label {
            Layout.fillWidth: true
            text: profile ? profile.displayName : ""
            color: TimelineManager.userColor(userID.text, palette.window)
            font.pointSize: Settings.uiFontSizePt
        }

        Label {
            Layout.fillWidth: true
            text: userID.text
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 0.9
        }
    }

    MatrixTextField {
        id: userID

        property bool isValidMxid: text.match("@.+?:.{3,}")

        Layout.fillWidth: true
        label: qsTr("User to invite")
        placeholderText: qsTr("@user:server.tld")
        onTextChanged: {
            // we can't use "isValidMxid" here, since the property might only be reevaluated after this change handler.
            if (text.match("@.+?:.{3,}"))
                profile = TimelineManager.getGlobalUserProfile(text);
            else
                profile = null;
        }
    }

    RowLayout {
        Layout.fillWidth: true

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
