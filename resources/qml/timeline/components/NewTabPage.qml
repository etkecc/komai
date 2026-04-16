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

    // Search field and button row share this maximum width so they stay aligned.
    readonly property real sharedMaxWidth: Math.min(
        Math.max(actions.implicitWidth, 400),
        root.width - 2 * Komai.paddingLarge)
    readonly property real resultsMaxWidth: Math.min(800, root.width * 0.8)

    property string greeting

    function pickGreeting() {
        const messages = [
            qsTr("The ten thousand chats can't happen in a void. Open a room?"),
            qsTr("Your friends are just a room away"),
            qsTr("Connect with friends. Or bots. We don't judge."),
            qsTr("Friends, bots, communities - all a click away"),
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

    Component.onCompleted: greeting = pickGreeting()

    onActiveChanged: {
        if (active) {
            greeting = pickGreeting();
            search.activate();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top spacer — centers content vertically when no search results.
        Item {
            Layout.fillHeight: !search.hasResults
        }

        Image {
            readonly property int logoDisplaySize: search.isSearching ? Komai.timelineLogoSize : Komai.timelineLogoSize * 1.5

            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: logoDisplaySize
            Layout.preferredHeight: logoDisplaySize
            source: "qrc:/logos/komai.svg"
            sourceSize.height: logoDisplaySize * 2
            sourceSize.width: logoDisplaySize * 2
            fillMode: Image.PreserveAspectFit
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Komai.paddingMedium
            Layout.bottomMargin: Komai.paddingLarge
            font.pointSize: Settings.uiFontSizePt * 1.6
            color: palette.buttonText
            text: root.greeting
            visible: !search.isSearching
        }

        NewTabPageSearch {
            id: search

            Layout.fillWidth: true
            Layout.fillHeight: hasResults
            Layout.maximumWidth: root.resultsMaxWidth
            Layout.alignment: Qt.AlignHCenter
            searchFieldMaxWidth: root.sharedMaxWidth
            tabController: root.tabController
        }

        NewTabPageActions {
            id: actions

            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: root.sharedMaxWidth
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
