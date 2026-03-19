// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import cc.etke.komai 1.0

ComboBox {
    id: control

    property int cursor: Qt.PointingHandCursor

    FontMetrics {
        id: comboFontMetrics
        font: control.font
    }

    readonly property int minimumTextWidth: Math.max(72,
                                                     Math.round(comboFontMetrics.averageCharacterWidth * 9))
    readonly property int controlHeight: Math.max(36, Math.round(Settings.uiFontSizePt * 2.7))

    font.pointSize: Settings.uiFontSizePt
    implicitWidth: Math.max(minimumTextWidth + leftPadding + rightPadding,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: controlHeight
    wheelEnabled: activeFocus
    implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
    leftPadding: Komai.paddingMedium + 2
    rightPadding: indicator.implicitWidth + Komai.paddingMedium * 2 + 2
    topPadding: Math.max(2, Komai.paddingSmall + 2)
    bottomPadding: Math.max(2, Komai.paddingSmall + 2)

    delegate: ItemDelegate {
        id: delegateItem

        required property int index

        width: ListView.view ? ListView.view.width : control.width
        highlighted: control.highlightedIndex === index
        padding: Komai.paddingSmall
        leftPadding: Komai.paddingMedium
        rightPadding: Komai.paddingMedium
        text: control.textAt(index)

        contentItem: Text {
            text: delegateItem.text
            color: delegateItem.highlighted ? control.palette.highlightedText : control.palette.windowText
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: delegateItem.highlighted
                ? control.palette.highlight
                : (delegateItem.hovered ? control.palette.alternateBase : "transparent")
            radius: Komai.paddingSmall
        }
    }

    indicator: Image {
        x: control.width - width - Komai.paddingMedium
        y: Math.round((control.height - height) / 2)
        source: "image://colorimage/:/icons/icons/ui/expanded.svg?" + (control.enabled
            ? control.palette.text
            : control.palette.buttonText)
        sourceSize.width: 16
        sourceSize.height: 16
        width: 16
        height: 16
    }

    // Use TextInput (not Text) so desktop styles that expect text-input APIs
    // (for example positionToRectangle/selectionStart) keep working.
    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: control.enabled ? control.palette.windowText : control.palette.buttonText
        enabled: false
        selectByMouse: false
        verticalAlignment: TextInput.AlignVCenter
        clip: true
    }

    background: Rectangle {
        color: control.palette.base
        radius: Komai.paddingSmall
        border.color: control.activeFocus ? control.palette.highlight : Komai.theme.separator
        border.width: control.activeFocus ? 2 : 1
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: control.enabled ? control.cursor : Qt.ArrowCursor
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        padding: 2
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 320)
        palette: control.palette

        background: Rectangle {
            color: control.palette.window
            radius: Komai.paddingSmall
            border.color: Komai.theme.separator
            border.width: 1
        }

        contentItem: ListView {
            id: comboListView
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds

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
                policy: comboListView.scrollbarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }
        }
    }
}
