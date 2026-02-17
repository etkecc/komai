// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../"
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

    function show(showAt, roomid_, callback, openAbove) {
        console.debug("Showing sticker picker");
        roomid = roomid_;
        stickerPopup.callback = callback;
        if (showAt) {
            if (openAbove) {
                // Position: right-aligned with showAt, bottom edge at top of openAbove
                var aboveGlobal = openAbove.mapToGlobal(0, 0);
                var aboveLocal = stickerPopup.parent.mapFromGlobal(aboveGlobal.x, aboveGlobal.y);
                var btnGlobal = showAt.mapToGlobal(showAt.width, 0);
                var btnLocal = stickerPopup.parent.mapFromGlobal(btnGlobal.x, btnGlobal.y);
                stickerPopup.x = btnLocal.x - stickerPopup.width;
                stickerPopup.y = aboveLocal.y - stickerPopup.height;
            } else {
                // Position: right-aligned with showAt, below it
                var global = showAt.mapToGlobal(showAt.width, showAt.height);
                var local = stickerPopup.parent.mapFromGlobal(global.x, global.y);
                stickerPopup.x = local.x - stickerPopup.width;
                stickerPopup.y = local.y;
            }
        }
        stickerPopup.open();
    }

    padding: Nheko.paddingMedium
    modal: true
    focus: true
    parent: Overlay.overlay

    Overlay.modal: Rectangle {
        color: "#aa1E1E1E"
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

                            contentItem: DelegateChooser {
                                roleValue: del.modelData.unicode != undefined

                                DelegateChoice {
                                    roleValue: true

                                    Text {
                                        width: stickerDim
                                        height: stickerDim
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        font.family: Settings.emojiFont != "" ? Settings.emojiFont : undefined
                                        font.pixelSize: 36
                                        text: del.modelData.unicode.replace('\ufe0f', '')
                                    }
                                }

                                DelegateChoice {
                                    roleValue: false
                                    Image {
                                        height: stickerDim
                                        width: stickerDim
                                        source: del.modelData.url.replace("mxc://", "image://MxcImage/") + "?scale"
                                        fillMode: Image.PreserveAspectFit
                                    }
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
