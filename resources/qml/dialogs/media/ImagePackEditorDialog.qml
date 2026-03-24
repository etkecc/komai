// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../ui"
import "../../components"
import Qt.labs.platform 1.1
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import cc.etke.komai 1.0

OverlayDialog {
    id: win

    property int avatarSize: Math.ceil(fontMetrics.lineSpacing * 2.3)
    property SingleImagePackModel imagePack
    property int currentImageIndex: -1
    readonly property int stickerDim: 128
    readonly property int stickerDimPad: 128 + Komai.paddingSmall

    title: qsTr("Editing image pack")
    titleIcon: ":/icons/icons/ui/edit.svg"
    overlayDialogMinWidth: 600
    overlayDialogMaxWidthRatio: 0.85

    AdaptiveLayout {
        id: adaptiveView

        Layout.fillWidth: true
        Layout.preferredHeight: {
            var vh = win.overlayDialogViewport ? win.overlayDialogViewport.height : 700;
            return Math.min(520, Math.round(vh * 0.55));
        }
        singlePageMode: false
        pageIndex: 0

        AdaptiveLayoutElement {
            id: packlistC

            visible: true
            minimumWidth: 200
            collapsedWidth: 200
            preferredWidth: 300
            maximumWidth: 300
            clip: true

            ListView {
                //required property bool isEmote
                //required property bool isSticker

                model: imagePack


                header: AvatarListTile {
                    title: imagePack.packname
                    avatarUrl: imagePack.avatarUrl
                    roomid: imagePack.statekey
                    subtitle: imagePack.statekey
                    index: -1
                    selectedIndex: currentImageIndex

                    TapHandler {
                        onSingleTapped: currentImageIndex = -1
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        height: parent.height - Komai.paddingSmall * 2
                        width: 3
                        color: palette.highlight
                    }

                }

                footer: Button {
                    onClicked: addFilesDialog.open()
                    width: ListView.view.width
                    text: qsTr("Add images")

                    FileDialog {
                        id: addFilesDialog

                        folder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
                        fileMode: FileDialog.OpenFiles
                        nameFilters: [qsTr("Images (*.png *.webp *.gif *.jpg *.jpeg)")]
                        title: qsTr("Select images for pack")
                        acceptLabel: qsTr("Add to pack")
                        onAccepted: imagePack.addStickers(files)
                    }

                }

                delegate: AvatarListTile {
                    id: packItem

                    property color background: palette.window
                    property color importantText: palette.text
                    property color unimportantText: palette.buttonText
                    property color bubbleBackground: palette.highlight
                    property color bubbleText: palette.highlightedText
                    required property string shortCode
                    required property string url
                    required property string body

                    title: shortCode
                    subtitle: body
                    avatarUrl: url
                    selectedIndex: currentImageIndex
                    crop: false

                    TapHandler {
                        onSingleTapped: currentImageIndex = index
                    }

                }

            }

        }

        AdaptiveLayoutElement {
            id: packinfoC

            Rectangle {
                color: palette.window

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    visible: currentImageIndex == -1
                    enabled: visible
                    columns: 2
                    rowSpacing: Komai.paddingLarge

                    Avatar {
                        Layout.columnSpan: 2
                        url: imagePack.avatarUrl.replace("mxc://", "image://MxcImage/")
                        displayName: imagePack.packname
                        roomid: imagePack.statekey
                        Layout.preferredHeight: 130
                        Layout.preferredWidth: 130
                        crop: false
                        Layout.alignment: Qt.AlignHCenter

                        ImageButton {
                            hoverEnabled: true
                            toolTipText: qsTr("Change the overview image for this pack")
                            toolTipVisible: hovered
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.leftMargin: Komai.paddingMedium
                            anchors.topMargin: Komai.paddingMedium
                            image: ":/icons/icons/ui/edit.svg"
                            onClicked: addAvatarDialog.open()

                            FileDialog {
                                id: addAvatarDialog

                                folder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
                                fileMode: FileDialog.OpenFile
                                nameFilters: [qsTr("Overview Image (*.png *.webp *.jpg *.jpeg)")]
                                title: qsTr("Select overview image for pack")
                                onAccepted: imagePack.setAvatar(file)
                            }
                        }
                    }

                    KomaiTextField {
                        id: statekeyField

                        visible: imagePack.roomid
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        placeholderText: qsTr("State key")
                        text: imagePack.statekey
                        onTextEdited: imagePack.statekey = text
                    }

                    KomaiTextField {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        placeholderText: qsTr("Packname")
                        text: imagePack.packname
                        onTextEdited: imagePack.packname = text
                    }

                    KomaiTextField {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        placeholderText: qsTr("Attribution")
                        text: imagePack.attribution
                        onTextEdited: imagePack.attribution = text
                    }

                    MatrixText {
                        Layout.margins: statekeyField.padding
                        font.weight: Font.DemiBold
                        text: qsTr("Use as Emoji")
                    }

                    ToggleButton {
                        checked: imagePack.isEmotePack
                        onCheckedChanged: imagePack.isEmotePack = checked
                        Layout.alignment: Qt.AlignRight
                    }

                    MatrixText {
                        Layout.margins: statekeyField.padding
                        font.weight: Font.DemiBold
                        text: qsTr("Use as Sticker")
                    }

                    ToggleButton {
                        checked: imagePack.isStickerPack
                        onCheckedChanged: imagePack.isStickerPack = checked
                        Layout.alignment: Qt.AlignRight
                    }

                    Item {
                        Layout.columnSpan: 2
                        Layout.fillHeight: true
                    }

                }

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    visible: currentImageIndex >= 0
                    enabled: visible
                    columns: 2
                    rowSpacing: Komai.paddingLarge

                    function imgData(role) {
                        if (currentImageIndex < 0) return undefined;
                        return imagePack.data(imagePack.index(currentImageIndex, 0), role);
                    }

                    Avatar {
                        Layout.columnSpan: 2
                        url: {
                            var u = parent.imgData(SingleImagePackModel.Url);
                            return u ? u.replace("mxc://", "image://MxcImage/") + "?scale" : "";
                        }
                        displayName: parent.imgData(SingleImagePackModel.ShortCode) || ""
                        roomid: displayName
                        Layout.preferredHeight: 130
                        Layout.preferredWidth: 130
                        crop: false
                        Layout.alignment: Qt.AlignHCenter
                    }

                        KomaiTextField {
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            placeholderText: qsTr("Shortcode")
                            property int bindingCounter: 0
                            text: {
                                const currentCode = parent.imgData(SingleImagePackModel.ShortCode);
                                if (bindingCounter % 2 === -1)
                                    return currentCode || "";
                                return currentCode || "";
                            }
                            onTextEdited: {
                                imagePack.setData(imagePack.index(currentImageIndex, 0), text, SingleImagePackModel.ShortCode);
                                // force text field to update in case the model disagreed with the new value.
                                bindingCounter++;
                            }
                    }

                    KomaiTextField {
                        id: bodyField

                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        placeholderText: qsTr("Body")
                        text: parent.imgData(SingleImagePackModel.Body) || ""
                        onTextEdited: imagePack.setData(imagePack.index(currentImageIndex, 0), text, SingleImagePackModel.Body)
                    }

                    MatrixText {
                        Layout.margins: bodyField.padding
                        font.weight: Font.DemiBold
                        text: qsTr("Use as Emoji")
                    }

                    ToggleButton {
                        checked: !!parent.imgData(SingleImagePackModel.IsEmote)
                        onCheckedChanged: imagePack.setData(imagePack.index(currentImageIndex, 0), checked, SingleImagePackModel.IsEmote)
                        Layout.alignment: Qt.AlignRight
                    }

                    MatrixText {
                        Layout.margins: bodyField.textPadding
                        font.weight: Font.DemiBold
                        text: qsTr("Use as Sticker")
                    }

                    ToggleButton {
                        checked: !!parent.imgData(SingleImagePackModel.IsSticker)
                        onCheckedChanged: imagePack.setData(imagePack.index(currentImageIndex, 0), checked, SingleImagePackModel.IsSticker)
                        Layout.alignment: Qt.AlignRight
                    }

                    MatrixText {
                        Layout.margins: bodyField.textPadding
                        font.weight: Font.DemiBold
                        text: qsTr("Remove from pack")
                    }

                    KomaiButton {
                        text: qsTr("Remove")
                        onClicked: {
                            let temp = currentImageIndex;
                            currentImageIndex = -1;
                            imagePack.remove(temp);
                        }
                        Layout.alignment: Qt.AlignRight
                    }

                    Item {
                        Layout.columnSpan: 2
                        Layout.fillHeight: true
                    }

                }

            }

        }

    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        KomaiButton {
            text: qsTr("Cancel")
            onClicked: win.close()
        }

        Item {
            Layout.fillWidth: true
        }

        KomaiButton {
            text: qsTr("Save")
            highlighted: true
            onClicked: {
                imagePack.save();
                win.close();
            }
        }
    }

}
