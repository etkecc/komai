// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import im.nheko

Text {
    id: root

    required property var model

    color: model.good ? "green" : Nheko.theme.error
    text: model.value ? qsTr("CACHED") : qsTr("NOT CACHED")
}
