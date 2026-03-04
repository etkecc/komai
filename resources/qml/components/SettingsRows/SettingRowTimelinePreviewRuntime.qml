// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: root

    required property string previewYouUserId
    required property string previewFallbackYouUserId
    required property string previewFallbackAvatarUrl
    required property var previewTypingUsers

    readonly property var room: previewRoom
    readonly property var messageContextMenu: messageContextMenuC
    readonly property var replyContextMenu: replyContextMenuC

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

        function formatTypingUsers(users, _bg) {
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
                const profile = Komai.currentUser;
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
        target: Komai

        function onProfileChanged() {
            previewRoom.roomAvatarUrlChanged();
        }
    }

    Connections {
        target: Komai.currentUser

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
}
