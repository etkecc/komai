// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../.."
import "../../components"
import "../../components/SettingsRows"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import im.nheko

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
        anchors.rightMargin: Nheko.paddingSmall

        contentWidth: width
        contentHeight: grid.height + Nheko.paddingLarge * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: grid
            y: Nheko.paddingLarge

            spacing: Nheko.paddingMedium
            property real contentMaxWidth: 1000
            property real sideMargin: Math.max(Nheko.paddingLarge, (scroll.width - contentMaxWidth) / 2)
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
                    Layout.fillWidth: true
                    implicitHeight: row.implicitHeight
                    width: grid.width

                    readonly property real controlWidth: Math.min(500, Math.max(240, grid.width - Nheko.paddingLarge * 2))

                    ColumnLayout {
                        id: row
                        width: grid.width
                        spacing: Nheko.paddingMedium

                        SettingsSection {
                            id: sectionLabel
                            Layout.fillWidth: true
                            Layout.topMargin: Nheko.paddingLarge
                            Layout.bottomMargin: Nheko.paddingSmall
                            visible: r.model.type == UserSettingsModel.SectionTitle
                            label: r.model.name
                        }

                        RowLayout {
                            id: settingRow
                            Layout.fillWidth: true
                            visible: r.model.type != UserSettingsModel.SectionTitle
                            Layout.leftMargin: 0
                            Layout.bottomMargin: Nheko.paddingMedium
                            spacing: Nheko.paddingSmall

                            TextEdit {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.alignment: Qt.AlignTop
                                Layout.rightMargin: Nheko.paddingSmall
                                color: palette.text
                                text: r.model.name
                                textFormat: Text.AutoText
                                font.pointSize: 1.1 * Settings.fontSize
                                wrapMode: Text.Wrap
                                readOnly: true
                                selectByMouse: true
                                onLinkActivated: function (link) {
                                    Qt.openUrlExternally(link);
                                }

                                HoverHandler {
                                    id: hovered
                                    enabled: r.model.description ?? false
                                }
                                ToolTip.visible: hovered.hovered && r.model.description
                                ToolTip.text: r.model.description ?? ""
                                ToolTip.delay: Nheko.tooltipDelay
                            }

                            Item {
                                id: chooserContainer
                                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                                Layout.preferredWidth: r.controlWidth
                                Layout.maximumWidth: r.controlWidth
                                Layout.minimumWidth: 140
                                Layout.preferredHeight: childrenRect.height
                                Layout.rightMargin: 0

                                DelegateChooser {
                                    id: chooser
                                    roleValue: r.model.type
                                    anchors.fill: parent

                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Toggle
                                        SettingControlToggle {
                                            anchors.right: parent.right
                                            value: r.model.value
                                            onToggledValue: function(value) {
                                                r.model.value = value;
                                            }
                                            enabled: r.model.enabled
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ToggleWithDescription
                                        SettingControlToggle {
                                            anchors.right: parent.right
                                            value: r.model.value
                                            onToggledValue: function(value) {
                                                r.model.value = value;
                                            }
                                            enabled: r.model.enabled
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Options
                                        SettingControlCombo {
                                            anchors.right: parent.right
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
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.OptionsWithDescription
                                        SettingControlCombo {
                                            anchors.right: parent.right
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
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.PresenceStatusMessageField
                                        SettingRowPresenceStatusMessage {
                                            anchors.right: parent.right
                                            width: r.controlWidth
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ThemeSelector
                                        SettingRowThemeSelector {
                                            anchors.right: parent.right
                                            model: r.model
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Integer
                                        SettingRowInteger {
                                            anchors.right: parent.right
                                            model: r.model
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Double
                                        SettingRowDouble {
                                            anchors.right: parent.right
                                            model: r.model
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ReadOnlyText
                                        SettingRowReadOnlyText {
                                            anchors.right: parent.right
                                            model: r.model
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.Link
                                        SettingRowLink {
                                            anchors.right: parent.right
                                            model: r.model
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.TextInput
                                        SettingControlTextInput {
                                            id: textSettingField
                                            anchors.right: parent.right
                                            textValue: r.model.value
                                            onSubmitted: function (value) {
                                                r.model.value = value;
                                            }
                                            width: Math.min(implicitWidth, r.controlWidth)
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.KeyStatus
                                        SettingRowKeyStatus {
                                            anchors.right: parent.right
                                            model: r.model
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.SessionKeyImportExport
                                        SettingRowSessionKeys {
                                            anchors.right: parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.XSignKeysRequestDownload
                                        SettingRowXSignKeys {
                                            anchors.right: parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ConfigureHiddenEvents
                                        SettingRowHiddenEvents {
                                            anchors.right: parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ManageIgnoredUsers
                                        SettingRowIgnoredUsers {
                                            anchors.right: parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.AccessTokenField
                                        SettingRowAccessTokenField {
                                            anchors.right: parent.right
                                            model: r.model
                                            width: Math.min(implicitWidth, r.controlWidth)
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.ProfileButton
                                        SettingRowProfileButton {
                                            anchors.right: parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        roleValue: UserSettingsModel.LogoutButton
                                        SettingRowLogout {
                                            anchors.right: parent.right
                                        }
                                    }
                                    DelegateChoice {
                                        SettingRowReadOnlyValue {
                                            anchors.right: parent.right
                                            model: r.model
                                        }
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: (r.model.type == UserSettingsModel.OptionsWithDescription
                                      || r.model.type == UserSettingsModel.ToggleWithDescription)
                                     && !!r.model.description
                            text: r.model.description ?? ""
                            textFormat: Text.RichText
                            color: palette.buttonText
                            font.pointSize: 0.9 * Settings.fontSize
                            wrapMode: Text.Wrap
                            Layout.topMargin: -Nheko.paddingSmall
                            Layout.bottomMargin: Nheko.paddingSmall
                            onLinkActivated: function(link) {
                                Qt.openUrlExternally(link);
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
