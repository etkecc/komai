// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Item {
    id: control

    property int cursor: Qt.PointingHandCursor
    property var model: []
    property int currentIndex: -1
    readonly property string displayText: currentIndex >= 0 && currentIndex < model.length
        ? model[currentIndex] : ""

    signal activated(int index)

    // Match Qt Quick's ComboBox so screen readers and Tab navigation treat
    // this custom Item like a real combo box rather than an inert region.
    activeFocusOnTab: true

    Accessible.role: Accessible.ComboBox
    Accessible.editable: false
    Accessible.name: control.displayText
    Accessible.onPressAction: popup.open()

    Keys.onSpacePressed: popup.open()
    Keys.onReturnPressed: popup.open()
    Keys.onEnterPressed: popup.open()
    Keys.onDownPressed: popup.open()

    FontMetrics {
        id: comboFontMetrics
        font: buttonLabel.font
    }

    readonly property int controlHeight: Math.max(36, Math.round(Settings.uiFontSizePt * 2.7))

    implicitWidth: Math.max(Math.round(comboFontMetrics.averageCharacterWidth * 25)
                                + Komai.paddingMedium * 2 + 16 + Komai.paddingMedium,
                            buttonLabel.implicitWidth + Komai.paddingMedium * 2 + 16 + Komai.paddingMedium)
    implicitHeight: controlHeight

    // Button-like display area
    Rectangle {
        id: buttonBackground
        readonly property bool emphasised: popup.visible || control.activeFocus
        anchors.fill: parent
        color: palette.base
        radius: Komai.paddingSmall
        border.color: emphasised ? palette.highlight : Komai.theme.separator
        border.width: emphasised ? 2 : 1
    }

    Text {
        id: buttonLabel
        anchors.left: parent.left
        anchors.right: indicator.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingSmall
        text: control.displayText
        font.pointSize: Settings.uiFontSizePt
        color: palette.windowText
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    Image {
        id: indicator
        anchors.right: parent.right
        anchors.rightMargin: Komai.paddingMedium
        anchors.verticalCenter: parent.verticalCenter
        source: "image://colorimage/:/icons/icons/ui/expanded.svg?" + palette.text
        sourceSize.width: 16
        sourceSize.height: 16
        width: 16
        height: 16
    }

    MouseArea {
        anchors.fill: parent
        enabled: control.enabled
        onPressed: {
            if (popup.visible) {
                popup.close();
            } else if (!control._suppressReopen) {
                popup.open();
            }
        }
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: control.enabled ? control.cursor : Qt.ArrowCursor
    }

    // Guard against close-then-immediately-reopen: when the popup closes because
    // the user clicks the button area, the popup closes first (losing focus),
    // then the MouseArea press fires and would reopen it.
    property bool _suppressReopen: false

    Timer {
        id: closeSuppressTimer
        interval: 50
        onTriggered: control._suppressReopen = false
    }

    ListModel {
        id: filteredModel
    }

    function selectItem(filteredIndex) {
        var item = filteredModel.get(filteredIndex);
        if (!item)
            return;
        control.activated(item.originalIndex);
        popup.close();
    }

    Popup {
        id: popup
        readonly property int gap: 2

        // Estimate the popup height from the model size up-front, not from
        // popupLayout's laid-out implicitHeight: the ListView delegates aren't
        // created until the popup actually shows, so on the first open the
        // laid-out height is tiny and placePopup() would mis-pick "downward".
        readonly property int searchFieldHeight: Math.max(36, Math.round(Settings.uiFontSizePt * 2.7))
        readonly property int rowHeight: Math.max(28, Math.round(Settings.uiFontSizePt * 2.4))
        readonly property int estimatedHeight: {
            const items = (control.model && control.model.length) || 0;
            // Cap visible rows so a long list still fits a reasonable popup.
            const visibleRows = Math.min(items, 10);
            return Math.min(searchFieldHeight + 2 + visibleRows * rowHeight + 4, 320);
        }

        // Flip upward when the control sits near the window's bottom edge so
        // the dropdown doesn't get squeezed into a sliver. The decision uses
        // estimatedHeight (data-derived, available before layout); the actual
        // y is bound to implicitHeight below so the popup sits flush against
        // the control regardless of how much shorter it ends up.
        property bool openUpward: false

        function placePopup() {
            const win = control.Window.window;
            if (!win) {
                openUpward = false;
                return;
            }
            const pos = control.mapToItem(null, 0, 0);
            const margin = 8;
            const spaceBelow = win.height - pos.y - control.height - margin;
            const spaceAbove = pos.y - margin;
            if (spaceBelow >= estimatedHeight + gap) {
                openUpward = false;
            } else if (spaceAbove >= estimatedHeight + gap) {
                openUpward = true;
            } else {
                openUpward = spaceAbove > spaceBelow;
            }
        }

        y: openUpward ? -implicitHeight - gap : control.height + gap
        width: control.width
        padding: 2
        implicitHeight: Math.min(popupLayout.implicitHeight + topPadding + bottomPadding, estimatedHeight)
        palette: control.palette

        onAboutToShow: placePopup()

        onOpened: {
            searchField.text = "";
            updateFilter();
            listView.currentIndex = -1;
            searchField.forceActiveFocus();
        }
        onClosed: {
            searchField.text = "";
            control._suppressReopen = true;
            closeSuppressTimer.restart();
        }

        background: Rectangle {
            color: control.palette.window
            radius: Komai.paddingSmall
            border.color: Komai.theme.separator
            border.width: 1
        }

        contentItem: ColumnLayout {
            id: popupLayout
            spacing: 2

            KomaiTextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Search…")
                onTextChanged: {
                    updateFilter();
                    listView.currentIndex = filteredModel.count > 0 ? 0 : -1;
                }
                Keys.onEscapePressed: popup.close()
                Keys.onDownPressed: {
                    if (listView.currentIndex < filteredModel.count - 1)
                        listView.currentIndex++;
                }
                Keys.onUpPressed: {
                    if (listView.currentIndex > 0)
                        listView.currentIndex--;
                }
                Keys.onReturnPressed: {
                    if (listView.currentIndex >= 0)
                        control.selectItem(listView.currentIndex);
                }
                Keys.onEnterPressed: {
                    if (listView.currentIndex >= 0)
                        control.selectItem(listView.currentIndex);
                }
            }

            ListView {
                id: listView
                Layout.fillWidth: true
                implicitHeight: contentHeight
                Layout.maximumHeight: 280 - searchField.height - popupLayout.spacing
                clip: true
                model: filteredModel
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 0

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

                ScrollBar.vertical: ScrollBar {
                    policy: listView.scrollbarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                }

                delegate: ItemDelegate {
                    id: delegateItem

                    required property int index
                    required property string label
                    required property int originalIndex

                    width: listView.width
                    highlighted: index === listView.currentIndex
                    padding: Komai.paddingSmall
                    leftPadding: Komai.paddingMedium
                    rightPadding: Komai.paddingMedium
                    text: label

                    contentItem: Text {
                        text: delegateItem.label
                        color: delegateItem.highlighted ? control.palette.highlightedText : control.palette.windowText
                        font.pointSize: Settings.uiFontSizePt
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: delegateItem.highlighted
                            ? control.palette.highlight
                            : (delegateItem.hovered ? control.palette.alternateBase : "transparent")
                        radius: Komai.paddingSmall
                    }

                    onClicked: control.selectItem(delegateItem.index)
                }
            }
        }
    }

    function updateFilter() {
        filteredModel.clear();
        var needle = searchField.text.toLowerCase();
        for (var i = 0; i < model.length; i++) {
            var entry = model[i];
            if (needle === "" || entry.toLowerCase().indexOf(needle) !== -1) {
                filteredModel.append({ label: entry, originalIndex: i });
            }
        }
    }
}
