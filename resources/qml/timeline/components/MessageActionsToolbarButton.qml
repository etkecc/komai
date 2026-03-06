// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

MessageActionsLabeledButton {
    required property var toolbarRef

    buttonTextColor: toolbarRef.actionButtonColor
    hoverIconColor: toolbarRef.actionButtonActiveColor
    hoverTextColor: toolbarRef.actionButtonActiveColor
    hoverBackgroundColor: toolbarRef.actionButtonHoverBackgroundColor
    buttonHeight: (toolbarRef && typeof toolbarRef.actionButtonHeight === "number") ? toolbarRef.actionButtonHeight : 0
    iconSize: toolbarRef.actionButtonIconSize
    contentHorizontalPadding: toolbarRef.itemHorizontalPadding
    contentVerticalPadding: toolbarRef.itemVerticalPadding
}
