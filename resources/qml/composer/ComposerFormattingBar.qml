// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../timeline/components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

// Selection-anchored Markdown formatting toolbar. Shown when the user has a
// non-empty selection in the composer textarea; offers buttons for Bold,
// Italic, Inline code, Quote, and Link, each toggling the same Markdown wrap
// that the keyboard shortcuts apply.
//
// Positioning mirrors the timeline message-actions bar: the bar is parented
// to Overlay.overlay (same as the completer popup) so it can render above
// surrounding chrome, and its x/y are computed from
// `messageInput.positionToRectangle(selectionStart)` mapped into the
// overlay's coordinate system. The bar opens above the selection by default
// and flips to below when the top would clip out of the overlay.
//
// Buttons use `focusPolicy: Qt.NoFocus` so clicking them does NOT move the
// active focus out of the textarea — this is what `messageInput`'s
// `persistentSelection: true` is supplementing, and together they let the
// programmatic transform run against a still-live selection.
Item {
    id: root

    required property var messageInput
    property bool popupOpen: false

    // Bumped whenever something that could move the anchor changes so the
    // x/y bindings re-run. Mirrors the popup._positionRefreshTick idiom.
    property int _positionRefreshTick: 0

    readonly property bool hasSelection: !!messageInput
        && messageInput.selectionStart !== messageInput.selectionEnd
    readonly property bool composerReady: !!messageInput
        && messageInput.preeditText.length === 0
        && !messageInput.readOnly
    readonly property bool shouldBeVisible: hasSelection
        && composerReady
        && !popupOpen
        && (messageInput.activeFocus || bar.activeFocus || bar.containsHover)

    function _bumpPosition() {
        root._positionRefreshTick = root._positionRefreshTick + 1
    }

    function _anchorRectInOverlay() {
        const _ = root._positionRefreshTick;
        if (!messageInput || !root.parent)
            return Qt.rect(0, 0, 0, 0);
        const rect = messageInput.positionToRectangle(messageInput.selectionStart);
        const topLeft = messageInput.mapToItem(root.parent, rect.x, rect.y);
        return Qt.rect(topLeft.x, topLeft.y, rect.width, rect.height);
    }

    parent: Overlay.overlay
    visible: showTimer.show && shouldBeVisible
    z: 11
    implicitWidth: bar.implicitWidth
    implicitHeight: bar.implicitHeight
    width: implicitWidth
    height: implicitHeight

    x: {
        const _ = root._positionRefreshTick;
        if (!root.parent)
            return 0;
        const anchor = root._anchorRectInOverlay();
        const margin = Komai.paddingLarge;
        const minX = margin;
        const maxX = Math.max(minX, root.parent.width - root.implicitWidth - margin);
        return Math.round(Math.max(minX, Math.min(anchor.x, maxX)));
    }
    y: {
        const _ = root._positionRefreshTick;
        if (!root.parent)
            return 0;
        const anchor = root._anchorRectInOverlay();
        const margin = Komai.paddingSmall;
        const aboveY = anchor.y - root.implicitHeight - margin;
        const belowY = anchor.y + anchor.height + margin;
        const overlayHeight = root.parent.height;
        const minY = margin;
        const maxY = Math.max(minY, overlayHeight - root.implicitHeight - margin);
        if (aboveY >= minY)
            return Math.round(aboveY);
        return Math.round(Math.min(belowY, maxY));
    }

    // 60ms show delay debounces drag-select churn so the bar doesn't jitter
    // mid-drag. Hide is immediate via the `visible` binding.
    Timer {
        id: showTimer
        interval: 60
        property bool show: false
        onTriggered: show = true
    }

    onShouldBeVisibleChanged: {
        if (shouldBeVisible) {
            if (!showTimer.show)
                showTimer.restart();
        } else {
            showTimer.stop();
            showTimer.show = false;
        }
    }

    Connections {
        target: root.messageInput || null
        function onSelectionStartChanged() { root._bumpPosition(); }
        function onSelectionEndChanged() { root._bumpPosition(); }
        function onCursorPositionChanged() { root._bumpPosition(); }
        function onContentHeightChanged() { root._bumpPosition(); }
        function onWidthChanged() { root._bumpPosition(); }
    }
    Connections {
        target: root.Window.window || null
        function onWidthChanged() { root._bumpPosition(); }
        function onHeightChanged() { root._bumpPosition(); }
    }

    Control {
        id: bar

        readonly property color actionButtonColor: palette.buttonText
        readonly property color actionButtonActiveColor: palette.brightText
        readonly property color actionButtonHoverBackgroundColor: palette.dark
        readonly property int actionButtonHeight: Komai.iconSize
        readonly property int actionButtonIconSize: Math.max(14, actionButtonHeight - 2 * Komai.paddingSmall)
        readonly property int itemHorizontalPadding: (Komai.density !== Settings.Density.Spacious)
            ? Komai.paddingSmall : Komai.paddingMedium
        readonly property int itemVerticalPadding: 0
        readonly property bool containsHover: hovered

        function _apply(kind) {
            if (!root.messageInput)
                return;
            root.messageInput.applyComposerFormat(kind);
            root._bumpPosition();
        }

        anchors.fill: parent
        hoverEnabled: true
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        Accessible.name: qsTr("Formatting")
        Accessible.role: Accessible.ToolBar

        background: TimelineFloatingActionBarBackground {
            barColor: palette.alternateBase
            barRadius: Komai.paddingSmall
            barBorderColor: Komai.theme.separator
            barBorderWidth: 1
        }

        contentItem: RowLayout {
            spacing: 0

            ComposerToolbarButton {
                Layout.alignment: Qt.AlignVCenter
                buttonSize: bar.actionButtonHeight
                buttonPaddingH: bar.itemHorizontalPadding
                buttonPaddingV: bar.itemVerticalPadding
                buttonTextColor: bar.actionButtonColor
                focusPolicy: Qt.NoFocus
                activeFocusOnTab: false
                image: ":/icons/icons/ui/text-bold.svg"
                toolTipText: qsTr("Bold (Ctrl+B)")
                onClicked: bar._apply(Komai.ComposerFormatKind.Bold)
            }
            ComposerToolbarButton {
                Layout.alignment: Qt.AlignVCenter
                buttonSize: bar.actionButtonHeight
                buttonPaddingH: bar.itemHorizontalPadding
                buttonPaddingV: bar.itemVerticalPadding
                buttonTextColor: bar.actionButtonColor
                focusPolicy: Qt.NoFocus
                activeFocusOnTab: false
                image: ":/icons/icons/ui/text-italic.svg"
                toolTipText: qsTr("Italic (Ctrl+I)")
                onClicked: bar._apply(Komai.ComposerFormatKind.Italic)
            }
            ComposerToolbarButton {
                Layout.alignment: Qt.AlignVCenter
                buttonSize: bar.actionButtonHeight
                buttonPaddingH: bar.itemHorizontalPadding
                buttonPaddingV: bar.itemVerticalPadding
                buttonTextColor: bar.actionButtonColor
                focusPolicy: Qt.NoFocus
                activeFocusOnTab: false
                image: ":/icons/icons/ui/code.svg"
                toolTipText: qsTr("Code (Ctrl+E)")
                onClicked: bar._apply(Komai.ComposerFormatKind.InlineCode)
            }
            ComposerToolbarButton {
                Layout.alignment: Qt.AlignVCenter
                buttonSize: bar.actionButtonHeight
                buttonPaddingH: bar.itemHorizontalPadding
                buttonPaddingV: bar.itemVerticalPadding
                buttonTextColor: bar.actionButtonColor
                focusPolicy: Qt.NoFocus
                activeFocusOnTab: false
                image: ":/icons/icons/ui/text-quote.svg"
                toolTipText: qsTr("Quote (Ctrl+Shift+>)")
                onClicked: bar._apply(Komai.ComposerFormatKind.Quote)
            }
            ComposerToolbarButton {
                Layout.alignment: Qt.AlignVCenter
                buttonSize: bar.actionButtonHeight
                buttonPaddingH: bar.itemHorizontalPadding
                buttonPaddingV: bar.itemVerticalPadding
                buttonTextColor: bar.actionButtonColor
                focusPolicy: Qt.NoFocus
                activeFocusOnTab: false
                image: ":/icons/icons/ui/link.svg"
                toolTipText: qsTr("Link (Ctrl+Shift+L)")
                onClicked: bar._apply(Komai.ComposerFormatKind.Link)
            }
        }
    }
}
