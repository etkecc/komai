// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

ColumnLayout {
    id: root

    property int dismissedStatus: SelfVerificationStatus.AllVerified
    readonly property bool shouldShow: SelfVerificationStatus.status !== SelfVerificationStatus.AllVerified
                                     && dismissedStatus !== SelfVerificationStatus.status
    readonly property color accentColor: Nheko.theme.orange
    readonly property int iconSize: Nheko.barIconSize
    readonly property int sidebarEntryHeight: Nheko.listIconSize + 2 * Nheko.paddingMedium
    readonly property int targetHeight: sidebarEntryHeight + 1
    readonly property string statusText: {
        switch (SelfVerificationStatus.status) {
        case SelfVerificationStatus.NoMasterKey:
            //: Cross-signing setup has not run yet.
            return qsTr("To prevent losing access to encrypted messages, click to set up encryption secrets backup.");
        case SelfVerificationStatus.UnverifiedMasterKey:
            //: The user just signed in with this device and hasn't verified their master key.
            return qsTr("This account already has encryption keys, but this login is not verified yet. Click to verify this device and unlock encrypted messages.");
        case SelfVerificationStatus.UnverifiedDevices:
            //: There are unverified devices signed in to this account.
            return qsTr("This device is verified, but some of your other logged-in devices are not. Click to review and verify them.");
        default:
            return "";
        }
    }

    spacing: 0
    visible: shouldShow

    Rectangle {
        id: banner

        Layout.fillWidth: true
        color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, bannerHover.hovered ? 0.18 : 0.12)
        implicitHeight: Math.max(contentRow.implicitHeight + 2 * Nheko.paddingSmall, root.targetHeight)
        Layout.minimumHeight: root.targetHeight

        RowLayout {
            id: contentRow

            anchors.fill: parent
            anchors.leftMargin: Nheko.paddingMedium
            anchors.rightMargin: Nheko.paddingMedium
            anchors.topMargin: Nheko.paddingSmall
            anchors.bottomMargin: Nheko.paddingSmall
            spacing: Nheko.paddingMedium

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: root.iconSize
                Layout.preferredWidth: root.iconSize
                source: "image://colorimage/:/icons/icons/ui/shield-regular-exclamation-mark.svg?" + root.accentColor
                sourceSize.height: root.iconSize
                sourceSize.width: root.iconSize
                fillMode: Image.PreserveAspectFit
            }
            Label {
                Layout.fillWidth: true
                color: palette.text
                text: root.statusText
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                verticalAlignment: Text.AlignVCenter
            }
            ImageButton {
                id: closeBannerButton

                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.visible: closeBannerButton.hovered
                Layout.preferredHeight: root.iconSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"
                Layout.preferredWidth: root.iconSize

                onClicked: root.dismissedStatus = SelfVerificationStatus.status
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.7)
            height: 1
        }
        HoverHandler {
            id: bannerHover

            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            enabled: !closeBannerButton.hovered
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            enabled: !closeBannerButton.hovered

            onSingleTapped: {
                if (SelfVerificationStatus.status === SelfVerificationStatus.UnverifiedDevices)
                    SelfVerificationStatus.verifyUnverifiedDevices();
                else if (SelfVerificationStatus.status === SelfVerificationStatus.NoMasterKey)
                    SelfVerificationStatus.setupEncryptionBackup();
                else
                    SelfVerificationStatus.promptCurrentVerificationAction();
            }
        }
    }
    Connections {
        function onStatusChanged() {
            if (SelfVerificationStatus.status === SelfVerificationStatus.AllVerified)
                root.dismissedStatus = SelfVerificationStatus.AllVerified;
        }

        target: SelfVerificationStatus
    }
}
