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
    id: root

    required property var room
    required property string roomId

    property string nextBatchToken: ""
    property bool loading: false
    property int includeMode: 0 // 0 = all, 1 = participated

    readonly property int dialogViewportWidth: overlayDialogViewport ? overlayDialogViewport.width : 760
    readonly property int dialogViewportHeight: overlayDialogViewport ? overlayDialogViewport.height : 600

    title: qsTr("Threads")
    titleIcon: ":/icons/icons/ui/thread.svg"
    overlayDialogMaxWidthRatio: 0.85

    width: Math.min(
        Math.max(240, dialogViewportWidth - Komai.paddingLarge * 2),
        Math.max(240, Math.floor(dialogViewportWidth * overlayDialogMaxWidthRatio))
    )
    height: Math.min(implicitHeight, dialogViewportHeight - Komai.paddingLarge * 2)
    x: Math.round((dialogViewportWidth - width) / 2)
    y: Math.max(Komai.paddingLarge, Math.round((dialogViewportHeight - height) / 2))

    Component.onCompleted: fetchThreads()

    function includeModeString() {
        return includeMode === 1 ? "participated" : "all";
    }

    function fetchThreads(from) {
        if (root.loading)
            return;
        root.loading = true;
        TimelineManager.fetchActiveMatrixRoomThreadRoots(
            includeModeString(),
            from || "",
            20);
    }

    function fetchMoreIfNeeded() {
        if (!root.loading && root.nextBatchToken.length > 0)
            root.fetchThreads(root.nextBatchToken);
    }

    function onThreadRootsReady(items, nextBatch) {
        root.loading = false;
        root.nextBatchToken = nextBatch;
        for (let i = 0; i < items.length; i++)
            threadListModel.append(items[i]);
    }

    property Item hoveredReplyBadge: null
    property int hoveredReplyCount: 0

    Connections {
        target: TimelineManager
        function onMatrixRoomThreadRootsReady(items, nextBatchToken) {
            root.onThreadRootsReady(items, nextBatchToken);
        }
    }

    KomaiToolTip {
        parent: root.contentItem.parent
        anchorItem: root.hoveredReplyBadge
        anchorX: root.hoveredReplyBadge ? root.hoveredReplyBadge.width / 2 : 0
        anchorY: 0
        gapY: Komai.paddingMedium
        preferBelow: false
        text: qsTr("%n thread reply(s)", "", root.hoveredReplyCount)
        delay: 0
        requestedVisible: root.hoveredReplyBadge !== null
    }

    SegmentedButton {
        id: filterSegment

        Layout.fillWidth: true
        implicitHeight: Math.max(36, Math.round(Settings.uiFontSizePt * 2.7))
        currentIndex: root.includeMode

        model: [
            { text: qsTr("All") },
            { text: qsTr("Participated") }
        ]

        onActivated: function(index) {
            root.includeMode = index;
            threadListModel.clear();
            root.nextBatchToken = "";
            root.fetchThreads();
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: Math.max(
            160,
            root.dialogViewportHeight - overlayDialogChromeHeight - Komai.paddingLarge * 2
        )

        ListView {
            id: threadList

            anchors.fill: parent
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: 2

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
                ? Math.max(threadsScrollbar.width, threadsScrollbar.implicitWidth) + Komai.paddingSmall
                : 0

            rightMargin: reservedScrollbarWidth

            model: ListModel {
                id: threadListModel
            }

            ScrollBar.vertical: ScrollBar {
                id: threadsScrollbar

                policy: threadList.scrollbarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            delegate: Item {
                id: threadDelegate

                required property int index
                required property string eventId
                required property string senderId
                required property string senderDisplayName
                required property string senderAvatarUrl
                required property string body
                required property var timestamp
                required property int replyCount

                width: threadList.width - threadList.reservedScrollbarWidth
                implicitHeight: delegateRow.implicitHeight + Komai.paddingMedium * 2

                readonly property bool activeState: delegateMouseArea.containsMouse
                readonly property color actionTextColor: activeState ? palette.brightText : palette.text
                readonly property string effectiveDisplayName: senderDisplayName || senderId

                Component.onCompleted: {
                    if (index >= threadListModel.count - 5)
                        root.fetchMoreIfNeeded();
                }

                Rectangle {
                    anchors.fill: parent
                    radius: Komai.paddingMedium
                    color: threadDelegate.activeState ? palette.dark : palette.window
                }

                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        TimelineManager.queueActiveMatrixThread(threadDelegate.eventId);
                        root.close();
                    }
                }

                RowLayout {
                    id: delegateRow

                    anchors.fill: parent
                    anchors.leftMargin: Komai.paddingMedium
                    anchors.rightMargin: Komai.paddingMedium
                    anchors.topMargin: Komai.paddingMedium
                    anchors.bottomMargin: Komai.paddingMedium
                    spacing: Komai.paddingMedium

                    AvatarUserFlipButton {
                        Layout.preferredWidth: Komai.listIconSize
                        Layout.preferredHeight: Komai.listIconSize
                        Layout.alignment: Qt.AlignTop
                        avatarButtonSize: Komai.listIconSize
                        cleanFront: true
                        avatarUserId: threadDelegate.senderId
                        avatarRoomId: root.roomId
                        avatarUrl: threadDelegate.senderAvatarUrl
                            ? threadDelegate.senderAvatarUrl.replace("mxc://", "image://MxcImage/")
                            : ""
                        avatarDisplayName: threadDelegate.effectiveDisplayName
                        onLeftClicked: TimelineManager.openRoomUserProfile(root.roomId, threadDelegate.senderId)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: threadDelegate.effectiveDisplayName
                            color: threadDelegate.activeState
                                ? palette.brightText
                                : TimelineManager.userColor(threadDelegate.senderId, palette.window)
                            font.pointSize: Settings.uiFontSizePt
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: threadDelegate.body
                            color: threadDelegate.activeState ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        Rectangle {
                            id: replyBadge

                            visible: threadDelegate.replyCount > 0
                            readonly property color badgeColor: threadDelegate.actionTextColor
                            readonly property int badgeIconSize: Math.max(14, Math.round(Settings.uiFontSizePt * 1.5))

                            Layout.alignment: Qt.AlignRight
                            implicitWidth: replyBadgeRow.implicitWidth + Komai.paddingSmall * 2
                            implicitHeight: replyBadgeRow.implicitHeight + Komai.paddingSmall * 0.5
                            radius: Komai.paddingSmall
                            color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
                            border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
                            border.width: 1

                            Row {
                                id: replyBadgeRow
                                anchors.centerIn: parent
                                spacing: Komai.paddingSmall * 0.5

                                Image {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: replyBadge.badgeIconSize
                                    height: replyBadge.badgeIconSize
                                    sourceSize.width: width
                                    sourceSize.height: height
                                    source: "image://colorimage/:/icons/icons/ui/thread.svg?" + replyBadge.badgeColor
                                }

                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: threadDelegate.replyCount.toLocaleString()
                                    color: replyBadge.badgeColor
                                    font.pointSize: Settings.uiFontSizePt * 0.8
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                                onContainsMouseChanged: {
                                    if (containsMouse) {
                                        root.hoveredReplyBadge = replyBadge;
                                        root.hoveredReplyCount = threadDelegate.replyCount;
                                    } else if (root.hoveredReplyBadge === replyBadge) {
                                        root.hoveredReplyBadge = null;
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: {
                                const ts = Number(threadDelegate.timestamp);
                                if (ts <= 0)
                                    return "";
                                return new Date(ts).toLocaleString(Locale.ShortFormat);
                            }
                            color: threadDelegate.activeState ? palette.brightText : palette.placeholderText
                            font.pointSize: Settings.uiFontSizePt * 0.8
                        }
                    }
                }
            }

            footer: Item {
                width: threadList.width - threadList.reservedScrollbarWidth
                height: 56

                Spinner {
                    anchors.centerIn: parent
                    running: root.loading
                    height: 40
                    opacity: root.loading ? 1 : 0
                }
            }
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - Komai.paddingMedium * 2
            spacing: Komai.paddingSmall
            visible: !root.loading && threadListModel.count === 0

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No threads found")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 1.2
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.includeMode === 1
                      ? qsTr("You haven't participated in any threads in this room yet.")
                      : qsTr("No one has started a thread in this room yet.")
                color: palette.placeholderText
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.WordWrap
            }
        }
    }
}
