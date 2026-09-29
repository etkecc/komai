// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../components/"
import "../ui/"

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: palette.window
    }

    // Caps the failure view's line length by characters rather than pixels,
    // so it follows the font size and still fits each sentence on one line.
    FontMetrics {
        id: detailMetrics

        font.pointSize: Settings.uiFontSizePt * 1.05
    }

    LoadingSplash {
        anchors.fill: parent
        visible: !MainWindow.startupFailed
        headline: MainWindow.startupHeadline
        detail: MainWindow.startupDetail
    }

    // Shown when the session exists but its local data can't be used. Signing
    // in again would not help, so this offers retry and the data location.
    ScrollView {
        id: failureView

        anchors.fill: parent
        visible: MainWindow.startupFailed
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(failureView.availableWidth - Komai.paddingLarge * 4,
                            detailMetrics.averageCharacterWidth * 100)
            x: (failureView.availableWidth - width) / 2
            spacing: Komai.paddingLarge

            Item {
                Layout.preferredHeight: Komai.paddingLarge * 2
            }

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                source: "image://colorimage/:/icons/icons/ui/warning.svg?" + palette.text
                sourceSize.width: 64
                sourceSize.height: 64
            }

            Label {
                Layout.fillWidth: true
                text: MainWindow.startupHeadline
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.7
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                text: MainWindow.startupDetail
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.05
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingLarge
                spacing: Komai.paddingLarge

                KomaiButton {
                    text: qsTr("Try again")
                    icon.source: "qrc:/icons/icons/ui/arrow-clockwise.svg"
                    highlighted: true
                    onClicked: MainWindow.retryStartup()
                }

                KomaiButton {
                    visible: MainWindow.startupDataPath.length > 0
                    text: qsTr("Open data folder")
                    icon.source: "qrc:/icons/icons/ui/folder-open.svg"
                    onClicked: MainWindow.openStartupDataFolder()
                }

                KomaiButton {
                    text: qsTr("Quit")
                    icon.source: "qrc:/icons/icons/ui/power-off.svg"
                    onClicked: Qt.quit()
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingLarge * 2
                text: qsTr("Technical details")
                color: palette.buttonText
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                visible: technicalDetails.text.length > 0
            }

            // Paths and error strings read better left-aligned; the box keeps
            // them lined up with the centered content around it.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: technicalDetails.implicitHeight + Komai.paddingMedium * 2
                visible: technicalDetails.text.length > 0
                radius: Komai.paddingMedium
                color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.06)
                border.width: 1
                border.color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.12)

                TextEdit {
                    id: technicalDetails

                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: palette.buttonText
                    selectedTextColor: palette.highlightedText
                    selectionColor: palette.highlight
                    font.family: "monospace"
                    font.pointSize: Settings.uiFontSizePt * 0.9
                    text: {
                        const lines = [];
                        if (MainWindow.startupDataPath.length > 0)
                            lines.push(qsTr("Data folder: %1").arg(MainWindow.startupDataPath));
                        if (MainWindow.startupTechnicalDetail.length > 0)
                            lines.push(MainWindow.startupTechnicalDetail);
                        return lines.join("\n\n");
                    }
                }
            }

            Item {
                Layout.preferredHeight: Komai.paddingLarge * 2
            }
        }
    }
}
