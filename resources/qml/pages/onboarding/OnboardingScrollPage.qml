// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0

Item {
    id: root

    property int maxContentWidth: 600
    property int horizontalContentPadding: Komai.paddingLarge * 2
    property int topSpacerHeight: Komai.paddingLarge * 2
    property int bottomSpacerHeight: Komai.paddingLarge
    property int contentSpacing: Komai.paddingMedium

    default property alias pageContent: pageContentColumn.data

    ScrollView {
        id: scroll
        anchors.fill: parent

        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: scroll.contentHeight > scroll.availableHeight ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

        ColumnLayout {
            id: contentColumn
            spacing: root.contentSpacing
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(root.maxContentWidth, Math.max(0, scroll.availableWidth - root.horizontalContentPadding))

            Item {
                visible: root.topSpacerHeight > 0
                Layout.preferredHeight: root.topSpacerHeight
                Layout.fillWidth: true
            }

            ColumnLayout {
                id: pageContentColumn
                spacing: root.contentSpacing
                Layout.fillWidth: true
            }

            Item {
                visible: root.bottomSpacerHeight > 0
                Layout.preferredHeight: root.bottomSpacerHeight
                Layout.fillWidth: true
            }
        }
    }
}
