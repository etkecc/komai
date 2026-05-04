// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../../components"
import "../../components/SettingsRows"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import cc.etke.komai

Item {
    id: root

    required property int tabFilter
    property bool collapsed: false
    property string scrollToTagId: ""
    readonly property bool mirrored: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
    // When search is active and yields no matches, hide custom header/footer
    // content (which isn't search-aware in v1) and show the empty-state label.
    readonly property bool hasActiveQuery: (UserSettingsModel.searchQuery ?? "").length > 0
    readonly property bool searchHidesEverything: hasActiveQuery && settingsRepeater.count === 0
    LayoutMirroring.childrenInherit: true

    onScrollToTagIdChanged: {
        if (scrollToTagId && settingsRepeater.count > 0)
            scrollTimer.restart();
    }
    // Extra content to show above the repeater (used by AboutTab for logo)
    property Component headerContent: null
    property Component footerContent: null

    // Map a `komai://settings/<tab>[/<section>]` link to a tab change + scroll.
    // Adding a new entry here is the only thing required to make a new
    // settings deeplink work from any help-text link in any settings tab.
    readonly property var settingsDeeplinkTabs: ({
        "look-feel": UserSettingsModel.TabLookFeel,
        "navigation": UserSettingsModel.TabNavigation,
        "timeline": UserSettingsModel.TabTimeline,
        "composer": UserSettingsModel.TabComposer,
        "desktop": UserSettingsModel.TabDesktop,
        "calls": UserSettingsModel.TabCalls,
        "network": UserSettingsModel.TabNetwork,
        "account": UserSettingsModel.TabAccount,
        "integrations": UserSettingsModel.TabIntegrations,
        "application-profiles": UserSettingsModel.TabApplicationProfiles,
        "about": UserSettingsModel.TabAbout
    })

    function openSettingsDeeplink(link) {
        // Strip "komai://settings/" prefix and split into <tab>/<section>.
        var path = link.slice("komai://settings/".length);
        var parts = path.split("/");
        var tabName = parts[0];
        var section = parts.slice(1).join("/");
        if (!(tabName in settingsDeeplinkTabs))
            return;
        MainWindow.showUserSettingsPage(settingsDeeplinkTabs[tabName], section);
    }

    Flickable {
        id: scroll
        anchors.left: parent.left
        anchors.right: scrollBar.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        contentWidth: width
        contentHeight: grid.height + Komai.paddingLarge
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: grid

            spacing: Komai.paddingSmall
            property real sideMargin: Komai.paddingMedium
            property int settingRowStackBreakpoint: Komai.settingRowStackBreakpoint
            width: Math.max(0, scroll.width - sideMargin * 2)
            x: sideMargin

            Loader {
                id: headerLoader
                Layout.fillWidth: true
                active: root.headerContent !== null && !root.searchHidesEverything
                visible: active
                sourceComponent: root.headerContent
            }

            Repeater {
                id: settingsRepeater
                model: UserSettingsModel.modelForTab(root.tabFilter)

                onCountChanged: {
                    if (root.scrollToTagId && count > 0)
                        scrollTimer.restart();
                }

                delegate: Item {
                    id: r

                    required property var model
                    required property int index
                    Layout.fillWidth: true
                    implicitHeight: row.implicitHeight
                    width: grid.width

                    readonly property bool isSelfContainedCardRow: r.model.type == UserSettingsModel.CommunityFilterRow || r.model.type == UserSettingsModel.SpacesFilterSection
                    readonly property bool isFullWidthPreviewRow: r.model.type == UserSettingsModel.TimelinePreview || r.model.type == UserSettingsModel.AvatarPreview || r.isSelfContainedCardRow
                    readonly property bool useStackedLayout: grid.width < grid.settingRowStackBreakpoint
                    readonly property bool hasSettingIcon: !r.isFullWidthPreviewRow && !!r.model.icon
                    readonly property string settingIconSource: {
                        if (!r.hasSettingIcon) {
                            return "";
                        }
                        if (r.model.icon.startsWith("image://colorimage/")) {
                            return r.model.icon;
                        }
                        return "image://colorimage/" + r.model.icon + "?" + (rowHover.hovered ? palette.brightText : palette.buttonText);
                    }
                    readonly property real controlWidth: r.useStackedLayout
                        ? Math.max(0, grid.width - Komai.paddingSmall * 2)
                        : Math.min(600, Math.max(240, grid.width - Komai.paddingLarge * 2))
                    readonly property bool hasSyncedToMatrix: r.model.type != UserSettingsModel.SectionTitle
                        && !!r.model.syncedToMatrix
                    readonly property bool hasDescription: r.model.type != UserSettingsModel.SectionTitle
                        && !r.isSelfContainedCardRow
                        && !!r.model.description

                    HoverHandler {
                        id: rowHover
                        blocking: false
                        enabled: settingRow.visible
                    }

                    Rectangle {
                        anchors.fill: row
                        color: settingRow.visible && rowHover.hovered && !r.isFullWidthPreviewRow ? palette.dark : palette.window
                        radius: Komai.paddingMedium
                        visible: settingRow.visible && !r.isSelfContainedCardRow
                        z: -1
                    }

                    ColumnLayout {
                        id: row
                        width: grid.width
                        height: implicitHeight
                        spacing: Komai.paddingMedium

                        SettingsSection {
                            id: sectionLabel
                            Layout.fillWidth: true
                            Layout.topMargin: r.index === 0 ? Komai.paddingMedium : Komai.paddingLarge
                            Layout.bottomMargin: Komai.paddingSmall
                            visible: r.model.type == UserSettingsModel.SectionTitle
                            label: r.model.name ?? ""
                            helperText: {
                                if (r.model.tagId === "notifications-system-section"
                                        && Settings.hasActiveSession
                                        && !Settings.notificationsAccountEnabled) {
                                    return qsTr("Options below have no effect because account notifications are disabled above.");
                                }
                                return "";
                            }
                            helperColor: r.model.tagId === "notifications-system-section"
                                && Settings.hasActiveSession
                                && !Settings.notificationsAccountEnabled
                                ? Komai.theme.attention
                                : palette.buttonText
                        }

                        GridLayout {
                            id: settingRow
                            Layout.fillWidth: true
                            visible: r.model.type != UserSettingsModel.SectionTitle
                            Layout.topMargin: r.isSelfContainedCardRow ? 0 : ((Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall : Komai.paddingMedium)
                            Layout.leftMargin: r.isSelfContainedCardRow ? 0 : Komai.paddingSmall
                            Layout.rightMargin: r.isSelfContainedCardRow ? 0 : Komai.paddingSmall
                            Layout.bottomMargin: r.isSelfContainedCardRow ? 0 : (r.hasDescription ? 0 : ((Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall : Komai.paddingMedium))
                            columns: r.useStackedLayout ? 1 : 2
                            rowSpacing: r.useStackedLayout && !r.isFullWidthPreviewRow ? Komai.paddingSmall : 0
                            columnSpacing: r.isFullWidthPreviewRow ? 0 : Komai.paddingSmall

                            RowLayout {
                                Layout.row: 0
                                Layout.column: 0
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.alignment: Qt.AlignTop
                                Layout.rightMargin: r.useStackedLayout ? 0 : Komai.paddingSmall
                                visible: !r.isFullWidthPreviewRow
                                readonly property int labelPixelSize: settingLabel.fontInfo.pixelSize > 0
                                    ? settingLabel.fontInfo.pixelSize
                                    : Math.round(1.1 * Settings.uiFontSizePt)
                                readonly property int iconPixelSize: Math.max(10, Math.round(labelPixelSize * 0.7))

                                Image {
                                    Layout.alignment: Qt.AlignTop
                                    Layout.topMargin: 2
                                    visible: r.hasSettingIcon
                                    source: r.settingIconSource
                                    sourceSize.width: parent.iconPixelSize
                                    sourceSize.height: parent.iconPixelSize
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                }

                                Text {
                                    id: settingLabel
                                    Layout.fillWidth: !r.hasSyncedToMatrix
                                    Layout.minimumWidth: 0
                                    color: rowHover.hovered ? palette.brightText : palette.text
                                    linkColor: palette.highlight
                                    text: r.model.name ?? ""
                                    textFormat: Text.AutoText
                                    font.pointSize: 1.1 * Settings.uiFontSizePt
                                    horizontalAlignment: root.mirrored ? Text.AlignRight : Text.AlignLeft
                                    LayoutMirroring.enabled: false
                                    wrapMode: Text.Wrap
                                    onLinkActivated: function (link) {
                                        Qt.openUrlExternally(link);
                                    }
                                }

                                SyncedToMatrixBadge {
                                    Layout.alignment: Qt.AlignVCenter
                                    visible: r.hasSyncedToMatrix
                                }

                                Item {
                                    Layout.fillWidth: true
                                    visible: r.hasSyncedToMatrix
                                }
                            }

                            Item {
                                id: chooserContainer
                                Layout.row: r.useStackedLayout && !r.isFullWidthPreviewRow ? 1 : 0
                                Layout.column: r.useStackedLayout ? 0 : (r.isFullWidthPreviewRow ? 0 : 1)
                                Layout.alignment: (r.useStackedLayout ? Qt.AlignLeft : Qt.AlignRight) | Qt.AlignTop
                                Layout.columnSpan: r.isFullWidthPreviewRow ? settingRow.columns : 1
                                Layout.fillWidth: r.isFullWidthPreviewRow || r.useStackedLayout
                                Layout.preferredWidth: r.isFullWidthPreviewRow ? grid.width : r.controlWidth
                                Layout.maximumWidth: r.isFullWidthPreviewRow ? grid.width : r.controlWidth
                                Layout.minimumWidth: r.isFullWidthPreviewRow ? 0 : (r.useStackedLayout ? 0 : 140)
                                readonly property real chooserHeight: chooser.child
                                    ? Math.max(
                                          chooser.child.implicitHeight || 0,
                                          chooser.child.height || 0,
                                          chooser.child.contentHeight || 0)
                                    : 0
                                Layout.preferredHeight: chooserHeight
                                Layout.minimumHeight: chooserHeight
                                Layout.rightMargin: 0

                                // Bindings inside these Components and the DelegateChoice items
                                // below intentionally use optional chaining (`r?.X`, `parent?.X`).
                                // When the search filter invalidates and a row is removed, QML
                                // re-evaluates child bindings while the delegate is being torn
                                // down — `r` and the chooser's `parent` are transiently null,
                                // and unguarded `r.foo` / `parent.foo` produces a flood of
                                // "Cannot read property of null" warnings on every keystroke.
                                Component {
                                    id: toggleDelegate
                                    SettingControlToggle {
                                        anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                        anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                        value: r?.model?.value
                                        textColor: rowHover.hovered ? palette.brightText : palette.buttonText
                                        onToggledValue: function(value) {
                                            if (r?.model)
                                                r.model.value = value;
                                        }
                                        enabled: r?.model?.enabled ?? false
                                    }
                                }

                                Component {
                                    id: optionsDelegate
                                    SettingControlCombo {
                                        anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                        anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                        value: r?.model?.value
                                        values: r?.model?.values
                                        width: Math.min(implicitWidth, r?.controlWidth ?? 0)
                                        onActivatedValueChanged: function(index) {
                                            if (r?.model && index !== r.model.value) {
                                                r.model.value = index;
                                            }
                                        }
                                    }
                                }

                                Component {
                                    id: searchableOptionsDelegate
                                    SettingControlComboSearch {
                                        anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                        anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                        value: r?.model?.value
                                        values: r?.model?.values
                                        width: Math.min(implicitWidth, r?.controlWidth ?? 0)
                                        onActivatedValueChanged: function(index) {
                                            if (r?.model && index !== r.model.value) {
                                                r.model.value = index;
                                            }
                                        }
                                    }
                                }

                                Component {
                                    id: integerDelegate
                                    SettingRowInteger {
                                        anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                        anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                        model: r?.model ?? null
                                    }
                                }

                                Component {
                                    id: doubleDelegate
                                    SettingRowDouble {
                                        anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                        anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                        model: r?.model ?? null
                                    }
                                }

                                DelegateChooser {
                                    id: chooser
                                    roleValue: r.model.type
                                    anchors.fill: parent

                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Toggle
                                        delegate: toggleDelegate
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ToggleWithDescription
                                        delegate: toggleDelegate
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Options
                                        delegate: optionsDelegate
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.OptionsWithDescription
                                        delegate: optionsDelegate
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.SegmentedOptions
                                        SegmentedButton {
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                            width: r?.controlWidth ?? 0
                                            currentIndex: r?.model?.value ?? 0
                                            model: (r?.model?.values ?? []).map(function(v) { return { text: v }; })
                                            onActivated: function(index) {
                                                if (r?.model && index !== r.model.value)
                                                    r.model.value = index;
                                            }
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.SearchableOptions
                                        delegate: searchableOptionsDelegate
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.PresenceStatusMessageField
                                        SettingRowPresenceStatusMessage {
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                            width: r?.controlWidth ?? 0
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ThemeSelector
                                        SettingRowThemeSelector {
                                            anchors.left: parent?.left
                                            anchors.right: parent?.right
                                            model: r?.model ?? null
                                            leftAligned: r?.useStackedLayout ?? false
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Integer
                                        delegate: integerDelegate
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.IntegerWithDescription
                                        delegate: integerDelegate
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Double
                                        delegate: doubleDelegate
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ReadOnlyText
                                        SettingRowReadOnlyText {
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                            model: r?.model ?? null
                                            leftAligned: r?.useStackedLayout ?? false
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Link
                                        SettingRowLink {
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                            model: r?.model ?? null
                                            hovered: rowHover.hovered
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.TextInput
                                        SettingControlTextInput {
                                            id: textSettingField
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                            textValue: r?.model?.value
                                            onSubmitted: function (value) {
                                                if (r?.model)
                                                    r.model.value = value;
                                            }
                                            width: r?.controlWidth ?? 0
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ManageIgnoredUsers
                                        SettingRowIgnoredUsers {
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.AccessTokenField
                                        SettingRowAccessTokenField {
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                            model: r?.model ?? null
                                            width: Math.min(implicitWidth, r?.controlWidth ?? 0)
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ProfileButton
                                        SettingRowProfileButton {
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.LogoutButton
                                        SettingRowLogout {
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.TimelinePreview
                                        SettingRowTimelinePreview {
                                            anchors.left: parent?.left
                                            anchors.right: parent?.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.AvatarPreview
                                        SettingRowAvatarPreview {
                                            anchors.left: parent?.left
                                            anchors.right: parent?.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.CommunityFilterRow
                                        SettingRowCommunityFilter {
                                            anchors.left: parent?.left
                                            anchors.right: parent?.right
                                            model: r?.model ?? null
                                            useStackedLayout: r?.useStackedLayout ?? false
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.SpacesFilterSection
                                        SettingRowSpacesFilter {
                                            anchors.left: parent?.left
                                            anchors.right: parent?.right
                                            useStackedLayout: r?.useStackedLayout ?? false
                                        }
                                    }
                                    DelegateChoice {
                                        SettingRowReadOnlyValue {
                                            anchors.left: r?.useStackedLayout ? parent?.left : undefined
                                            anchors.right: r?.useStackedLayout ? undefined : parent?.right
                                            model: r?.model ?? null
                                            leftAligned: r?.useStackedLayout ?? false
                                            hovered: rowHover.hovered
                                        }
                                    }
                                }
                            }
                        }

                        TextEdit {
                            Layout.fillWidth: true
                            Layout.leftMargin: Komai.paddingSmall
                            Layout.rightMargin: Komai.paddingSmall
                            visible: r.hasDescription
                            text: r.model.description ?? ""
                            textFormat: Text.RichText
                            color: rowHover.hovered ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            horizontalAlignment: root.mirrored ? Text.AlignRight : Text.AlignLeft
                            LayoutMirroring.enabled: false
                            wrapMode: Text.Wrap
                            readOnly: true
                            selectByMouse: true
                            Layout.topMargin: -Komai.paddingSmall
                            Layout.bottomMargin: Komai.paddingMedium
                            onLinkActivated: function(link) {
                                if (link === "komai://media-cache") {
                                    var info = Komai.localCacheInfo();
                                    if (info.mediaCachePathExists)
                                        Komai.openLocalPath(info.mediaCachePath);
                                } else if (link === "komai://rooms-directory") {
                                    MainWindow.openRoomDirectory();
                                } else if (link.startsWith("komai://settings/")) {
                                    root.openSettingsDeeplink(link);
                                } else {
                                    Qt.openUrlExternally(link);
                                }
                            }
                        }
                    }
                }
            }

            Loader {
                id: footerLoader
                Layout.fillWidth: true
                active: root.footerContent !== null && !root.searchHidesEverything
                visible: active
                sourceComponent: root.footerContent
            }
        }
    }

    Label {
        anchors.centerIn: parent
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        width: Math.min(parent.width - Komai.paddingLarge * 2, 480)
        color: palette.buttonText
        font.pointSize: Settings.uiFontSizePt
        text: qsTr("No settings match your search.")
        visible: root.searchHidesEverything
    }

    ScrollBar {
        id: scrollBar
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        policy: ScrollBar.AlwaysOn
        size: scroll.contentHeight > 0 ? scroll.height / scroll.contentHeight : 1
        position: scroll.visibleArea.yPosition
        visible: scroll.contentHeight > 0
        onPositionChanged: {
            if (active)
                scroll.contentY = position * scroll.contentHeight
        }
    }

    Timer {
        id: scrollTimer
        interval: 50
        onTriggered: root._scrollToTag()
    }

    function _scrollToTag() {
        if (!scrollToTagId)
            return;
        var tag = scrollToTagId;
        scrollToTagId = "";
        var maxY = Math.max(0, scroll.contentHeight - scroll.height);
        for (var i = 0; i < settingsRepeater.count; i++) {
            var item = settingsRepeater.itemAt(i);
            if (!item || !item.model)
                continue;
            if (item.model.tagId === tag) {
                scroll.contentY = Math.min(item.y, maxY);
                return;
            }
        }
        // Also walk the header / footer content trees — custom QML pages
        // (e.g. IntegrationsTab/TranscriptionSetting.qml) expose a `tagId`
        // property at their root that the deeplink dispatcher can target.
        var anchor = _findCustomTagAnchor(headerLoader.item, tag);
        if (!anchor)
            anchor = _findCustomTagAnchor(footerLoader.item, tag);
        if (anchor) {
            var p = anchor.mapToItem(grid, 0, 0);
            scroll.contentY = Math.min(p.y, maxY);
        }
    }

    function _findCustomTagAnchor(node, tag) {
        if (!node)
            return null;
        if (node.tagId !== undefined && node.tagId === tag)
            return node;
        var children = node.children || [];
        for (var i = 0; i < children.length; i++) {
            // Loader items expose their loaded content via `.item` instead
            // of as a regular child.
            if (children[i].item) {
                var found = _findCustomTagAnchor(children[i].item, tag);
                if (found)
                    return found;
            }
            var nested = _findCustomTagAnchor(children[i], tag);
            if (nested)
                return nested;
        }
        return null;
    }
}
