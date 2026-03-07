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
    property bool badgesHidden
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

    function show(menuParent, id_, hidden_, badgesHidden_, displayName_) {
        tagId = id_;
        hidden = hidden_;
        badgesHidden = badgesHidden_;
        displayName = displayName_ ?? "";
        popup(menuParent);
    }

    Component.onCompleted: {
        if (root.popupType != undefined)
            root.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
    }

    MenuItem {
        checkable: true
        checked: true
        visible: root.isHideableFilter
        icon.source: "qrc:/icons/icons/ui/eye-show.svg"
        text: qsTr("Show")

        onTriggered: root.hideFilterRequested()
    }
    MenuItem {
        checkable: true
        checked: !root.badgesHidden
        icon.source: "qrc:/icons/icons/ui/counter.svg"
        text: qsTr("Show attention badges")

        onTriggered: Communities.toggleFilterBadges(root.tagId)
    }
    MenuItem {
        checkable: true
        checked: !root.hidden
        icon.source: "qrc:/icons/icons/ui/globe.svg"
        text: qsTr("Include in 'All rooms'")

        onTriggered: Communities.toggleGlobalExclude(root.tagId)
    }
}
