// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    required property string roomId
    required property string roomName
    required property var timelineRoot

    title: roomName.length > 0
        ? qsTr("Place a call to %1?").arg(roomName)
        : qsTr("Place a call?")
    titleIcon: ":/icons/icons/ui/place-call.svg"

    function validateMic() {
        if (CallManager.mics.length === 0) {
            const dialog = deviceErrorComponent.createObject(root.timelineRoot, {
                "errorString": qsTr("No microphone found."),
                "image": ":/icons/icons/ui/place-call.svg"
            });
            dialog.open();
            root.timelineRoot.destroyOnClose(dialog);
            return false;
        }
        return true;
    }

    Component {
        id: deviceErrorComponent

        DeviceError {
        }
    }

    Component {
        id: screenShareDialog

        ScreenShare {
        }
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("Choose how to start the call:")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Image {
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            sourceSize.width: 22
            sourceSize.height: 22
            source: "image://colorimage/:/icons/icons/ui/microphone-unmute.svg?" + palette.windowText
        }

        Components.KomaiComboBox {
            id: micCombo

            Layout.fillWidth: true
            model: CallManager.mics
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium
        visible: CallManager.cameras.length > 0

        Image {
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            sourceSize.width: 22
            sourceSize.height: 22
            source: "image://colorimage/:/icons/icons/ui/video.svg?" + palette.windowText
        }

        Components.KomaiComboBox {
            id: cameraCombo

            Layout.fillWidth: true
            model: CallManager.cameras
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
            text: qsTr("Voice")
            icon.source: "qrc:/icons/icons/ui/place-call.svg"
            highlighted: true
            onClicked: {
                if (root.validateMic()) {
                    Settings.callsDevicesMicrophone = micCombo.currentText;
                    CallManager.sendInvite(root.roomId, Voip.VOICE);
                    root.close();
                }
            }
        }

        Components.KomaiButton {
            visible: CallManager.cameras.length > 0
            text: qsTr("Video")
            icon.source: "qrc:/icons/icons/ui/video.svg"
            highlighted: true
            onClicked: {
                if (root.validateMic()) {
                    Settings.callsDevicesMicrophone = micCombo.currentText;
                    Settings.callsDevicesCamera = cameraCombo.currentText;
                    CallManager.sendInvite(root.roomId, Voip.VIDEO);
                    root.close();
                }
            }
        }

        Components.KomaiButton {
            text: qsTr("Share screen")
            icon.source: "qrc:/icons/icons/ui/screen-share.svg"
            highlighted: true
            onClicked: {
                if (root.validateMic()) {
                    Settings.callsDevicesMicrophone = micCombo.currentText;
                    Settings.callsDevicesCamera = cameraCombo.currentText;

                    // Keep PlaceCall open underneath ScreenShare. On Wayland,
                    // ScreenShare's nested ComboBox popups break (auto-
                    // dismissed by the compositor) unless another modal
                    // dialog is alive to anchor the popup grab. Close
                    // PlaceCall only after ScreenShare goes away.
                    const dialog = screenShareDialog.createObject(root.timelineRoot, {
                        "roomId": root.roomId,
                        "roomName": root.roomName,
                        "timelineRoot": root.timelineRoot
                    });
                    dialog.closed.connect(() => root.close());
                    dialog.open();
                    root.timelineRoot.destroyOnClose(dialog);
                }
            }
        }
    }
}
