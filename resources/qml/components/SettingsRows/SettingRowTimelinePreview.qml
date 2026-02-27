// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../timeline"
import "../../timeline/styles/bubble"
import "../../timeline/styles/minimal"
import ".."
import "../.." as RootQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import im.nheko

Item {
    id: root

    readonly property int previewFrameMinHeight: 220
    readonly property int previewFrameHardMaxHeight: 680
    readonly property int previewFrameSoftMaxHeight: Math.max(360, Math.floor(((Window.window ? Window.window.height : 900) * 45) / 100))
    readonly property int previewFrameMaxHeight: Math.max(previewFrameMinHeight, Math.min(previewFrameHardMaxHeight, previewFrameSoftMaxHeight))
    readonly property int previewFrameVerticalPadding: Nheko.paddingMedium * 2
    readonly property var previewTypingUsers: Settings.timelineTypingShowEnabled ? [qsTr("Alice"), qsTr("Bob")] : []
    readonly property int previewTypingIndicatorHeight: (Settings.timelineTypingShowEnabled && previewTypingUsers.length > 0) ? (previewTypingIndicator.implicitHeight + Nheko.paddingSmall) : 0
    readonly property int previewHeaderHeight: previewHeader.implicitHeight + Nheko.paddingSmall
    readonly property int previewFooterHeight: previewFooter.implicitHeight + Nheko.paddingSmall
    readonly property int previewFrameDesiredHeight: Math.ceil(previewHeaderHeight + previewFooterHeight + chat.contentHeight + chat.topMargin + chat.bottomMargin + previewFrameVerticalPadding + previewTypingIndicatorHeight)
    implicitHeight: timelinePreviewFrame.implicitHeight
    implicitWidth: parent ? parent.width : 700
    readonly property string previewKomaiUrl: "https://github.com/etkecc/komai"
    readonly property string previewMatrixUrl: "https://matrix.org/"
    readonly property string previewKomaiLabel: "Komai"
    readonly property string previewMatrixLabel: "Matrix"
    readonly property string previewFallbackYouUserId: "@you:example.com"
    readonly property string previewYouUserId: (Nheko.currentUser && Nheko.currentUser.userid) ? Nheko.currentUser.userid : previewFallbackYouUserId
    readonly property string previewFallbackAvatarUrl: "qrc:/logos/komai.svg"
    readonly property string previewLookFeelLabel: qsTr("Look & Feel")
    readonly property string previewFooterText: qsTr("This semi-functional preview shows how settings from the <b>%1</b> tab and those below affect the timeline.")
        .arg(previewLookFeelLabel)
    readonly property string previewYouAvatarUrl: (Nheko.currentUser && Nheko.currentUser.avatarUrl && Nheko.currentUser.avatarUrl.length > 0)
        ? Nheko.currentUser.avatarUrl
        : previewFallbackAvatarUrl
    readonly property string previewAliceTemplate: qsTr("I just stumbled upon %1 - finally, a %2 chat app that I really like! 🦁")
    readonly property string previewAliceBody: previewAliceTemplate.arg(previewKomaiLabel).arg(previewMatrixLabel)
    readonly property string previewAliceFormattedBody: previewAliceTemplate
        .arg("<a href=\"" + previewKomaiUrl + "\">" + previewKomaiLabel + "</a>")
        .arg("<a href=\"" + previewMatrixUrl + "\">" + previewMatrixLabel + "</a>")
    readonly property string previewCarolBody: qsTr("I'm testing it as we speak and currently configuring how messages look..\n\nIt's quite pleasing to the eye, but also insanely fast! ⚡")
    readonly property string previewCarolFormattedBody: previewCarolBody.split("\n").join("<br>")
    readonly property int previewOwnMessageStatus: Settings.timelineReadReceiptsEnabled ? MtxEvent.Read : MtxEvent.Received
    readonly property var previewEventsForList: previewEvents.slice().reverse()

    function previewEventsModelFor(_style, _positioning) {
        // Re-evaluate model on style/positioning changes without a hard teardown cycle.
        return previewEventsForList.slice();
    }

    readonly property var previewEvents: [
        {
            body: root.previewAliceBody,
            day: 20260225,
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
            room: previewRoom,
            status: MtxEvent.Empty,
            threadId: "",
            timestamp: new Date(2026, 1, 25, 8, 34),
            trustlevel: 0,
            type: MtxEvent.TextMessage,
            avatarUrl: "qrc:/preview-avatars/alice.png",
            userId: "@alice:example.org",
            userName: "Alice",
            userPowerlevel: 100
        },
        {
            body: "🚀",
            day: 20260225,
            eventId: "$preview-2",
            formattedBody: "🚀",
            isOnlyEmoji: true,
            isEditable: false,
            isEdited: false,
            isEncrypted: false,
            isSender: false,
            isStateEvent: false,
            notificationlevel: MtxEvent.Empty,
            reactions: [],
            replyTo: "",
            room: previewRoom,
            status: MtxEvent.Empty,
            threadId: "",
            timestamp: new Date(2026, 1, 25, 8, 36),
            trustlevel: 0,
            type: MtxEvent.TextMessage,
            avatarUrl: "qrc:/preview-avatars/bob.png",
            userId: "@bob:example.org",
            userName: "Bob",
            userPowerlevel: 0
        },
        {
            body: root.previewCarolBody,
            day: 20260225,
            eventId: "$preview-3",
            formattedBody: root.previewCarolFormattedBody,
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
            room: previewRoom,
            status: root.previewOwnMessageStatus,
            threadId: "",
            timestamp: new Date(2026, 1, 25, 8, 40),
            trustlevel: 0,
            type: MtxEvent.TextMessage,
            isCurrentUser: true,
            avatarUrl: root.previewYouAvatarUrl,
            userId: root.previewYouUserId,
            userName: qsTr("You"),
            userPowerlevel: 50
        }
    ]

    QtObject {
        id: previewPermissions

        function canSend(_eventType) {
            return true;
        }

        function changeLevel(_eventType) {
            return 100;
        }

        function redactLevel() {
            return 50;
        }

        function defaultLevel() {
            return 0;
        }
    }

    QtObject {
        id: previewInput

        function reaction(_eventId, _key) {
        }
    }

    QtObject {
        id: previewRoom

        property string edit: ""
        property string fullyReadEventId: "$preview-2"
        property var input: previewInput
        property bool isEncrypted: false
        property var permissions: previewPermissions
        property string reply: ""
        property string roomId: "!timeline-preview:example.org"
        property int roomMemberCount: 8
        property string thread: ""
        property var typingUsers: root.previewTypingUsers

        signal roomAvatarUrlChanged()

        function formatDateSeparator(timestamp) {
            return Qt.formatDate(timestamp, "ddd, MMM d");
        }

        function formatLaterSeparator(_previous, timestamp) {
            return Qt.formatTime(timestamp, "hh:mm");
        }

        function formatTypingUsers(users, _bg, _accent) {
            if (!users || users.length === 0)
                return "";
            if (users.length === 1)
                return qsTr("%1 is typing…").arg(users[0]);
            if (users.length === 2)
                return qsTr("%1 and %2 are typing…").arg(users[0]).arg(users[1]);
            return qsTr("%1, %2 and %3 others are typing…").arg(users[0]).arg(users[1]).arg(users.length - 2);
        }

        function avatarUrl(_userId) {
            if (_userId == root.previewYouUserId || _userId == root.previewFallbackYouUserId) {
                const profile = Nheko.currentUser;
                if (profile && profile.avatarUrl && profile.avatarUrl.length > 0)
                    return profile.avatarUrl;
                return root.previewFallbackAvatarUrl;
            }
            return "";
        }

        function openUserProfile(_userId) {
        }

        function showEvent(_eventId) {
        }

        function eventShown() {
        }
    }

    Connections {
        target: Nheko

        function onProfileChanged() {
            previewRoom.roomAvatarUrlChanged();
        }
    }

    Connections {
        target: Nheko.currentUser

        function onAvatarUrlChanged() {
            previewRoom.roomAvatarUrlChanged();
        }
    }

    QtObject {
        id: messageContextMenuC

        function show(_eventId, _threadId, _type, _isSender, _isEncrypted, _isEditable, _hoveredLink, _copyText) {
        }

        function close() {
        }
    }

    QtObject {
        id: replyContextMenuC

        function show(_copyText, _link, _replyTo) {
        }

        function close() {
        }
    }

    Rectangle {
        id: timelinePreviewFrame

        anchors.left: parent.left
        anchors.right: parent.right
        color: palette.base
        border.color: palette.mid
        border.width: 1
        implicitHeight: Math.max(root.previewFrameMinHeight, Math.min(root.previewFrameMaxHeight, root.previewFrameDesiredHeight))
        radius: Nheko.paddingMedium

        Rectangle {
            id: previewHeader

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            color: palette.alternateBase
            implicitHeight: previewHeaderLabel.implicitHeight + 2 * Nheko.paddingSmall
            radius: timelinePreviewFrame.radius

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.radius
                color: parent.color
            }

            Label {
                id: previewHeaderLabel

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Nheko.paddingMedium
                anchors.rightMargin: Nheko.paddingMedium
                color: palette.text
                font.bold: true
                text: qsTr("Timeline preview")
            }
        }

        ListView {
            id: chat

            property int delegateMaxWidth: Math.max(120, width - 2 * Nheko.paddingMedium - root.previewScrollBarWidth)

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: previewHeader.bottom
            anchors.bottom: previewTypingIndicator.visible ? previewTypingIndicator.top : previewFooter.top
            anchors.leftMargin: Nheko.paddingMedium
            anchors.rightMargin: Nheko.paddingMedium
            anchors.topMargin: Nheko.paddingSmall
            anchors.bottomMargin: Nheko.paddingSmall
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            interactive: contentHeight > height
            model: root.previewEventsModelFor(Settings.timelineMessagesStyle, Settings.timelineMessagesPositioning)
            topMargin: Nheko.paddingSmall
            bottomMargin: Nheko.paddingSmall
            spacing: 2
            verticalLayoutDirection: ListView.BottomToTop

            ScrollBar.vertical: ScrollBar {
                id: previewScrollBar
                policy: ScrollBar.AsNeeded
            }

            Component {
                id: minimalMessageStyle

                TimelineDefaultMessageStyle {
                    required property var modelData
                    messageActions: messageActionsC
                    messageContextMenu: messageContextMenuC
                    previewData: modelData
                    replyContextMenu: replyContextMenuC
                    scrolledToThis: false
                }
            }

            Component {
                id: bubbleMessageStyle

                TimelineBubbleMessageStyle {
                    required property var modelData
                    messageActions: messageActionsC
                    messageContextMenu: messageContextMenuC
                    previewData: modelData
                    replyContextMenu: replyContextMenuC
                    scrolledToThis: false
                }
            }

            function styleDelegateFor(style, _positioning) {
                switch (style) {
                case Settings.TimelineMessagesStyle.Bubbles:
                    return bubbleMessageStyle;
                case Settings.TimelineMessagesStyle.Minimal:
                default:
                    return minimalMessageStyle;
                }
            }

            delegate: styleDelegateFor(Settings.timelineMessagesStyle, Settings.timelineMessagesPositioning)
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

        Control {
            id: messageActionsC

            property Item attached: null
            property var model: null
            property bool pinned: false

            function dismiss() {
                pinned = false;
                attached = null;
            }

            hoverEnabled: true
            padding: Nheko.paddingSmall
            visible: Settings.timelineMessageActionsActivationPolicy !== Settings.TimelineMessageActionsActivationPolicy.Never && !!attached && (pinned || Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.OnHover)
            z: 10
            parent: timelinePreviewFrame

            background: Rectangle {
                border.color: palette.buttonText
                border.width: 1
                color: palette.window
                radius: messageActionsC.padding
            }

            contentItem: RowLayout {
                id: actionRow

                property int itemPadding: Math.round(messageActionsC.padding / 2)
                property var pinnedReactions: Settings.timelineMessageActionsPinnedReactions
                    .split(",")
                    .map(function (s) { return s.trim(); })
                    .filter(function (s) { return s.length > 0; })
                    .slice(0, 10)

                spacing: 0

                Repeater {
                    model: actionRow.pinnedReactions

                    delegate: ToolButton {
                        id: pinnedReactionButton

                        required property string modelData

                        Layout.preferredHeight: 32
                        Layout.preferredWidth: Math.max(32, emojiLabel.implicitWidth + 2 * actionRow.itemPadding)
                        focusPolicy: Qt.NoFocus
                        hoverEnabled: true
                        leftPadding: actionRow.itemPadding
                        rightPadding: actionRow.itemPadding

                        onClicked: messageActionsC.dismiss()
                        onHoveredChanged: {
                            if (hovered)
                                pinnedReactionPulse.pulse();
                        }

                        contentItem: Label {
                            id: emojiLabel

                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: modelData
                            font.pixelSize: 24
                            font.family: Settings.uiFontEmojiFamily
                        }

                        HoverPulseAnimation {
                            id: pinnedReactionPulse

                            targetItem: pinnedReactionButton
                        }
                    }
                }

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    Layout.leftMargin: Nheko.paddingSmall
                    Layout.rightMargin: Nheko.paddingSmall
                    color: palette.mid
                }

                ToolButton {
                    id: reactButton

                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("React")
                    ToolTip.visible: hovered
                    focusPolicy: Qt.NoFocus
                    hoverEnabled: true
                    text: "\u263A"
                    onClicked: messageActionsC.dismiss()
                    onHoveredChanged: {
                        if (hovered)
                            reactPulse.pulse();
                    }

                    HoverPulseAnimation {
                        id: reactPulse

                        targetItem: reactButton
                    }
                }

                ToolButton {
                    id: editButton

                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Edit")
                    ToolTip.visible: hovered
                    focusPolicy: Qt.NoFocus
                    hoverEnabled: true
                    text: "\u270E"
                    visible: !!messageActionsC.model && messageActionsC.model.isEditable
                    onClicked: messageActionsC.dismiss()
                    onHoveredChanged: {
                        if (hovered)
                            editPulse.pulse();
                    }

                    HoverPulseAnimation {
                        id: editPulse

                        targetItem: editButton
                    }
                }

                ToolButton {
                    id: replyButton

                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Reply")
                    ToolTip.visible: hovered
                    focusPolicy: Qt.NoFocus
                    hoverEnabled: true
                    text: "\u21A9"
                    onClicked: messageActionsC.dismiss()
                    onHoveredChanged: {
                        if (hovered)
                            replyPulse.pulse();
                    }

                    HoverPulseAnimation {
                        id: replyPulse

                        targetItem: replyButton
                    }
                }

                ToolButton {
                    id: optionsButton

                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Options")
                    ToolTip.visible: hovered
                    focusPolicy: Qt.NoFocus
                    hoverEnabled: true
                    text: "\u22EF"
                    onClicked: messageActionsC.dismiss()
                    onHoveredChanged: {
                        if (hovered)
                            optionsPulse.pulse();
                    }

                    HoverPulseAnimation {
                        id: optionsPulse

                        targetItem: optionsButton
                    }
                }
            }
        }

        RootQml.TypingIndicator {
            id: previewTypingIndicator

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: previewFooter.top
            anchors.leftMargin: Nheko.paddingMedium
            anchors.rightMargin: Nheko.paddingMedium
            anchors.bottomMargin: Nheko.paddingSmall
            room: previewRoom
            visible: Settings.timelineTypingShowEnabled
        }

        Rectangle {
            id: previewFooter

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            color: palette.alternateBase
            implicitHeight: previewFooterLabel.implicitHeight + 2 * Nheko.paddingSmall
            radius: timelinePreviewFrame.radius

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                color: parent.color
                height: parent.radius
            }

            Label {
                id: previewFooterLabel

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Nheko.paddingMedium
                anchors.rightMargin: Nheko.paddingMedium
                color: palette.text
                font.pointSize: 0.92 * Settings.uiFontSizePt
                text: root.previewFooterText
                textFormat: Text.RichText
                wrapMode: Text.Wrap
            }
        }
    }

    readonly property int previewScrollBarWidth: previewScrollBar.visible ? ((previewScrollBar.width > 0 ? previewScrollBar.width : previewScrollBar.implicitWidth) + Nheko.paddingSmall) : 0
}
