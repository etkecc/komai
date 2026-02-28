// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import im.nheko

Popup {
    id: stickerPopup

    property var callback
    property string roomid
    property alias model: gridView.model
    required property bool emoji
    property var textArea
    property real highlightHue: palette.highlight.hslHue
    property real highlightSat: palette.highlight.hslSaturation
    property real highlightLight: palette.highlight.hslLightness
    readonly property int stickerDim: emoji ? 48 : 128
    readonly property int stickerDimPad: stickerDim + Nheko.paddingSmall
    readonly property int stickersPerRow: emoji ? 7 : 3
    readonly property int sidebarAvatarSize: 32
    property int textHeight: Math.round(Qt.application.font.pixelSize * 2.4)

    function clamp(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(value, maxValue));
    }

    function show(showAt, roomid_, callback, openAbove) {
        console.debug("Showing sticker picker");
        roomid = roomid_;
        stickerPopup.callback = callback;
        if (showAt && stickerPopup.parent) {
            const parentItem = stickerPopup.parent;
            const popupWidth = Math.max(stickerPopup.width, stickerPopup.implicitWidth);
            const popupHeight = Math.max(stickerPopup.height, stickerPopup.implicitHeight);
            const maxX = Math.max(0, parentItem.width - popupWidth);
            const maxY = Math.max(0, parentItem.height - popupHeight);

            const anchorTopLeftGlobal = showAt.mapToGlobal(0, 0);
            const anchorBottomRightGlobal = showAt.mapToGlobal(showAt.width, showAt.height);
            const anchorTopLeft = parentItem.mapFromGlobal(anchorTopLeftGlobal.x, anchorTopLeftGlobal.y);
            const anchorBottomRight = parentItem.mapFromGlobal(anchorBottomRightGlobal.x, anchorBottomRightGlobal.y);

            const preferredX = anchorBottomRight.x - popupWidth;
            const belowY = anchorBottomRight.y;

            let aboveReferenceY = anchorTopLeft.y;
            if (openAbove) {
                const aboveGlobal = openAbove.mapToGlobal(0, 0);
                const aboveLocal = parentItem.mapFromGlobal(aboveGlobal.x, aboveGlobal.y);
                aboveReferenceY = aboveLocal.y;
            }
            const aboveY = aboveReferenceY - popupHeight;

            const canOpenAbove = aboveY >= 0;
            const canOpenBelow = belowY + popupHeight <= parentItem.height;
            const visibleAreaAbove = Math.max(0, Math.min(aboveReferenceY, popupHeight));
            const visibleAreaBelow = Math.max(0, Math.min(parentItem.height - belowY, popupHeight));

            let targetY = aboveY;
            if (!canOpenAbove) {
                if (canOpenBelow)
                    targetY = belowY;
                else
                    targetY = visibleAreaAbove >= visibleAreaBelow ? aboveY : belowY;
            }

            stickerPopup.x = clamp(preferredX, 0, maxX);
            stickerPopup.y = clamp(targetY, 0, maxY);
        }
        stickerPopup.open();
    }

    padding: Nheko.paddingMedium
    modal: true
    focus: true
    parent: Overlay.overlay

    Overlay.modal: Rectangle {
        color: timelineRoot.overlayBackdropColor
    }
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: sidebarAvatarSize + Nheko.paddingSmall + stickersPerRow * stickerDimPad + 20 + padding * 2
    height: contentColumn.implicitHeight

    background: Rectangle {
        color: palette.alternateBase
        radius: 8
    }

    contentItem: Column {
        id: contentColumn

        spacing: Nheko.paddingSmall

        Row {
            spacing: Nheko.paddingSmall
            width: parent.width

            Image {
                anchors.verticalCenter: parent.verticalCenter
                height: headerLabel.font.pixelSize
                width: height
                source: "image://colorimage/:/icons/icons/ui/" + (stickerPopup.emoji ? "smile.svg" : "sticky-note-solid.svg") + "?" + palette.text
                sourceSize.height: height * Screen.devicePixelRatio
                sourceSize.width: width * Screen.devicePixelRatio
            }

            Label {
                id: headerLabel

                text: stickerPopup.emoji ? qsTr("Pick an Emoji") : qsTr("Pick a Sticker")
                color: palette.text
                font.pixelSize: Math.ceil(stickerPopup.textHeight * 0.6)
                font.bold: true
            }

            Item {
                height: 1
                width: parent.width - headerLabel.implicitWidth - headerLabel.font.pixelSize - closeButton.width - parent.spacing * 3
            }

            ImageButton {
                id: closeButton

                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.visible: hovered
                anchors.verticalCenter: parent.verticalCenter
                height: headerLabel.font.pixelSize
                width: height
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"
                onClicked: stickerPopup.close()
            }
        }

        Rectangle {
            color: palette.window
            radius: 4
            width: parent.width
            height: columnView.implicitHeight + Nheko.paddingSmall * 2

            GridLayout {
                id: columnView

                anchors.fill: parent
                anchors.margins: Nheko.paddingSmall
            columns: 2
            rows: 2

            // Search field
            TextField {
                id: emojiSearch

                Layout.preferredWidth: stickersPerRow * stickerDimPad + 20 - Nheko.paddingSmall
                Layout.row: 0
                Layout.column: 1
                background: null
                placeholderTextColor: palette.buttonText
                placeholderText: qsTr("Search")
                selectByMouse: true
                rightPadding: clearSearch.width
                onTextChanged: searchTimer.restart()
                onVisibleChanged: {
                    if (visible)
                        forceActiveFocus();
                    else
                        clear();
                }

                Timer {
                    id: searchTimer

                    interval: 350 // tweak as needed?
                    onTriggered: stickerPopup.model.searchString = emojiSearch.text
                }

                ImageButton {
                    id: clearSearch

                    visible: emojiSearch.text !== ''

                    image: ":/icons/icons/ui/round-remove-button.svg"
                    focusPolicy: Qt.NoFocus
                    onClicked: emojiSearch.clear()
                    hoverEnabled: true
                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        right: parent.right
                        rightMargin: Nheko.paddingSmall
                    }
                }
            }

            // sticker grid
            ListView {
                id: gridView

                model: roomid ? TimelineManager.completerFor(stickerPopup.emoji ? "emojigrid" : "stickergrid", roomid) : null
                Layout.row: 1
                Layout.column: 1
                Layout.preferredHeight: cellHeight * (stickersPerRow + 0.5)
                Layout.preferredWidth: stickersPerRow * stickerDimPad + 20 - Nheko.paddingSmall
                property int cellHeight: stickerDimPad
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                currentIndex: -1 // prevent sorting from stealing focus

                section.property: "packname"
                section.criteria: ViewSection.FullString
                section.delegate: Rectangle {
                    width: gridView.width
                    height: childrenRect.height
                    color: palette.alternateBase

                    required property string section

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        text: parent.section
                        font.bold: true
			            color: palette.text
                    }
                }
                section.labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart

                spacing: Nheko.paddingSmall

                // Individual emoji
                delegate: Row {
                    required property var row;

                    spacing: Nheko.paddingSmall

                    Repeater {
                        model: row

                        delegate: AbstractButton {
                            id: del

                            required property var modelData

                            width: stickerDim
                            height: stickerDim
                            hoverEnabled: true
                            ToolTip.text: ":" + modelData.shortcode + ": - " + (modelData.unicode ? modelData.unicodeName : modelData.body)
                            ToolTip.visible: hovered
                            // TODO: maybe add favorites at some point?
                            onClicked: {
                                console.debug("Picked " + modelData);
                                stickerPopup.close();
                                if (!stickerPopup.emoji) {
                                    // return descriptor to calculate sticker to send
                                    callback(modelData.descriptor);
                                } else if (modelData.unicode) {
                                    // return the emoji unicode as both plain text and markdown
                                    callback(modelData.unicode, modelData.unicode);
                                } else {
                                    // return the emoji url as plain text and a markdown link as markdown
                                    callback(modelData.url, modelData.markdown);
                                }
                            }

                            contentItem: Item {
                                Text {
                                    width: stickerDim
                                    height: stickerDim
                                    visible: del.modelData.unicode !== undefined
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.family: Settings.uiFontEmojiFamily
                                    font.pixelSize: 36
                                    text: del.modelData.unicode !== undefined ? del.modelData.unicode.replace('\ufe0f', '') : ""
                                }

                                Image {
                                    height: stickerDim
                                    width: stickerDim
                                    visible: del.modelData.unicode === undefined
                                    source: del.modelData.url ? del.modelData.url.replace("mxc://", "image://MxcImage/") + "?scale" : ""
                                    fillMode: Image.PreserveAspectFit
                                }
                            }

                            background: Rectangle {
                                anchors.fill: parent
                                color: hovered ? palette.highlight : 'transparent'
                                radius: 5
                            }

                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    id: emojiScroll
                }

            }

            ListView {
                Layout.row: 1
                Layout.column: 0
                Layout.preferredWidth: sidebarAvatarSize
                Layout.fillHeight: true
                Layout.rightMargin: Nheko.paddingSmall

                model: gridView.model ? gridView.model.sections : null
                spacing: Nheko.paddingSmall
                clip: true

                delegate: Avatar {
                    height: sidebarAvatarSize
                    width: sidebarAvatarSize
                    url: modelData.url.replace("mxc://", "image://MxcImage/")
                    textColor: modelData.url.startsWith("mxc://") ? palette.text : palette.buttonText
                    displayName: modelData.name
                    roomid: modelData.name

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: modelData.name
                    onClicked: gridView.positionViewAtIndex(modelData.firstRowWith, ListView.Beginning)
                }
            }

            ImageButton {
                Layout.row: 0
                Layout.column: 0
                Layout.preferredWidth: sidebarAvatarSize
                Layout.preferredHeight: sidebarAvatarSize
                Layout.rightMargin: Nheko.paddingSmall

                image: ":/icons/icons/ui/settings.svg"

                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: qsTr("Change what packs are enabled, remove packs, or create new ones")
                onClicked: TimelineManager.openImagePackSettings(stickerPopup.roomid)
            }
            }

        }

    }

}
