// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import cc.etke.komai

ApplicationWindow {
    id: createDirectRoot
    title: qsTr("Create Direct Chat")
    property var profile
    property bool otherUserHasE2ee: profile? profile.deviceList.rowCount() > 0 : true
    minimumHeight: layout.implicitHeight + footer.implicitHeight + Komai.paddingLarge*2
    minimumWidth: Math.max(footer.implicitWidth, layout.implicitWidth)
    modality: Qt.NonModal
    flags: Qt.Dialog | Qt.WindowCloseButtonHint | Qt.WindowTitleHint

    onVisibilityChanged: {
        userID.forceActiveFocus();
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: createDirectRoot.close()
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Komai.paddingLarge
        spacing: userID.height/4

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
                userid: profile? profile.userid : ""
                url: profile? profile.avatarUrl.replace("mxc://", "image://MxcImage/") : null
                displayName: profile? profile.displayName : ""
                enabled: false
            }
            Label {
                Layout.fillWidth: true
                text: profile? profile.displayName : ""
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
            focus: true
            label: qsTr("User to invite")
            placeholderText: qsTr("@user:server.tld")
            onTextChanged: {
                // we can't use "isValidMxid" here, since the property might only be reevaluated after this change handler.
                if(text.match("@.+?:.{3,}")) {
                    profile = TimelineManager.getGlobalUserProfile(text);
                } else
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
                Layout.alignment: Qt.AlignRight
                id: encryption
                checked: otherUserHasE2ee
            }
        }

        Item {Layout.fillHeight: true}
    }
    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Cancel
        Button {
            text: "Start Direct Chat"
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: userID.isValidMxid && profile
        }
        onRejected: createDirectRoot.close();
        onAccepted: {
            profile.startChat(encryption.checked)
            createDirectRoot.close()
        }
    }
}
