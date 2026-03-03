// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import cc.etke.komai

TextEdit {
    id: root

    required property var model

    color: model.good ? Komai.theme.success : Komai.theme.error
    text: model.value ? qsTr("CACHED") : qsTr("NOT CACHED")
    readOnly: true
    selectByMouse: true
}
