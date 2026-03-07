// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import cc.etke.komai 1.0

Control {
    id: popup

    property int avatarHeight: 24
    property int avatarWidth: 24
    property bool bottomToTop: true
    property bool centerRowContent: true
    property var completer
    property string completerName
    property alias count: listView.count
    property alias currentIndex: listView.currentIndex
    property bool fullWidth: false
    property string roomId
    property int rowMargin: 0
    property int rowSpacing: Komai.paddingSmall

    signal completionClicked(string completion)
    signal completionSelected(string id)
    signal dismissed()

    function changeCompleter() {
        if (completerName) {
            var needsRoom = completerName !== "room" && completerName !== "roomAliases" && completerName !== "command";
            if (needsRoom && !popup.roomId) {
                completer = undefined;
                currentIndex = -1;
                return;
            }
            completer = TimelineManager.completerFor(completerName, needsRoom ? popup.roomId : "");
            completer.setSearchString("");
        } else {
            completer = undefined;
        }
        currentIndex = -1;
        listView.maxContentWidth = 20;
    }
    function currentCompletion() {
        if (currentIndex > -1 && currentIndex < listView.count)
            return completer.completionAt(currentIndex);
        else
            return null;
    }
    function currentUserid() {
        if (popup.completerName == "user") {
            return listView.itemAtIndex(currentIndex).modelData.userid;
        } else {
            return "";
        }
    }
    function down() {
        if (bottomToTop)
            up_();
        else
            down_();
    }
    function down_() {
        currentIndex = currentIndex + 1;
        if (currentIndex >= listView.count)
            currentIndex = -1;
    }
    function finishCompletion() {
        if (popup.completerName == "room")
            popup.completionSelected(listView.itemAtIndex(currentIndex).modelData.rawroomid);
        else if (popup.completerName == "user")
            popup.completionSelected(listView.itemAtIndex(currentIndex).modelData.userid);
    }
    function up() {
        if (bottomToTop)
            down_();
        else
            up_();
    }
    function up_() {
        currentIndex = currentIndex - 1;
        if (currentIndex == -2)
            currentIndex = listView.count - 1;
    }

    bottomPadding: 1
    leftPadding: 1

    // Workaround palettes not inheriting for popups
    palette: timelineRoot.palette
    rightPadding: 1
    topPadding: 1

    background: Rectangle {
        border.color: palette.mid
        color: palette.window
        radius: Komai.paddingSmall
    }
    contentItem: ColumnLayout {
        spacing: 0

        // Header row (shown for emoji/customEmoji completers)
        Rectangle {
            id: headerBackground

            Layout.fillWidth: true
            color: palette.alternateBase
            implicitHeight: headerRow.implicitHeight + 2 * Komai.paddingSmall
            radius: Komai.paddingSmall
            visible: popup.completerName === "emoji" || popup.completerName === "customEmoji"

            // Square off bottom corners by overlaying a rect at the bottom
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.radius
                color: parent.color
            }

            RowLayout {
                id: headerRow

                anchors.fill: parent
                anchors.leftMargin: Komai.paddingMedium
                anchors.rightMargin: Komai.paddingSmall
                anchors.topMargin: Komai.paddingSmall
                anchors.bottomMargin: Komai.paddingSmall
                spacing: Komai.paddingSmall

                Image {
                    Layout.preferredWidth: headerTitle.font.pixelSize
                    Layout.preferredHeight: headerTitle.font.pixelSize
                    Layout.alignment: Qt.AlignVCenter
                    source: "image://colorimage/:/icons/icons/ui/smile.svg?" + palette.text
                    sourceSize.width: headerTitle.font.pixelSize
                    sourceSize.height: headerTitle.font.pixelSize
                }

                Label {
                    id: headerTitle

                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Emojis")
                    font.bold: true
                    color: palette.text
                }

                ImageButton {
                    Layout.preferredWidth: headerTitle.font.pixelSize
                    Layout.preferredHeight: headerTitle.font.pixelSize
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip.delay: Komai.tooltipDelay
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    hoverEnabled: true
                    image: ":/icons/icons/ui/dismiss.svg"
                    onClicked: popup.dismissed()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Komai.theme.separator
            visible: headerBackground.visible
        }

        ListView {
            id: listView

            // Track the widest delegate content to size the popup without binding loops.
            // Delegates report their content width here; the ListView uses it for implicitWidth.
            // Delegate width then binds to listView.width (from parent layout), not childrenRect.
            property real maxContentWidth: 20

            Layout.fillWidth: true
            clip: true
            displayMarginBeginning: height / 2
            displayMarginEnd: height / 2
            highlightFollowsCurrentItem: true

            implicitHeight: Math.min(contentHeight, Window.height / 2)
            implicitWidth: maxContentWidth + (scrollBar.visible ? scrollBar.width : 0)

            // Broken, see https://bugreports.qt.io/browse/QTBUG-102811
            //reuseItems: true
            model: completer
            pixelAligned: true
            spacing: rowSpacing
            verticalLayoutDirection: popup.bottomToTop ? ListView.BottomToTop : ListView.TopToBottom

            ScrollBar.vertical: ScrollBar {
                id: scrollBar

                policy: listView.contentHeight > listView.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            delegate: Rectangle {
                property variant modelData: model
                property real contentWidth: (chooser.child ? chooser.child.implicitWidth : 0) + 4 + 2 * Komai.paddingSmall

                ListView.delayRemove: true
                color: model.index == popup.currentIndex ? palette.highlight : palette.window
                height: (chooser.child?.implicitHeight ?? 0) + 2 * popup.rowMargin
                width: listView.width - (scrollBar.visible ? scrollBar.width : 0)

                onContentWidthChanged: {
                    if (contentWidth > listView.maxContentWidth)
                        listView.maxContentWidth = contentWidth;
                }
                Component.onCompleted: {
                    if (contentWidth > listView.maxContentWidth)
                        listView.maxContentWidth = contentWidth;
                }

                MouseArea {
                    id: mouseArea

                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: {
                        popup.completionClicked(completer.completionAt(model.index));
                        if (popup.completerName == "room")
                            popup.completionSelected(model.roomid);
                        else if (popup.completerName == "user")
                            popup.completionSelected(model.userid);
                    }
                    onPositionChanged: if (!listView.moving && !deadTimer.running)
                        popup.currentIndex = model.index
                }
                Ripple {
                    color: Qt.rgba(palette.window.r, palette.window.g, palette.window.b, 0.5)
                }
                DelegateChooser {
                    id: chooser

                    anchors.fill: parent
                    anchors.margins: popup.rowMargin
                    enabled: false
                    roleValue: popup.completerName

                    DelegateChoice {
                        roleValue: "user"

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Komai.paddingSmall
                            anchors.rightMargin: Komai.paddingSmall
                            spacing: Komai.paddingSmall

                            Avatar {
                                displayName: model.displayName
                                enabled: false
                                Layout.preferredHeight: popup.avatarHeight
                                Layout.preferredWidth: popup.avatarWidth
                                url: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                                userid: model.userid
                            }
                            Label {
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                text: model.displayName
                            }
                            Label {
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                text: "(" + model.userid + ")"
                            }
                        }
                    }
                    DelegateChoice {
                        roleValue: "emoji"

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Komai.paddingSmall
                            anchors.rightMargin: Komai.paddingSmall
                            spacing: Komai.paddingSmall

                            Label {
                                Layout.preferredWidth: Math.ceil(font.pixelSize * 1.5)
                                Layout.alignment: Qt.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                font.family: Settings.uiFontEmojiFamily
                                font.pointSize: Settings.uiFontSizePt * 2.2
                                text: model.unicode
                                visible: !!model.unicode
                            }
                            Avatar {
                                crop: false
                                displayName: model.shortcode
                                enabled: false
                                Layout.preferredHeight: Math.ceil(Settings.uiFontSizePt * 3)
                                //userid: model.shortcode
                                url: (model.url ? model.url : "").replace("mxc://", "image://MxcImage/")
                                visible: !model.unicode
                                Layout.preferredWidth: Layout.preferredHeight
                            }
                            Label {
                                Layout.leftMargin: Komai.paddingSmall
                                Layout.alignment: Qt.AlignVCenter
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                text: model.shortcode
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Label {
                                Layout.alignment: Qt.AlignVCenter
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                text: model.packname
                            }
                        }
                    }
                    DelegateChoice {
                        roleValue: "command"

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Komai.paddingSmall
                            anchors.rightMargin: Komai.paddingSmall
                            spacing: Komai.paddingSmall

                            Label {
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                font.bold: true
                                text: model.name
                            }
                            Label {
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                text: model.description
                            }
                        }
                    }
                    DelegateChoice {
                        roleValue: "room"

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Komai.paddingSmall
                            anchors.rightMargin: Komai.paddingSmall
                            spacing: Komai.paddingSmall

                            Avatar {
                                displayName: model.roomName
                                enabled: false
                                Layout.preferredHeight: popup.avatarHeight
                                roomid: model.roomid
                                url: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                                Layout.preferredWidth: popup.avatarWidth
                            }
                            Label {
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                font.italic: model.isTombstoned
                                font.bold: model.isSpace
                                font.pixelSize: popup.avatarHeight * 0.5
                                text: model.roomName
                                textFormat: Text.RichText
                            }
                        }
                    }
                    DelegateChoice {
                        roleValue: "roomAliases"

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Komai.paddingSmall
                            anchors.rightMargin: Komai.paddingSmall
                            spacing: Komai.paddingSmall

                            Avatar {
                                displayName: model.roomName
                                enabled: false
                                Layout.preferredHeight: popup.avatarHeight
                                roomid: model.roomid
                                url: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                                Layout.preferredWidth: popup.avatarWidth
                            }
                            Label {
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                font.italic: model.isTombstoned
                                font.bold: model.isSpace
                                text: model.roomName
                                textFormat: Text.RichText
                            }
                            Label {
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                text: "(" + model.roomAlias + ")"
                                textFormat: Text.RichText
                            }
                        }
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    color: Komai.theme.separator
                    height: 1
                    visible: model.index < listView.count - 1
                }
            }

            onContentYChanged: deadTimer.restart()

            Timer {
                id: deadTimer

                interval: 50
            }
        }
    }

    onCompleterNameChanged: changeCompleter()
    onRoomIdChanged: changeCompleter()
    Component.onCompleted: changeCompleter()
}
