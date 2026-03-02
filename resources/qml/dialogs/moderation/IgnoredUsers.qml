// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

OverlayDialog {
    id: ignoredUsers

    title: qsTr("Ignored users")
    titleIcon: ":/icons/icons/ui/ban.svg"

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: palette.text
        text: qsTr("Ignoring a user hides their messages (they can still see yours!).")
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 300
        ScrollBar.horizontal.visible: false

        ListView {
            id: view

            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: TimelineManager.ignoredUsers
            spacing: Komai.paddingMedium

            delegate: RowLayout {
                property var profile: TimelineManager.getGlobalUserProfile(modelData)

                width: view.width

                Avatar {
                    enabled: false
                    displayName: profile.displayName
                    userid: profile.userid
                    url: profile.avatarUrl.replace("mxc://", "image://MxcImage/")
                }

                Text {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignLeft
                    elide: Text.ElideRight
                    color: palette.text
                    text: modelData
                }

                ImageButton {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: 24
                    image: ":/icons/icons/ui/dismiss.svg"
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Stop Ignoring.")
                    onClicked: profile.ignored = false
                }
            }
        }
    }
}
