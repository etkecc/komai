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
    id: reactionDetailsRoot

    // The full list of `Reaction` value objects for the message (key,
    // userIds, count, selfReactedEvent, ...). The Repeater consumes this
    // directly — no separate request/proxy is needed because the data is
    // already on the timeline item.
    property var reactions: []
    property string eventId
    // Used to open user profiles from the user-card click.
    property var roomModel: null

    title: qsTr("Reactions")
    titleIcon: ":/icons/icons/ui/smile.svg"

    ListView {
        id: reactionList

        // Same scrollbar behaviour as RoomDirectory: WhenNeeded resolves to
        // "AlwaysOn when content overflows, AlwaysOff otherwise" so the bar
        // stays visible (instead of fading out on hover) and we can reserve
        // matching right-margin space, avoiding the bar overlaying content.
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
            ? Math.max(reactionScrollbar.width, reactionScrollbar.implicitWidth) + Komai.paddingSmall
            : 0

        Layout.fillWidth: true
        Layout.preferredHeight: 420
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        topMargin: Komai.paddingSmall
        bottomMargin: Komai.paddingSmall
        rightMargin: reservedScrollbarWidth

        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: Komai.paddingMedium
        model: reactionDetailsRoot.reactions

        ScrollBar.vertical: ScrollBar {
            id: reactionScrollbar

            policy: reactionList.scrollbarVisible
                ? ScrollBar.AlwaysOn
                : ScrollBar.AlwaysOff
        }

        delegate: Item {
            id: section

            // Initially collapsed; click the header to expand.
            property bool expanded: false

            width: ListView.view.width - ListView.view.rightMargin
            implicitHeight: sectionLayout.implicitHeight

            ColumnLayout {
                id: sectionLayout

                width: parent.width
                spacing: 0

                // ---- Section header ---------------------------------------
                // Reuses the timeline reaction-pill layout (emoji | counter)
                // — so we don't need a new translation string and the visual
                // language stays consistent. Below the header sits the
                // SettingsSection-style horizontal rule. Hover paints the row
                // with `palette.highlight` and lifts text to `brightText`,
                // matching how toggle/active rows are styled elsewhere.
                Rectangle {
                    id: header

                    Layout.fillWidth: true
                    implicitHeight: headerColumn.implicitHeight + Komai.paddingSmall * 2
                    color: headerHover.hovered ? palette.highlight : "transparent"
                    radius: Komai.paddingMedium

                    HoverHandler {
                        id: headerHover
                        blocking: false
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton
                        onClicked: section.expanded = !section.expanded
                    }

                    ColumnLayout {
                        id: headerColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Komai.paddingSmall
                        anchors.rightMargin: Komai.paddingSmall
                        spacing: 0

                        RowLayout {
                            id: headerLayout

                            Layout.fillWidth: true
                            spacing: countLabel.implicitHeight / 4

                            Text {
                                id: emojiText

                                Layout.alignment: Qt.AlignVCenter
                                color: headerHover.hovered ? palette.brightText : palette.text
                                font.family: Settings.uiFontEmojiFamily
                                font.pointSize: Settings.uiFontSizePt * 1.5
                                text: modelData && !modelData.key.startsWith("mxc://")
                                    ? modelData.displayKey
                                    : ""
                                visible: modelData && !modelData.key.startsWith("mxc://")
                            }

                            Image {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: emojiText.implicitHeight > 0
                                    ? emojiText.implicitHeight
                                    : Komai.iconSize
                                Layout.preferredHeight: emojiText.implicitHeight > 0
                                    ? emojiText.implicitHeight
                                    : Komai.iconSize
                                fillMode: Image.PreserveAspectFit
                                mipmap: true
                                source: modelData && modelData.key.startsWith("mxc://")
                                    ? (modelData.key.replace("mxc://", "image://MxcImage/") + "?scale")
                                    : ""
                                visible: modelData && modelData.key.startsWith("mxc://")
                            }

                            Rectangle {
                                id: divider

                                Layout.alignment: Qt.AlignVCenter
                                color: headerHover.hovered ? palette.brightText : palette.buttonText
                                implicitHeight: Math.floor(countLabel.implicitHeight * 1.4)
                                implicitWidth: 1
                            }

                            Label {
                                id: countLabel

                                Layout.alignment: Qt.AlignVCenter
                                color: headerHover.hovered ? palette.brightText : palette.windowText
                                font.pointSize: Settings.uiFontSizePt
                                text: modelData ? modelData.count : 0
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Image {
                                // Sized to the count label so it reads as a
                                // small toggle hint, not a heavy action icon.
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: countLabel.implicitHeight
                                Layout.preferredHeight: countLabel.implicitHeight
                                fillMode: Image.PreserveAspectFit
                                source: section.expanded
                                    ? "image://colorimage/:/icons/icons/ui/chevron-up.svg?"
                                        + (headerHover.hovered ? palette.brightText : palette.buttonText)
                                    : "image://colorimage/:/icons/icons/ui/chevron-down.svg?"
                                        + (headerHover.hovered ? palette.brightText : palette.buttonText)
                                sourceSize.width: width
                                sourceSize.height: height
                            }
                        }
                    }
                }

                // Horizontal rule — matches SettingsSection styling.
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Komai.paddingLarge

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        color: palette.buttonText
                        height: 1
                    }
                }

                // ---- Expanded body: one user card per reactor -------------
                // Cards stretch the full section width — no horizontal margin
                // — since the section header above already establishes the
                // visual grouping.
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: section.expanded ? Komai.paddingSmall : 0
                    spacing: Komai.paddingSmall
                    visible: section.expanded

                    Repeater {
                        model: section.expanded
                            ? (modelData && modelData.userIds ? modelData.userIds : [])
                            : []

                        delegate: Rectangle {
                            id: userCard

                            required property string modelData

                            readonly property string userId: userCard.modelData
                            readonly property string resolvedAvatarUrl: {
                                if (!reactionDetailsRoot.roomModel
                                        || !reactionDetailsRoot.roomModel.avatarUrl)
                                    return "";
                                const url = reactionDetailsRoot.roomModel.avatarUrl(userCard.userId);
                                return url ? url.replace("mxc://", "image://MxcImage/") : "";
                            }

                            Layout.fillWidth: true
                            implicitHeight: userCardLayout.implicitHeight + Komai.paddingSmall * 2
                            color: cardHover.hovered ? palette.dark : palette.window
                            radius: Komai.paddingMedium
                            border.color: Komai.theme.separator
                            border.width: 1

                            HoverHandler {
                                id: cardHover
                                blocking: false
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton
                                onClicked: {
                                    if (reactionDetailsRoot.roomModel
                                            && reactionDetailsRoot.roomModel.openUserProfile) {
                                        reactionDetailsRoot.roomModel.openUserProfile(userCard.userId);
                                    } else if (reactionDetailsRoot.roomModel
                                            && reactionDetailsRoot.roomModel.roomId) {
                                        TimelineManager.openRoomUserProfile(
                                            reactionDetailsRoot.roomModel.roomId,
                                            userCard.userId);
                                    } else {
                                        TimelineManager.openGlobalUserProfile(userCard.userId);
                                    }
                                }
                            }

                            RowLayout {
                                id: userCardLayout

                                anchors.fill: parent
                                anchors.margins: Komai.paddingSmall
                                spacing: Komai.paddingMedium

                                Avatar {
                                    Layout.preferredWidth: Komai.iconSize
                                    Layout.preferredHeight: Komai.iconSize
                                    Layout.alignment: Qt.AlignVCenter
                                    userid: userCard.userId
                                    url: userCard.resolvedAvatarUrl
                                    // We only have the MXID here; the UI has no
                                    // synchronous display-name lookup, so the MXID
                                    // is also used as the fallback display name.
                                    // Avatar still produces colorized initials.
                                    displayName: userCard.userId
                                    enabled: false
                                }

                                ElidedLabel {
                                    Layout.fillWidth: true
                                    fullText: userCard.userId
                                    color: cardHover.hovered ? palette.brightText : palette.text
                                    font.pointSize: Settings.uiFontSizePt
                                    elideWidth: userCard.width - Komai.paddingMedium * 3 - Komai.iconSize
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
