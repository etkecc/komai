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
    readonly property int sidebarPaneWidth: {
        // Read font height to track font size changes in this binding
        var _d = sidebarCategoryFontMetrics.height;
        var sections = gridView.model ? gridView.model.sections : null;
        var maxWidth = sidebarCategoryFontMetrics.advanceWidth(qsTr("Settings"));
        if (sections) {
            for (var i = 0; i < sections.length; ++i) {
                var name = stickerPopup.sectionName(sections[i]);
                if (name)
                    maxWidth = Math.max(maxWidth, sidebarCategoryFontMetrics.advanceWidth(name));
            }
        }
        return Math.max(132, Math.ceil(Komai.paddingSmall + sidebarIconSize + Komai.paddingSmall + maxWidth + Komai.paddingMedium));
    }
    readonly property int gridColumnWidth: stickersPerRow * stickerDimPad + 20 - Komai.paddingSmall
    readonly property var sidebarPalette: timelineRoot ? timelineRoot.palette : palette
    readonly property color sidebarHoverBackground: Qt.rgba(sidebarPalette.dark.r * 0.30 + sidebarPalette.window.r * 0.70, sidebarPalette.dark.g * 0.30 + sidebarPalette.window.g * 0.70, sidebarPalette.dark.b * 0.30 + sidebarPalette.window.b * 0.70, 1)
    readonly property color sidebarHoverText: sidebarPalette.text
    readonly property color sidebarActiveBackground: Qt.rgba(sidebarPalette.dark.r * 0.85 + sidebarPalette.window.r * 0.15, sidebarPalette.dark.g * 0.85 + sidebarPalette.window.g * 0.15, sidebarPalette.dark.b * 0.85 + sidebarPalette.window.b * 0.15, 1)
    readonly property color sidebarActiveText: sidebarPalette.brightText
    property int activeSectionIndex: -1
    property int activeSectionFirstRow: -1
    property string activeSectionName: ""
    property int focusedColumn: 0
    property int focusedRowLength: 0
    property bool pendingGoToTopRequest: false
    property int textHeight: Math.round(Komai.fontPixelSize * 2.4)
    readonly property bool darkPopupChrome: palette.window.hslLightness < 0.5
    readonly property color popupOutlineColor: Qt.tint(
        palette.mid,
        Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, darkPopupChrome ? 0.22 : 0.32))

    FontMetrics {
        id: sidebarCategoryFontMetrics

        font.bold: true
        font.pointSize: Settings.uiFontSizePt
    }

    function clamp(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(value, maxValue));
    }

    function eventMatchesLatinKey(event, latinKey) {
        if (!event)
            return false;

        return LayoutAgnosticKeys.matchesLatinKey(latinKey,
                                                  event.key,
                                                  event.nativeScanCode);
    }

    function eventUsesNoModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier)) === 0;
    }

    function eventUsesNavigationModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
    }

    function eventUsesCtrlOnlyModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & Qt.ControlModifier) !== 0
            && (modifiers & (Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier)) === 0;
    }

    function eventUsesShiftOnlyModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & Qt.ShiftModifier) !== 0
            && (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
    }

    function isForwardTabEvent(event) {
        if (!event)
            return false;
        const modifiers = Number(event.modifiers);
        if ((modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier)) !== 0)
            return false;
        return event.key === Qt.Key_Tab;
    }

    function isBackwardTabEvent(event) {
        if (!event)
            return false;
        const modifiers = Number(event.modifiers);
        if ((modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) !== 0)
            return false;
        return event.key === Qt.Key_Backtab
            || (event.key === Qt.Key_Tab && (modifiers & Qt.ShiftModifier) !== 0);
    }

    function resetGoToTopSequence() {
        pendingGoToTopRequest = false;
        goToTopSequenceTimer.stop();
    }

    function resetKeyboardCursor() {
        focusedColumn = 0;
        focusedRowLength = 0;
        if (gridView)
            gridView.currentIndex = -1;
    }

    function enterGridFromSearch() {
        if (!gridView || gridView.count <= 0)
            return false;
        if (gridView.currentIndex < 0)
            gridView.currentIndex = 0;
        focusedColumn = clamp(focusedColumn, 0, Math.max(0, focusedRowLength - 1));
        gridView.positionViewAtIndex(gridView.currentIndex, ListView.Contain);
        gridView.forceActiveFocus();
        return true;
    }

    function moveGridCursorVertical(delta) {
        if (!gridView || gridView.count <= 0)
            return;
        const next = clamp(gridView.currentIndex + delta, 0, gridView.count - 1);
        if (next === gridView.currentIndex)
            return;
        gridView.currentIndex = next;
        gridView.positionViewAtIndex(next, ListView.Contain);
    }

    function moveGridCursorByChunk(delta) {
        if (!gridView || gridView.count <= 0 || gridView.cellHeight <= 0)
            return;
        const visibleRows = Math.max(1, Math.floor(gridView.height / gridView.cellHeight));
        const chunk = Math.max(1, Math.floor(visibleRows / 2));
        moveGridCursorVertical(delta * chunk);
    }

    function moveGridCursorHorizontal(delta) {
        if (!gridView || gridView.count <= 0 || focusedRowLength <= 0)
            return;
        const next = clamp(focusedColumn + delta, 0, focusedRowLength - 1);
        focusedColumn = next;
    }

    function gridGoToFirstRow() {
        if (!gridView || gridView.count <= 0)
            return;
        gridView.currentIndex = 0;
        gridView.positionViewAtIndex(0, ListView.Beginning);
    }

    function gridGoToLastRow() {
        if (!gridView || gridView.count <= 0)
            return;
        gridView.currentIndex = gridView.count - 1;
        gridView.positionViewAtIndex(gridView.currentIndex, ListView.End);
    }

    function pickItem(modelData) {
        if (!modelData || typeof callback !== "function")
            return;
        stickerPopup.close();
        if (!stickerPopup.emoji) {
            callback.call(null, modelData.descriptor);
        } else if (modelData.unicode) {
            callback.call(null, modelData.unicode, modelData.unicode);
        } else {
            callback.call(null, modelData.url, modelData.markdown);
        }
    }

    function activateCategoryIndex(idx) {
        if (!gridView.model || !gridView.model.sections)
            return false;
        const sections = gridView.model.sections;
        if (idx < 0 || idx >= sections.length)
            return false;
        const section = sections[idx];
        activeSectionIndex = idx;
        activeSectionFirstRow = sectionFirstRow(section);
        activeSectionName = sectionName(section);
        gridView.positionViewAtIndex(section.firstRowWith, ListView.Beginning);
        Qt.callLater(updateActiveSectionIndex);
        return true;
    }

    function activateFocusedCell() {
        if (!gridView || gridView.currentIndex < 0)
            return false;
        const rowItem = gridView.itemAtIndex(gridView.currentIndex);
        if (!rowItem || !rowItem.row)
            return false;
        const col = clamp(focusedColumn, 0, rowItem.row.length - 1);
        const data = rowItem.row[col];
        if (!data)
            return false;
        pickItem(data);
        return true;
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
        if (stickerPopup.parent) {
            const parentItem = stickerPopup.parent;
            const popupWidth = Math.max(stickerPopup.width, stickerPopup.implicitWidth);
            const popupHeight = Math.max(stickerPopup.height, stickerPopup.implicitHeight);
            const maxX = Math.max(0, parentItem.width - popupWidth);
            const maxY = Math.max(0, parentItem.height - popupHeight);

            if (showAt && typeof showAt.mapToGlobal === "function") {
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
            } else {
                // Fallback: center in parent rather than leaving the popup at
                // (0, 0) in the top-left when no anchor is provided.
                stickerPopup.x = clamp((parentItem.width - popupWidth) / 2, 0, maxX);
                stickerPopup.y = clamp((parentItem.height - popupHeight) / 2, 0, maxY);
            }
        }
        stickerPopup.open();
    }

    padding: Komai.paddingMedium
    modal: true
    focus: true

    // Workaround palettes not inheriting for popups
    palette: timelineRoot.palette
    parent: Overlay.overlay

    Overlay.modal: Rectangle {
        color: timelineRoot.overlayBackdropColor
    }
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: {
        resetKeyboardCursor();
        Qt.callLater(updateActiveSectionIndex);
    }
    onClosed: resetGoToTopSequence()

    Timer {
        id: goToTopSequenceTimer

        interval: 500
        repeat: false
        onTriggered: stickerPopup.pendingGoToTopRequest = false
    }
    width: sidebarPaneWidth + Komai.paddingSmall + gridColumnWidth + padding * 2
    height: contentColumn.implicitHeight + topPadding + bottomPadding

    background: Rectangle {
        color: palette.alternateBase
        radius: 8
        border.color: stickerPopup.popupOutlineColor
        border.width: 2
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

                toolTipText: qsTr("Close")
                toolTipVisible: hovered
                anchors.verticalCenter: parent.verticalCenter
                height: headerLabel.font.pixelSize
                width: height
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"
                activeFocusOnTab: false
                onClicked: stickerPopup.close()

                Keys.onShortcutOverride: event => {
                    if (stickerPopup.isForwardTabEvent(event) || stickerPopup.isBackwardTabEvent(event))
                        event.accepted = true;
                }
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: event => {
                    if (stickerPopup.isForwardTabEvent(event)) {
                        emojiSearch.forceActiveFocus();
                        event.accepted = true;
                        return;
                    }
                    if (stickerPopup.isBackwardTabEvent(event)) {
                        settingsButton.forceActiveFocus();
                        event.accepted = true;
                        return;
                    }
                }
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
            KomaiTextField {
                id: emojiSearch

                Layout.preferredWidth: gridColumnWidth
                Layout.preferredHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                Layout.alignment: Qt.AlignVCenter
                Layout.row: 0
                Layout.column: 1
                background: null
                placeholderText: qsTr("Search")
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

                Keys.onShortcutOverride: event => {
                    if (stickerPopup.isForwardTabEvent(event) || stickerPopup.isBackwardTabEvent(event))
                        event.accepted = true;
                }
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: event => {
                    if (stickerPopup.isForwardTabEvent(event)) {
                        stickerPopup.enterGridFromSearch();
                        event.accepted = true;
                        return;
                    }
                    if (stickerPopup.isBackwardTabEvent(event)) {
                        closeButton.forceActiveFocus();
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Down && stickerPopup.eventUsesNoModifiers(event)) {
                        if (stickerPopup.enterGridFromSearch())
                            event.accepted = true;
                    }
                }

                Timer {
                    id: searchTimer

                    interval: 350 // tweak as needed?
                    onTriggered: {
                        stickerPopup.model.searchString = emojiSearch.text;
                        stickerPopup.resetKeyboardCursor();
                    }
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
                activeFocusOnTab: false
                onContentYChanged: stickerPopup.updateActiveSectionIndex()
                onModelChanged: {
                    stickerPopup.resetKeyboardCursor();
                    Qt.callLater(stickerPopup.updateActiveSectionIndex);
                }

                Keys.onShortcutOverride: event => {
                    if (stickerPopup.isForwardTabEvent(event) || stickerPopup.isBackwardTabEvent(event))
                        event.accepted = true;
                }
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: event => {
                    const gKeyPressed = stickerPopup.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.G);
                    const plainGPressed = gKeyPressed && stickerPopup.eventUsesNoModifiers(event);
                    const shiftGPressed = gKeyPressed && stickerPopup.eventUsesShiftOnlyModifiers(event);

                    if (stickerPopup.isForwardTabEvent(event)) {
                        stickerPopup.resetGoToTopSequence();
                        categoriesListView.focusFromNeighbor();
                        event.accepted = true;
                        return;
                    }
                    if (stickerPopup.isBackwardTabEvent(event)) {
                        stickerPopup.resetGoToTopSequence();
                        emojiSearch.forceActiveFocus();
                        event.accepted = true;
                        return;
                    }

                    if (!plainGPressed)
                        stickerPopup.resetGoToTopSequence();

                    if ((event.key === Qt.Key_Up
                                || stickerPopup.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.K))
                            && stickerPopup.eventUsesNavigationModifiers(event)) {
                        stickerPopup.moveGridCursorVertical(-1);
                        event.accepted = true;
                        return;
                    }

                    if ((event.key === Qt.Key_Down
                                || stickerPopup.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.J))
                            && stickerPopup.eventUsesNavigationModifiers(event)) {
                        stickerPopup.moveGridCursorVertical(1);
                        event.accepted = true;
                        return;
                    }

                    if (event.key === Qt.Key_Left && stickerPopup.eventUsesNavigationModifiers(event)) {
                        stickerPopup.moveGridCursorHorizontal(-1);
                        event.accepted = true;
                        return;
                    }

                    if (event.key === Qt.Key_Right && stickerPopup.eventUsesNavigationModifiers(event)) {
                        stickerPopup.moveGridCursorHorizontal(1);
                        event.accepted = true;
                        return;
                    }

                    if (stickerPopup.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.U)
                            && stickerPopup.eventUsesCtrlOnlyModifiers(event)) {
                        stickerPopup.moveGridCursorByChunk(-1);
                        event.accepted = true;
                        return;
                    }

                    if (stickerPopup.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.D)
                            && stickerPopup.eventUsesCtrlOnlyModifiers(event)) {
                        stickerPopup.moveGridCursorByChunk(1);
                        event.accepted = true;
                        return;
                    }

                    if (shiftGPressed) {
                        stickerPopup.gridGoToLastRow();
                        event.accepted = true;
                        return;
                    }

                    if (plainGPressed) {
                        if (stickerPopup.pendingGoToTopRequest) {
                            stickerPopup.resetGoToTopSequence();
                            stickerPopup.gridGoToFirstRow();
                        } else {
                            stickerPopup.pendingGoToTopRequest = true;
                            goToTopSequenceTimer.restart();
                        }
                        event.accepted = true;
                        return;
                    }

                    switch (event.key) {
                    case Qt.Key_Home:
                        stickerPopup.gridGoToFirstRow();
                        event.accepted = true;
                        return;
                    case Qt.Key_End:
                        stickerPopup.gridGoToLastRow();
                        event.accepted = true;
                        return;
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                        if (stickerPopup.activateFocusedCell())
                            event.accepted = true;
                        return;
                    }

                    // Any other printable character routes back to the search field.
                    if (event.text && event.text.length > 0
                            && event.text.charCodeAt(0) >= 0x20
                            && !(Number(event.modifiers) & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
                        emojiSearch.forceActiveFocus();
                        emojiSearch.insert(emojiSearch.cursorPosition, event.text);
                        event.accepted = true;
                    }
                }

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
                            font.pointSize: Settings.uiFontSizePt
                            text: parent.parent.section
                        }
                    }
                }
                section.labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart

                spacing: Komai.paddingSmall

                // Individual emoji
                delegate: Row {
                    id: rowDelegate

                    required property var row;
                    required property int index

                    readonly property int rowLength: (row && row.length !== undefined) ? row.length : 0
                    readonly property bool isCurrentRow: index === gridView.currentIndex

                    spacing: Komai.paddingSmall

                    onIsCurrentRowChanged: {
                        if (isCurrentRow) {
                            stickerPopup.focusedRowLength = rowLength;
                            if (stickerPopup.focusedColumn >= rowLength)
                                stickerPopup.focusedColumn = Math.max(0, rowLength - 1);
                        }
                    }
                    onRowLengthChanged: {
                        if (isCurrentRow) {
                            stickerPopup.focusedRowLength = rowLength;
                            if (stickerPopup.focusedColumn >= rowLength)
                                stickerPopup.focusedColumn = Math.max(0, rowLength - 1);
                        }
                    }
                    Component.onCompleted: {
                        if (isCurrentRow) {
                            stickerPopup.focusedRowLength = rowLength;
                            if (stickerPopup.focusedColumn >= rowLength)
                                stickerPopup.focusedColumn = Math.max(0, rowLength - 1);
                        }
                    }

                    Repeater {
                        model: row

                        delegate: AbstractButton {
                            id: del

                            required property var modelData
                            required property int index

                            readonly property bool keyboardFocused: rowDelegate.isCurrentRow && stickerPopup.focusedColumn === index

                            width: stickerDim
                            height: stickerDim
                            hoverEnabled: true

                            KomaiToolTip {
                                anchorItem: del
                                anchorX: del.width / 2
                                anchorY: 0
                                text: ":" + del.modelData.shortcode + ": - " + (del.modelData.unicode ? del.modelData.unicodeName : del.modelData.body)
                                delay: Komai.tooltipDelay
                                requestedVisible: del.hovered || del.keyboardFocused
                            }

                            // TODO: maybe add favorites at some point?
                            onClicked: stickerPopup.pickItem(modelData)

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
                                color: del.hovered ? palette.highlight : 'transparent'
                                radius: 5
                                border.color: palette.highlight
                                border.width: (del.keyboardFocused && !del.hovered) ? 2 : 0
                            }

                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    id: emojiScroll
                }

            }

            ListView {
                id: categoriesListView

                function focusFromNeighbor() {
                    if (count <= 0)
                        return false;
                    if (currentIndex < 0)
                        currentIndex = Math.max(0, stickerPopup.activeSectionIndex);
                    positionViewAtIndex(currentIndex, ListView.Contain);
                    forceActiveFocus();
                    return true;
                }

                function activateCurrent() {
                    return stickerPopup.activateCategoryIndex(currentIndex);
                }

                Layout.row: 1
                Layout.column: 0
                Layout.preferredWidth: sidebarPaneWidth
                Layout.fillHeight: true
                Layout.rightMargin: Komai.paddingSmall

                model: gridView.model ? gridView.model.sections : null
                boundsBehavior: Flickable.StopAtBounds
                spacing: 0
                clip: true
                currentIndex: -1
                activeFocusOnTab: false
                highlightFollowsCurrentItem: false

                Keys.onShortcutOverride: event => {
                    if (stickerPopup.isForwardTabEvent(event) || stickerPopup.isBackwardTabEvent(event))
                        event.accepted = true;
                }
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: event => {
                    if (stickerPopup.isForwardTabEvent(event)) {
                        settingsButton.forceActiveFocus();
                        event.accepted = true;
                        return;
                    }
                    if (stickerPopup.isBackwardTabEvent(event)) {
                        gridView.forceActiveFocus();
                        event.accepted = true;
                        return;
                    }

                    if ((event.key === Qt.Key_Up
                                || stickerPopup.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.K))
                            && stickerPopup.eventUsesNavigationModifiers(event)) {
                        if (count > 0)
                            currentIndex = Math.max(0, currentIndex - 1);
                        event.accepted = true;
                        return;
                    }

                    if ((event.key === Qt.Key_Down
                                || stickerPopup.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.J))
                            && stickerPopup.eventUsesNavigationModifiers(event)) {
                        if (count > 0)
                            currentIndex = Math.min(count - 1, currentIndex + 1);
                        event.accepted = true;
                        return;
                    }

                    switch (event.key) {
                    case Qt.Key_Home:
                        if (count > 0) currentIndex = 0;
                        event.accepted = true;
                        return;
                    case Qt.Key_End:
                        if (count > 0) currentIndex = count - 1;
                        event.accepted = true;
                        return;
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                        if (activateCurrent())
                            event.accepted = true;
                        return;
                    }
                }

                delegate: AbstractButton {
                    id: categoryButton

                    required property var modelData
                    required property int index
                    readonly property string sectionName: stickerPopup.sectionName(modelData)
                    readonly property bool active: sectionName.length > 0 && sectionName === stickerPopup.activeSectionName
                    readonly property bool keyboardFocused: categoriesListView.activeFocus && categoriesListView.currentIndex === index
                    property color backgroundColor: "transparent"
                    property color textColor: stickerPopup.sidebarPalette.text

                    width: categoriesListView.width
                    height: sidebarRowHeight
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    leftPadding: Komai.paddingSmall
                    rightPadding: Komai.paddingSmall
                    topPadding: Komai.paddingMedium
                    bottomPadding: Komai.paddingMedium

                    KomaiToolTip {
                        anchorItem: categoryButton
                        anchorX: categoryButton.width / 2
                        anchorY: categoryButton.height
                        gapX: Komai.paddingMedium
                        gapY: Komai.paddingMedium
                        text: categoryButton.modelData.name
                        delay: Komai.tooltipDelay
                        requestedVisible: categoryButton.hovered
                    }

                    onClicked: {
                        categoriesListView.currentIndex = index;
                        stickerPopup.activateCategoryIndex(index);
                    }

                    states: [
                        State {
                            name: "hover"
                            when: (categoryButton.hovered || categoryButton.keyboardFocused) && !categoryButton.active

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
                        border.color: palette.highlight
                        border.width: (categoryButton.keyboardFocused && !categoryButton.active) ? 2 : 0
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
                            font.pointSize: Settings.uiFontSizePt
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
                activeFocusOnTab: false
                property color backgroundColor: "transparent"
                property color textColor: stickerPopup.sidebarPalette.text
                leftPadding: Komai.paddingSmall
                rightPadding: Komai.paddingSmall
                topPadding: Komai.paddingMedium
                bottomPadding: Komai.paddingMedium

                KomaiToolTip {
                    anchorItem: settingsButton
                    anchorX: settingsButton.width / 2
                    anchorY: settingsButton.height
                    gapX: Komai.paddingMedium
                    gapY: Komai.paddingMedium
                    text: qsTr("Change what packs are enabled, remove packs, or create new ones")
                    delay: Komai.tooltipDelay
                    requestedVisible: settingsButton.hovered
                }

                onClicked: TimelineManager.openImagePackSettings(stickerPopup.roomid)

                Keys.onShortcutOverride: event => {
                    if (stickerPopup.isForwardTabEvent(event) || stickerPopup.isBackwardTabEvent(event))
                        event.accepted = true;
                }
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: event => {
                    if (stickerPopup.isForwardTabEvent(event)) {
                        closeButton.forceActiveFocus();
                        event.accepted = true;
                        return;
                    }
                    if (stickerPopup.isBackwardTabEvent(event)) {
                        categoriesListView.focusFromNeighbor();
                        event.accepted = true;
                        return;
                    }
                }

                states: [
                    State {
                        name: "hover"
                        when: settingsButton.hovered || settingsButton.activeFocus

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
                    border.color: palette.highlight
                    border.width: (settingsButton.activeFocus && !settingsButton.hovered) ? 2 : 0
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
                        font.pointSize: Settings.uiFontSizePt
                        text: qsTr("Settings")
                    }
                }
            }
            }

        }

    }

}
