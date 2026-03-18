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
    // Extra content to show above the repeater (used by AboutTab for logo)
    property Component headerContent: null
    property Component footerContent: null

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
            property real contentMaxWidth: Settings.uiLayoutContentMaxWidthEffectivePx > 0 ? Settings.uiLayoutContentMaxWidthEffectivePx : Number.POSITIVE_INFINITY
            property real sideMargin: Math.max(Komai.paddingMedium, (scroll.width - contentMaxWidth) / 2)
            property int settingRowStackBreakpoint: 700
            width: Math.max(0, scroll.width - sideMargin * 2)
            x: sideMargin

            Loader {
                Layout.fillWidth: true
                active: root.headerContent !== null
                sourceComponent: root.headerContent
            }

            Repeater {
                model: UserSettingsModel.modelForTab(root.tabFilter)

                delegate: Item {
                    id: r

                    required property var model
                    required property int index
                    Layout.fillWidth: true
                    implicitHeight: row.implicitHeight
                    width: grid.width

                    readonly property bool isFullWidthPreviewRow: r.model.type == UserSettingsModel.TimelinePreview || r.model.type == UserSettingsModel.AvatarPreview
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
                        : Math.min(500, Math.max(240, grid.width - Komai.paddingLarge * 2))
                    readonly property bool hasSyncedToMatrix: r.model.type != UserSettingsModel.SectionTitle
                        && !!r.model.syncedToMatrix
                    readonly property bool hasDescription: r.model.type != UserSettingsModel.SectionTitle
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
                        visible: settingRow.visible
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
                            label: r.model.name
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
                            Layout.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
                            Layout.leftMargin: Komai.paddingSmall
                            Layout.rightMargin: Komai.paddingSmall
                            Layout.bottomMargin: r.hasDescription ? 0 : (Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium)
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
                                    text: r.model.name
                                    textFormat: Text.AutoText
                                    font.pointSize: 1.1 * Settings.uiFontSizePt
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

                                Component {
                                    id: toggleDelegate
                                    SettingControlToggle {
                                        anchors.left: r.useStackedLayout ? parent.left : undefined
                                        anchors.right: r.useStackedLayout ? undefined : parent.right
                                        value: r.model.value
                                        textColor: rowHover.hovered ? palette.brightText : palette.buttonText
                                        onToggledValue: function(value) {
                                            r.model.value = value;
                                        }
                                        enabled: r.model.enabled
                                    }
                                }

                                Component {
                                    id: optionsDelegate
                                    SettingControlCombo {
                                        anchors.left: r.useStackedLayout ? parent.left : undefined
                                        anchors.right: r.useStackedLayout ? undefined : parent.right
                                        value: r.model.value
                                        values: r.model.values
                                        width: Math.min(implicitWidth, r.controlWidth)
                                        onActivatedValueChanged: function(index) {
                                            if (index !== r.model.value) {
                                                r.model.value = index;
                                            }
                                        }
                                    }
                                }

                                Component {
                                    id: searchableOptionsDelegate
                                    SettingControlComboSearch {
                                        anchors.left: r.useStackedLayout ? parent.left : undefined
                                        anchors.right: r.useStackedLayout ? undefined : parent.right
                                        value: r.model.value
                                        values: r.model.values
                                        width: Math.min(implicitWidth, r.controlWidth)
                                        onActivatedValueChanged: function(index) {
                                            if (index !== r.model.value) {
                                                r.model.value = index;
                                            }
                                        }
                                    }
                                }

                                Component {
                                    id: integerDelegate
                                    SettingRowInteger {
                                        anchors.left: r.useStackedLayout ? parent.left : undefined
                                        anchors.right: r.useStackedLayout ? undefined : parent.right
                                        model: r.model
                                    }
                                }

                                Component {
                                    id: doubleDelegate
                                    SettingRowDouble {
                                        anchors.left: r.useStackedLayout ? parent.left : undefined
                                        anchors.right: r.useStackedLayout ? undefined : parent.right
                                        model: r.model
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
                                        roleValue: UserSettingsModel.SearchableOptions
                                        delegate: searchableOptionsDelegate
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.PresenceStatusMessageField
                                        SettingRowPresenceStatusMessage {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                            width: r.controlWidth
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ThemeSelector
                                        SettingRowThemeSelector {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                            model: r.model
                                            leftAligned: r.useStackedLayout
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
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                            model: r.model
                                            leftAligned: r.useStackedLayout
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Link
                                        SettingRowLink {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                            model: r.model
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.TextInput
                                        SettingControlTextInput {
                                            id: textSettingField
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                            textValue: r.model.value
                                            onSubmitted: function (value) {
                                                r.model.value = value;
                                            }
                                            width: r.controlWidth
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.KeyStatus
                                        SettingRowKeyStatus {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                            model: r.model
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.SessionKeyImportExport
                                        SettingRowSessionKeys {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.XSignKeysRequestDownload
                                        SettingRowXSignKeys {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ManageIgnoredUsers
                                        SettingRowIgnoredUsers {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.AccessTokenField
                                        SettingRowAccessTokenField {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                            model: r.model
                                            width: Math.min(implicitWidth, r.controlWidth)
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ProfileButton
                                        SettingRowProfileButton {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.LogoutButton
                                        SettingRowLogout {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.TimelinePreview
                                        SettingRowTimelinePreview {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.AvatarPreview
                                        SettingRowAvatarPreview {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.CommunityFilterRow
                                        SettingRowCommunityFilter {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                            model: r.model
                                            useStackedLayout: r.useStackedLayout
                                        }
                                    }
                                    DelegateChoice {
                                        SettingRowReadOnlyValue {
                                            anchors.left: r.useStackedLayout ? parent.left : undefined
                                            anchors.right: r.useStackedLayout ? undefined : parent.right
                                            model: r.model
                                            leftAligned: r.useStackedLayout
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
                            font.pointSize: 0.9 * Settings.uiFontSizePt
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
                                } else {
                                    Qt.openUrlExternally(link);
                                }
                            }
                        }
                    }
                }
            }

            Loader {
                Layout.fillWidth: true
                active: root.footerContent !== null
                sourceComponent: root.footerContent
            }
        }
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
}
