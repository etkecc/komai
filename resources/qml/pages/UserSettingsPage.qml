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
    property string scrollToSection: ""
    // True while the search box holds a query. Root's Escape shortcut reads
    // this to clear the search before falling back to closing the page.
    readonly property bool searchActive: (UserSettingsModel.searchQuery ?? "").length > 0
    function clearSearch() { settingsSearchField.clear(); }
    readonly property bool mirrored: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true
    property int sidebarWidth: {
        // Read font height to track font size changes in this binding
        var _d1 = sidebarNavFontMetrics.height;

        var maxWidth = sidebarNavFontMetrics.advanceWidth(qsTr("Back"));
        for (var i = 0; i < navModel.length; i++)
            maxWidth = Math.max(maxWidth, sidebarNavFontMetrics.advanceWidth(navModel[i].text));
        return Math.max(120, Math.ceil(Komai.paddingMedium + 24 + Komai.paddingMedium + maxWidth + Komai.paddingLarge));
    }
    property int headerIconSize: Komai.iconSize
    property var navModel: [
        { text: qsTr("Look & Feel"), icon: "qrc:/icons/icons/ui/toggles.svg", tab: UserSettingsModel.TabLookFeel },
        { text: qsTr("Navigation"), icon: "qrc:/icons/icons/ui/panel-left-header.svg", tab: UserSettingsModel.TabNavigation },
        { text: qsTr("Timeline"), icon: "qrc:/icons/icons/ui/speech-bubbles.svg", tab: UserSettingsModel.TabTimeline },
        { text: qsTr("Composer"), icon: "qrc:/icons/icons/ui/edit.svg", tab: UserSettingsModel.TabComposer },
        { text: qsTr("Desktop"), icon: "qrc:/icons/icons/ui/desktop.svg", tab: UserSettingsModel.TabDesktop },
        { text: qsTr("Calls"), icon: "qrc:/icons/icons/ui/place-call.svg", tab: UserSettingsModel.TabCalls },
        { text: qsTr("Network"), icon: "qrc:/icons/icons/ui/world.svg", tab: UserSettingsModel.TabNetwork },
        { text: qsTr("Account"), icon: "qrc:/icons/icons/ui/person.svg", tab: UserSettingsModel.TabAccount, requiresSession: true },
        { text: qsTr("Integrations"), icon: "qrc:/icons/icons/ui/integrations.svg", tab: UserSettingsModel.TabIntegrations },
        { text: qsTr("Application Profiles"), icon: "qrc:/icons/icons/ui/people.svg", tab: UserSettingsModel.TabApplicationProfiles },
        { text: qsTr("About Komai"), icon: "qrc:/logos/komai.svg", tab: UserSettingsModel.TabAbout }
    ]
    color: palette.window

    // Font metrics for dynamic sidebar width measurement
    FontMetrics {
        id: sidebarNavFontMetrics
        font.bold: true
        font.pointSize: Settings.uiFontSizePt
    }

    // Ctrl+F jumps to the search box (and selects any existing query so it
    // can be typed over), mirroring the room timeline's search shortcut.
    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: {
            settingsSearchField.forceActiveFocus();
            settingsSearchField.selectAll();
        }
    }

    // Sidebar + Content layout, with AttributionFooter capping the page
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
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
                        topPadding: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingSmall
                        bottomPadding: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingSmall
                        leftPadding: Komai.paddingMedium
                        rightPadding: Komai.paddingMedium

                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }

                        background: Rectangle {
                            color: headerBack.hovered ? palette.dark : "transparent"
                        }

                        onClicked: mainWindow.pop()

                        contentItem: Item {
                            implicitWidth: backIcon.implicitWidth + Komai.paddingMedium + backLabel.implicitWidth
                            implicitHeight: Math.max(backIcon.implicitHeight, backLabel.implicitHeight)

                            Image {
                                id: backIcon
                                width: 24
                                height: 24
                                anchors.verticalCenter: parent.verticalCenter
                                x: userSettingsDialog.mirrored ? parent.width - width : 0
                                source: "image://colorimage/:/icons/icons/ui/angle-arrow-left.svg?" + (headerBack.hovered ? palette.brightText : palette.text)
                                sourceSize.width: 24
                                sourceSize.height: 24
                                mirror: userSettingsDialog.mirrored
                            }

                            Label {
                                id: backLabel
                                width: Math.max(0, parent.width - backIcon.width - Komai.paddingMedium)
                                height: implicitHeight
                                anchors.verticalCenter: parent.verticalCenter
                                x: userSettingsDialog.mirrored ? 0 : backIcon.width + Komai.paddingMedium
                                text: qsTr("Back")
                                font.pointSize: Settings.uiFontSizePt
                                font.bold: true
                                color: headerBack.hovered ? palette.brightText : palette.text
                                horizontalAlignment: userSettingsDialog.mirrored ? Text.AlignRight : Text.AlignLeft
                                LayoutMirroring.enabled: false
                            }
                        }

                        KomaiToolTip {
                            anchorItem: headerBack
                            anchorX: headerBack.width / 2
                            anchorY: headerBack.height
                            gapX: Komai.paddingMedium
                            gapY: Komai.paddingMedium
                            text: qsTr("Back")
                            delay: Komai.tooltipDelay
                            requestedVisible: headerBack.hovered
                        }
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
                            // Re-evaluates when the global search query changes (the explicit read
                            // of UserSettingsModel.searchQuery is the binding's dependency hook;
                            // matchCountForTab() is Q_INVOKABLE and not a property).
                            readonly property bool hasActiveQuery: (UserSettingsModel.searchQuery ?? "").length > 0
                            readonly property int searchMatchCount: {
                                var _ = UserSettingsModel.searchQuery;
                                return UserSettingsModel.matchCountForTab(modelData.tab);
                            }
                            enabled: availableInCurrentSession
                            opacity: hasActiveQuery && searchMatchCount === 0 ? 0.6 : 1.0

                            HoverHandler {
                                cursorShape: navItem.enabled && !navItem.isActive ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }

                            width: ListView.view.width
                            height: Komai.navigationRowHeight
                            topPadding: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingSmall
                            bottomPadding: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingSmall
                            leftPadding: Komai.paddingMedium
                            rightPadding: Komai.paddingMedium

                            background: Rectangle {
                                color: navItem.backgroundColor
                            }

                            readonly property color hoverBackground: Qt.rgba(palette.dark.r * 0.30 + palette.window.r * 0.70, palette.dark.g * 0.30 + palette.window.g * 0.70, palette.dark.b * 0.30 + palette.window.b * 0.70, 1)
                            readonly property color selectedBackground: Qt.rgba(palette.dark.r * 0.85 + palette.window.r * 0.15, palette.dark.g * 0.85 + palette.window.g * 0.15, palette.dark.b * 0.85 + palette.window.b * 0.15, 1)

                            states: [
                                State {
                                    name: "hover"
                                    when: navItem.hovered && !navItem.isActive && navItem.enabled

                                    PropertyChanges {
                                        navItem {
                                            backgroundColor: navItem.hoverBackground
                                            textColor: palette.text
                                        }
                                    }
                                },
                                State {
                                    name: "active"
                                    when: navItem.isActive && navItem.enabled

                                    PropertyChanges {
                                        navItem {
                                            backgroundColor: navItem.selectedBackground
                                            textColor: palette.brightText
                                        }
                                    }
                                }
                            ]

                            onClicked: {
                                if (!enabled)
                                    return;
                                userSettingsDialog.currentTab = modelData.tab;
                            }

                            contentItem: Item {
                                implicitWidth: navIcon.implicitWidth + Komai.paddingMedium + navLabel.implicitWidth
                                implicitHeight: Math.max(navIcon.implicitHeight, navLabel.implicitHeight)

                                Image {
                                    id: navIcon
                                    width: 24
                                    height: 24
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: userSettingsDialog.mirrored ? parent.width - width : 0
                                    // Don't colorize the Komai logo (About tab)
                                    source: navItem.modelData.icon.startsWith("qrc:/logos/")
                                        ? navItem.modelData.icon
                                        : "image://colorimage/" + navItem.modelData.icon.replace("qrc:/", ":/") + "?" + navItem.textColor
                                    sourceSize.width: 24
                                    sourceSize.height: 24
                                }

                                Label {
                                    id: navLabel
                                    width: Math.max(0, parent.width - navIcon.width - Komai.paddingMedium - (navBadge.visible ? navBadge.width + Komai.paddingSmall : 0))
                                    height: implicitHeight
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: {
                                        if (userSettingsDialog.mirrored)
                                            return navBadge.visible ? navBadge.width + Komai.paddingSmall : 0;
                                        return navIcon.width + Komai.paddingMedium;
                                    }
                                    text: navItem.modelData.text
                                    color: navItem.enabled ? navItem.textColor : palette.buttonText
                                    font.pointSize: Settings.uiFontSizePt
                                    font.bold: navItem.isActive
                                    horizontalAlignment: userSettingsDialog.mirrored ? Text.AlignRight : Text.AlignLeft
                                    elide: userSettingsDialog.mirrored ? Text.ElideLeft : Text.ElideRight
                                    LayoutMirroring.enabled: false
                                }

                                NotificationBubble {
                                    id: navBadge
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: userSettingsDialog.mirrored ? 0 : parent.width - width
                                    unreadCount: navItem.searchMatchCount
                                    hasLoudNotification: false
                                    bubbleBackgroundColor: palette.highlight
                                    bubbleTextColor: palette.highlightedText
                                    mayBeVisible: navItem.hasActiveQuery
                                }
                            }

                            KomaiToolTip {
                                anchorItem: navItem
                                anchorX: navItem.width / 2
                                anchorY: navItem.height
                                gapX: Komai.paddingMedium
                                gapY: Komai.paddingMedium
                                text: qsTr("Available after login")
                                delay: Komai.tooltipDelay
                                requestedVisible: navItem.hovered && !navItem.enabled && navItem.requiresSession
                            }
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
                    // Use headerBack as the baseline so the right-side header
                    // visually aligns with the sidebar back button on
                    // Compact/Spacious, but grow past it on Dense where
                    // navigationRowHeight is shorter than the search field's
                    // implicit height and would otherwise crop it.
                    Layout.preferredHeight: Math.max(headerBack.Layout.preferredHeight,
                                                     settingsSearchField.implicitHeight + 2 * Komai.paddingSmall)
                    color: palette.alternateBase

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Komai.paddingMedium
                        anchors.rightMargin: Komai.paddingMedium
                        anchors.topMargin: Komai.paddingSmall
                        anchors.bottomMargin: Komai.paddingSmall
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
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Settings")
                            font.pointSize: Settings.uiFontSizePt * 1.1
                            font.bold: true
                            color: palette.text
                        }

                        Item { Layout.fillWidth: true }

                        KomaiSearchField {
                            id: settingsSearchField
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: Math.min(360, parent.width / 2)
                            placeholderText: qsTr("Search settings…")
                            text: UserSettingsModel.searchQuery
                            onTextChanged: {
                                if (text !== UserSettingsModel.searchQuery)
                                    UserSettingsModel.searchQuery = text;
                            }
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
                    LayoutMirroring.enabled: userSettingsDialog.mirrored
                    LayoutMirroring.childrenInherit: true

                    Rectangle {
                        anchors.fill: parent
                        color: palette.alternateBase
                        z: -1
                    }

                    source: {
                        switch (userSettingsDialog.currentTab) {
                        case UserSettingsModel.TabLookFeel:
                            return "settings/LookFeelTab.qml";
                        case UserSettingsModel.TabNavigation:
                            return "settings/NavigationTab.qml";
                        case UserSettingsModel.TabTimeline:
                            return "settings/TimelineTab.qml";
                        case UserSettingsModel.TabComposer:
                            return "settings/ComposerTab.qml";
                        case UserSettingsModel.TabDesktop:
                            return "settings/DesktopTab.qml";
                        case UserSettingsModel.TabCalls:
                            return "settings/CallsTab.qml";
                        case UserSettingsModel.TabNetwork:
                            return "settings/NetworkTab.qml";
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
                        if (userSettingsDialog.scrollToSection) {
                            item.scrollToTagId = userSettingsDialog.scrollToSection;
                            userSettingsDialog.scrollToSection = "";
                        }
                    }
                }
            }
        }

        AttributionFooter {}
    }
}
