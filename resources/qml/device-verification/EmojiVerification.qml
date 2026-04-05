// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick 2.3
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.10
import cc.etke.komai 1.0

ColumnLayout {
    property string title: qsTr("Do both devices show the same sequence of emojis?")

    spacing: 16

    RowLayout {
        id: emojis

        property var mapping: [{
            "number": 0,
            "emoji": "\u{1F436}",
            "description": "Dog",
            "unicode": "U+1F436"
        }, {
            "number": 1,
            "emoji": "\u{1F431}",
            "description": "Cat",
            "unicode": "U+1F431"
        }, {
            "number": 2,
            "emoji": "\u{1F981}",
            "description": "Lion",
            "unicode": "U+1F981"
        }, {
            "number": 3,
            "emoji": "\u{1F40E}",
            "description": "Horse",
            "unicode": "U+1F40E"
        }, {
            "number": 4,
            "emoji": "\u{1F984}",
            "description": "Unicorn",
            "unicode": "U+1F984"
        }, {
            "number": 5,
            "emoji": "\u{1F437}",
            "description": "Pig",
            "unicode": "U+1F437"
        }, {
            "number": 6,
            "emoji": "\u{1F418}",
            "description": "Elephant",
            "unicode": "U+1F418"
        }, {
            "number": 7,
            "emoji": "\u{1F430}",
            "description": "Rabbit",
            "unicode": "U+1F430"
        }, {
            "number": 8,
            "emoji": "\u{1F43C}",
            "description": "Panda",
            "unicode": "U+1F43C"
        }, {
            "number": 9,
            "emoji": "\u{1F413}",
            "description": "Rooster",
            "unicode": "U+1F413"
        }, {
            "number": 10,
            "emoji": "\u{1F427}",
            "description": "Penguin",
            "unicode": "U+1F427"
        }, {
            "number": 11,
            "emoji": "\u{1F422}",
            "description": "Turtle",
            "unicode": "U+1F422"
        }, {
            "number": 12,
            "emoji": "\u{1F41F}",
            "description": "Fish",
            "unicode": "U+1F41F"
        }, {
            "number": 13,
            "emoji": "\u{1F419}",
            "description": "Octopus",
            "unicode": "U+1F419"
        }, {
            "number": 14,
            "emoji": "\u{1F98B}",
            "description": "Butterfly",
            "unicode": "U+1F98B"
        }, {
            "number": 15,
            "emoji": "\u{1F337}",
            "description": "Flower",
            "unicode": "U+1F337"
        }, {
            "number": 16,
            "emoji": "\u{1F333}",
            "description": "Tree",
            "unicode": "U+1F333"
        }, {
            "number": 17,
            "emoji": "\u{1F335}",
            "description": "Cactus",
            "unicode": "U+1F335"
        }, {
            "number": 18,
            "emoji": "\u{1F344}",
            "description": "Mushroom",
            "unicode": "U+1F344"
        }, {
            "number": 19,
            "emoji": "\u{1F30F}",
            "description": "Globe",
            "unicode": "U+1F30F"
        }, {
            "number": 20,
            "emoji": "\u{1F319}",
            "description": "Moon",
            "unicode": "U+1F319"
        }, {
            "number": 21,
            "emoji": "\u2601\uFE0F",
            "description": "Cloud",
            "unicode": "U+2601U+FE0F"
        }, {
            "number": 22,
            "emoji": "\u{1F525}",
            "description": "Fire",
            "unicode": "U+1F525"
        }, {
            "number": 23,
            "emoji": "\u{1F34C}",
            "description": "Banana",
            "unicode": "U+1F34C"
        }, {
            "number": 24,
            "emoji": "\u{1F34E}",
            "description": "Apple",
            "unicode": "U+1F34E"
        }, {
            "number": 25,
            "emoji": "\u{1F353}",
            "description": "Strawberry",
            "unicode": "U+1F353"
        }, {
            "number": 26,
            "emoji": "\u{1F33D}",
            "description": "Corn",
            "unicode": "U+1F33D"
        }, {
            "number": 27,
            "emoji": "\u{1F355}",
            "description": "Pizza",
            "unicode": "U+1F355"
        }, {
            "number": 28,
            "emoji": "\u{1F382}",
            "description": "Cake",
            "unicode": "U+1F382"
        }, {
            "number": 29,
            "emoji": "\u2764\uFE0F",
            "description": "Heart",
            "unicode": "U+2764U+FE0F"
        }, {
            "number": 30,
            "emoji": "\u{1F600}",
            "description": "Smiley",
            "unicode": "U+1F600"
        }, {
            "number": 31,
            "emoji": "\u{1F916}",
            "description": "Robot",
            "unicode": "U+1F916"
        }, {
            "number": 32,
            "emoji": "\u{1F3A9}",
            "description": "Hat",
            "unicode": "U+1F3A9"
        }, {
            "number": 33,
            "emoji": "\u{1F453}",
            "description": "Glasses",
            "unicode": "U+1F453"
        }, {
            "number": 34,
            "emoji": "\u{1F527}",
            "description": "Spanner",
            "unicode": "U+1F527"
        }, {
            "number": 35,
            "emoji": "\u{1F385}",
            "description": "Santa",
            "unicode": "U+1F385"
        }, {
            "number": 36,
            "emoji": "\u{1F44D}",
            "description": "Thumbs Up",
            "unicode": "U+1F44D"
        }, {
            "number": 37,
            "emoji": "\u2602\uFE0F",
            "description": "Umbrella",
            "unicode": "U+2602U+FE0F"
        }, {
            "number": 38,
            "emoji": "\u231B",
            "description": "Hourglass",
            "unicode": "U+231B"
        }, {
            "number": 39,
            "emoji": "\u23F0",
            "description": "Clock",
            "unicode": "U+23F0"
        }, {
            "number": 40,
            "emoji": "\u{1F381}",
            "description": "Gift",
            "unicode": "U+1F381"
        }, {
            "number": 41,
            "emoji": "\u{1F4A1}",
            "description": "Light Bulb",
            "unicode": "U+1F4A1"
        }, {
            "number": 42,
            "emoji": "\u{1F4D5}",
            "description": "Book",
            "unicode": "U+1F4D5"
        }, {
            "number": 43,
            "emoji": "\u270F\uFE0F",
            "description": "Pencil",
            "unicode": "U+270FU+FE0F"
        }, {
            "number": 44,
            "emoji": "\u{1F4CE}",
            "description": "Paperclip",
            "unicode": "U+1F4CE"
        }, {
            "number": 45,
            "emoji": "\u2702\uFE0F",
            "description": "Scissors",
            "unicode": "U+2702U+FE0F"
        }, {
            "number": 46,
            "emoji": "\u{1F512}",
            "description": "Lock",
            "unicode": "U+1F512"
        }, {
            "number": 47,
            "emoji": "\u{1F511}",
            "description": "Key",
            "unicode": "U+1F511"
        }, {
            "number": 48,
            "emoji": "\u{1F528}",
            "description": "Hammer",
            "unicode": "U+1F528"
        }, {
            "number": 49,
            "emoji": "\u260E\uFE0F",
            "description": "Telephone",
            "unicode": "U+260EU+FE0F"
        }, {
            "number": 50,
            "emoji": "\u{1F3C1}",
            "description": "Flag",
            "unicode": "U+1F3C1"
        }, {
            "number": 51,
            "emoji": "\u{1F682}",
            "description": "Train",
            "unicode": "U+1F682"
        }, {
            "number": 52,
            "emoji": "\u{1F6B2}",
            "description": "Bicycle",
            "unicode": "U+1F6B2"
        }, {
            "number": 53,
            "emoji": "\u2708\uFE0F",
            "description": "Aeroplane",
            "unicode": "U+2708U+FE0F"
        }, {
            "number": 54,
            "emoji": "\u{1F680}",
            "description": "Rocket",
            "unicode": "U+1F680"
        }, {
            "number": 55,
            "emoji": "\u{1F3C6}",
            "description": "Trophy",
            "unicode": "U+1F3C6"
        }, {
            "number": 56,
            "emoji": "\u26BD",
            "description": "Ball",
            "unicode": "U+26BD"
        }, {
            "number": 57,
            "emoji": "\u{1F3B8}",
            "description": "Guitar",
            "unicode": "U+1F3B8"
        }, {
            "number": 58,
            "emoji": "\u{1F3BA}",
            "description": "Trumpet",
            "unicode": "U+1F3BA"
        }, {
            "number": 59,
            "emoji": "\u{1F514}",
            "description": "Bell",
            "unicode": "U+1F514"
        }, {
            "number": 60,
            "emoji": "\u2693",
            "description": "Anchor",
            "unicode": "U+2693"
        }, {
            "number": 61,
            "emoji": "\u{1F3A7}",
            "description": "Headphones",
            "unicode": "U+1F3A7"
        }, {
            "number": 62,
            "emoji": "\u{1F4C1}",
            "description": "Folder",
            "unicode": "U+1F4C1"
        }, {
            "number": 63,
            "emoji": "\u{1F4CC}",
            "description": "Pin",
            "unicode": "U+1F4CC"
        }]

        Layout.fillWidth: true

        Repeater {
            id: repeater

            model: 7

            delegate: Rectangle {
                color: "transparent"
                implicitHeight: Komai.fontPixelSize * 4.5
                Layout.fillWidth: true

                ColumnLayout {
                    id: col

                    property var emoji: (index < flow.sasList.length) ? emojis.mapping[flow.sasList[index]] : null

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom

                    Label {
                        //height: font.pixelSize * 2
                        Layout.alignment: Qt.AlignHCenter
                        text: col.emoji ? col.emoji.emoji : ""
                        font.pointSize: Settings.uiFontSizePt * 2
                        font.family: Settings.uiFontEmojiFamily
                        color: palette.text
                    }

                    Label {
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                        text: col.emoji ? col.emoji.description : ""
                        color: palette.text
                    }

                }

            }

        }

    }

    RowLayout {
        Components.KomaiButton {
            Layout.alignment: Qt.AlignLeft
            text: qsTr("They do not match!")
            onClicked: {
                flow.cancel();
                dialog.close();
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            Layout.alignment: Qt.AlignRight
            highlighted: true
            text: qsTr("They match!")
            onClicked: flow.next()
        }

    }

}
