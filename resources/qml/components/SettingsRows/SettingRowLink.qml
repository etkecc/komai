// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick

TextEdit {
    id: root

    required property var model

    color: palette.text
    text: model.value
    textFormat: Text.RichText
    readOnly: true
    selectByMouse: true
    onLinkActivated: function(link) {
        Qt.openUrlExternally(link);
    }
}
