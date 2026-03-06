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

    title: qsTr("Hide filter")
    titleIcon: ":/icons/icons/ui/eye-hide.svg"

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("Are you sure you want to hide the \"%1\" filter?").arg(hideFilterRoot.filterName)
    }

    Label {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingMedium
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("You'll still be able to always find rooms that belonged to it in \"All rooms\".")
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("To show this filter again, go to Application Settings → Sidebars → Communities Sidebar and enable the \"Show %1 filter\" toggle.").arg(hideFilterRoot.filterName)
    }

    Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Hide")
        highlighted: true
        onClicked: {
            hideFilterRoot.hideFilter(hideFilterRoot.tagId);
            hideFilterRoot.close();
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
