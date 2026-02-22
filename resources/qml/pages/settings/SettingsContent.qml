// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../.."
import "../../dialogs"
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
            property real contentMaxWidth: 800
            property real sideMargin: Math.max(Nheko.paddingLarge, (scroll.width - contentMaxWidth) / 2)
            width: scroll.width - sideMargin * 2
            x: sideMargin

            Loader {
                Layout.fillWidth: true
                active: root.headerContent !== null
                sourceComponent: root.headerContent
            }

            Repeater {
                model: UserSettingsModel.modelForTab(root.tabFilter)

                delegate: GridLayout {
                    id: r

                    Layout.preferredWidth: scroll.width
                    columns: root.collapsed ? 1 : 2
                    rows: root.collapsed ? 2 : 1
                    required property var model

                    RowLayout {
                        Layout.alignment: Qt.AlignLeft
                        Layout.fillWidth: true
                        Layout.columnSpan: (r.model.type == UserSettingsModel.SectionTitle && !root.collapsed) ? 2 : 1
                        Layout.leftMargin: r.model.type == UserSettingsModel.SectionTitle ? 0 : Nheko.paddingMedium
                        Layout.topMargin: r.model.type == UserSettingsModel.SectionTitle ? Nheko.paddingLarge : 0
                        spacing: Nheko.paddingSmall

                        Label {
                            visible: r.model.type != UserSettingsModel.SectionTitle
                            Layout.alignment: Qt.AlignLeft
                            Layout.fillWidth: true
                            color: palette.text
                            text: r.model.name
                            textFormat: Text.AutoText
                            font.pointSize: 1.1 * fontMetrics.font.pointSize
                            onLinkActivated: function(link) {
                                Qt.openUrlExternally(link);
                            }

                            HoverHandler {
                                id: hovered
                                enabled: r.model.description ?? false
                            }
                            ToolTip.visible: hovered.hovered && r.model.description
                            ToolTip.text: r.model.description ?? ""
                            ToolTip.delay: Nheko.tooltipDelay
                            wrapMode: Text.Wrap

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }

                    DelegateChooser {
                        id: chooser
                        visible: r.model.type != UserSettingsModel.SectionTitle

                        roleValue: r.model.type
                        Layout.alignment: Qt.AlignRight

                        Layout.columnSpan: (r.model.type == UserSettingsModel.SectionTitle && !root.collapsed) ? 2 : 1
                        Layout.preferredHeight: child.height
                        Layout.preferredWidth: child.implicitWidth
                        Layout.maximumWidth: r.model.type == UserSettingsModel.SectionTitle ? Number.POSITIVE_INFINITY : 400
                        Layout.fillWidth: r.model.type == UserSettingsModel.SectionTitle || r.model.type == UserSettingsModel.Options || r.model.type == UserSettingsModel.Number
                        Layout.rightMargin: r.model.type == UserSettingsModel.SectionTitle ? 0 : Nheko.paddingMedium

                        DelegateChoice {
                            roleValue: UserSettingsModel.Toggle
                            SettingControlToggle {
                                value: r.model.value
                                onToggled: r.model.value = value
                                enabled: r.model.enabled
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Options
                            SettingControlCombo {
                                anchors.right: parent.right
                                value: r.model.value
                                values: r.model.values
                                width: Math.min(implicitWidth, scroll.width - Nheko.paddingMedium)
                                onActivatedValueChanged: function(index) {
                                    if (index !== r.model.value) {
                                        r.model.value = index;
                                    }
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.ThemeSelector
                            SettingRowThemeSelector {
                                model: r.model
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Integer
                            SettingRowInteger {
                                model: r.model
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Double
                            SettingRowDouble {
                                model: r.model
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.ReadOnlyText
                            SettingRowReadOnlyText {
                                model: r.model
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Link
                            SettingRowLink {
                                model: r.model
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.TextInput
                            SettingControlTextInput {
                                id: textSettingField
                                anchors.right: parent.right
                                textValue: r.model.value
                                onSubmitted: r.model.value = text
                                width: Math.min(implicitWidth, scroll.width - Nheko.paddingMedium)
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.SectionTitle
                            SettingsSection {
                                width: grid.width
                                label: r.model.name
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.KeyStatus
                            SettingRowKeyStatus {
                                model: r.model
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.SessionKeyImportExport
                            SettingRowSessionKeys {}
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.XSignKeysRequestDownload
                            SettingRowXSignKeys {}
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.ConfigureHiddenEvents
                            SettingRowHiddenEvents {}
                        }

                        DelegateChoice {
                            roleValue: UserSettingsModel.ManageIgnoredUsers
                            SettingRowIgnoredUsers {}
                        }

                        DelegateChoice {
                            roleValue: UserSettingsModel.AccessTokenField
                            SettingRowAccessTokenField {
                                model: r.model
                                width: Math.min(implicitWidth, scroll.width - Nheko.paddingMedium)
                            }
                        }

                        DelegateChoice {
                            roleValue: UserSettingsModel.ProfileButton
                            SettingRowProfileButton {}
                        }

                        DelegateChoice {
                            roleValue: UserSettingsModel.LogoutButton
                            SettingRowLogout {}
                        }

                        DelegateChoice {
                            SettingRowReadOnlyValue {
                                model: r.model
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
