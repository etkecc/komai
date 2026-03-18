// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    property bool standalone: false
    property string chooserText: qsTr("Choose an application profile to launch, or create a new one.")
    property string helpText: qsTr("Each application profile is a separate Komai instance with its own login, settings, and local data.")

    signal launchSucceeded(profileId: string)

    function refreshProfiles() {
        Komai.refreshApplicationProfiles();
    }

    function showError(message) {
        statusError = true;
        statusText = message;
    }

    function clearStatus() {
        statusError = false;
        statusText = "";
    }
    function launchProfile(profileId) {
        const error = Komai.launchApplicationProfile(profileId);
        if (error.length > 0) {
            root.showError(error);
            return;
        }

        root.clearStatus();
        root.launchSucceeded(profileId);
        root.closeStandaloneSoon();
    }
    function closeStandaloneSoon() {
        if (!root.standalone)
            return;
        root.enabled = false;
        Qt.callLater(function () {
            if (MainWindow)
                MainWindow.close();
            else
                Qt.quit();
        });
    }

    property string statusText: ""
    property bool statusError: false
    property string pendingDeleteProfileId: ""

    Component.onCompleted: refreshProfiles()

    OverlayDialog {
        id: createProfileDialog

        title: qsTr("Create Application Profile")
        titleIcon: ":/icons/icons/ui/plus-circle.svg"

        property string validationMessage: ""

        onOpened: {
            validationMessage = "";
            profileNameField.text = "";
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: palette.text
            text: qsTr("Profile name")
        }

        MatrixTextField {
            id: profileNameField

            Layout.fillWidth: true
            onAccepted: createProfileButton.clicked()
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: palette.buttonText
            text: qsTr("Examples: work, personal")
        }

        Label {
            Layout.fillWidth: true
            visible: createProfileDialog.validationMessage.length > 0
            wrapMode: Text.Wrap
            color: Komai.theme.error
            text: createProfileDialog.validationMessage
        }

        RowLayout {
            Layout.fillWidth: true

            KomaiButton {
                text: qsTr("Cancel")
                onClicked: createProfileDialog.close()
            }

            Item {
                Layout.fillWidth: true
            }

            KomaiButton {
                id: createProfileButton

                highlighted: true
                text: qsTr("Create and Launch")
                icon.source: "qrc:/icons/icons/ui/open-externally.svg"
                onClicked: {
                    const error = Komai.createAndLaunchApplicationProfile(profileNameField.text);
                    if (error.length > 0) {
                        createProfileDialog.validationMessage = error;
                        return;
                    }

                    createProfileDialog.close();
                    root.clearStatus();
                    root.launchSucceeded(profileNameField.text.trim());
                    root.refreshProfiles();
                    root.closeStandaloneSoon();
                }
            }
        }
    }

    OverlayDialog {
        id: deleteProfileDialog

        title: qsTr("Delete Application Profile '%1'?").arg(root.pendingDeleteProfileId)
        titleIcon: ":/icons/icons/ui/delete.svg"

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: palette.text
            text: qsTr("This removes its config, cache, local database, and stored secrets.")
        }

        RowLayout {
            Layout.fillWidth: true

            KomaiButton {
                text: qsTr("Cancel")
                onClicked: deleteProfileDialog.close()
            }

            Item {
                Layout.fillWidth: true
            }

            KomaiButton {
                highlighted: true
                text: qsTr("Delete")
                icon.source: "qrc:/icons/icons/ui/delete.svg"
                onClicked: {
                    const error = Komai.deleteApplicationProfile(root.pendingDeleteProfileId, root.standalone);
                    if (error.length > 0) {
                        root.showError(error);
                    } else {
                        root.clearStatus();
                    }
                    deleteProfileDialog.close();
                    root.pendingDeleteProfileId = "";
                    root.refreshProfiles();
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: palette.text
            font.pointSize: Settings.uiFontSizePt * 1.2
            text: root.chooserText
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: palette.buttonText
            text: root.helpText
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Komai.paddingLarge
        }

        RowLayout {
            Layout.fillWidth: true

            KomaiButton {
                id: refreshProfilesButton

                property bool refreshed: false

                text: refreshed ? qsTr("Refreshed") : qsTr("Refresh")
                icon.source: refreshed ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/refresh.svg"
                onClicked: {
                    root.refreshProfiles();
                    refreshed = true;
                    refreshFeedbackTimer.restart();
                }

                Timer {
                    id: refreshFeedbackTimer

                    interval: 1200
                    repeat: false
                    onTriggered: refreshProfilesButton.refreshed = false
                }
            }

            Item {
                Layout.fillWidth: true
            }

            KomaiButton {
                text: qsTr("New")
                icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
                onClicked: {
                    root.clearStatus();
                    createProfileDialog.open();
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Komai.paddingLarge
        }

        Label {
            id: statusLabel

            Layout.fillWidth: true
            visible: root.statusError && root.statusText.length > 0
            Layout.preferredHeight: visible ? implicitHeight : 0
            Layout.maximumHeight: visible ? implicitHeight : 0
            wrapMode: Text.Wrap
            color: Komai.theme.error
            text: root.statusText
        }

        Item {
            Layout.fillWidth: true
            visible: statusLabel.visible
            Layout.preferredHeight: visible ? Komai.paddingLarge : 0
            Layout.maximumHeight: visible ? Komai.paddingLarge : 0
        }

        Label {
            Layout.fillWidth: true
            visible: Komai.applicationProfiles.length === 0
            Layout.preferredHeight: visible ? implicitHeight : 0
            Layout.maximumHeight: visible ? implicitHeight : 0
            wrapMode: Text.Wrap
            color: palette.buttonText
            text: qsTr("No profiles found yet.")
        }

        ListView {
            id: profilesList

            readonly property bool hasVerticalOverflow: contentHeight > height
            readonly property real scrollbarGutterPadding: Komai.paddingSmall
            readonly property real reservedScrollbarWidth: hasVerticalOverflow
                ? Math.max(scrollbar.width, scrollbar.implicitWidth) + scrollbarGutterPadding
                : 0

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Komai.paddingLarge
            model: Komai.applicationProfiles
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AlwaysOff
            }
            ScrollBar.vertical: ScrollBar {
                id: scrollbar

                parent: profilesList
                policy: profilesList.contentHeight > profilesList.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            delegate: Rectangle {
                id: row

                required property var modelData
                readonly property bool canLaunch: root.standalone || !row.modelData.isCurrent
                readonly property bool canDelete: root.standalone || !row.modelData.isCurrent
                readonly property bool isHovered: rowHoverHandler.hovered && row.canLaunch

                readonly property color accentColor: modelData.themeAccentColor
                    ? modelData.themeAccentColor
                    : palette.highlight
                readonly property color rowWindowColor: modelData.themeWindowColor
                    ? modelData.themeWindowColor
                    : palette.window
                readonly property color rowDarkColor: modelData.themeDarkColor
                    ? modelData.themeDarkColor
                    : palette.dark
                readonly property color rowTextColor: modelData.themeTextColor
                    ? modelData.themeTextColor
                    : palette.text
                readonly property color rowBrightTextColor: modelData.themeBrightTextColor
                    ? modelData.themeBrightTextColor
                    : palette.brightText
                readonly property color primaryTextColor: isHovered ? rowBrightTextColor : rowTextColor
                readonly property color secondaryTextColor: isHovered ? rowBrightTextColor : rowTextColor
                readonly property color profileNameBadgeTextColor: accentColor
                readonly property color profileNameBadgeBackgroundColor: "transparent"
                readonly property color profileNameBadgeBorderColor: accentColor

                width: ListView.view.width - profilesList.reservedScrollbarWidth
                implicitHeight: rowContent.implicitHeight + Komai.paddingMedium * 2
                height: implicitHeight
                radius: Komai.paddingMedium
                border.width: 1
                border.color: row.isHovered ? row.rowDarkColor : Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.35)
                color: row.isHovered ? row.rowDarkColor : row.rowWindowColor

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                Behavior on border.color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                HoverHandler {
                    id: rowHoverHandler

                    enabled: row.canLaunch
                    cursorShape: Qt.PointingHandCursor
                }

                MouseArea {
                    id: cardMouseArea

                    anchors.fill: parent
                    enabled: row.canLaunch
                    hoverEnabled: true
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: root.launchProfile(row.modelData.id)
                }

                ColumnLayout {
                    id: rowContent

                    z: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingSmall

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall

                        Rectangle {
                            radius: 8
                            color: row.profileNameBadgeBackgroundColor
                            border.width: 1
                            border.color: row.profileNameBadgeBorderColor
                            implicitHeight: profileNameLabel.implicitHeight + Komai.paddingSmall
                            implicitWidth: profileNameLabel.implicitWidth + Komai.paddingMedium * 2

                            Label {
                                id: profileNameLabel

                                anchors.centerIn: parent
                                text: row.modelData.id
                                color: row.profileNameBadgeTextColor
                                font.bold: true
                                font.pointSize: Settings.uiFontSizePt * 1.1
                            }
                        }

                        Rectangle {
                            visible: row.modelData.isCurrent && !root.standalone
                            radius: 8
                            color: Qt.rgba(Komai.theme.success.r, Komai.theme.success.g, Komai.theme.success.b, 0.20)
                            implicitHeight: currentLabel.implicitHeight + Komai.paddingSmall
                            implicitWidth: currentLabel.implicitWidth + Komai.paddingMedium * 2

                            Label {
                                id: currentLabel

                                anchors.centerIn: parent
                                text: qsTr("Current")
                                color: row.primaryTextColor
                                font.pointSize: Settings.uiFontSizePt * 1.1
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        KomaiButton {
                            text: qsTr("Delete")
                            display: row.width >= 500 ? AbstractButton.TextBesideIcon : AbstractButton.IconOnly
                            icon.source: "qrc:/icons/icons/ui/delete.svg"
                            enabled: row.canDelete
                            toolTipVisible: hovered
                            toolTipText: qsTr("Delete")
                            onClicked: {
                                root.clearStatus();
                                root.pendingDeleteProfileId = row.modelData.id;
                                deleteProfileDialog.open();
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall

                        Image {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: Math.ceil(Settings.uiFontSizePt * 1.2)
                            Layout.preferredHeight: Math.ceil(Settings.uiFontSizePt * 1.2)
                            source: "image://colorimage/:/icons/icons/ui/person.svg?" + row.primaryTextColor
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: row.primaryTextColor
                            text: row.modelData.userId && row.modelData.userId.length > 0
                                ? qsTr("User: %1").arg(row.modelData.userId)
                                : qsTr("User: not signed in yet")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall

                        Image {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: Math.ceil(Settings.uiFontSizePt * 1.2)
                            Layout.preferredHeight: Math.ceil(Settings.uiFontSizePt * 1.2)
                            source: "image://colorimage/:/icons/icons/ui/world.svg?" + row.secondaryTextColor
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: row.secondaryTextColor
                            text: row.modelData.homeserver && row.modelData.homeserver.length > 0
                                ? qsTr("Homeserver: %1").arg(row.modelData.homeserver)
                                : qsTr("Homeserver: not available")
                        }
                    }
                }
            }
        }
    }
}
