// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

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
    readonly property int stickerDimPad: stickerDim + Komai.paddingSmall
    readonly property int stickersPerRow: emoji ? 7 : 3
    readonly property int sidebarAvatarSize: 32
    readonly property int sidebarIconSize: 20
    readonly property int sidebarRowHeight: Math.max(36, sidebarIconSize + Komai.paddingMedium * 2)
    readonly property int sidebarPaneWidth: Math.max(132, sidebarAvatarSize + Komai.paddingMedium + 64)
    readonly property int gridColumnWidth: stickersPerRow * stickerDimPad + 20 - Komai.paddingSmall
    readonly property var sidebarPalette: timelineRoot ? timelineRoot.palette : palette
    readonly property color sidebarHoverBackground: sidebarPalette.dark
    readonly property color sidebarHoverText: sidebarPalette.brightText
    readonly property color sidebarActiveBackground: sidebarPalette.highlight
    readonly property color sidebarActiveText: sidebarPalette.highlightedText
    property int activeSectionIndex: -1
    property int activeSectionFirstRow: -1
    property string activeSectionName: ""
    property int textHeight: Math.round(Qt.application.font.pixelSize * 2.4)

    function clamp(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(value, maxValue));
    }

    function isMxcUrl(url) {
        return typeof url === "string" && url.startsWith("mxc://");
    }

    function colorizedIconSource(url, color) {
        if (typeof url !== "string" || url.length === 0)
            return "";
        if (isMxcUrl(url))
            return url.replace("mxc://", "image://MxcImage/");

        const normalized = url.startsWith("qrc:/")
            ? url.replace("qrc:/", ":/")
            : url;
        return "image://colorimage/" + normalized + "?" + color;
    }

    function sectionFirstRow(sectionData) {
        const firstRow = Number(sectionData && sectionData.firstRowWith);
        return Number.isFinite(firstRow) ? firstRow : -1;
    }

    function sectionName(sectionData) {
        if (typeof sectionData === "string")
            return sectionData;
        if (sectionData && typeof sectionData.name === "string")
            return sectionData.name;
        return "";
    }

    function sectionMetaByName(name) {
        if (!gridView.model || !gridView.model.sections || !name)
            return null;

        for (let i = 0; i < gridView.model.sections.length; ++i) {
            if (gridView.model.sections[i].name === name)
                return gridView.model.sections[i];
        }
        return null;
    }

    function updateActiveSectionIndex() {
        if (!gridView.model || !gridView.model.sections || gridView.model.sections.length === 0) {
            activeSectionIndex = -1;
            activeSectionFirstRow = -1;
            activeSectionName = "";
            return;
        }

        const probeX = Math.max(1, Math.floor(gridView.width / 2));
        let topIndex = gridView.indexAt(probeX, gridView.contentY + 1);
        if (topIndex < 0)
            topIndex = gridView.indexAt(probeX, gridView.contentY + Math.max(1, Math.floor(gridView.cellHeight / 2)));
        if (topIndex < 0)
            return;

        let resolvedIndex = 0;
        for (let i = gridView.model.sections.length - 1; i >= 0; --i) {
            if (topIndex >= sectionFirstRow(gridView.model.sections[i])) {
                resolvedIndex = i;
                break;
            }
        }

        activeSectionIndex = resolvedIndex;
        const resolvedSection = gridView.model.sections[resolvedIndex];
        activeSectionFirstRow = sectionFirstRow(resolvedSection);
        activeSectionName = sectionName(resolvedSection);
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

    padding: Komai.paddingMedium
    modal: true
    focus: true
    parent: Overlay.overlay

    Overlay.modal: Rectangle {
        color: timelineRoot.overlayBackdropColor
    }
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: Qt.callLater(updateActiveSectionIndex)
    width: sidebarPaneWidth + Komai.paddingSmall + gridColumnWidth + padding * 2
    height: contentColumn.implicitHeight

    background: Rectangle {
        color: palette.alternateBase
        radius: 8
    }

    contentItem: Column {
        id: contentColumn

        spacing: Komai.paddingSmall

        Row {
            spacing: Komai.paddingSmall
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

                ToolTip.delay: Komai.tooltipDelay
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
            height: columnView.implicitHeight + Komai.paddingSmall * 2

            GridLayout {
                id: columnView

                anchors.fill: parent
                anchors.margins: Komai.paddingSmall
            columns: 2
            rows: 2

            // Search field
            TextField {
                id: emojiSearch

                Layout.preferredWidth: gridColumnWidth
                Layout.preferredHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                Layout.alignment: Qt.AlignVCenter
                Layout.row: 0
                Layout.column: 1
                background: null
                placeholderTextColor: palette.buttonText
                placeholderText: qsTr("Search")
                selectByMouse: true
                rightPadding: clearSearch.visible
                    ? (clearSearch.width + Komai.paddingLarge + Komai.paddingSmall)
                    : Komai.paddingSmall
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

                    image: ":/icons/icons/ui/dismiss.svg"
                    focusPolicy: Qt.NoFocus
                    onClicked: emojiSearch.clear()
                    hoverEnabled: true
                    width: Math.round(emojiSearch.height * 0.68)
                    height: width
                    anchors {
                        verticalCenter: parent.verticalCenter
                        right: parent.right
                        rightMargin: Komai.paddingLarge
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
                Layout.preferredWidth: gridColumnWidth
                property int cellHeight: stickerDimPad
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                currentIndex: -1 // prevent sorting from stealing focus
                onContentYChanged: stickerPopup.updateActiveSectionIndex()
                onModelChanged: Qt.callLater(stickerPopup.updateActiveSectionIndex)

                section.property: "packname"
                section.criteria: ViewSection.FullString
                section.delegate: Rectangle {
                    required property string section
                    readonly property var sectionMeta: stickerPopup.sectionMetaByName(section)
                    readonly property real reservedScrollbarWidth: (emojiScroll.visible || emojiScroll.active)
                        ? Math.max(emojiScroll.width, emojiScroll.implicitWidth, Komai.paddingMedium)
                        : 0

                    width: Math.max(0, gridView.width - reservedScrollbarWidth - Komai.paddingSmall)
                    height: headerRow.implicitHeight + Komai.paddingSmall * 2
                    radius: Komai.paddingSmall
                    color: palette.alternateBase

                    RowLayout {
                        id: headerRow

                        anchors.fill: parent
                        anchors.leftMargin: Komai.paddingSmall
                        anchors.rightMargin: Komai.paddingSmall
                        anchors.topMargin: Komai.paddingSmall
                        anchors.bottomMargin: Komai.paddingSmall
                        spacing: Komai.paddingSmall

                        Avatar {
                            Layout.preferredHeight: sidebarIconSize
                            Layout.preferredWidth: sidebarIconSize
                            displayName: parent.parent.section
                            roomid: parent.parent.section
                            url: (parent.parent.sectionMeta && stickerPopup.isMxcUrl(parent.parent.sectionMeta.url))
                                ? stickerPopup.colorizedIconSource(parent.parent.sectionMeta.url, "")
                                : ""
                            textColor: palette.buttonText
                            visible: parent.parent.sectionMeta && stickerPopup.isMxcUrl(parent.parent.sectionMeta.url)
                            enabled: false
                        }

                        Image {
                            Layout.preferredHeight: sidebarIconSize
                            Layout.preferredWidth: sidebarIconSize
                            source: (!parent.parent.sectionMeta || stickerPopup.isMxcUrl(parent.parent.sectionMeta.url))
                                ? ""
                                : stickerPopup.colorizedIconSource(parent.parent.sectionMeta.url, palette.buttonText)
                            sourceSize.height: sidebarIconSize
                            sourceSize.width: sidebarIconSize
                            visible: parent.parent.sectionMeta && !stickerPopup.isMxcUrl(parent.parent.sectionMeta.url)
                        }

                        Label {
                            Layout.fillWidth: true
                            color: palette.text
                            elide: Text.ElideRight
                            font.bold: true
                            text: parent.parent.section
                        }
                    }
                }
                section.labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart

                spacing: Komai.paddingSmall

                // Individual emoji
                delegate: Row {
                    required property var row;

                    spacing: Komai.paddingSmall

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
                Layout.preferredWidth: sidebarPaneWidth
                Layout.fillHeight: true
                Layout.rightMargin: Komai.paddingSmall

                model: gridView.model ? gridView.model.sections : null
                boundsBehavior: Flickable.StopAtBounds
                spacing: 0
                clip: true

                delegate: AbstractButton {
                    id: categoryButton

                    required property var modelData
                    required property int index
                    readonly property string sectionName: stickerPopup.sectionName(modelData)
                    readonly property bool active: sectionName.length > 0 && sectionName === stickerPopup.activeSectionName
                    property color backgroundColor: "transparent"
                    property color textColor: stickerPopup.sidebarPalette.text

                    width: ListView.view.width
                    height: sidebarRowHeight
                    hoverEnabled: true
                    leftPadding: Komai.paddingSmall
                    rightPadding: Komai.paddingSmall
                    topPadding: Komai.paddingMedium
                    bottomPadding: Komai.paddingMedium
                    ToolTip.visible: hovered
                    ToolTip.delay: Komai.tooltipDelay
                    ToolTip.text: modelData.name

                    onClicked: {
                        stickerPopup.activeSectionIndex = index;
                        stickerPopup.activeSectionFirstRow = stickerPopup.sectionFirstRow(modelData);
                        stickerPopup.activeSectionName = sectionName;
                        gridView.positionViewAtIndex(modelData.firstRowWith, ListView.Beginning);
                        Qt.callLater(stickerPopup.updateActiveSectionIndex);
                    }

                    states: [
                        State {
                            name: "hover"
                            when: categoryButton.hovered && !categoryButton.active

                            PropertyChanges {
                                categoryButton {
                                    backgroundColor: stickerPopup.sidebarHoverBackground
                                    textColor: stickerPopup.sidebarHoverText
                                }
                            }
                        },
                        State {
                            name: "active"
                            when: categoryButton.active

                            PropertyChanges {
                                categoryButton {
                                    backgroundColor: stickerPopup.sidebarActiveBackground
                                    textColor: stickerPopup.sidebarActiveText
                                }
                            }
                        }
                    ]

                    background: Rectangle {
                        radius: Komai.paddingSmall
                        color: categoryButton.backgroundColor
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: categoryButton.leftPadding
                        anchors.rightMargin: categoryButton.rightPadding
                        anchors.topMargin: categoryButton.topPadding
                        anchors.bottomMargin: categoryButton.bottomPadding
                        spacing: Komai.paddingSmall

                        Avatar {
                            Layout.preferredHeight: sidebarIconSize
                            Layout.preferredWidth: sidebarIconSize
                            displayName: categoryButton.modelData.name
                            roomid: categoryButton.modelData.name
                            url: stickerPopup.isMxcUrl(categoryButton.modelData.url)
                                ? stickerPopup.colorizedIconSource(categoryButton.modelData.url, "")
                                : ""
                            textColor: categoryButton.textColor
                            visible: stickerPopup.isMxcUrl(categoryButton.modelData.url)
                            enabled: false
                        }

                        Image {
                            Layout.preferredHeight: sidebarIconSize
                            Layout.preferredWidth: sidebarIconSize
                            source: stickerPopup.isMxcUrl(categoryButton.modelData.url)
                                ? ""
                                : stickerPopup.colorizedIconSource(categoryButton.modelData.url, categoryButton.textColor)
                            sourceSize.height: sidebarIconSize
                            sourceSize.width: sidebarIconSize
                            visible: !stickerPopup.isMxcUrl(categoryButton.modelData.url)
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            color: categoryButton.textColor
                            elide: Text.ElideRight
                            font.bold: categoryButton.active
                            text: categoryButton.modelData.name
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            AbstractButton {
                id: settingsButton

                Layout.row: 0
                Layout.column: 0
                Layout.preferredWidth: sidebarPaneWidth
                Layout.preferredHeight: sidebarRowHeight
                Layout.rightMargin: Komai.paddingSmall
                hoverEnabled: true
                property color backgroundColor: "transparent"
                property color textColor: stickerPopup.sidebarPalette.text
                leftPadding: Komai.paddingSmall
                rightPadding: Komai.paddingSmall
                topPadding: Komai.paddingMedium
                bottomPadding: Komai.paddingMedium
                ToolTip.visible: hovered
                ToolTip.delay: Komai.tooltipDelay
                ToolTip.text: qsTr("Change what packs are enabled, remove packs, or create new ones")
                onClicked: TimelineManager.openImagePackSettings(stickerPopup.roomid)

                states: [
                    State {
                        name: "hover"
                        when: settingsButton.hovered

                        PropertyChanges {
                            settingsButton {
                                backgroundColor: stickerPopup.sidebarHoverBackground
                                textColor: stickerPopup.sidebarHoverText
                            }
                        }
                    }
                ]

                background: Rectangle {
                    radius: Komai.paddingSmall
                    color: settingsButton.backgroundColor
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: settingsButton.leftPadding
                    anchors.rightMargin: settingsButton.rightPadding
                    anchors.topMargin: settingsButton.topPadding
                    anchors.bottomMargin: settingsButton.bottomPadding
                    spacing: Komai.paddingSmall

                    Image {
                        Layout.preferredHeight: sidebarIconSize
                        Layout.preferredWidth: sidebarIconSize
                        source: stickerPopup.colorizedIconSource(":/icons/icons/ui/settings.svg", settingsButton.textColor)
                        sourceSize.height: sidebarIconSize
                        sourceSize.width: sidebarIconSize
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        color: settingsButton.textColor
                        elide: Text.ElideRight
                        text: qsTr("Settings")
                    }
                }
            }
            }

        }

    }

}
