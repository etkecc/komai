// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    property int dismissedStatus: SelfVerificationStatus.AllVerified
    readonly property bool shouldShow: SelfVerificationStatus.status !== SelfVerificationStatus.AllVerified
                                     && dismissedStatus !== SelfVerificationStatus.status
    readonly property color accentColor: Komai.theme.warning
    readonly property int iconSize: Komai.barIconSize
    readonly property int targetHeight: Komai.navigationRowHeight + 1
    readonly property string statusText: {
        switch (SelfVerificationStatus.status) {
        case SelfVerificationStatus.NoMasterKey:
            //: Cross-signing setup has not run yet.
            return qsTr("To prevent losing access to encrypted messages, set up encryption secrets backup.");
        case SelfVerificationStatus.UnverifiedMasterKey:
            //: The user just signed in with this device and hasn't verified their master key.
            return qsTr("This account already has encryption keys, but this device is not verified. Verify it to unlock encrypted messages.");
        case SelfVerificationStatus.UnverifiedDevices:
            //: There are unverified devices signed in to this account.
            return qsTr("This device is verified, but some of your other logged-in devices are not. Review and verify them.");
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
        implicitHeight: Math.max(contentRow.implicitHeight + 2 * Komai.paddingSmall, root.targetHeight)
        Layout.minimumHeight: root.targetHeight

        RowLayout {
            id: contentRow

            anchors.fill: parent
            anchors.leftMargin: Komai.paddingMedium
            anchors.rightMargin: Komai.paddingMedium
            anchors.topMargin: Komai.paddingSmall
            anchors.bottomMargin: Komai.paddingSmall
            spacing: Komai.paddingMedium

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
                ToolTip.delay: Komai.tooltipDelay
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
