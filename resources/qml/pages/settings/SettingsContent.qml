// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../.."
import "../../dialogs"
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
                model: UserSettingsModel

                delegate: GridLayout {
                    id: r

                    visible: model.tab === root.tabFilter
                    Layout.preferredWidth: visible ? scroll.width : 0
                    Layout.preferredHeight: visible ? implicitHeight : 0
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
                            ToggleButton {
                                checked: r.model.value
                                onClicked: r.model.value = checked
                                enabled: r.model.enabled
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Options
                            ComboBox {
                                anchors.right: parent.right
                                model: r.model.values
                                currentIndex: r.model.value
                                width: Math.min(implicitWidth, scroll.width - Nheko.paddingMedium)
                                onActivated: {
                                    r.model.value = currentIndex
                                }
                                implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
                                wheelEnabled: activeFocus
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.ThemeSelector
                            RowLayout {
                                anchors.right: parent.right
                                spacing: Nheko.paddingSmall

                                ComboBox {
                                    id: variantCombo
                                    model: r.model.themeVariantValues
                                    currentIndex: r.model.themeVariantValue
                                    onActivated: {
                                        if (currentIndex !== r.model.themeVariantValue)
                                            r.model.themeVariantValue = currentIndex
                                    }
                                    implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
                                    wheelEnabled: activeFocus
                                }

                                ComboBox {
                                    id: themeCombo
                                    visible: variantCombo.currentIndex !== 2
                                    model: r.model.values
                                    currentIndex: r.model.value
                                    onActivated: {
                                        if (currentIndex >= 0 && currentIndex !== r.model.value)
                                            r.model.value = currentIndex
                                    }
                                    implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
                                    wheelEnabled: activeFocus
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Integer

                            SpinBox {
                                anchors.right: parent.right
                                from: r.model.valueLowerBound
                                to: r.model.valueUpperBound
                                stepSize: r.model.valueStep
                                value: r.model.value
                                onValueChanged: r.model.value = value
                                editable: true
                                wheelEnabled: activeFocus
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Double

                            SpinBox {
                                id: spinbox

                                readonly property double div: 100
                                readonly property int decimals: 2

                                anchors.right: parent.right
                                from: r.model.valueLowerBound * div
                                to: r.model.valueUpperBound * div
                                stepSize: r.model.valueStep * div
                                value: r.model.value * div
                                onValueModified: r.model.value = value/div
                                editable: true

                                property real realValue: value / div

                                validator: DoubleValidator {
                                    bottom: Math.min(spinbox.from/spinbox.div, spinbox.to/spinbox.div)
                                    top:  Math.max(spinbox.from/spinbox.div, spinbox.to/spinbox.div)
                                }

                                textFromValue: function(value, locale) {
                                    return Number(value / spinbox.div).toLocaleString(locale, 'f', spinbox.decimals)
                                }

                                valueFromText: function(text, locale) {
                                    return Number.fromLocaleString(locale, text) * spinbox.div
                                }

                                wheelEnabled: activeFocus
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.ReadOnlyText
                            TextEdit {
                                color: palette.text
                                text: r.model.value
                                readOnly: true
                                textFormat: Text.PlainText
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Link
                            Text {
                                color: palette.text
                                text: r.model.value
                                textFormat: Text.RichText
                                onLinkActivated: function(link) { Qt.openUrlExternally(link); }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    acceptedButtons: Qt.NoButton
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.TextInput
                            TextField {
                                id: textSettingField
                                anchors.right: parent.right
                                text: r.model.value
                                function applyText()
                                {
                                    r.model.value = text.trim();
                                }
                                onEditingFinished: applyText()
                                onAccepted: applyText()
                                onActiveFocusChanged: if (!activeFocus) applyText()
                                Component.onDestruction: applyText()
                                width: Math.min(implicitWidth, scroll.width - Nheko.paddingMedium)
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.SectionTitle
                            ColumnLayout {
                                width: grid.width
                                spacing: 0

                                Item {
                                    Layout.fillWidth: true
                                    height: fontMetrics.lineSpacing
                                    Rectangle {
                                        anchors.topMargin: Nheko.paddingSmall
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        color: palette.buttonText
                                        height: 1
                                    }
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.KeyStatus
                            Text {
                                color: r.model.good ? "green" : Nheko.theme.error
                                text: r.model.value ? qsTr("CACHED") : qsTr("NOT CACHED")
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.SessionKeyImportExport
                            RowLayout {
                                Button {
                                    text: qsTr("IMPORT")
                                    onClicked: UserSettingsModel.importSessionKeys()
                                }
                                Button {
                                    text: qsTr("EXPORT")
                                    onClicked: UserSettingsModel.exportSessionKeys()
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.XSignKeysRequestDownload
                            RowLayout {
                                Button {
                                    text: qsTr("DOWNLOAD")
                                    onClicked: UserSettingsModel.downloadCrossSigningSecrets()
                                }
                                Button {
                                    text: qsTr("REQUEST")
                                    onClicked: UserSettingsModel.requestCrossSigningSecrets()
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.ConfigureHiddenEvents
                            Button {
                                text: qsTr("CONFIGURE")
                                onClicked: {
                                    var dialog = hiddenEventsDialog.createObject();
                                    dialog.show();
                                    destroyOnClose(dialog);
                                }

                                Component {
                                    id: hiddenEventsDialog

                                    HiddenEventsDialog {}
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: UserSettingsModel.ManageIgnoredUsers
                            Button {
                                text: qsTr("MANAGE")
                                onClicked: {
                                    var dialog = ignoredUsersDialog.createObject();
                                    dialog.show();
                                    destroyOnClose(dialog);
                                }

                                Component {
                                    id: ignoredUsersDialog

                                    IgnoredUsers {}
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: UserSettingsModel.AccessTokenField
                            Loader {
                                id: accessTokenLoader
                                property bool revealed: false
                                sourceComponent: revealed ? revealedComponent : hiddenComponent

                                Component {
                                    id: hiddenComponent
                                    Button {
                                        anchors.right: parent.right
                                        text: qsTr("Click to reveal")
                                        onClicked: accessTokenLoader.revealed = true
                                    }
                                }

                                Component {
                                    id: revealedComponent
                                    RowLayout {
                                        spacing: Nheko.paddingSmall

                                        TextField {
                                            text: r.model.value
                                            readOnly: true
                                            Layout.preferredWidth: 350
                                        }

                                        ImageButton {
                                            id: copyBtn
                                            property bool copied: false
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                                            ToolTip.visible: hovered
                                            ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                                            onClicked: {
                                                Clipboard.text = r.model.value;
                                                copied = true;
                                                copyFeedbackTimer.start();
                                            }

                                            Timer {
                                                id: copyFeedbackTimer
                                                interval: 2000
                                                onTriggered: copyBtn.copied = false
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: UserSettingsModel.ProfileButton
                            Button {
                                text: qsTr("Open Profile Settings")
                                icon.source: "qrc:/icons/icons/ui/person.svg"

                                onClicked: {
                                    Nheko.updateUserProfile();
                                    var component = Qt.createComponent("qrc:/resources/qml/dialogs/UserProfile.qml");
                                    if (component.status == Component.Ready) {
                                        var userProfile = component.createObject(timelineRoot, {
                                                "profile": Nheko.currentUser
                                            });
                                        userProfile.show();
                                        timelineRoot.destroyOnClose(userProfile);
                                    } else {
                                        console.error("Failed to create component: " + component.errorString());
                                    }
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: UserSettingsModel.LogoutButton
                            Button {
                                id: logoutBtn
                                text: qsTr("Logout")
                                icon.source: "qrc:/icons/icons/ui/power-off.svg"

                                onClicked: {
                                    var dialog = logoutDialog.createObject();
                                    dialog.open();
                                    destroyOnClose(dialog);
                                }

                                Component {
                                    id: logoutDialog

                                    LogoutDialog {}
                                }
                            }
                        }

                        DelegateChoice {
                            Text {
                                text: r.model.value
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
