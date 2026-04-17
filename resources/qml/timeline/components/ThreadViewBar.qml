// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Item {
    id: threadBar

    readonly property string threadEventId: TimelineManager.matrixTimelineThreadEventId
    readonly property bool active: threadEventId.length > 0
    property string roomId: ""

    // Thread root event data (refreshed when threadEventId changes).
    property string _rootUserId: ""
    property string _rootUserName: ""
    property string _rootBody: ""
    property string _rootAvatarUrl: ""
    property int _replyCount: 0

    readonly property color threadColor: active
        ? TimelineManager.userColor(threadEventId, palette.base)
        : palette.buttonText
    readonly property int buttonPaddingH: Komai.uiLayoutCompactMode
        ? Komai.paddingSmall : Komai.paddingMedium
    readonly property int closeIconSize: Math.max(
        14, Komai.iconSize - 2 * buttonPaddingH)

    implicitHeight: active
        ? Math.max(Komai.navigationRowHeight,
                   barRow.implicitHeight + Komai.paddingMedium)
        : 0
    visible: active

    function _tryLoadFromModel(model) {
        if (!model)
            return false;

        var uid = model.userIdForEvent(threadEventId);
        if (!uid || uid.length === 0)
            return false;

        _rootUserId = uid;
        _rootUserName = model.userNameForEvent(threadEventId) || uid;
        _rootBody = model.bodyForEvent(threadEventId) || "";
        var mxc = model.avatarUrl(uid) || "";
        _rootAvatarUrl = mxc.length > 0
            ? mxc.replace("mxc://", "image://MxcImage/") : "";
        return true;
    }

    function _refreshRootData() {
        if (!active) {
            _rootUserId = "";
            _rootUserName = "";
            _rootBody = "";
            _rootAvatarUrl = "";
            _replyCount = 0;
            return;
        }

        // Try main timeline model first, then thread model.
        if (!_tryLoadFromModel(TimelineManager.matrixTimelineModel))
            _tryLoadFromModel(TimelineManager.matrixThreadTimelineModel);

        _updateReplyCount();
    }

    function _updateReplyCount() {
        var m = TimelineManager.matrixThreadTimelineModel;
        _replyCount = m ? Math.max(0, m.count - 1) : 0;
    }

    onThreadEventIdChanged: _refreshRootData()
    Component.onCompleted: if (active) _refreshRootData()

    // Retry until root data loads (models populate asynchronously).
    Timer {
        interval: 250
        repeat: true
        running: threadBar.active && threadBar._rootUserId.length === 0

        onTriggered: threadBar._refreshRootData()
    }

    Connections {
        target: TimelineManager.matrixThreadTimelineModel

        function onRowsInserted() {
            if (threadBar._rootUserId.length === 0)
                threadBar._refreshRootData();
            threadBar._updateReplyCount();
        }
        function onRowsRemoved() { threadBar._updateReplyCount() }
        function onModelReset() {
            threadBar._refreshRootData();
            threadBar._updateReplyCount();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.tint(palette.window,
            Qt.hsla(threadBar.threadColor.hslHue, 0.7,
                    threadBar.threadColor.hslLightness, 0.1))

        RowLayout {
            id: barRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Komai.paddingMedium
            anchors.rightMargin: Komai.paddingMedium
            anchors.verticalCenter: parent.verticalCenter
            spacing: Komai.paddingMedium

            // ── Sender avatar ──
            AvatarUserFlipButton {
                Layout.preferredWidth: Komai.iconSize
                Layout.preferredHeight: Komai.iconSize
                Layout.alignment: Qt.AlignVCenter
                avatarButtonSize: Komai.iconSize
                cleanFront: true
                avatarUserId: threadBar._rootUserId
                avatarRoomId: threadBar.roomId
                avatarUrl: threadBar._rootAvatarUrl
                avatarDisplayName: threadBar._rootUserName
                    || threadBar._rootUserId
                visible: threadBar._rootUserId.length > 0

                onLeftClicked: TimelineManager.openRoomUserProfile(
                    threadBar.roomId, threadBar._rootUserId)
            }

            // ── Two-line text ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                // Line 1: Thread by Name (MXID)
                Label {
                    id: threadTextLabel

                    Layout.fillWidth: true
                    text: {
                        if (threadBar._rootUserId.length === 0)
                            return qsTr("Thread");
                        if (threadBar._rootUserName.length === 0
                                || threadBar._rootUserName === threadBar._rootUserId)
                            return qsTr("Thread by %1")
                                .arg(threadBar._rootUserId);
                        return qsTr("Thread by %1 (%2)")
                            .arg(threadBar._rootUserName)
                            .arg(threadBar._rootUserId);
                    }
                    color: palette.text
                    font.pointSize: Settings.uiFontSizePt
                    font.bold: true
                    elide: Text.ElideRight
                }

                // Line 2: message body
                Label {
                    Layout.fillWidth: true
                    text: threadBar._rootBody.replace(/\n/g, " ").trim()
                    color: palette.text
                    font.pointSize: Settings.uiFontSizePt
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    visible: threadBar._rootBody.length > 0
                }
            }

            // ── Reply count badge ──
            Rectangle {
                id: replyBadge

                visible: threadBar._replyCount > 0

                readonly property color badgeColor: palette.text
                readonly property int badgeIconSize: Math.max(
                    16, Math.round(Settings.uiFontSizePt * 1.8))

                Layout.alignment: Qt.AlignVCenter
                implicitWidth: replyBadgeRow.implicitWidth
                    + Komai.paddingSmall * 3
                implicitHeight: replyBadgeRow.implicitHeight
                    + Komai.paddingSmall
                radius: Komai.paddingSmall
                color: Qt.rgba(
                    badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
                border.color: Qt.rgba(
                    badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
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
                        source: "image://colorimage/:/icons/icons/ui/thread.svg?"
                            + replyBadge.badgeColor
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: threadBar._replyCount.toLocaleString()
                        color: replyBadge.badgeColor
                        font.pointSize: Settings.uiFontSizePt
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton

                    onContainsMouseChanged: {
                        replyBadgeTooltip.requestedVisible = containsMouse;
                    }
                }

                KomaiToolTip {
                    id: replyBadgeTooltip

                    anchorItem: replyBadge
                    anchorX: replyBadge.width / 2
                    anchorY: 0
                    gapY: Komai.paddingMedium
                    preferBelow: false
                    text: qsTr("%n thread reply(s)", "",
                        threadBar._replyCount)
                    delay: 0
                    requestedVisible: false
                }
            }

            // ── Close button (styled like room-header Leave) ──
            AbstractButton {
                id: closeButton

                readonly property bool activeState: hovered || pressed
                    || visualFocus
                readonly property color actionTextColor: activeState
                    ? palette.brightText : palette.buttonText
                readonly property color actionLabelColor: activeState
                    ? palette.brightText : palette.text

                Layout.alignment: Qt.AlignVCenter
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                activeFocusOnTab: true
                implicitHeight: Komai.iconSize
                implicitWidth: Komai.iconSize + Komai.paddingSmall
                    + closeLabel.implicitWidth
                leftPadding: threadBar.buttonPaddingH
                rightPadding: threadBar.buttonPaddingH
                topPadding: 0
                bottomPadding: 0

                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Return
                            || event.key === Qt.Key_Enter
                            || event.key === Qt.Key_Space) {
                        closeButton.clicked();
                        event.accepted = true;
                    }
                }

                background: Rectangle {
                    radius: Komai.paddingSmall
                    color: closeButton.activeState
                        ? palette.dark : "transparent"
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: closeButton.leftPadding
                    anchors.rightMargin: closeButton.rightPadding
                    spacing: Komai.paddingSmall

                    Image {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: threadBar.closeIconSize
                        Layout.preferredHeight: threadBar.closeIconSize
                        source: "image://colorimage/:/icons/icons/ui/dismiss.svg?"
                            + closeButton.actionTextColor
                        sourceSize.width: threadBar.closeIconSize
                        sourceSize.height: threadBar.closeIconSize
                    }

                    Label {
                        id: closeLabel

                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("Close")
                        color: closeButton.actionLabelColor
                        font.bold: true
                    }
                }

                onClicked: TimelineManager.clearActiveMatrixThread()

                KomaiCursorShape {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                }

                KomaiToolTip {
                    anchorItem: closeButton
                    anchorX: closeButton.width / 2
                    anchorY: closeButton.height
                    gapY: Komai.paddingMedium
                    text: qsTr("Close this thread and show the main timeline [Escape]")
                    delay: 0
                    requestedVisible: closeButton.hovered
                }
            }
        }

        // Dashed bottom border in thread color (matches bubble outline)
        Canvas {
            id: dashedBorder

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = threadBar.threadColor;
                ctx.lineWidth = 1.5;
                ctx.setLineDash([6, 10]);
                ctx.beginPath();
                ctx.moveTo(0, height / 2);
                ctx.lineTo(width, height / 2);
                ctx.stroke();
            }

            onWidthChanged: requestPaint()

            Connections {
                target: threadBar
                function onThreadColorChanged() { dashedBorder.requestPaint() }
            }
        }
    }

    TextMetrics {
        id: threadLabelMetrics

        font: threadTextLabel.font
        text: "M"
    }
}
