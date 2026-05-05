// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Components.OverlayDialog {
    id: root

    required property string roomId
    required property string roomName
    required property var timelineRoot

    title: roomName.length > 0
        ? qsTr("Share desktop with %1?").arg(roomName)
        : qsTr("Share desktop?")
    titleIcon: ":/icons/icons/ui/screen-share.svg"

    // Cache model results so the QStringList stays alive for the dialog's
    // lifetime.
    readonly property var screenShareTypes: CallManager.screenShareTypeList()
    readonly property var availableWindows: CallManager.windowList()

    Component.onCompleted: {
        frameRateCombo.currentIndex = frameRateCombo.find(Settings.callsScreenshareFrameRate);
    }

    onClosed: {
        CallManager.closeScreenShare();
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Label {
            text: qsTr("Method:")
            color: palette.text
        }

        Components.KomaiComboBox {
            id: screenshareType

            Layout.fillWidth: true
            model: root.screenShareTypes
            onCurrentIndexChanged: CallManager.setScreenShareType(currentIndex)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium
        visible: CallManager.screenShareType === Voip.X11 || CallManager.screenShareType === Voip.D3D11

        Label {
            text: qsTr("Window:")
            color: palette.text
        }

        Components.KomaiComboBox {
            id: windowCombo

            Layout.fillWidth: true
            model: root.availableWindows
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Label {
            text: qsTr("Frame rate:")
            color: palette.text
        }

        Components.KomaiComboBox {
            id: frameRateCombo

            Layout.fillWidth: true
            model: ["120", "90", "60", "50", "48", "30", "25", "20", "15", "10", "5", "2", "1"]
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Label {
            Layout.fillWidth: true
            text: qsTr("Include your camera picture-in-picture")
            color: palette.text
            wrapMode: Text.WordWrap
        }

        ToggleButton {
            id: pipCheckBox

            enabled: CallManager.cameras.length > 0
            checked: CallManager.cameras.length > 0 && Settings.callsScreensharePictureInPicture
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Label {
            id: remoteVideoLabel

            Layout.fillWidth: true
            text: qsTr("Request remote camera")
            color: palette.text
            wrapMode: Text.WordWrap
        }

        ToggleButton {
            id: remoteVideoCheckBox

            checked: Settings.callsScreenshareIncludeRemoteVideo
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Label {
            Layout.fillWidth: true
            text: qsTr("Show mouse cursor")
            color: palette.text
            wrapMode: Text.WordWrap
        }

        ToggleButton {
            id: showCursorCheckBox

            checked: Settings.callsScreenshareShowCursor
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            visible: CallManager.screenShareType === Voip.XDP
            // Highlighted until the user has picked a source -- on XDP that's
            // the only path to making Share appear. Stays visible afterwards
            // so the user can re-pick if they change their mind.
            highlighted: !CallManager.screenShareReady
            text: qsTr("Request screencast")
            onClicked: {
                Settings.callsScreenshareShowCursor = showCursorCheckBox.checked;
                CallManager.setupScreenShareXDP();
            }
        }

        Components.KomaiButton {
            visible: CallManager.screenShareReady
            text: qsTr("Preview")
            onClicked: CallManager.previewWindow(windowCombo.currentIndex)
        }

        Components.KomaiButton {
            visible: CallManager.screenShareReady
            text: qsTr("Share")
            icon.source: "qrc:/icons/icons/ui/screen-share.svg"
            highlighted: true
            onClicked: {
                Settings.callsScreenshareFrameRate = frameRateCombo.currentText;
                Settings.callsScreensharePictureInPicture = pipCheckBox.checked;
                Settings.callsScreenshareIncludeRemoteVideo = remoteVideoCheckBox.checked;
                Settings.callsScreenshareShowCursor = showCursorCheckBox.checked;

                CallManager.sendInvite(root.roomId, Voip.SCREEN, windowCombo.currentIndex);
                root.close();
            }
        }
    }
}
