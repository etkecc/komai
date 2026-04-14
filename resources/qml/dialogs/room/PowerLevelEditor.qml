// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

OverlayDialog {
    id: plEditorW

    property var roomSettings
    property var appRoot
    property var editingModel: Komai.editPowerlevels(roomSettings.roomId)

    title: qsTr("Permissions in %1").arg(roomSettings.roomName)
    titleIcon: ":/icons/icons/ui/settings.svg"

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: {
            if (!plEditorW.parent) return 600;
            // Cap so the dialog (at y ≈ 25%) doesn't overflow the viewport.
            var available = plEditorW.parent.height - plEditorW.y
                - plEditorW.overlayDialogChromeHeight - Komai.paddingLarge;
            return Math.min(plEditorW.parent.height * 0.7, Math.max(300, available));
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Komai.paddingMedium

            MatrixText {
                text: qsTr("Be careful when editing permissions. You can't lower the permissions of people with a same or higher level than you. Be careful when promoting others.")
                font.pixelSize: Math.floor(fontMetrics.font.pixelSize * 1.1)
                Layout.fillWidth: true
                color: palette.text
            }

            TabBar {
                id: bar

                Layout.fillWidth: true

                KomaiTabButton {
                    text: qsTr("Roles")
                }

                KomaiTabButton {
                    text: qsTr("Users")
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: palette.alternateBase
                border.width: 1
                border.color: Komai.theme.separator

                StackLayout {
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    currentIndex: bar.currentIndex

                    ColumnLayout {
                        spacing: Komai.paddingMedium

                        MatrixText {
                            text: qsTr("Move permissions between roles to change them")
                            font.pixelSize: Math.floor(fontMetrics.font.pixelSize * 1.1)
                            Layout.fillWidth: true
                            color: palette.text
                        }

                        ReorderableListview {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            model: plEditorW.editingModel.types

                            delegate: RowLayout {
                                Column {
                                    Layout.fillWidth: true

                                    Text {
                                        visible: model ? model.isType : false
                                        text: model ? model.displayName : ""
                                        color: palette.text
                                    }

                                    Text {
                                        visible: model ? !model.isType : false
                                        text: {
                                            if (!model) return "";
                                            let pl = model.powerlevel.toLocaleString(Qt.locale(), "f", 0);
                                            if (plEditorW.editingModel.adminLevel == model.powerlevel)
                                                return qsTr("Administrator (%1)").arg(pl);
                                            else if (plEditorW.editingModel.moderatorLevel == model.powerlevel)
                                                return qsTr("Moderator (%1)").arg(pl);
                                            else if (plEditorW.editingModel.defaultUserLevel == model.powerlevel)
                                                return qsTr("User (%1)").arg(pl);
                                            else
                                                return qsTr("Custom (%1)").arg(pl);
                                        }
                                        color: palette.text
                                    }
                                }

                                ImageButton {
                                    Layout.alignment: Qt.AlignRight
                                    Layout.rightMargin: 2
                                    image: model ? (model.isType ? ":/icons/icons/ui/dismiss.svg" : ":/icons/icons/ui/plus-circle.svg") : ""
                                    visible: model ? (!model.isType || model.removeable) : false
                                    hoverEnabled: true
                                    toolTipVisible: hovered
                                    toolTipText: model ? (model.isType ? qsTr("Remove event type") : qsTr("Add event type")) : ""
                                    onClicked: {
                                        if (model.isType) {
                                            plEditorW.editingModel.types.remove(index);
                                        } else {
                                            typeEntry.y = offset;
                                            typeEntry.visible = true;
                                            typeEntry.index = index;
                                            typeEntry.forceActiveFocus();
                                        }
                                    }
                                }
                            }

                            KomaiTextField {
                                id: typeEntry

                                property int index

                                width: parent.width
                                z: 5
                                visible: false

                                Keys.onPressed: event => {
                                    if (typeEntry.text.includes('.') && event.matches(StandardKey.InsertParagraphSeparator)) {
                                        plEditorW.editingModel.types.add(typeEntry.index, typeEntry.text);
                                        typeEntry.visible = false;
                                        typeEntry.clear();
                                        event.accepted = true;
                                    } else if (event.matches(StandardKey.Cancel)) {
                                        typeEntry.visible = false;
                                        typeEntry.clear();
                                        event.accepted = true;
                                    }
                                }
                            }
                        }

                        KomaiButton {
                            Layout.fillWidth: true
                            text: qsTr("Add new role")
                            onClicked: newPLLay.visible = true

                            Rectangle {
                                id: newPLLay

                                anchors.fill: parent
                                visible: false
                                color: palette.alternateBase

                                RowLayout {
                                    spacing: Komai.paddingMedium
                                    anchors.fill: parent

                                    KomaiSpinBox {
                                        id: newPLVal

                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        editable: true
                                        from: -2000000000
                                        to: 2000000000

                                        Keys.onPressed: {
                                            if (event.matches(StandardKey.InsertParagraphSeparator)) {
                                                plEditorW.editingModel.addRole(newPLVal.value);
                                                newPLLay.visible = false;
                                            }
                                        }
                                    }

                                    KomaiButton {
                                        text: qsTr("Add")
                                        Layout.preferredWidth: 100
                                        onClicked: {
                                            plEditorW.editingModel.addRole(newPLVal.value);
                                            newPLLay.visible = false;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: Komai.paddingMedium

                        MatrixText {
                            text: qsTr("Move users up or down to change their permissions")
                            font.pixelSize: Math.floor(fontMetrics.font.pixelSize * 1.1)
                            Layout.fillWidth: true
                        }

                        ReorderableListview {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            model: plEditorW.editingModel.users

                            Column {
                                id: userEntryCompleter

                                property int index: 0

                                visible: false
                                width: parent.width
                                spacing: 1
                                z: 5

                                KomaiTextField {
                                    id: userEntry

                                    width: parent.width
                                    onTextEdited: {
                                        userCompleter.completer.searchString = text;
                                    }
                                    Keys.onPressed: {
                                        if (event.key == Qt.Key_Up || event.key == Qt.Key_Backtab) {
                                            event.accepted = true;
                                            userCompleter.up();
                                        } else if (event.key == Qt.Key_Down || event.key == Qt.Key_Tab) {
                                            event.accepted = true;
                                            if (event.key == Qt.Key_Tab && (event.modifiers & Qt.ShiftModifier))
                                                userCompleter.up();
                                            else
                                                userCompleter.down();
                                        } else if (event.matches(StandardKey.InsertParagraphSeparator)) {
                                            if (userCompleter.currentCompletion()) {
                                                userCompleter.finishCompletion();
                                            } else if (userEntry.text.startsWith("@") && userEntry.text.includes(":")) {
                                                userCompletionConnections.onCompletionSelected(userEntry.text);
                                            }
                                            event.accepted = true;
                                        } else if (event.matches(StandardKey.Cancel)) {
                                            userEntryCompleter.visible = false;
                                            userEntry.clear();
                                            event.accepted = true;
                                        }
                                    }
                                }

                                Completer {
                                    id: userCompleter

                                    visible: userEntry.text.length > 0
                                    width: parent.width
                                    roomId: plEditorW.roomSettings.roomId
                                    completerType: "user"
                                    bottomToTop: false
                                    fullWidth: true
                                    avatarHeight: Komai.listIconSize / 2
                                    avatarWidth: Komai.listIconSize / 2
                                    rowMargin: 2
                                    rowSpacing: 2
                                }
                            }

                            Connections {
                                id: userCompletionConnections

                                function onCompletionSelected(id) {
                                    console.log("selected: " + id);
                                    plEditorW.editingModel.users.add(userEntryCompleter.index, id);
                                    userEntry.clear();
                                    userEntryCompleter.visible = false;
                                }

                                function onCountChanged() {
                                    if (userCompleter.count > 0 && (userCompleter.currentIndex < 0 || userCompleter.currentIndex >= userCompleter.count))
                                        userCompleter.currentIndex = 0;
                                }

                                target: userCompleter
                            }

                            delegate: RowLayout {
                                id: row

                                Avatar {
                                    id: avatar

                                    Layout.preferredHeight: Komai.listIconSize / 2
                                    Layout.preferredWidth: Komai.listIconSize / 2
                                    Layout.leftMargin: 2
                                    userid: model ? model.mxid : ""
                                    url: {
                                        if (!model) return "";
                                        if (model.isUser)
                                            return (model.avatarUrl || "").replace("mxc://", "image://MxcImage/");
                                        else if (model.isCreator)
                                            return "image://colorimage/:/icons/icons/ui/ribbon_star.svg?" + palette.buttonText;
                                        else if (plEditorW.editingModel.adminLevel >= model.powerlevel)
                                            return "image://colorimage/:/icons/icons/ui/ribbon_star.svg?" + palette.buttonText;
                                        else if (plEditorW.editingModel.moderatorLevel >= model.powerlevel)
                                            return "image://colorimage/:/icons/icons/ui/ribbon.svg?" + palette.buttonText;
                                        else
                                            return "image://colorimage/:/icons/icons/ui/person.svg?" + palette.buttonText;
                                    }
                                    displayName: model ? model.displayName : ""
                                    enabled: false
                                }

                                Column {
                                    Layout.fillWidth: true

                                    Text {
                                        visible: model ? model.isUser : false
                                        text: model ? model.displayName : ""
                                        color: palette.text
                                    }

                                    Text {
                                        visible: model ? model.isUser : false
                                        text: model ? model.mxid : ""
                                        color: palette.text
                                    }

                                    Text {
                                        visible: model ? !model.isUser : false
                                        text: {
                                            if (!model) return "";
                                            let pl = model.powerlevel.toLocaleString(Qt.locale(), "f", 0);
                                            if (model.isCreator)
                                                return qsTr("Creator");
                                            if (plEditorW.editingModel.adminLevel == model.powerlevel)
                                                return qsTr("Administrator (%1)").arg(pl);
                                            else if (plEditorW.editingModel.moderatorLevel == model.powerlevel)
                                                return qsTr("Moderator (%1)").arg(pl);
                                            else if (plEditorW.editingModel.defaultUserLevel == model.powerlevel)
                                                return qsTr("User (%1)").arg(pl);
                                            else
                                                return qsTr("Custom (%1)").arg(pl);
                                        }
                                        color: palette.text
                                    }
                                }

                                ImageButton {
                                    Layout.alignment: Qt.AlignRight
                                    Layout.rightMargin: 2
                                    image: model ? (model.isUser ? ":/icons/icons/ui/dismiss.svg" : ":/icons/icons/ui/plus-circle.svg") : ""
                                    visible: model ? ((!model.isUser || model.removeable) && !model.isCreator) : false
                                    hoverEnabled: true
                                    toolTipVisible: hovered
                                    toolTipText: model ? (model.isUser ? qsTr("Remove user") : qsTr("Add user")) : ""
                                    onClicked: {
                                        if (model.isUser) {
                                            plEditorW.editingModel.users.remove(index);
                                        } else {
                                            userEntryCompleter.y = offset;
                                            userEntryCompleter.visible = true;
                                            userEntryCompleter.index = index;
                                            userEntry.forceActiveFocus();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            KomaiButton {
                Layout.alignment: Qt.AlignRight
                text: plEditorW.editingModel && plEditorW.editingModel.committing ? qsTr("Saving…") : qsTr("Save")
                enabled: plEditorW.editingModel && plEditorW.editingModel.loaded && !plEditorW.editingModel.committing
                highlighted: true
                onClicked: {
                    if (plEditorW.editingModel.isSpace) {
                        plEditorW.editingModel.updateSpacesModel();
                        plEditorW.close();
                        plEditorW.appRoot.showSpacePLApplyPrompt(roomSettings, plEditorW.editingModel);
                    } else {
                        plEditorW.editingModel.commit();
                        plEditorW.close();
                    }
                }
            }
        }
    }
}
