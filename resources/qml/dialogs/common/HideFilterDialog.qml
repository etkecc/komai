// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
        text: qsTr("To show this filter again, go to Application Settings → Navigation → Communities Sidebar and enable the \"Show\" toggle for \"%1\".").arg(hideFilterRoot.filterName)
    }

    Label {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingMedium
        color: Communities.isGlobalExcluded(hideFilterRoot.tagId) ? Komai.theme.warning : palette.text
        wrapMode: Text.WordWrap
        text: Communities.isGlobalExcluded(hideFilterRoot.tagId)
            ? qsTr("This filter is currently excluded from \"All rooms\", so its rooms won't appear there either. You can change this in Application Settings → Navigation.")
            : qsTr("You'll still be able to find rooms that belonged to it in \"All rooms\".")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: hideFilterRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            text: qsTr("Hide")
            highlighted: true
            onClicked: {
                hideFilterRoot.hideFilter(hideFilterRoot.tagId);
                hideFilterRoot.close();
            }
        }
    }

    function hideFilter(tagId) {
        if (tagId.startsWith("space:")) {
            Communities.toggleSpaceHidden(tagId);
            return;
        }

        switch (tagId) {
        case "people":
            Settings.navigationCommunitiesFilterPeople = false;
            break;
        case "bot":
            Settings.navigationCommunitiesFilterBots = false;
            break;
        case "group":
            Settings.navigationCommunitiesFilterGroups = false;
            break;
        case "tag:m.favourite":
            Settings.navigationCommunitiesFilterFavourites = false;
            break;
        case "tag:m.server_notice":
            Settings.navigationCommunitiesFilterServerNotices = false;
            break;
        case "tag:m.lowpriority":
            Settings.navigationCommunitiesFilterLowPriority = false;
            break;
        }
    }
}
