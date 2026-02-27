// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls

Menu {
    id: replyContextMenuRoot

    required property var roomModel
    property string eventId
    property string link
    property string text

    function show(text_, link_, eventId_) {
        text = text_;
        link = link_;
        eventId = eventId_;

        replyContextMenuFilter.updateTarget();
        popup();
    }

    Component.onCompleted: {
        if (replyContextMenuRoot.popupType != undefined) {
            replyContextMenuRoot.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
        }
    }

    NhekoMenuVisibilityFilter on contentData {
        id: replyContextMenuFilter

        Component {
            MenuItem {
                text: qsTr("&Copy")
                visible: replyContextMenuRoot.text

                onTriggered: Clipboard.text = replyContextMenuRoot.text
            }
        }
        Component {
            MenuItem {
                text: qsTr("Copy &link location")
                visible: replyContextMenuRoot.link

                onTriggered: Clipboard.text = replyContextMenuRoot.link
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Go to quoted message")
                visible: true

                onTriggered: roomModel.showEvent(replyContextMenuRoot.eventId)
            }
        }
    }
}
