// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../timeline/styles/bubble"
import "../../timeline/styles/plain"
import "../../composer" as Composer
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai

Item {
    id: root

    readonly property int previewFrameMinHeight: 220
    readonly property int previewFrameHardMaxHeight: 680
    readonly property int previewFrameSoftMaxHeight: Math.max(360, Math.floor(((Window.window ? Window.window.height : 900) * 45) / 100))
    readonly property int previewFrameMaxHeight: Math.max(previewFrameMinHeight, Math.min(previewFrameHardMaxHeight, previewFrameSoftMaxHeight))
    readonly property int previewFrameVerticalPadding: Komai.paddingMedium * 2
    readonly property var previewTypingUsers: Settings.timelineTypingShowEnabled ? [qsTr("Alice"), qsTr("Bob")] : []
    readonly property int previewTypingIndicatorHeight: (Settings.timelineTypingShowEnabled && previewTypingUsers.length > 0) ? (previewTypingIndicator.implicitHeight + Komai.paddingSmall) : 0
    readonly property int previewHeaderHeight: previewHeader.implicitHeight + Komai.paddingSmall
    readonly property int previewFooterHeight: previewFooter.implicitHeight + Komai.paddingSmall
    readonly property int previewFrameDesiredHeight: Math.ceil(previewHeaderHeight + previewFooterHeight + chat.contentHeight + chat.topMargin + chat.bottomMargin + previewFrameVerticalPadding + previewTypingIndicatorHeight)
    implicitHeight: timelinePreviewFrame.implicitHeight
    implicitWidth: parent ? parent.width : 700
    readonly property string previewKomaiUrl: "https://komai.chat/?utm_source=komai&amp;utm_medium=app&amp;utm_campaign=settings/timeline-preview"
    readonly property string previewMatrixUrl: "https://matrix.org/"
    readonly property string previewKomaiLabel: "Komai"
    readonly property string previewMatrixLabel: "Matrix"
    readonly property string previewFallbackYouUserId: "@you:example.com"
    readonly property string previewYouUserId: (Komai.currentUser && Komai.currentUser.userid) ? Komai.currentUser.userid : previewFallbackYouUserId
    readonly property string previewFallbackAvatarUrl: "qrc:/logos/komai.svg"
    readonly property string previewLookFeelLabel: qsTr("Look & Feel")
    readonly property string previewFooterText: qsTr("This semi-functional preview shows how settings from the <b>%1</b> tab and those below affect the timeline.")
        .arg(previewLookFeelLabel)
    readonly property string previewYouAvatarUrl: (Komai.currentUser && Komai.currentUser.avatarUrl && Komai.currentUser.avatarUrl.length > 0)
        ? Komai.currentUser.avatarUrl
        : previewFallbackAvatarUrl
    readonly property string previewAliceTemplate: qsTr("I just stumbled upon %1 - finally, a %2 chat app I love! ❤️")
    readonly property string previewAliceBody: previewAliceTemplate.arg(previewKomaiLabel).arg(previewMatrixLabel)
    readonly property string previewAliceFormattedBody: Komai.formatHtmlEmojis(previewAliceTemplate
        .arg("<a href=\"" + previewKomaiUrl + "\">" + previewKomaiLabel + "</a>")
        .arg("<a href=\"" + previewMatrixUrl + "\">" + previewMatrixLabel + "</a>"))
    readonly property string previewOwnBody: qsTr("I'm giving it a try too! Currently tweaking how messages look.\nIt seems pleasing to the eye and insanely fast! 🚀")
    readonly property string previewOwnFormattedBody: Komai.formatHtmlEmojis(previewOwnBody.split("\n").join("<br>"))
    readonly property int previewOwnMessageStatus: MtxEvent.Read
    readonly property date previewTsAlice: new Date(Date.now() - (9 * 60 * 1000))
    readonly property date previewTsBob: new Date(Date.now() - (6 * 60 * 1000))
    readonly property date previewTsYou: new Date(Date.now() - (2 * 60 * 1000))

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
        // Re-evaluate model on style/positioning changes without a hard teardown cycle.
        return previewEventsWithPrevious(previewEvents.slice().reverse());
    }

    readonly property var previewEvents: [
        {
            body: root.previewAliceBody,
            day: root.previewDayKey(root.previewTsAlice),
            eventId: "$preview-1",
            formattedBody: root.previewAliceFormattedBody,
            isOnlyEmoji: 0,
            isEditable: true,
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
            body: "🔥",
            day: root.previewDayKey(root.previewTsBob),
            eventId: "$preview-2",
            formattedBody: Komai.formatHtmlEmojis("🔥"),
            isOnlyEmoji: true,
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
            timestamp: root.previewTsBob,
            trustlevel: 0,
            type: MtxEvent.TextMessage,
            avatarUrl: "qrc:/preview-avatars/bob.png",
            userId: "@bob:example.org",
            userName: "Bob",
            userPowerlevel: 0
        },
        {
            body: root.previewOwnBody,
            day: root.previewDayKey(root.previewTsYou),
            eventId: "$preview-3",
            formattedBody: root.previewOwnFormattedBody,
            isOnlyEmoji: 0,
            isEditable: true,
            isEdited: false,
            isEncrypted: false,
            isSender: true,
            isStateEvent: false,
            notificationlevel: MtxEvent.Empty,
            reactions: [
                {
                    count: 1,
                    displayKey: "👍",
                    key: "👍",
                    selfReactedEvent: "",
                    users: "Alice"
                }
            ],
            replyTo: "",
            room: previewRuntime.room,
            status: root.previewOwnMessageStatus,
            threadId: "",
            isThreadRoot: false,
            timestamp: root.previewTsYou,
            trustlevel: 0,
            type: MtxEvent.TextMessage,
            isCurrentUser: true,
            avatarUrl: root.previewYouAvatarUrl,
            userId: root.previewYouUserId,
            userName: qsTr("You"),
            userPowerlevel: 50
        }
    ]

    SettingRowTimelinePreviewRuntime {
        id: previewRuntime

        previewFallbackAvatarUrl: root.previewFallbackAvatarUrl
        previewFallbackYouUserId: root.previewFallbackYouUserId
        previewTypingUsers: root.previewTypingUsers
        previewYouUserId: root.previewYouUserId
    }

    Rectangle {
        id: timelinePreviewFrame

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
            frameRadius: timelinePreviewFrame.radius
            text: qsTr("Timeline preview")
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
            anchors.bottom: previewTypingIndicator.visible ? previewTypingIndicator.top : previewFooter.top
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

            parent: timelinePreviewFrame
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

            parent: timelinePreviewFrame
        }

        Composer.TypingIndicator {
            id: previewTypingIndicator

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: previewFooter.top
            anchors.leftMargin: Komai.paddingMedium
            anchors.rightMargin: Komai.paddingMedium
            anchors.bottomMargin: Komai.paddingSmall
            room: previewRuntime.room
            visible: Settings.timelineTypingShowEnabled
        }

        SettingRowTimelinePreviewFooter {
            id: previewFooter

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            frameRadius: timelinePreviewFrame.radius
            text: root.previewFooterText
        }
    }
}
