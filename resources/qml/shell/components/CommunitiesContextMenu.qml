// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Menu {
    id: root

    property bool hidden
    property bool muted
    property string tagId
    property string displayName

    signal hideFilterRequested()

    readonly property bool isHideableFilter: {
        switch (tagId) {
        case "people":
        case "bot":
        case "group":
        case "tag:m.favourite":
        case "tag:m.server_notice":
        case "tag:m.lowpriority":
            return true;
        default:
            return false;
        }
    }

    function show(menuParent, id_, hidden_, muted_, displayName_) {
        tagId = id_;
        hidden = hidden_;
        muted = muted_;
        displayName = displayName_ ?? "";
        popup(menuParent);
    }

    Component.onCompleted: {
        if (root.popupType != undefined)
            root.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
    }

    MenuItem {
        checkable: true
        checked: root.muted
        text: qsTr("Do not show notification counts for this community or tag.")

        onTriggered: Communities.toggleTagMute(root.tagId)
    }
    MenuItem {
        checkable: true
        checked: root.hidden
        text: qsTr("Hide rooms with this tag or from this community by default.")

        onTriggered: Communities.toggleTagId(root.tagId)
    }
    MenuItem {
        text: qsTr("Hide this filter")
        visible: root.isHideableFilter

        onTriggered: root.hideFilterRequested()
    }
}
