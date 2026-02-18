// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import ".."
import "../dialogs"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQml.Models
import im.nheko

Rectangle {
    id: userSettingsDialog

    property int collapsePoint: 600
    property bool collapsed: width < collapsePoint
    property int currentTab: UserSettingsModel.TabLookFeel
    color: palette.window

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Tab bar at the top - centered
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: tabRow.height
            Layout.topMargin: collapsed ? backButton.height + Nheko.paddingMedium : Nheko.paddingMedium

            Row {
                id: tabRow
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 2

                // Calculate the width of the widest tab for uniform sizing
                property real maxTabWidth: {
                    var maxW = 0;
                    for (var i = 0; i < tabRepeater.count; i++) {
                        var item = tabRepeater.itemAt(i);
                        if (item && item.naturalWidth > maxW)
                            maxW = item.naturalWidth;
                    }
                    return maxW;
                }

                Repeater {
                    id: tabRepeater
                    model: [
                        { text: qsTr("Look & Feel"), icon: "qrc:/icons/icons/ui/toggles.svg", tab: UserSettingsModel.TabLookFeel },
                        { text: qsTr("Timeline"), icon: "qrc:/icons/icons/ui/speech-bubbles.svg", tab: UserSettingsModel.TabTimeline },
                        { text: qsTr("Composer"), icon: "qrc:/icons/icons/ui/edit.svg", tab: UserSettingsModel.TabComposer },
                        { text: qsTr("Notifications"), icon: "qrc:/icons/icons/ui/alert.svg", tab: UserSettingsModel.TabNotifications },
                        { text: qsTr("Calls"), icon: "qrc:/icons/icons/ui/place-call.svg", tab: UserSettingsModel.TabCalls },
                        { text: qsTr("Privacy"), icon: "qrc:/icons/icons/ui/eye-hide.svg", tab: UserSettingsModel.TabPrivacy },
                        { text: qsTr("Encryption"), icon: "qrc:/icons/icons/ui/shield-filled.svg", tab: UserSettingsModel.TabEncryption },
                        { text: qsTr("Session"), icon: "qrc:/icons/icons/ui/person.svg", tab: UserSettingsModel.TabSession },
                        { text: qsTr("About"), icon: "qrc:/logos/komai.svg", tab: UserSettingsModel.TabAbout }
                    ]

                    delegate: AbstractButton {
                        id: tabBtn
                        required property var modelData
                        property bool isActive: userSettingsDialog.currentTab === modelData.tab
                        property bool isHovered: hoverHandler.hovered
                        // Natural width before equalization
                        property real naturalWidth: tabContent.implicitWidth + Nheko.paddingSmall * 2

                        width: tabRow.maxTabWidth
                        implicitHeight: tabContent.implicitHeight + Nheko.paddingMedium * 2

                        background: Rectangle {
                            radius: 6
                            color: tabBtn.isActive ? palette.highlight : (tabBtn.isHovered ? palette.mid : "transparent")

                            // Bottom border/underline for active tab
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 2
                                color: palette.highlight
                                visible: tabBtn.isActive
                            }
                        }

                        contentItem: Item {
                            implicitWidth: tabContent.implicitWidth
                            implicitHeight: tabContent.implicitHeight

                            Row {
                                id: tabContent
                                anchors.centerIn: parent
                                spacing: Nheko.paddingSmall

                                Image {
                                    width: 18
                                    height: 18
                                    anchors.verticalCenter: parent.verticalCenter
                                    source: tabBtn.modelData.icon
                                    sourceSize.width: 18
                                    sourceSize.height: 18
                                }

                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: tabBtn.modelData.text
                                    color: tabBtn.isActive ? palette.highlightedText : (tabBtn.isHovered ? palette.highlightedText : palette.text)
                                    font.bold: tabBtn.isActive
                                }
                            }
                        }

                        onClicked: userSettingsDialog.currentTab = modelData.tab

                        HoverHandler {
                            id: hoverHandler
                        }

                        ToolTip.visible: hoverHandler.hovered
                        ToolTip.text: tabBtn.modelData.text
                        ToolTip.delay: Nheko.tooltipDelay
                    }
                }
            }
        }

        // Settings content - use Item + Flickable + ScrollBar for consistent layout
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

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
                    // Fixed width content area, centered via equal margins
                    property real contentMaxWidth: 1000
                    property real sideMargin: Math.max(collapsed ? Nheko.paddingMedium : Nheko.paddingLarge, (scroll.width - contentMaxWidth) / 2)
                    width: scroll.width - sideMargin * 2
                    x: sideMargin

                    Repeater {
                    model: UserSettingsModel

                    delegate: GridLayout {
                        id: r

                        // Only show items for the current tab
                        visible: model.tab === userSettingsDialog.currentTab
                        Layout.preferredWidth: visible ? scroll.width : 0
                        Layout.preferredHeight: visible ? implicitHeight : 0
                        columns: userSettingsDialog.collapsed ? 1 : 2
                        rows: userSettingsDialog.collapsed ? 2 : 1
                        required property var model

                        RowLayout {
                            Layout.alignment: Qt.AlignLeft
                            Layout.fillWidth: true
                            Layout.columnSpan: (r.model.type == UserSettingsModel.SectionTitle && !userSettingsDialog.collapsed) ? 2 : 1
                            Layout.leftMargin: r.model.type == UserSettingsModel.SectionTitle ? 0 : Nheko.paddingMedium
                            Layout.topMargin: r.model.type == UserSettingsModel.SectionTitle ? Nheko.paddingLarge : 0
                            spacing: Nheko.paddingSmall

                            Image {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignVCenter
                                sourceSize.width: 24
                                sourceSize.height: 24
                                source: r.model.settingImage ?? ""
                                visible: r.model.type != UserSettingsModel.SectionTitle
                                opacity: source != "" ? 1 : 0
                            }

                            Label {
                                Layout.alignment: Qt.AlignLeft
                                Layout.fillWidth: true
                                color: palette.text
                                text: r.model.name
                                font.pointSize: 1.1 * fontMetrics.font.pointSize

                                HoverHandler {
                                    id: hovered
                                    enabled: r.model.description ?? false
                                }
                                ToolTip.visible: hovered.hovered && r.model.description
                                ToolTip.text: r.model.description ?? ""
                                ToolTip.delay: Nheko.tooltipDelay
                                wrapMode: Text.Wrap
                            }
                        }

                        DelegateChooser {
                            id: chooser

                            roleValue: r.model.type
                            Layout.alignment: Qt.AlignRight

                            Layout.columnSpan: (r.model.type == UserSettingsModel.SectionTitle && !userSettingsDialog.collapsed) ? 2 : 1
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
                                    anchors.right: parent.right
                                    text: r.model.value
                                    onEditingFinished: r.model.value = text
                                    width: Math.min(implicitWidth, scroll.width - Nheko.paddingMedium)
                                }
                            }
                            DelegateChoice {
                                roleValue: UserSettingsModel.SectionTitle
                                Item {
                                    width: grid.width
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

                                    palette.button: Nheko.theme.red
                                    palette.buttonText: "white"
                                    palette.highlight: Qt.darker(Nheko.theme.red, 1.2)

                                    font.bold: true

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
                }
            }

            // Always-visible scrollbar to prevent content shifting
            ScrollBar {
                id: scrollBar
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                policy: ScrollBar.AlwaysOn
                size: scroll.height / scroll.contentHeight
                position: scroll.visibleArea.yPosition
                onPositionChanged: {
                    if (active)
                        scroll.contentY = position * scroll.contentHeight
                }
            }
        }
    }

    ImageButton {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Nheko.paddingMedium
        width: Nheko.avatarSize
        height: Nheko.avatarSize
        image: ":/icons/icons/ui/angle-arrow-left.svg"
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Back")
        onClicked: mainWindow.pop()
    }
}
