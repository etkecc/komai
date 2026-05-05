// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../timeline/styles/bubble"
import "../../timeline/styles/plain"
import QtQuick
import QtQuick.Controls
import cc.etke.komai

Item {
    id: root

    readonly property int previewFrameMinHeight: 180
    readonly property int previewFrameHardMaxHeight: 520
    readonly property int previewFrameSoftMaxHeight: Math.max(280, Math.floor(((Window.window ? Window.window.height : 900) * 35) / 100))
    readonly property int previewFrameMaxHeight: Math.max(previewFrameMinHeight, Math.min(previewFrameHardMaxHeight, previewFrameSoftMaxHeight))
    readonly property int previewFrameVerticalPadding: Komai.paddingMedium * 2
    readonly property int previewHeaderHeight: previewHeader.implicitHeight + Komai.paddingSmall
    readonly property int previewFooterHeight: previewFooter.implicitHeight + Komai.paddingSmall
    readonly property int previewFrameDesiredHeight: Math.ceil(previewHeaderHeight + previewFooterHeight + chat.contentHeight + chat.topMargin + chat.bottomMargin + previewFrameVerticalPadding)
    implicitHeight: avatarPreviewFrame.implicitHeight
    implicitWidth: parent ? parent.width : 700

    readonly property string previewFallbackYouUserId: "@you:example.com"
    readonly property string previewYouUserId: (Komai.currentUser && Komai.currentUser.userid) ? Komai.currentUser.userid : previewFallbackYouUserId
    readonly property string previewFallbackAvatarUrl: "qrc:/logos/komai.svg"
    readonly property string previewFooterText: qsTr("This preview shows how avatar settings affect rendering throughout the app.")

    readonly property date previewTsAlice: new Date(Date.now() - (12 * 60 * 1000))
    readonly property date previewTsCarol: new Date(Date.now() - (5 * 60 * 1000))
    readonly property date previewTsDave: new Date(Date.now() - (2 * 60 * 1000))

    function previewDayKey(timestamp) {
        return timestamp.getFullYear() * 10000 + (timestamp.getMonth() + 1) * 100 + timestamp.getDate();
    }

    function previewEventsWithPrevious(eventsInDisplayOrder) {
        const withPrevious = [];
        for (let i = 0; i < eventsInDisplayOrder.length; i++) {
            const current = eventsInDisplayOrder[i];
            const previous = (i + 1 < eventsInDisplayOrder.length) ? eventsInDisplayOrder[i + 1] : null;
            withPrevious.push(Object.assign({}, current, {
                previousDay: previous ? previous.day : current.day,
                previousTimestamp: previous ? previous.timestamp : current.timestamp,
                previousIsStateEvent: previous ? previous.isStateEvent : true,
                previousUserId: previous ? previous.userId : ""
            }));
        }
        return withPrevious;
    }

    function previewEventsModelFor(_style, _positioning) {
        return previewEventsWithPrevious(previewEvents.slice().reverse());
    }

    readonly property var previewEvents: [
        {
            body: qsTr("Hey everyone! Just joined the chat."),
            day: root.previewDayKey(root.previewTsAlice),
            eventId: "$avatar-preview-1",
            formattedBody: qsTr("Hey everyone! Just joined the chat."),
            isOnlyEmoji: 0,
            isEditable: false,
            isEdited: false,
            isEncrypted: false,
            isSender: false,
            isStateEvent: false,
            notificationlevel: MtxEvent.Empty,
            reactions: [],
            replyTo: "",
            room: previewRuntime.room,
            status: MtxEvent.Empty,
            threadId: "",
            isThreadRoot: false,
            timestamp: root.previewTsAlice,
            trustlevel: 0,
            type: MtxEvent.TextMessage,
            avatarUrl: "qrc:/preview-avatars/alice.png",
            userId: "@alice:example.org",
            userName: "Alice",
            userPowerlevel: 100
        },
        {
            body: qsTr("Welcome, Alice! I'm still setting up my profile."),
            day: root.previewDayKey(root.previewTsCarol),
            eventId: "$avatar-preview-2",
            formattedBody: qsTr("Welcome, Alice! I'm still setting up my profile."),
            isOnlyEmoji: 0,
            isEditable: false,
            isEdited: false,
            isEncrypted: false,
            isSender: false,
            isStateEvent: false,
            notificationlevel: MtxEvent.Empty,
            reactions: [],
            replyTo: "",
            room: previewRuntime.room,
            status: MtxEvent.Empty,
            threadId: "",
            isThreadRoot: false,
            timestamp: root.previewTsCarol,
            trustlevel: 0,
            type: MtxEvent.TextMessage,
            avatarUrl: "",
            userId: "@carol:example.net",
            userName: "Carol",
            userPowerlevel: 0
        },
        {
            body: qsTr("Same here, still no avatar yet."),
            day: root.previewDayKey(root.previewTsDave),
            eventId: "$avatar-preview-3",
            formattedBody: qsTr("Same here, still no avatar yet."),
            isOnlyEmoji: 0,
            isEditable: false,
            isEdited: false,
            isEncrypted: false,
            isSender: false,
            isStateEvent: false,
            notificationlevel: MtxEvent.Empty,
            reactions: [],
            replyTo: "",
            room: previewRuntime.room,
            status: MtxEvent.Empty,
            threadId: "",
            isThreadRoot: false,
            timestamp: root.previewTsDave,
            trustlevel: 0,
            type: MtxEvent.TextMessage,
            avatarUrl: "",
            userId: "@dave:example.com",
            userName: "Dave",
            userPowerlevel: 0
        }
    ]

    SettingRowTimelinePreviewRuntime {
        id: previewRuntime

        previewFallbackAvatarUrl: root.previewFallbackAvatarUrl
        previewFallbackYouUserId: root.previewFallbackYouUserId
        previewTypingUsers: []
        previewYouUserId: root.previewYouUserId
    }

    Rectangle {
        id: avatarPreviewFrame

        anchors.left: parent.left
        anchors.right: parent.right
        color: palette.base
        border.color: palette.mid
        border.width: 1
        implicitHeight: Math.max(root.previewFrameMinHeight, Math.min(root.previewFrameMaxHeight, root.previewFrameDesiredHeight))
        radius: Komai.paddingMedium

        SettingRowTimelinePreviewHeader {
            id: previewHeader

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            frameRadius: avatarPreviewFrame.radius
            text: qsTr("Avatar preview")
        }

        ListView {
            id: chat

            // Match the real timeline: scrollbar width is reserved by
            // anchors.rightMargin below, so delegates fill the full ListView
            // width without an extra inner inset.
            property int delegateMaxWidth: width

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: previewHeader.bottom
            anchors.bottom: previewFooter.top
            anchors.leftMargin: Komai.paddingMedium
            anchors.rightMargin: Komai.paddingMedium + (previewScrollBar.visible ? previewScrollBar.width : 0)
            anchors.topMargin: Komai.paddingSmall
            anchors.bottomMargin: Komai.paddingSmall
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            interactive: contentHeight > height
            model: root.previewEventsModelFor(Settings.timelineMessagesStyle, Settings.timelineMessagesLayoutPositioning)
            topMargin: Komai.paddingSmall
            bottomMargin: Komai.paddingSmall
            spacing: 2
            verticalLayoutDirection: ListView.BottomToTop

            ScrollBar.vertical: ScrollBar {
                id: previewScrollBar
                policy: ScrollBar.AsNeeded
            }

            Component {
                id: plainMessageStyle

                TimelinePlainMessageStyle {
                    required property var modelData
                    isHiddenEvent: modelData && modelData.isHiddenEvent !== undefined ? modelData.isHiddenEvent : false
                    messageActions: messageActionsC
                    messageContextMenu: previewRuntime.messageContextMenu
                    metadataActionsEnabled: false
                    previewData: modelData
                    replyContextMenu: previewRuntime.replyContextMenu
                    scrolledToThis: false
                }
            }

            Component {
                id: bubbleMessageStyle

                TimelineBubbleMessageStyle {
                    required property var modelData
                    isHiddenEvent: modelData && modelData.isHiddenEvent !== undefined ? modelData.isHiddenEvent : false
                    messageActions: messageActionsC
                    messageContextMenu: previewRuntime.messageContextMenu
                    metadataActionsEnabled: false
                    previewData: modelData
                    replyContextMenu: previewRuntime.replyContextMenu
                    scrolledToThis: false
                }
            }

            function styleDelegateFor(style, _positioning) {
                switch (style) {
                case Settings.TimelineMessagesStyle.Bubbles:
                    return bubbleMessageStyle;
                case Settings.TimelineMessagesStyle.Plain:
                default:
                    return plainMessageStyle;
                }
            }

            delegate: styleDelegateFor(Settings.timelineMessagesStyle, Settings.timelineMessagesLayoutPositioning)
        }

        MouseArea {
            id: actionBarDismissOverlay

            parent: avatarPreviewFrame
            x: 0
            y: 0
            width: parent.width
            height: parent.height
            visible: messageActionsC.pinned
            z: 9
            onClicked: messageActionsC.dismiss()
        }

        SettingRowTimelinePreviewMessageActions {
            id: messageActionsC

            parent: avatarPreviewFrame
        }

        SettingRowTimelinePreviewFooter {
            id: previewFooter

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            frameRadius: avatarPreviewFrame.radius
            text: root.previewFooterText
        }
    }
}
