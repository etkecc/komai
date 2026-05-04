// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import cc.etke.komai

Item {
    id: root

    property bool collapsed: false

    // ApplicationProfilesTab is fully custom QML; mirror the empty-state
    // behavior of SettingsContent for the case where search is active and
    // none of the registered custom keywords for this tab match.
    //
    // The explicit `var _ = root._searchQuery` is the binding's tracked
    // dep — Q_INVOKABLE methods don't notify QML, so without that read the
    // visible bindings only re-evaluate on the empty/non-empty transition,
    // not when going from "abc" to "device".
    readonly property string _searchQuery: UserSettingsModel.searchQuery ?? ""
    readonly property bool hasActiveQuery: _searchQuery.length > 0
    readonly property bool searchHidesEverything: {
        var _ = root._searchQuery;
        return root.hasActiveQuery && !UserSettingsModel.tabHasCustomMatches(UserSettingsModel.TabApplicationProfiles);
    }

    ApplicationProfilesView {
        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingMedium
        anchors.topMargin: Komai.paddingLarge
        anchors.bottomMargin: Komai.paddingLarge
        standalone: false
        visible: !root.searchHidesEverything
    }

    Label {
        anchors.centerIn: parent
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        width: Math.min(parent.width - Komai.paddingLarge * 2, 480)
        color: palette.buttonText
        font.pointSize: Settings.uiFontSizePt
        text: qsTranslate("UserSettingsModel", "No settings in this tab match your search.")
        visible: root.searchHidesEverything
    }
}
