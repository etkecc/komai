// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: hideFilterRoot

    property string tagId
    property string filterName

    title: qsTr("Hide the \"%1\" filter?").arg(hideFilterRoot.filterName)
    titleIcon: ":/icons/icons/ui/eye-hide.svg"

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("To show this filter again, go to Application Settings → Sidebars → Communities Sidebar and enable the \"Show\" toggle for \"%1\".").arg(hideFilterRoot.filterName)
    }

    Label {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingMedium
        color: Communities.isTagHidden(hideFilterRoot.tagId) ? Komai.theme.warning : palette.text
        wrapMode: Text.WordWrap
        text: Communities.isTagHidden(hideFilterRoot.tagId)
            ? qsTr("This filter is currently excluded from \"All rooms\", so its rooms won't appear there either. You can change this in Application Settings → Sidebars.")
            : qsTr("You'll still be able to find rooms that belonged to it in \"All rooms\".")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Button {
            text: qsTr("Cancel")
            onClicked: hideFilterRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("Hide")
            highlighted: true
            onClicked: {
                hideFilterRoot.hideFilter(hideFilterRoot.tagId);
                hideFilterRoot.close();
            }
        }
    }

    function hideFilter(tagId) {
        switch (tagId) {
        case "people":
            Settings.sidebarsCommunitiesFilterPeople = false;
            break;
        case "bot":
            Settings.sidebarsCommunitiesFilterBots = false;
            break;
        case "group":
            Settings.sidebarsCommunitiesFilterGroups = false;
            break;
        case "tag:m.favourite":
            Settings.sidebarsCommunitiesFilterFavourites = false;
            break;
        case "tag:m.server_notice":
            Settings.sidebarsCommunitiesFilterServerNotices = false;
            break;
        case "tag:m.lowpriority":
            Settings.sidebarsCommunitiesFilterLowPriority = false;
            break;
        }
    }
}
