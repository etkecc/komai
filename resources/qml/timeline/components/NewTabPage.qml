// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    property bool active: false
    required property var dialogHost
    required property var tabController

    // Search field and button row share this width so they stay aligned.
    // Based on the button row's natural width (computed even when hidden).
    readonly property real searchFieldMaxWidth: Math.min(
        Math.max(actions.implicitWidth, 400),
        root.width - 2 * Komai.paddingLarge)
    readonly property real resultsMaxWidth: Math.min(800, root.width * 0.8)

    onActiveChanged: {
        if (active)
            search.activate();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top spacer — centers content vertically when no search results.
        Item {
            Layout.fillHeight: !search.hasResults
        }

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Komai.timelineLogoSize
            Layout.preferredHeight: Komai.timelineLogoSize
            source: "qrc:/logos/komai.svg"
            sourceSize.height: Komai.timelineLogoSize * 2
            sourceSize.width: Komai.timelineLogoSize * 2
            fillMode: Image.PreserveAspectFit
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Komai.paddingMedium
            Layout.bottomMargin: Komai.paddingLarge
            font.pointSize: Settings.uiFontSizePt * 1.4
            color: palette.buttonText
            text: {
                const messages = [
                    qsTr("The ten thousand chats can't happen in a void. Open a room?"),
                    qsTr("Your friends are just a room away"),
                    qsTr("Connect with friends. Or bots. We don't judge."),
                    qsTr("Friends, bots, communities - all one click away"),
                    qsTr("Be present for a bit. Then open a room."),
                    qsTr("The best conversations haven't happened yet"),
                    qsTr("An empty screen, a full inbox of possibilities"),
                    qsTr("Open a room. The rest follows."),
                    qsTr("Open a room to start a conversation"),
                    qsTr("Next conversation, a click away"),
                    qsTr("Ready to chat - pick a room"),
                    qsTr("All quiet here. Open a room?"),
                    qsTr("Chat rooms await - pick one or start your own"),
                    qsTr("No room leads to no chat"),
                ];
                return messages[Math.floor(Math.random() * messages.length)];
            }
        }

        NewTabPageSearch {
            id: search

            Layout.fillWidth: true
            Layout.fillHeight: hasResults
            Layout.maximumWidth: root.resultsMaxWidth
            Layout.alignment: Qt.AlignHCenter
            searchFieldMaxWidth: root.searchFieldMaxWidth
            tabController: root.tabController
        }

        NewTabPageActions {
            id: actions

            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Komai.paddingLarge
            dialogHost: root.dialogHost
            visible: !search.isSearching
        }

        // Bottom spacer — visible only when no search results (centering partner).
        Item {
            Layout.fillHeight: true
            visible: !search.hasResults
        }
    }
}
