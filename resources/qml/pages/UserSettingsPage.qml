// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

Rectangle {
    id: userSettingsDialog
    objectName: "userSettingsPage"

    property int collapsePoint: 600
    property bool collapsed: width < collapsePoint
    property int currentTab: UserSettingsModel.TabLookFeel
    property int sidebarWidth: {
        // Read font heights to track font size changes in this binding
        var _d1 = sidebarNavFontMetrics.height;
        var _d2 = sidebarHeaderFontMetrics.height;

        var maxWidth = sidebarHeaderFontMetrics.advanceWidth(qsTr("Back to main"));
        for (var i = 0; i < navModel.length; i++)
            maxWidth = Math.max(maxWidth, sidebarNavFontMetrics.advanceWidth(navModel[i].text));
        return Math.max(120, Math.ceil(Komai.paddingMedium + 24 + Komai.paddingMedium + maxWidth + Komai.paddingLarge));
    }
    property int headerIconSize: Komai.barIconSize
    property int headerButtonPaddingH: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    property var navModel: [
        { text: qsTr("Look & Feel"), icon: "qrc:/icons/icons/ui/toggles.svg", tab: UserSettingsModel.TabLookFeel },
        { text: qsTr("Sidebars"), icon: "qrc:/icons/icons/ui/sidebar.svg", tab: UserSettingsModel.TabSidebars },
        { text: qsTr("Timeline"), icon: "qrc:/icons/icons/ui/speech-bubbles.svg", tab: UserSettingsModel.TabTimeline },
        { text: qsTr("Composer"), icon: "qrc:/icons/icons/ui/edit.svg", tab: UserSettingsModel.TabComposer },
        { text: qsTr("Notifications"), icon: "qrc:/icons/icons/ui/alert.svg", tab: UserSettingsModel.TabNotifications },
        { text: qsTr("Calls"), icon: "qrc:/icons/icons/ui/place-call.svg", tab: UserSettingsModel.TabCalls },
        { text: qsTr("Network"), icon: "qrc:/icons/icons/ui/world.svg", tab: UserSettingsModel.TabNetwork },
        { text: qsTr("Privacy"), icon: "qrc:/icons/icons/ui/eye-hide.svg", tab: UserSettingsModel.TabPrivacy },
        { text: qsTr("Encryption"), icon: "qrc:/icons/icons/ui/shield-regular.svg", tab: UserSettingsModel.TabEncryption },
        { text: qsTr("Account"), icon: "qrc:/icons/icons/ui/person.svg", tab: UserSettingsModel.TabAccount, requiresSession: true },
        { text: qsTr("Integrations"), icon: "qrc:/icons/icons/ui/integrations.svg", tab: UserSettingsModel.TabIntegrations },
        { text: qsTr("Application Profiles"), icon: "qrc:/icons/icons/ui/people.svg", tab: UserSettingsModel.TabApplicationProfiles },
        { text: qsTr("About"), icon: "qrc:/logos/komai.svg", tab: UserSettingsModel.TabAbout }
    ]
    color: palette.window

    // Handle Escape key to go back
    focus: true
    Keys.onEscapePressed: mainWindow.pop()

    // Font metrics for dynamic sidebar width measurement
    FontMetrics {
        id: sidebarNavFontMetrics
        font.bold: true
        font.pointSize: Settings.uiFontSizePt
    }

    FontMetrics {
        id: sidebarHeaderFontMetrics
        font.bold: true
        font.pointSize: Settings.uiFontSizePt * 1.1
    }

    // Sidebar + Content layout
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar
        Rectangle {
            id: sidebar
            Layout.preferredWidth: userSettingsDialog.sidebarWidth
            Layout.fillHeight: true
            color: palette.alternateBase

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header with back button and title - full width clickable
                ItemDelegate {
                    id: headerBack
                    Layout.fillWidth: true
                    Layout.preferredHeight: Komai.navigationRowHeight
                    padding: Komai.paddingSmall
                    leftPadding: Komai.paddingMedium
                    rightPadding: Komai.paddingMedium

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }

                    background: Rectangle {
                        color: headerBack.hovered ? palette.dark : "transparent"
                    }

                    onClicked: mainWindow.pop()

                    contentItem: RowLayout {
                        spacing: Komai.paddingMedium

                        Image {
                            Layout.preferredWidth: userSettingsDialog.headerIconSize
                            Layout.preferredHeight: userSettingsDialog.headerIconSize
                            Layout.alignment: Qt.AlignVCenter
                            source: "image://colorimage/:/icons/icons/ui/angle-arrow-left.svg?" + (headerBack.hovered ? palette.brightText : palette.text)
                            sourceSize.width: userSettingsDialog.headerIconSize
                            sourceSize.height: userSettingsDialog.headerIconSize
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Back to main")
                            font.pointSize: Settings.uiFontSizePt * 1.1
                            font.bold: true
                            color: headerBack.hovered ? palette.brightText : palette.text
                        }
                    }

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Back to main")
                    ToolTip.delay: Komai.tooltipDelay
                }

                // Navigation items
                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    model: userSettingsDialog.navModel

                    delegate: ItemDelegate {
                        id: navItem
                        required property var modelData
                        required property int index
                        property bool isActive: userSettingsDialog.currentTab === modelData.tab
                        property bool requiresSession: modelData.requiresSession === true
                        property bool availableInCurrentSession: !requiresSession || Settings.hasActiveSession
                        property color backgroundColor: palette.window
                        property color textColor: palette.text
                        enabled: availableInCurrentSession

                        HoverHandler {
                            cursorShape: navItem.enabled && !navItem.isActive ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }

                        width: ListView.view.width
                        height: Komai.navigationRowHeight
                        padding: Komai.paddingSmall
                        leftPadding: Komai.paddingMedium
                        rightPadding: Komai.paddingMedium

                        background: Rectangle {
                            color: navItem.backgroundColor
                        }

                        states: [
                            State {
                                name: "hover"
                                when: navItem.hovered && !navItem.isActive && navItem.enabled

                                PropertyChanges {
                                    navItem {
                                        backgroundColor: palette.dark
                                        textColor: palette.brightText
                                    }
                                }
                            },
                            State {
                                name: "active"
                                when: navItem.isActive && navItem.enabled

                                PropertyChanges {
                                    navItem {
                                        backgroundColor: palette.highlight
                                        textColor: palette.highlightedText
                                    }
                                }
                            }
                        ]

                        onClicked: {
                            if (!enabled)
                                return;
                            userSettingsDialog.currentTab = modelData.tab;
                        }

                        contentItem: RowLayout {
                            spacing: Komai.paddingMedium

                            Image {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignVCenter
                                // Don't colorize the Komai logo (About tab)
                                source: navItem.modelData.icon.startsWith("qrc:/logos/")
                                    ? navItem.modelData.icon
                                    : "image://colorimage/" + navItem.modelData.icon.replace("qrc:/", ":/") + "?" + navItem.textColor
                                sourceSize.width: 24
                                sourceSize.height: 24
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: navItem.modelData.text
                                color: navItem.enabled ? navItem.textColor : palette.buttonText
                                font.pointSize: Settings.uiFontSizePt
                                font.bold: navItem.isActive
                                elide: Text.ElideRight
                            }
                        }

                        ToolTip.visible: hovered && !enabled && requiresSession
                        ToolTip.delay: Komai.tooltipDelay
                        ToolTip.text: qsTr("Available after login")
                    }
                }
            }
        }

        // Separator between sidebar and content
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Komai.theme.separator
        }

        // Settings content area
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: headerBack.Layout.preferredHeight
                color: palette.alternateBase

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Komai.paddingMedium
                    anchors.rightMargin: Komai.paddingMedium
                    spacing: Komai.paddingMedium

                    Image {
                        Layout.preferredWidth: userSettingsDialog.headerIconSize
                        Layout.preferredHeight: userSettingsDialog.headerIconSize
                        Layout.alignment: Qt.AlignVCenter
                        source: "qrc:/logos/komai.svg"
                        sourceSize.width: userSettingsDialog.headerIconSize
                        sourceSize.height: userSettingsDialog.headerIconSize
                        fillMode: Image.PreserveAspectFit
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("Settings")
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        font.bold: true
                        color: palette.text
                        elide: Text.ElideRight
                    }

                    ImageButton {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: userSettingsDialog.headerIconSize
                        Layout.preferredHeight: userSettingsDialog.headerIconSize
                        leftPadding: userSettingsDialog.headerButtonPaddingH
                        rightPadding: userSettingsDialog.headerButtonPaddingH
                        topPadding: 0
                        bottomPadding: 0
                        ToolTip.delay: Komai.tooltipDelay
                        ToolTip.text: qsTr("Close")
                        ToolTip.visible: hovered
                        image: ":/icons/icons/ui/dismiss.svg"

                        onClicked: mainWindow.pop()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Komai.theme.separator
            }

            // Loader loads only the active tab
            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true

                source: {
                    switch (userSettingsDialog.currentTab) {
                    case UserSettingsModel.TabLookFeel:
                        return "settings/LookFeelTab.qml";
                    case UserSettingsModel.TabSidebars:
                        return "settings/SidebarsTab.qml";
                    case UserSettingsModel.TabTimeline:
                        return "settings/TimelineTab.qml";
                    case UserSettingsModel.TabComposer:
                        return "settings/ComposerTab.qml";
                    case UserSettingsModel.TabNotifications:
                        return "settings/NotificationsTab.qml";
                    case UserSettingsModel.TabCalls:
                        return "settings/CallsTab.qml";
                    case UserSettingsModel.TabNetwork:
                        return "settings/NetworkTab.qml";
                    case UserSettingsModel.TabPrivacy:
                        return "settings/PrivacyTab.qml";
                    case UserSettingsModel.TabEncryption:
                        return "settings/EncryptionTab.qml";
                    case UserSettingsModel.TabAccount:
                        return "settings/AccountTab.qml";
                    case UserSettingsModel.TabApplicationProfiles:
                        return "settings/ApplicationProfilesTab.qml";
                    case UserSettingsModel.TabIntegrations:
                        return "settings/IntegrationsTab.qml";
                    case UserSettingsModel.TabAbout:
                        return "settings/AboutTab.qml";
                    }
                }

                onLoaded: {
                    item.collapsed = Qt.binding(function() { return userSettingsDialog.collapsed; });
                }
            }
        }
    }
}
