// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: applyDialog

    property RoomSettings roomSettings
    property PowerlevelEditingModels editingModel

    title: qsTr("Apply permission changes")
    titleIcon: ":/icons/icons/ui/settings.svg"

    MatrixText {
        text: qsTr("Which of the subcommunities and rooms should these permissions be applied to?")
        font.pixelSize: Math.floor(fontMetrics.font.pixelSize * 1.1)
        Layout.fillWidth: true
        color: palette.text
        Layout.bottomMargin: Komai.paddingMedium
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2

        Label {
            text: qsTr("Apply permissions recursively")
            Layout.fillWidth: true
            color: palette.text
        }

        ToggleButton {
            checked: editingModel.spaces.applyToChildren
            Layout.alignment: Qt.AlignRight
            onCheckedChanged: editingModel.spaces.applyToChildren = checked
        }

        Label {
            text: qsTr("Overwrite exisiting modifications in rooms")
            Layout.fillWidth: true
            color: palette.text
        }

        ToggleButton {
            checked: editingModel.spaces.overwriteDiverged
            Layout.alignment: Qt.AlignRight
            onCheckedChanged: editingModel.spaces.overwriteDiverged = checked
        }
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 300
        ScrollBar.horizontal.visible: false

        ListView {
            id: view

            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: editingModel.spaces
            spacing: 4
            cacheBuffer: 50

            delegate: RowLayout {
                width: view.width

                ColumnLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: model.displayName
                        color: palette.text
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: {
                            if (!model.isEditable)
                                return qsTr("No permissions to apply the new permissions here");
                            if (model.isAlreadyUpToDate)
                                return qsTr("No changes needed");
                            if (model.isDifferentFromBase)
                                return qsTr("Existing modifications to the permissions in this room will be overwritten");
                            return qsTr("Permissions synchronized with community");
                        }
                        elide: Text.ElideRight
                        color: palette.buttonText
                        textFormat: Text.PlainText
                    }
                }

                ToggleButton {
                    checked: model.applyPermissions
                    Layout.alignment: Qt.AlignRight
                    onCheckedChanged: model.applyPermissions = checked
                    enabled: model.isEditable
                }
            }
        }
    }

    Components.KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Apply")
        highlighted: true
        onClicked: {
            editingModel.spaces.commit();
            applyDialog.close();
        }
    }
}
