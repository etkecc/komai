// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

TextEdit {
    id: root

    required property var model

    color: palette.text
    text: model.value
    readOnly: true
    textFormat: Text.PlainText
}
