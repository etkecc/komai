// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../../components" as Components
import "../../ui"

AbstractButton {
    id: encryptionButton

    required property bool isEncrypted
    required property bool roomAvailable
    required property int trustlevel
    required property int topBarAvatarSize
    required property int buttonPaddingH
    required property int buttonPaddingV
    property bool showLabel: false
    readonly property bool hasLabel: showLabel && encryptionShortLabel().length > 0
    readonly property int iconSize: Math.max(14, topBarAvatarSize - 2 * buttonPaddingH)
    readonly property bool activeState: hovered || pressed || visualFocus
    readonly property string encryptionIcon: {
        if (!isEncrypted)
            return ":/icons/icons/ui/shield-regular-cross.svg";
        switch (trustlevel) {
        case Crypto.Verified:
            return ":/icons/icons/ui/shield-regular-checkmark.svg";
        case Crypto.TOFU:
            return ":/icons/icons/ui/shield-regular.svg";
        case Crypto.Unverified:
        case Crypto.MessageUnverified:
            return ":/icons/icons/ui/shield-regular-exclamation-mark.svg";
        default:
            return ":/icons/icons/ui/shield-regular-cross.svg";
        }
    }
    readonly property color encryptionColor: {
        if (!isEncrypted)
            return Komai.theme.error;
        switch (trustlevel) {
        case Crypto.Verified:
            return Komai.theme.success;
        case Crypto.TOFU:
            return palette.buttonText;
        default:
            return Komai.theme.error;
        }
    }

    Layout.alignment: Qt.AlignVCenter
    Layout.column: 7
    Layout.preferredHeight: topBarAvatarSize
    Layout.preferredWidth: implicitWidth
    Layout.row: 1
    font.pointSize: Settings.uiFontSizePt
    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: buttonPaddingV
    bottomPadding: buttonPaddingV
    implicitWidth: topBarAvatarSize + (hasLabel ? (Komai.paddingSmall + labelTextItem.implicitWidth) : 0)
    background: Rectangle {
        radius: Komai.paddingSmall
        color: encryptionButton.activeState ? palette.dark : "transparent"
    }
    visible: roomAvailable

    ToolTip.delay: Komai.tooltipDelay
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

    function encryptionShortLabel() {
        if (!isEncrypted)
            return qsTr("Unencrypted");
        switch (trustlevel) {
        case Crypto.Verified:
            return qsTr("Verified");
        case Crypto.TOFU:
            return qsTr("Trusted");
        default:
            return qsTr("Warning");
        }
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: encryptionButton.leftPadding
        anchors.rightMargin: encryptionButton.rightPadding
        anchors.topMargin: encryptionButton.topPadding
        anchors.bottomMargin: encryptionButton.bottomPadding
        spacing: Komai.paddingSmall

        EncryptionIndicator {
            ToolTip.delay: Komai.tooltipDelay
            ToolTip.text: encryptionButton.encryptionDialogTitle()
            ToolTip.visible: encryptionButton.hovered && !encryptionButton.hasLabel
            enabled: false
            encrypted: isEncrypted
            hovered: encryptionButton.hovered
            trust: trustlevel
            unencryptedColor: palette.buttonText
            unencryptedHoverColor: palette.brightText
            Layout.preferredHeight: encryptionButton.iconSize
            Layout.preferredWidth: encryptionButton.iconSize
            sourceSize.height: encryptionButton.iconSize
            sourceSize.width: encryptionButton.iconSize
        }
        Label {
            id: labelTextItem

            Layout.alignment: Qt.AlignVCenter
            color: encryptionButton.activeState ? palette.brightText : palette.text
            font.bold: true
            text: encryptionButton.encryptionShortLabel()
            visible: encryptionButton.hasLabel
        }
    }

    onClicked: encryptionDialog.open()

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }

    Components.OverlayDialog {
        id: encryptionDialog

        title: qsTr("Encryption status")
        titleIcon: encryptionButton.encryptionIcon
        titleIconColor: encryptionButton.encryptionColor

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
