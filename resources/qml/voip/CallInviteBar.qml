// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    visible: CallManager.haveCallInvite && Settings.uiInputMode && Settings.callsLegacyEnabled
    color: "#2ECC71"
    implicitHeight: visible ? rowLayout.height + 8 : 0

    Component {
        id: devicesDialog

        CallDevices {
        }

    }

    Component {
        id: deviceError

        DeviceError {
        }

    }

    RowLayout {
        id: rowLayout

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 8

        Avatar {
            implicitWidth: Komai.avatarSize
            implicitHeight: Komai.avatarSize
            url: CallManager.callPartyAvatarUrl.replace("mxc://", "image://MxcImage/")
            userid: CallManager.callParty
            displayName: CallManager.callPartyDisplayName
            onClicked: TimelineManager.openMediaOverlay(room, room.avatarUrl(userid), room.data.eventId)
        }

        Label {
            Layout.leftMargin: 8
            font.pointSize: Settings.uiFontSizePt * 1.1
            text: CallManager.callPartyDisplayName
            color: "#000000"
        }

        Image {
            Layout.leftMargin: 4
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            source: CallManager.callType == Voip.VIDEO ? "qrc:/icons/icons/ui/video.svg" : "qrc:/icons/icons/ui/place-call.svg"
        }

        Label {
            font.pointSize: Settings.uiFontSizePt * 1.1
            text: CallManager.callType == Voip.VIDEO ? qsTr("Video Call") : qsTr("Voice Call")
            color: "#000000"
        }

        Item {
            Layout.fillWidth: true
        }

        ImageButton {
            Layout.rightMargin: 16
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            buttonTextColor: "#000000"
            image: ":/icons/icons/ui/settings.svg"
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Devices")
            onClicked: {
                var dialog = devicesDialog.createObject(timelineRoot);
                dialog.open();
                timelineRoot.destroyOnClose(dialog);
            }
        }

        KomaiButton {
            Layout.rightMargin: 4
            icon.source: CallManager.callType == Voip.VIDEO ? "qrc:/icons/icons/ui/video.svg" : "qrc:/icons/icons/ui/place-call.svg"
            text: qsTr("Accept")
            onClicked: {
                if (CallManager.mics.length == 0) {
                    var dialog = deviceError.createObject(timelineRoot, {
                        "errorString": qsTr("No microphone found."),
                        "image": ":/icons/icons/ui/place-call.svg"
                    });
                    dialog.open();
            timelineRoot.destroyOnClose(dialog);
                    return ;
                } else if (!CallManager.mics.includes(Settings.callsDevicesMicrophone)) {
                    var dialog = deviceError.createObject(timelineRoot, {
                        "errorString": qsTr("Unknown microphone: %1").arg(Settings.callsDevicesMicrophone),
                        "image": ":/icons/icons/ui/place-call.svg"
                    });
                    dialog.open();
            timelineRoot.destroyOnClose(dialog);
                    return ;
                }
                if (CallManager.callType == Voip.VIDEO && CallManager.cameras.length > 0 && !CallManager.cameras.includes(Settings.callsDevicesCamera)) {
                    var dialog = deviceError.createObject(timelineRoot, {
                        "errorString": qsTr("Unknown camera: %1").arg(Settings.callsDevicesCamera),
                        "image": ":/icons/icons/ui/video.svg"
                    });
                    dialog.open();
            timelineRoot.destroyOnClose(dialog);
                    return ;
                }
                CallManager.acceptInvite();
            }
        }

        KomaiButton {
            Layout.rightMargin: 16
            icon.source: "qrc:/icons/icons/ui/end-call.svg"
            text: qsTr("Decline")
            onClicked: {
                CallManager.rejectInvite();
            }
        }

    }

}
