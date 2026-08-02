// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai 1.0

Control {
    id: popup

    property int avatarHeight: Komai.iconSize
    property int avatarWidth: Komai.iconSize
    property bool bottomToTop: true
    property var completer
    property string completerType
    property string commandValidationMessage: ""
    property string commandValidationState: "none"
    // Default empty: changeCompleter() falls back to completerType. Avoids a
    // stale read when changeCompleter runs from onCompleterTypeChanged before
    // the completerType-bound default has re-evaluated. ForwardCompleter etc.
    // can still override this with a fixed backend name.
    property string backendModel: ""
    property alias count: listView.count
    property alias currentIndex: listView.currentIndex
    property bool fullWidth: false
    property string roomId
    property string roomAvatarUrl: ""
    property int rowMargin: 0
    property int rowSpacing: Komai.paddingSmall
    readonly property color commandFooterBorderColor: commandValidation.footerAccentVisible
        ? commandValidation.footerAccentColor
        : palette.mid
    readonly property color commandFooterColor: commandValidation.messageVisible
        ? Qt.rgba(commandValidation.footerAccentColor.r, commandValidation.footerAccentColor.g, commandValidation.footerAccentColor.b, 0.3)
        : commandValidation.successVisible
        ? Qt.rgba(commandValidation.footerAccentColor.r, commandValidation.footerAccentColor.g, commandValidation.footerAccentColor.b, 0.2)
        : palette.base
    readonly property string commandFooterText: commandValidation.footerText
    readonly property color commandFooterTextColor: commandValidation.messageVisible
        ? palette.text
        : commandValidation.successVisible
        ? commandValidation.footerAccentColor
        : palette.buttonText
    readonly property bool commandFooterVisible: completerType === "command"
    readonly property int secondaryTextMaxWidth: Math.max(180, Math.ceil(Settings.uiFontSizePt * 18))
    readonly property int emptyStateMinWidth: Math.max(Math.ceil(Settings.uiFontSizePt * 22), 280)
    implicitWidth: Math.max(emptyStateMinWidth, contentColumn.implicitWidth || 0)
    implicitHeight: contentColumn.implicitHeight || 0

    signal completionClicked(string completion)
    signal completionSelected(string id)
    signal dismissed()

    function changeCompleter() {
        listView.mouseActivated = false;
        if (completerType) {
            // user-mxid uses a sibling backend that excludes the @room
            // pseudo-user, since /ban, /kick and friends need real MXIDs.
            // Otherwise behaves like the mention picker.
            var resolvedType = completerType;
            var backend = backendModel || resolvedType;
            var needsRoom = resolvedType !== "room" && resolvedType !== "roomAliases" && resolvedType !== "command";
            if (needsRoom && !popup.roomId) {
                completer = undefined;
                currentIndex = -1;
                return;
            }
            completer = TimelineManager.completerFor(backend, needsRoom ? popup.roomId : "");
            if (completer)
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
        if (popup.completerType == "user" || popup.completerType == "user-mxid") {
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
        if (popup.completerType == "room") {
            var item = listView.itemAtIndex(currentIndex);
            lastCompletionWasSpace = item && item.modelData && item.modelData.isSpace;
            popup.completionSelected(item.modelData.rawroomid);
        } else if (popup.completerType == "user" || popup.completerType == "user-mxid") {
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

    CommandValidationPresentation {
        id: commandValidation

        selectionActive: popup.currentIndex >= 0
        validationMessage: popup.commandValidationMessage
        validationState: popup.commandValidationState
    }

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

            readonly property bool hasHeader: popup.completerType === "emoji"
                || popup.completerType === "customEmoji"
                || popup.completerType === "user"
                || popup.completerType === "user-mxid"
                || popup.completerType === "roomAliases"
                || popup.completerType === "command"
            readonly property int headerGlyphSize: Math.max(14, Math.ceil(Settings.uiFontSizePt * 1.05), Math.round(Komai.iconSize * 0.62))
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
                        if (popup.completerType === "emoji" || popup.completerType === "customEmoji")
                            icon = "smile.svg";
                        else if (popup.completerType === "user" || popup.completerType === "user-mxid")
                            icon = "mention.svg";
                        else if (popup.completerType === "roomAliases")
                            icon = "tag.svg";
                        else if (popup.completerType === "command")
                            icon = "textbox-more.svg";
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
                        if (popup.completerType === "emoji")
                            return qsTr("Pick an emoji");
                        else if (popup.completerType === "customEmoji")
                            return qsTr("Pick a custom emoji or sticker");
                        else if (popup.completerType === "user")
                            return qsTr("Pick a user to mention");
                        else if (popup.completerType === "user-mxid")
                            return qsTr("Pick a user");
                        else if (popup.completerType === "command")
                            return qsTr("Pick a command");
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
                    toolTipText: qsTr("Close")
                    toolTipVisible: hovered
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

        // Mirrors the source model so we can detect a genuinely-empty custom-emoji
        // pool independently of completer.searchString — that property's update
        // arrives via a QueuedConnection and lags QML bindings by one keystroke.
        Instantiator {
            id: customEmojiSourceCounter

            active: popup.completerType === "customEmoji" && !!completer
            model: active ? completer.sourceModel : null
            delegate: QtObject {}
        }

        Item {
            id: emptyState

            readonly property bool isCustomEmojiEmpty: popup.completerType === "customEmoji"
                && !!completer
                && customEmojiSourceCounter.count === 0
            readonly property bool isNoMatch: !isCustomEmojiEmpty
                && !!completer
                && !!completer.searchString
                && completer.searchString.length > 0
                && listView.count === 0

            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(
                Math.ceil(Settings.uiFontSizePt * 4),
                emptyContent.implicitHeight + 2 * Komai.paddingLarge)
            implicitWidth: emptyContent.implicitWidth + 2 * Komai.paddingLarge
            visible: isCustomEmojiEmpty || isNoMatch

            ColumnLayout {
                id: emptyContent

                anchors.centerIn: parent
                width: parent.width - 2 * Komai.paddingLarge
                spacing: Komai.paddingSmall

                Label {
                    Layout.fillWidth: true
                    color: palette.buttonText
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: emptyState.isCustomEmojiEmpty
                        ? qsTr("No custom emojis defined yet.")
                        : qsTr("No matches found.")
                }

                Label {
                    Layout.fillWidth: true
                    color: palette.buttonText
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("See Room Settings -> Sticker & Emote Settings.")
                    visible: emptyState.isCustomEmojiEmpty
                }
            }
        }

        ListView {
            id: listView

            // Track the widest delegate content to size the popup without binding loops.
            // Delegates report their content width here; the ListView uses it for implicitWidth.
            // Delegate width then binds to listView.width (from parent layout), not childrenRect.
            property real maxContentWidth: 20
            property int hoveredIndex: -1
            // True once the mouse has genuinely moved after the popup
            // appeared — prevents a stationary cursor from selecting
            // whatever item happens to be underneath it.
            property bool mouseActivated: false

            function syncHoverIndex() {
                if (!moving && mouseActivated && hoveredIndex >= 0 && hoveredIndex < count)
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
            onCountChanged: {
                // Preselect the top emoji match so Enter inserts it right
                // away, without an extra arrow-key press first. Scoped to
                // "emoji" -- the mention/room/command pickers intentionally
                // require an explicit selection so a partially typed name
                // doesn't autocomplete to the wrong person/room/command.
                if (popup.completerType === "emoji")
                    popup.currentIndex = count > 0 ? 0 : -1;
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
                        if (popup.completerType == "room") {
                            lastCompletionWasSpace = model.isSpace;
                            popup.completionSelected(model.roomid);
                        } else if (popup.completerType == "user" || popup.completerType == "user-mxid") {
                            lastCompletionWasSpace = false;
                            popup.completionSelected(model.userid);
                        }
                    }
                }
                HoverHandler {
                    id: rowHoverHandler

                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

                    // Position recorded when hover starts; compared against
                    // subsequent point updates to detect genuine movement.
                    property point entryPoint: Qt.point(-1, -1)

                    onHoveredChanged: {
                        if (hovered) {
                            entryPoint = point.position;
                            listView.hoveredIndex = model.index;
                            if (listView.mouseActivated)
                                listView.syncHoverIndex();
                        } else {
                            entryPoint = Qt.point(-1, -1);
                            if (listView.hoveredIndex === model.index)
                                listView.hoveredIndex = -1;
                        }
                    }
                    onPointChanged: {
                        if (!listView.mouseActivated && hovered && entryPoint.x >= 0) {
                            var dx = point.position.x - entryPoint.x;
                            var dy = point.position.y - entryPoint.y;
                            if (dx !== 0 || dy !== 0) {
                                listView.mouseActivated = true;
                                listView.hoveredIndex = model.index;
                                listView.syncHoverIndex();
                            }
                        }
                    }
                }
                DelegateChooser {
                    id: chooser

                    anchors.fill: parent
                    anchors.margins: popup.rowMargin
                    roleValue: popup.completerType === "customEmoji"
                        ? "emoji"
                        : popup.completerType === "user-mxid"
                            ? "user"
                            : popup.completerType

                    DelegateChoice {
                        roleValue: "user"

                        RowLayout {
                            property int pickerAvatarSize: Komai.iconSize

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Komai.paddingMedium
                            anchors.rightMargin: Komai.paddingMedium
                            spacing: Komai.paddingMedium

                            Avatar {
                                readonly property bool isRoomMention: model.userid === "@room"
                                readonly property string effectiveAvatarUrl: isRoomMention
                                    ? popup.roomAvatarUrl
                                    : model.avatarUrl

                                displayName: model.displayName
                                enabled: false
                                Layout.alignment: Qt.AlignTop
                                Layout.preferredHeight: parent.pickerAvatarSize
                                Layout.preferredWidth: parent.pickerAvatarSize
                                url: effectiveAvatarUrl.replace("mxc://", "image://MxcImage/")
                                userid: isRoomMention ? "" : model.userid
                                roomid: isRoomMention ? popup.roomId : ""
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
                            property int pickerIconSize: Komai.iconSize

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
                                    text: popup.completerType === "emoji"
                                        ? (model.body || model.shortcode)
                                        : model.shortcode
                                }
                                Label {
                                    readonly property string secondaryText: popup.completerType === "emoji"
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
                            anchors.leftMargin: Komai.paddingMedium
                            anchors.rightMargin: Komai.paddingMedium
                            spacing: Komai.paddingMedium

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
                                    font.bold: true
                                    text: model.name
                                }
                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    color: model.index == popup.currentIndex ? palette.highlightedText : palette.buttonText
                                    elide: Text.ElideRight
                                    font.pointSize: Settings.uiFontSizePt
                                    text: model.description
                                    textFormat: Text.PlainText
                                }
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
                            property int pickerAvatarSize: Komai.iconSize

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

        Rectangle {
            id: commandFooter

            Layout.fillWidth: true
            color: popup.commandFooterColor
            implicitHeight: commandFooterLabel.implicitHeight + 2 * Komai.paddingSmall
            visible: popup.commandFooterVisible

            Label {
                id: commandFooterLabel

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Komai.paddingSmall
                anchors.rightMargin: Komai.paddingSmall
                color: popup.commandFooterTextColor
                elide: Text.ElideRight
                text: popup.commandFooterText
                textFormat: Text.PlainText
                wrapMode: Text.NoWrap
            }
        }
    }

    onCompleterTypeChanged: changeCompleter()
    onRoomIdChanged: changeCompleter()
    Component.onCompleted: changeCompleter()
}
