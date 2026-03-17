// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import cc.etke.komai 1.0

OverlayDialog {
    id: readReceiptsRoot

    property ReadReceiptsProxy readReceipts
    property Room room

    title: qsTr("Read receipts")
    titleIcon: ":/icons/icons/ui/eye-show.svg"

    ScrollView {
        padding: Komai.paddingMedium
        ScrollBar.horizontal.visible: false
        Layout.fillWidth: true
        Layout.preferredHeight: 300

        ListView {
            id: readReceiptsList

            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: readReceipts

            delegate: ItemDelegate {
                id: del

                onClicked: room.openUserProfile(model.mxid)
                padding: Komai.paddingMedium
                width: ListView.view.width
                height: receiptLayout.implicitHeight + Komai.paddingSmall * 2
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.text: model.mxid
                background: Rectangle {
                    color: del.hovered ? palette.dark : palette.window
                }

                RowLayout {
                    id: receiptLayout

                    spacing: Komai.paddingMedium
                    anchors.fill: parent
                    anchors.margins: Komai.paddingSmall

                    Avatar {
                        id: avatar

                        Layout.preferredWidth: Komai.listIconSize
                        Layout.preferredHeight: Komai.listIconSize
                        userid: model.mxid
                        url: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                        displayName: model.displayName
                        enabled: false
                    }

                    ColumnLayout {
                        spacing: Komai.paddingSmall
                        Layout.fillWidth: true

                        ElidedLabel {
                            fullText: model.displayName
                            color: Komai.readableAccentTextColor(
                                readReceiptsRoot.room ? TimelineManager.roomUserColor(readReceiptsRoot.room.roomId, model ? model.mxid : "", palette.window, Settings.timelineUserColorCodingPolicy)
                                                      : TimelineManager.userColor(model ? model.mxid : "", palette.window),
                                palette.window)
                            font.pointSize: Settings.uiFontSizePt
                            elideWidth: del.width - Komai.paddingMedium - avatar.width
                            Layout.fillWidth: true
                        }

                        ElidedLabel {
                            fullText: model.timestamp
                            color: palette.buttonText
                            font.pointSize: Settings.uiFontSizePt * 0.9
                            elideWidth: del.width - Komai.paddingMedium - avatar.width
                            Layout.fillWidth: true
                        }
                    }
                }

                KomaiCursorShape {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }
}
