// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: aliasEditorW

    property var roomSettings
    property var editingModel: Komai.editAliases(roomSettings.roomId)

    title: qsTr("Aliases to %1").arg(roomSettings.roomName)
    titleIcon: ":/icons/icons/ui/link.svg"
    initialFocusItem: newAliasVal

    MatrixText {
        text: qsTr("List of aliases to this room. Usually you can only add aliases on your server. You can have one canonical alias and many alternate aliases.")
        font.pixelSize: Math.floor(fontMetrics.font.pixelSize * 1.1)
        Layout.fillWidth: true
        color: palette.text
        Layout.bottomMargin: Komai.paddingMedium
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 250
        ScrollBar.horizontal.visible: false

        ListView {
            id: view

            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: aliasEditorW.editingModel
            spacing: 4
            cacheBuffer: 50

            delegate: RowLayout {
                width: view.width

                Text {
                    Layout.fillWidth: true
                    text: model.name
                    color: model.isPublished ? palette.text : Komai.theme.error
                    textFormat: Text.PlainText
                }

                ImageButton {
                    Layout.alignment: Qt.AlignRight
                    Layout.margins: 2
                    image: ":/icons/icons/ui/star.svg"
                    hoverEnabled: true
                    buttonTextColor: model.isCanonical ? palette.highlight : palette.text
                    highlightColor: aliasEditorW.editingModel.canAdvertize ? palette.highlight : buttonTextColor

                    ToolTip.visible: hovered
                    ToolTip.text: model.isCanonical ? qsTr("Primary alias") : qsTr("Make primary alias")

                    onClicked: aliasEditorW.editingModel.makeCanonical(model.index)
                }

                ImageButton {
                    Layout.alignment: Qt.AlignRight
                    Layout.margins: 2
                    image: ":/icons/icons/ui/building-shop.svg"
                    hoverEnabled: true
                    buttonTextColor: model.isAdvertized ? palette.highlight : palette.text
                    highlightColor: aliasEditorW.editingModel.canAdvertize ? palette.highlight : buttonTextColor

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Advertise as an alias in this room")

                    onClicked: aliasEditorW.editingModel.toggleAdvertize(model.index)
                }

                ImageButton {
                    Layout.alignment: Qt.AlignRight
                    Layout.margins: 2
                    image: ":/icons/icons/ui/room-directory.svg"
                    hoverEnabled: true
                    buttonTextColor: model.isPublished ? palette.highlight : palette.text

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Publish in room directory")

                    onClicked: aliasEditorW.editingModel.togglePublish(model.index)
                }

                ImageButton {
                    Layout.alignment: Qt.AlignRight
                    Layout.margins: 2
                    image: ":/icons/icons/ui/dismiss.svg"
                    hoverEnabled: true

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Remove this alias")

                    onClicked: aliasEditorW.editingModel.deleteAlias(model.index)
                }
            }
        }
    }

    RowLayout {
        spacing: Komai.paddingMedium
        Layout.fillWidth: true

        MatrixTextField {
            id: newAliasVal

            Layout.fillWidth: true
            selectByMouse: true
            font.pixelSize: fontMetrics.font.pixelSize
            color: palette.text
            placeholderText: qsTr("#new-alias:server.tld")

            Keys.onPressed: {
                if (event.matches(StandardKey.InsertParagraphSeparator)) {
                    aliasEditorW.editingModel.addAlias(newAliasVal.text);
                    newAliasVal.clear();
                }
            }
        }

        Button {
            text: qsTr("Add")
            Layout.preferredWidth: 100
            onClicked: {
                aliasEditorW.editingModel.addAlias(newAliasVal.text);
                newAliasVal.clear();
            }
        }
    }

    Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Save")
        highlighted: true
        onClicked: {
            aliasEditorW.editingModel.commit();
            aliasEditorW.close();
        }
    }
}
