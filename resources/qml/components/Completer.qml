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

    property int avatarHeight: Komai.listIconSize
    property int avatarWidth: Komai.listIconSize
    property bool bottomToTop: true
    property var completer
    property string completerName
    property string backendCompleterName: completerName
    property alias count: listView.count
    property alias currentIndex: listView.currentIndex
    property bool fullWidth: false
    property string roomId
    property int rowMargin: 0
    property int rowSpacing: Komai.paddingSmall
    readonly property int secondaryTextMaxWidth: Math.max(180, Math.ceil(Settings.uiFontSizePt * 18))
    readonly property int emptyStateMinWidth: Math.max(Math.ceil(Settings.uiFontSizePt * 22), 280)
    implicitWidth: Math.max(emptyStateMinWidth, contentColumn.implicitWidth || 0)
    implicitHeight: contentColumn.implicitHeight || 0

    signal completionClicked(string completion)
    signal completionSelected(string id)
    signal dismissed()

    function changeCompleter() {
        if (completerName) {
            var backend = backendCompleterName || completerName;
            var needsRoom = completerName !== "room" && completerName !== "roomAliases" && completerName !== "command";
            if (needsRoom && !popup.roomId) {
                completer = undefined;
                currentIndex = -1;
                return;
            }
            completer = TimelineManager.completerFor(backend, needsRoom ? popup.roomId : "");
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
        if (popup.completerName == "room") {
            var item = listView.itemAtIndex(currentIndex);
            lastCompletionWasSpace = item && item.modelData && item.modelData.isSpace;
            popup.completionSelected(item.modelData.rawroomid);
        } else if (popup.completerName == "user") {
            lastCompletionWasSpace = false;
            popup.completionSelected(listView.itemAtIndex(currentIndex).modelData.userid);
        }
    }
    // Tracks whether the last finishCompletion() was for a space room.
    property bool lastCompletionWasSpace: false
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
        id: contentColumn

        implicitWidth: Math.max(
            popup.emptyStateMinWidth,
            emptyState.implicitWidth || 0,
            listView.implicitWidth || 0,
            headerBackground.visible ? (headerRow.implicitWidth || 0) + 2 * Komai.paddingMedium : 0
        )
        spacing: 0

        // Header row (shown for completers with a heading)
        Rectangle {
            id: headerBackground

            readonly property bool hasHeader: popup.completerName === "emoji"
                || popup.completerName === "customEmoji"
                || popup.completerName === "user"
                || popup.completerName === "roomAliases"
            readonly property int headerGlyphSize: Math.max(14, Math.ceil(Settings.uiFontSizePt * 1.05), Math.round(Komai.listIconSize * 0.62))
            readonly property int headerButtonSize: headerGlyphSize + Komai.paddingSmall

            Layout.fillWidth: true
            color: palette.alternateBase
            implicitHeight: headerRow.implicitHeight + 2 * Komai.paddingSmall
            radius: Komai.paddingSmall
            visible: hasHeader

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
                    Layout.preferredWidth: headerBackground.headerGlyphSize
                    Layout.preferredHeight: headerBackground.headerGlyphSize
                    Layout.alignment: Qt.AlignVCenter
                    source: {
                        var icon;
                        if (popup.completerName === "emoji" || popup.completerName === "customEmoji")
                            icon = "smile.svg";
                        else if (popup.completerName === "user")
                            icon = "mention.svg";
                        else if (popup.completerName === "roomAliases")
                            icon = "tag.svg";
                        else
                            icon = "link.svg";
                        return "image://colorimage/:/icons/icons/ui/" + icon + "?" + palette.text;
                    }
                    sourceSize.width: headerBackground.headerGlyphSize
                    sourceSize.height: headerBackground.headerGlyphSize
                }

                Label {
                    id: headerTitle

                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: {
                        if (popup.completerName === "emoji" || popup.completerName === "customEmoji")
                            return qsTr("Pick an emoji");
                        else if (popup.completerName === "user")
                            return qsTr("Pick a user to mention");
                        else
                            return qsTr("Pick a room to link to");
                    }
                    font.bold: true
                    font.pointSize: Settings.uiFontSizePt * 1.1
                    color: palette.text
                }

                ImageButton {
                    Layout.preferredWidth: headerBackground.headerButtonSize
                    Layout.preferredHeight: headerBackground.headerButtonSize
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip.delay: Komai.tooltipDelay
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    hoverEnabled: true
                    leftPadding: Math.ceil(Komai.paddingSmall / 2)
                    rightPadding: Math.ceil(Komai.paddingSmall / 2)
                    topPadding: Math.ceil(Komai.paddingSmall / 2)
                    bottomPadding: Math.ceil(Komai.paddingSmall / 2)
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

        Item {
            id: emptyState

            Layout.fillWidth: true
            Layout.preferredHeight: Math.ceil(Settings.uiFontSizePt * 4)
            implicitWidth: noMatchesLabel.implicitWidth + 2 * Komai.paddingLarge
            visible: !!completer
                && !!completer.searchString
                && completer.searchString.length > 0
                && listView.count === 0

            Label {
                id: noMatchesLabel

                anchors.centerIn: parent
                color: palette.buttonText
                text: qsTr("No matches found.")
            }
        }

        ListView {
            id: listView

            // Track the widest delegate content to size the popup without binding loops.
            // Delegates report their content width here; the ListView uses it for implicitWidth.
            // Delegate width then binds to listView.width (from parent layout), not childrenRect.
            property real maxContentWidth: 20
            property int hoveredIndex: -1

            function syncHoverIndex() {
                if (!moving && hoveredIndex >= 0 && hoveredIndex < count)
                    popup.currentIndex = hoveredIndex;
            }

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
            visible: !emptyState.visible
            onMovingChanged: {
                if (!moving)
                    syncHoverIndex();
            }

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
                    hoverEnabled: false

                    onClicked: {
                        popup.completionClicked(completer.completionAt(model.index));
                        if (popup.completerName == "room") {
                            lastCompletionWasSpace = model.isSpace;
                            popup.completionSelected(model.roomid);
                        } else if (popup.completerName == "user") {
                            lastCompletionWasSpace = false;
                            popup.completionSelected(model.userid);
                        }
                    }
                }
                HoverHandler {
                    id: rowHoverHandler

                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

                    onHoveredChanged: {
                        if (hovered) {
                            listView.hoveredIndex = model.index;
                            listView.syncHoverIndex();
                        } else if (listView.hoveredIndex === model.index) {
                            listView.hoveredIndex = -1;
                        }
                    }
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
                            property int pickerAvatarSize: Komai.listIconSize

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Komai.paddingMedium
                            anchors.rightMargin: Komai.paddingMedium
                            spacing: Komai.paddingMedium

                            Avatar {
                                displayName: model.displayName
                                enabled: false
                                Layout.alignment: Qt.AlignTop
                                Layout.preferredHeight: parent.pickerAvatarSize
                                Layout.preferredWidth: parent.pickerAvatarSize
                                url: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                                userid: model.userid === "@room" ? "" : model.userid
                            }
                            ColumnLayout {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 1

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                    elide: Text.ElideRight
                                    font.pointSize: Settings.uiFontSizePt * 1.1
                                    text: model.displayName
                                }
                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                    elide: Text.ElideRight
                                    font.pointSize: Settings.uiFontSizePt
                                    text: model.userid === "@room" ? qsTr("Notify the whole room") : model.userid
                                    textFormat: Text.PlainText
                                }
                            }
                        }
                    }
                    DelegateChoice {
                        roleValue: "emoji"

                        RowLayout {
                            property int pickerIconSize: Komai.listIconSize

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Komai.paddingMedium
                            anchors.rightMargin: Komai.paddingMedium
                            spacing: Komai.paddingMedium

                            Label {
                                Layout.preferredWidth: parent.pickerIconSize
                                Layout.preferredHeight: parent.pickerIconSize
                                Layout.alignment: Qt.AlignTop
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                font.family: Settings.uiFontEmojiFamily
                                font.pixelSize: Math.round(parent.pickerIconSize * 0.9)
                                text: model.unicode
                                visible: !!model.unicode
                            }
                            Avatar {
                                crop: false
                                displayName: model.shortcode
                                enabled: false
                                Layout.alignment: Qt.AlignTop
                                Layout.preferredHeight: parent.pickerIconSize
                                //userid: model.shortcode
                                url: (model.url ? model.url : "").replace("mxc://", "image://MxcImage/")
                                visible: !model.unicode
                                Layout.preferredWidth: parent.pickerIconSize
                            }
                            ColumnLayout {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 1

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                    elide: Text.ElideRight
                                    font.pointSize: Settings.uiFontSizePt * 1.1
                                    text: popup.completerName === "emoji"
                                        ? (model.body || model.shortcode)
                                        : model.shortcode
                                }
                                Label {
                                    readonly property string secondaryText: popup.completerName === "emoji"
                                        ? (model.shortcode ? ":" + model.shortcode + ":" : "")
                                        : (model.body || model.packname)
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                    elide: Text.ElideRight
                                    font.pointSize: Settings.uiFontSizePt
                                    text: secondaryText
                                    textFormat: Text.PlainText
                                    visible: secondaryText.length > 0
                                }
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
                            anchors.leftMargin: Komai.paddingMedium
                            anchors.rightMargin: Komai.paddingMedium
                            spacing: Komai.paddingMedium

                            Avatar {
                                displayName: model.roomName
                                enabled: false
                                Layout.preferredHeight: popup.avatarHeight
                                Layout.preferredWidth: popup.avatarWidth
                                Layout.minimumWidth: popup.avatarWidth
                                Layout.maximumWidth: popup.avatarWidth
                                roomid: model.roomid
                                url: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                            }
                            Label {
                                Layout.fillWidth: true
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                elide: Text.ElideRight
                                font.italic: model.isTombstoned
                                font.pixelSize: popup.avatarHeight * 0.5
                                text: model.roomName
                                textFormat: Text.RichText
                            }
                            Label {
                                visible: model.isSpace
                                color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                opacity: model.index == popup.currentIndex ? 0.6 : 1.0
                                font.pixelSize: popup.avatarHeight * 0.5
                                text: qsTr("(Space)")
                            }
                        }
                    }
                    DelegateChoice {
                        roleValue: "roomAliases"

                        RowLayout {
                            property int pickerAvatarSize: Komai.listIconSize

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Komai.paddingMedium
                            anchors.rightMargin: Komai.paddingMedium
                            spacing: Komai.paddingMedium

                            Avatar {
                                displayName: model.roomName
                                enabled: false
                                Layout.alignment: Qt.AlignTop
                                Layout.preferredHeight: parent.pickerAvatarSize
                                Layout.preferredWidth: parent.pickerAvatarSize
                                roomid: model.roomid
                                url: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                            }
                            ColumnLayout {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 1

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: Komai.paddingSmall

                                    Label {
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        color: model.index == popup.currentIndex ? palette.highlightedText : palette.text
                                        elide: Text.ElideRight
                                        font.italic: model.isTombstoned
                                        font.bold: model.isSpace
                                        font.pointSize: Settings.uiFontSizePt * 1.1
                                        text: model.roomName
                                        textFormat: Text.RichText
                                    }
                                    Label {
                                        visible: model.isSpace
                                        Layout.alignment: Qt.AlignVCenter
                                        color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                        opacity: model.index == popup.currentIndex ? 0.6 : 1.0
                                        font.pointSize: Settings.uiFontSizePt
                                        text: qsTr("(Space)")
                                    }
                                }

                                Label {
                                    visible: !!model.roomAlias
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                    elide: Text.ElideRight
                                    font.pointSize: Settings.uiFontSizePt
                                    text: model.roomAlias
                                    textFormat: Text.PlainText
                                }
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
        }
    }

    onCompleterNameChanged: changeCompleter()
    onRoomIdChanged: changeCompleter()
    Component.onCompleted: changeCompleter()
}
