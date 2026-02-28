// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

MessageActionsLabeledButton {
    required property var toolbarRef

    buttonTextColor: toolbarRef.actionButtonColor
    hoverIconColor: toolbarRef.actionButtonHoverColor
    hoverTextColor: toolbarRef.actionButtonHoverColor
    hoverBackgroundColor: toolbarRef.actionButtonHoverBackgroundColor
    iconSize: toolbarRef.actionButtonIconSize
    contentHorizontalPadding: toolbarRef.itemHorizontalPadding
    contentVerticalPadding: toolbarRef.itemVerticalPadding
}
