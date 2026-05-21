// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

OverlayDialog {
    id: readReceiptsRoot

    property ReadReceiptsProxy readReceipts
    property var room

    title: qsTr("Read receipts")
    titleIcon: ":/icons/icons/ui/eye-show.svg"

    ListView {
        id: readReceiptsList

        // Tri-state scrollbar policy (Always / Never / WhenNeeded) driven by
        // the global UI setting, matching how RoomDirectory, ReactionDetails,
        // and the room list resolve their bars. `reservedScrollbarWidth`
        // keeps the bar from sitting flush against the row cards.
        readonly property bool hasVerticalOverflow: contentHeight > height
        readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
        readonly property bool scrollbarVisible: {
            switch (scrollbarPolicy) {
            case Settings.ScrollbarPolicy.Always:
                return true;
            case Settings.ScrollbarPolicy.Never:
                return false;
            case Settings.ScrollbarPolicy.WhenNeeded:
            default:
                return hasVerticalOverflow;
            }
        }
        readonly property real reservedScrollbarWidth: scrollbarVisible
            ? Math.max(receiptsScrollbar.width, receiptsScrollbar.implicitWidth) + Komai.paddingSmall
            : 0

        Layout.fillWidth: true
        Layout.preferredHeight: 300
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        topMargin: Komai.paddingSmall
        bottomMargin: Komai.paddingSmall
        rightMargin: reservedScrollbarWidth

        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: Komai.paddingSmall
        model: readReceipts

        ScrollBar.vertical: ScrollBar {
            id: receiptsScrollbar

            policy: readReceiptsList.scrollbarVisible
                ? ScrollBar.AlwaysOn
                : ScrollBar.AlwaysOff
        }

        delegate: Rectangle {
            id: del

            width: ListView.view.width - ListView.view.rightMargin
            implicitHeight: receiptLayout.implicitHeight + Komai.paddingSmall * 2
            color: delHover.hovered ? palette.dark : palette.window
            radius: Komai.paddingMedium
            border.color: Komai.theme.separator
            border.width: 1

            HoverHandler {
                id: delHover
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton
                onClicked: {
                    if (readReceiptsRoot.room && readReceiptsRoot.room.openUserProfile)
                        readReceiptsRoot.room.openUserProfile(model.mxid);
                }
            }

            RowLayout {
                id: receiptLayout

                anchors.fill: parent
                anchors.margins: Komai.paddingSmall
                spacing: Komai.paddingMedium

                Avatar {
                    id: avatar

                    Layout.preferredWidth: Komai.iconSize
                    Layout.preferredHeight: Komai.iconSize
                    Layout.alignment: Qt.AlignVCenter
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
                        color: delHover.hovered
                            ? palette.brightText
                            : Komai.readableAccentTextColor(
                                readReceiptsRoot.room ? TimelineManager.roomUserColor(readReceiptsRoot.room.roomId, model ? model.mxid : "", palette.window, Settings.timelineUserColorCodingPolicy)
                                                      : TimelineManager.userColor(model ? model.mxid : "", palette.window),
                                palette.window)
                        font.pointSize: Settings.uiFontSizePt
                        elideWidth: del.width - Komai.paddingMedium * 3 - avatar.width - timeLabel.width
                        Layout.fillWidth: true
                    }

                    ElidedLabel {
                        fullText: model.mxid
                        color: delHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 0.9
                        elideWidth: del.width - Komai.paddingMedium * 3 - avatar.width - timeLabel.width
                        Layout.fillWidth: true
                    }
                }

                Label {
                    id: timeLabel

                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    text: model.timestamp
                    color: delHover.hovered ? palette.brightText : palette.buttonText
                    font.pointSize: Settings.uiFontSizePt * 0.85
                }
            }
        }
    }
}
