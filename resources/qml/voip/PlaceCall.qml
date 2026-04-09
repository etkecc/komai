// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Popup {
    modal: true
    // only set the anchors on Qt 5.12 or higher
    // see https://doc.qt.io/qt-5/qml-qtquick-controls2-popup.html#anchors.centerIn-prop
    Component.onCompleted: {
        if (anchors)
            anchors.centerIn = parent;

    }

    Component {
        id: deviceError

        DeviceError {
        }

    }

    ColumnLayout {
        id: columnLayout

        spacing: 16

        RowLayout {
            Layout.topMargin: 8
            Layout.leftMargin: 8

            Label {
                text: qsTr("Place a call to %1?").arg(room.roomName)
                color: palette.windowText
            }

            Item {
                Layout.fillWidth: true
            }

        }

        RowLayout {
            id: buttonLayout

            function validateMic() {
                if (CallManager.mics.length == 0) {
                    var dialog = deviceError.createObject(timelineRoot, {
                        "errorString": qsTr("No microphone found."),
                        "image": ":/icons/icons/ui/place-call.svg"
                    });
                    dialog.open();
                    timelineRoot.destroyOnClose(dialog);
                    return false;
                }
                return true;
            }

            Layout.leftMargin: 8
            Layout.rightMargin: 8

            Avatar {
                Layout.rightMargin: cameraCombo.visible ? 16 : 64
                Layout.preferredWidth: Komai.listIconSize
                Layout.preferredHeight: Komai.listIconSize
                url: room.roomAvatarUrl.replace("mxc://", "image://MxcImage/")
                displayName: room.roomName
                roomid: room.roomId
                onClicked: TimelineManager.openMediaOverlay(room, room.avatarUrl(userid), room.data.eventId)
            }

            KomaiButton {
                text: qsTr("Voice")
                icon.source: "qrc:/icons/icons/ui/place-call.svg"
                onClicked: {
                    if (buttonLayout.validateMic()) {
                        Settings.callsDevicesMicrophone = micCombo.currentText;
                        CallManager.sendInvite(room.roomId, Voip.VOICE);
                        close();
                    }
                }
            }

            KomaiButton {
                visible: CallManager.cameras.length > 0
                text: qsTr("Video")
                icon.source: "qrc:/icons/icons/ui/video.svg"
                onClicked: {
                    if (buttonLayout.validateMic()) {
                        Settings.callsDevicesMicrophone = micCombo.currentText;
                        Settings.callsDevicesCamera = cameraCombo.currentText;
                        CallManager.sendInvite(room.roomId, Voip.VIDEO);
                        close();
                    }
                }
            }

            KomaiButton {
                text: qsTr("Screen")
                icon.source: "qrc:/icons/icons/ui/screen-share.svg"
                onClicked: {
                    if (buttonLayout.validateMic()) {
                        Settings.callsDevicesMicrophone = micCombo.currentText;
                        Settings.callsDevicesCamera = cameraCombo.currentText;

                        var dialog = screenShareDialog.createObject(timelineRoot);
                        dialog.open();
                        timelineRoot.destroyOnClose(dialog);
                        close();
                    }
                }
            }

            KomaiButton {
                text: qsTr("Cancel")
                onClicked: {
                    close();
                }
            }

        }

        ColumnLayout {
            spacing: 8

            RowLayout {
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.bottomMargin: cameraCombo.visible ? 0 : 8

                Image {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    sourceSize.width: 22
                    sourceSize.height: 22
                    source: "image://colorimage/:/icons/icons/ui/microphone-unmute.svg?" + palette.windowText
                }

                KomaiComboBox {
                    id: micCombo

                    Layout.fillWidth: true
                    model: CallManager.mics
                }

            }

            RowLayout {
                visible: CallManager.cameras.length > 0
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.bottomMargin: 8

                Image {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    sourceSize.width: 22
                    sourceSize.height: 22
                    source: "image://colorimage/:/icons/icons/ui/video.svg?" + palette.windowText
                }

                KomaiComboBox {
                    id: cameraCombo

                    Layout.fillWidth: true
                    model: CallManager.cameras
                }

            }

        }

    }

    background: Rectangle {
        color: palette.window
        border.color: palette.windowText
    }

}
