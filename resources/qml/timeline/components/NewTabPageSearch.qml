// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    required property var tabController
    property real searchFieldMaxWidth: 500

    readonly property int resultAvatarSize: Komai.listIconSize
    readonly property bool hasResults: searchField.text.length > 0 && resultsList.count > 0
    readonly property bool isSearching: searchField.text.length > 0
    // Match the quick switcher's font sizing.
    readonly property int searchFontPixelSize: Math.ceil(Komai.fontPixelSize * 1.4)

    spacing: 0

    property var completer: null

    // Called by the parent when the new tab page becomes visible.
    // Clears stale state and focuses the search field.
    // The completer is created lazily on first keystroke to avoid a startup race
    // where matrixJoinedRooms is not yet populated when waitingForFirstSync clears.
    function activate() {
        completer = null;
        searchField.text = "";
        resultsList.currentIndex = -1;
        // Defer focus to the next event loop iteration so the layout has finished
        // sizing the search field (its width depends on parent geometry).
        Qt.callLater(searchField.forceActiveFocus);
    }

    function selectResult() {
        if (resultsList.currentIndex < 0 || resultsList.currentIndex >= resultsList.count)
            return;
        var item = resultsList.itemAtIndex(resultsList.currentIndex);
        if (!item || !item.modelData)
            return;
        var roomId = item.modelData.rawroomid;
        if (item.modelData.isSpace)
            Communities.setCurrentFilterId("space:" + roomId);
        root.tabController.navigateFromNewTab(roomId);
    }

    KomaiTextField {
        id: searchField

        Layout.fillWidth: true
        Layout.maximumWidth: root.searchFieldMaxWidth
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Komai.paddingLarge
        font.pixelSize: root.searchFontPixelSize
        implicitHeight: Math.max(controlHeight, Math.round(font.pixelSize * 2.0))
        placeholderText: qsTr("Search your rooms & spaces...")

        Keys.onPressed: event => {
            const isBackTab = event.key === Qt.Key_Tab && (event.modifiers & Qt.ShiftModifier);
            const isTab = event.key === Qt.Key_Tab && !(event.modifiers & Qt.ShiftModifier);
            if (event.key === Qt.Key_Up || (isBackTab && resultsList.count > 0)) {
                event.accepted = true;
                resultsList.currentIndex--;
                if (resultsList.currentIndex < 0)
                    resultsList.currentIndex = resultsList.count - 1;
            } else if (event.key === Qt.Key_Down || (isTab && resultsList.count > 0)) {
                event.accepted = true;
                resultsList.currentIndex++;
                if (resultsList.currentIndex >= resultsList.count)
                    resultsList.currentIndex = 0;
            } else if (event.matches(StandardKey.InsertParagraphSeparator)) {
                event.accepted = true;
                root.selectResult();
            } else if (event.key === Qt.Key_Escape) {
                event.accepted = true;
                searchField.text = "";
                if (root.completer)
                    root.completer.searchString = "";
            }
        }
        onTextEdited: {
            if (!root.completer)
                root.completer = TimelineManager.completerFor("room", "");
            root.completer.searchString = text;
            resultsList.currentIndex = resultsList.count > 0 ? 0 : -1;
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingMedium
        horizontalAlignment: Text.AlignHCenter
        color: palette.buttonText
        text: qsTr("No matches found.")
        visible: searchField.text.length > 0 && resultsList.count === 0
    }

    ListView {
        id: resultsList

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.topMargin: Komai.paddingSmall
        clip: true
        visible: root.hasResults
        model: root.completer
        currentIndex: -1
        highlightFollowsCurrentItem: true
        pixelAligned: true

        // Track genuine mouse movement to avoid phantom hover selection.
        property bool mouseActivated: false
        property int hoveredIndex: -1

        function syncHoverIndex() {
            if (!moving && mouseActivated && hoveredIndex >= 0 && hoveredIndex < count)
                currentIndex = hoveredIndex;
        }
        onMovingChanged: {
            if (!moving)
                syncHoverIndex();
        }

        ScrollBar.vertical: ScrollBar {
            id: scrollBar

            policy: resultsList.contentHeight > resultsList.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }

        delegate: Rectangle {
            id: resultDelegate

            property variant modelData: model

            width: resultsList.width - (scrollBar.visible ? scrollBar.width : 0)
            height: resultRow.implicitHeight + 2 * Komai.paddingSmall
            color: model.index === resultsList.currentIndex ? palette.highlight : "transparent"
            radius: Komai.paddingSmall

            RowLayout {
                id: resultRow

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Komai.paddingMedium
                anchors.rightMargin: Komai.paddingMedium
                spacing: Komai.paddingMedium

                Avatar {
                    displayName: model.roomName
                    enabled: false
                    Layout.preferredHeight: root.resultAvatarSize
                    Layout.preferredWidth: root.resultAvatarSize
                    Layout.minimumWidth: root.resultAvatarSize
                    Layout.maximumWidth: root.resultAvatarSize
                    roomid: model.roomid
                    url: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                }

                Label {
                    Layout.fillWidth: true
                    color: model.index === resultsList.currentIndex ? palette.highlightedText : palette.text
                    elide: Text.ElideRight
                    font.italic: model.isTombstoned
                    font.pixelSize: root.resultAvatarSize * 0.5
                    text: model.roomName
                    textFormat: Text.RichText
                }

                Label {
                    visible: model.isSpace
                    color: model.index === resultsList.currentIndex ? palette.highlightedText : palette.buttonText
                    opacity: model.index === resultsList.currentIndex ? 0.6 : 1.0
                    font.pixelSize: root.resultAvatarSize * 0.5
                    text: qsTr("(Space)")
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: false

                onClicked: {
                    resultsList.currentIndex = model.index;
                    root.selectResult();
                }
            }

            HoverHandler {
                id: rowHoverHandler

                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                property point entryPoint: Qt.point(-1, -1)

                onHoveredChanged: {
                    if (hovered) {
                        entryPoint = point.position;
                        resultsList.hoveredIndex = model.index;
                        if (resultsList.mouseActivated)
                            resultsList.syncHoverIndex();
                    } else {
                        entryPoint = Qt.point(-1, -1);
                        if (resultsList.hoveredIndex === model.index)
                            resultsList.hoveredIndex = -1;
                    }
                }
                onPointChanged: {
                    if (!resultsList.mouseActivated && hovered && entryPoint.x >= 0) {
                        var dx = point.position.x - entryPoint.x;
                        var dy = point.position.y - entryPoint.y;
                        if (dx !== 0 || dy !== 0) {
                            resultsList.mouseActivated = true;
                            resultsList.hoveredIndex = model.index;
                            resultsList.syncHoverIndex();
                        }
                    }
                }
            }

            Ripple {
                color: Qt.rgba(palette.window.r, palette.window.g, palette.window.b, 0.5)
            }
        }
    }
}
