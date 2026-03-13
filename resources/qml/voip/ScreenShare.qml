// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Popup {
    modal: true

    anchors.centerIn: parent;

    Component.onCompleted: {
        frameRateCombo.currentIndex = frameRateCombo.find(Settings.callsScreenshareFrameRate);
    }
    Component.onDestruction: {
        CallManager.closeScreenShare();
    }

    ColumnLayout {
        Label {
            Layout.topMargin: 16
            Layout.bottomMargin: 16
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.alignment: Qt.AlignLeft
            text: qsTr("Share desktop with %1?").arg(room.roomName)
            color: palette.windowText
        }

        RowLayout {
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.bottomMargin: 8

            Label {
            Layout.alignment: Qt.AlignLeft
            text: qsTr("Method:")
            color: palette.windowText
            }

          ComboBox {
            id: screenshareType

            Layout.fillWidth: true
            model: CallManager.screenShareTypeList()
            onCurrentIndexChanged: CallManager.setScreenShareType(currentIndex);
          }
        }

        RowLayout {
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.bottomMargin: 8

            Label {
                Layout.alignment: Qt.AlignLeft
                text: qsTr("Window:")
                color: palette.windowText
            }

            ComboBox {
                visible: CallManager.screenShareType == Voip.X11 || CallManager.screenShareType == Voip.D3D11
                id: windowCombo

                Layout.fillWidth: true
                model: CallManager.windowList()
            }

            KomaiButton {
                visible: CallManager.screenShareType == Voip.XDP
                highlighted: !CallManager.screenShareReady
                text: qsTr("Request screencast")
                onClicked: {
                  Settings.callsScreenshareShowCursor = showCursorCheckBox.checked;
                  CallManager.setupScreenShareXDP();
                }
            }

        }

        RowLayout {
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.bottomMargin: 8

            Label {
                Layout.alignment: Qt.AlignLeft
                text: qsTr("Frame rate:")
                color: palette.windowText
            }

            ComboBox {
                id: frameRateCombo

                Layout.fillWidth: true
                model: ["120", "90", "60", "50", "48", "30", "25", "20", "15", "10", "5", "2", "1"]
            }

        }

        GridLayout {
            columns: 2
            rowSpacing: 10
            Layout.margins: 8

            MatrixText {
                text: qsTr("Include your camera picture-in-picture")
            }

            ToggleButton {
                id: pipCheckBox

                enabled: CallManager.cameras.length > 0
                checked: CallManager.cameras.length > 0 && Settings.callsScreensharePictureInPicture
                Layout.alignment: Qt.AlignRight
            }

            MatrixText {
                text: qsTr("Request remote camera")
                ToolTip.text: qsTr("View your callee's camera like a regular video call")
                ToolTip.visible: hovered
            }

            ToggleButton {
                id: remoteVideoCheckBox

                Layout.alignment: Qt.AlignRight
                checked: Settings.callsScreenshareIncludeRemoteVideo
                ToolTip.text: qsTr("View your callee's camera like a regular video call")
                ToolTip.visible: hovered
            }

            MatrixText {
                text: qsTr("Show mouse cursor")
            }

            ToggleButton {
                id: showCursorCheckBox

                Layout.alignment: Qt.AlignRight
                checked: Settings.callsScreenshareShowCursor
            }

        }

        RowLayout {
            Layout.margins: 8

            Item {
                Layout.fillWidth: true
            }

            KomaiButton {
                visible: CallManager.screenShareReady
                text: qsTr("Share")
                icon.source: "qrc:/icons/icons/ui/screen-share.svg"

                onClicked: {
                    Settings.callsScreenshareFrameRate = frameRateCombo.currentText;
                    Settings.callsScreensharePictureInPicture = pipCheckBox.checked;
                    Settings.callsScreenshareIncludeRemoteVideo = remoteVideoCheckBox.checked;
                    Settings.callsScreenshareShowCursor = showCursorCheckBox.checked;

                    CallManager.sendInvite(room.roomId, Voip.SCREEN, windowCombo.currentIndex);
                    close();
                }
            }

            KomaiButton {
                visible: CallManager.screenShareReady
                text: qsTr("Preview")
                onClicked: {
                    CallManager.previewWindow(windowCombo.currentIndex);
                }
            }

            KomaiButton {
                text: qsTr("Cancel")
                onClicked: {
                    close();
                }
            }

        }

    }

    background: Rectangle {
        color: palette.window
        border.color: palette.windowText
    }

}
