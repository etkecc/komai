// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko
import "../../ui"

AbstractButton {
    id: encryptionButton

    required property bool isEncrypted
    required property bool roomAvailable
    required property int trustlevel
    required property int topBarAvatarSize
    required property int buttonPaddingH
    required property int buttonPaddingV

    Layout.alignment: Qt.AlignVCenter
    Layout.column: 7
    Layout.preferredHeight: topBarAvatarSize
    Layout.preferredWidth: topBarAvatarSize
    Layout.row: 1
    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: buttonPaddingV
    bottomPadding: buttonPaddingV
    background: null
    visible: roomAvailable

    ToolTip.delay: Nheko.tooltipDelay
    ToolTip.text: encryptionDialogTitle()
    ToolTip.visible: hovered

    function encryptionDialogTitle() {
        if (!isEncrypted)
            return qsTr("This room is not encrypted.");
        switch (trustlevel) {
        case Crypto.Verified:
            return qsTr("This room contains only verified devices.");
        case Crypto.TOFU:
            return qsTr("This room contains verified devices and devices which have never changed their master key.");
        default:
            return qsTr("This room contains unverified devices!");
        }
    }

    function encryptionDialogBody() {
        if (!isEncrypted)
            return qsTr("Messages in this room are not end-to-end encrypted. See the Members list for device details.");
        switch (trustlevel) {
        case Crypto.Verified:
            return qsTr("Messages are end-to-end encrypted and all devices are verified. See the Members list for device details.");
        case Crypto.TOFU:
            return qsTr("Messages are end-to-end encrypted. Some devices are verified, others are trusted by first use. See the Members list for device details.");
        case Crypto.MessageUnverified:
            return qsTr("Messages are end-to-end encrypted, but the key is from an untrusted source. See the Members list for device details.");
        default:
            return qsTr("Messages are end-to-end encrypted, but some devices are unverified. See the Members list for device details.");
        }
    }

    contentItem: EncryptionIndicator {
        ToolTip.delay: Nheko.tooltipDelay
        ToolTip.text: encryptionButton.encryptionDialogTitle()
        ToolTip.visible: encryptionButton.hovered
        enabled: false
        encrypted: isEncrypted
        hovered: parent.hovered
        trust: trustlevel
        unencryptedColor: palette.buttonText
        unencryptedHoverColor: palette.highlight
        sourceSize.height: topBarAvatarSize - 2 * buttonPaddingH
        sourceSize.width: topBarAvatarSize - 2 * buttonPaddingH
    }

    onClicked: encryptionDialog.open()

    NhekoCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }

    Dialog {
        id: encryptionDialog

        parent: Overlay.overlay
        x: Math.round(((parent ? parent.width : width) - width) / 2)
        y: Math.round(((parent ? parent.height : height) - height) / 2)
        modal: true
        standardButtons: Dialog.Ok
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        title: qsTr("Encryption status")
        implicitWidth: Math.max(240, Math.min(520, (parent ? parent.width : 520) - Nheko.paddingLarge * 2))
        padding: Nheko.paddingLarge

        contentItem: ColumnLayout {
            spacing: Nheko.paddingMedium

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: encryptionButton.encryptionDialogTitle()
                color: palette.text
                font.bold: true
            }
            MatrixText {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: encryptionButton.encryptionDialogBody()
                color: palette.buttonText
            }
        }
    }
}
