// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai 1.0
import "onboarding" as Onboarding
import "../components/"
import "../ui/"

Rectangle {
    id: registrationPage
    property int maxExpansion: 900
    property int headerIconSize: Komai.barIconSize
    required property var registrationController

    readonly property var reg: registrationController

    color: palette.alternateBase

    // ── State ──
    property int initialServerTab: 0    // Set before push to pre-select tab
    property int currentStep: 0
    property int serverTab: 0           // 0 = Public, 1 = Custom
    property string selectedServer: ""
    property string selectedServerDomain: ""
    property bool selectedServerVanillaReg: true
    property string selectedServerRegLink: ""
    property int currentStageIndex: 0

    // Shared label width for consistent row alignment (same pattern as LoginPage)
    readonly property int fieldLabelWidth: Math.max(
        regUsernameLabelMetrics.advanceWidth,
        regPasswordLabelMetrics.advanceWidth,
        regConfirmLabelMetrics.advanceWidth,
        regDeviceLabelMetrics.advanceWidth
    ) + Komai.paddingMedium * 2
    TextMetrics { id: regUsernameLabelMetrics; font.pointSize: Settings.uiFontSizePt * 1.1; text: qsTr("Username") }
    TextMetrics { id: regPasswordLabelMetrics; font.pointSize: Settings.uiFontSizePt * 1.1; text: qsTr("Password") }
    TextMetrics { id: regConfirmLabelMetrics; font.pointSize: Settings.uiFontSizePt * 1.1; text: qsTr("Confirm") }
    TextMetrics { id: regDeviceLabelMetrics; font.pointSize: Settings.uiFontSizePt * 1.1; text: qsTr("Device name") }

    // ── Tooltip state ──
    property Item hoveredBadge: null
    property string hoveredBadgeText: ""

    StackView.onActivated: {
        reg.reset();
        registrationPage.currentStep = 0;
        registrationPage.serverTab = registrationPage.initialServerTab;
        registrationPage.selectedServer = "";
        registrationPage.selectedServerDomain = "";
        registrationPage.selectedServerVanillaReg = true;
        registrationPage.selectedServerRegLink = "";
                registrationPage.currentStageIndex = 0;
        if (registrationPage.initialServerTab === 1)
            customServerField.forceActiveFocus();
    }

    // Track stage completion to auto-advance
    Connections {
        target: reg
        function onStageCompleted() {
            registrationPage.currentStageIndex++;
            registrationPage.autoSubmitNextStage();
        }
    }

    // Current stage type from flow
    function currentStageType() {
        if (currentStageIndex >= 0 && currentStageIndex < reg.flowStages.length)
            return reg.flowStages[currentStageIndex];
        return "";
    }

    function stageDisplayName(stageType) {
        switch (stageType) {
        case "m.login.dummy": return qsTr("Verification");
        case "m.login.email.identity": return qsTr("Email verification");
        case "m.login.registration_token": return qsTr("Token");
        case "m.login.terms": return qsTr("Terms");
        case "m.login.recaptcha": return qsTr("CAPTCHA");
        case "m.login.sso": return qsTr("SSO");
        default: return qsTr("Verify");
        }
    }

    // Build step indicator model dynamically
    function buildStepModel() {
        var steps = [{ text: qsTr("Server") }, { text: qsTr("Account") }];
        if (reg.probed) {
            for (var i = 0; i < reg.flowStages.length; i++) {
                var stage = reg.flowStages[i];
                if (stage === "m.login.dummy") continue;
                steps.push({ text: stageDisplayName(stage) });
            }
        }
        return steps;
    }

    // Effective step index accounting for auto-completed dummy stages
    function effectiveStepIndex() {
        if (currentStep <= 1) return currentStep;
        var visualStep = 2;
        for (var i = 0; i < currentStageIndex && i < reg.flowStages.length; i++) {
            if (reg.flowStages[i] !== "m.login.dummy")
                visualStep++;
        }
        return visualStep;
    }

    // Auto-submit dummy stages silently
    function autoSubmitNextStage() {
        var stage = currentStageType();
        if (stage === "m.login.dummy") {
            reg.submitStage(usernameField.text.trim(), passwordField.text,
                regDeviceField.text.trim(), "m.login.dummy", "", "", "");
        }
    }

    // Badge tooltip (shared across all badges, positioned on hovered badge)
    KomaiToolTip {
        parent: registrationPage
        anchorItem: registrationPage.hoveredBadge
        anchorX: registrationPage.hoveredBadge ? registrationPage.hoveredBadge.width / 2 : 0
        anchorY: registrationPage.hoveredBadge ? registrationPage.hoveredBadge.height : 0
        gapY: Komai.paddingSmall
        preferBelow: true
        text: registrationPage.hoveredBadgeText
        delay: 0
        requestedVisible: registrationPage.hoveredBadge !== null
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header bar ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Komai.navigationRowHeight
            color: palette.alternateBase

            ItemDelegate {
                id: headerCancel
                anchors.left: parent.left
                height: parent.height
                topPadding: 0
                bottomPadding: 0
                leftPadding: Komai.paddingMedium
                rightPadding: Komai.paddingMedium

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    color: headerCancel.hovered ? palette.dark : "transparent"
                }

                onClicked: {
                    reg.cancelRegistration();
                    mainWindow.pop();
                }

                contentItem: RowLayout {
                    spacing: Komai.paddingSmall

                    Image {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        Layout.alignment: Qt.AlignVCenter
                        source: "image://colorimage/:/icons/icons/ui/angle-arrow-left.svg?" + (headerCancel.hovered ? palette.brightText : palette.text)
                        sourceSize.width: 24
                        sourceSize.height: 24
                    }

                    Label {
                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("Cancel")
                        font.pointSize: Settings.uiFontSizePt
                        font.bold: true
                        color: headerCancel.hovered ? palette.brightText : palette.text
                    }
                }
            }

            RowLayout {
                anchors.centerIn: parent
                spacing: Komai.paddingMedium

                Image {
                    Layout.preferredWidth: registrationPage.headerIconSize
                    Layout.preferredHeight: registrationPage.headerIconSize
                    Layout.alignment: Qt.AlignVCenter
                    source: "qrc:/logos/komai.svg"
                    sourceSize.width: registrationPage.headerIconSize
                    sourceSize.height: registrationPage.headerIconSize
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Register")
                    font.pointSize: Settings.uiFontSizePt * 1.1
                    font.bold: true
                    color: palette.text
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Komai.theme.separator
        }

        // ── Content ──
        Onboarding.OnboardingScrollPage {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            maxContentWidth: registrationPage.maxExpansion
            topSpacerHeight: Komai.paddingLarge

            // ── Step Indicator ──
            StepIndicator {
                Layout.fillWidth: true
                Layout.bottomMargin: Komai.paddingLarge
                currentIndex: registrationPage.effectiveStepIndex()
                model: registrationPage.buildStepModel()
                onActivated: function(index) {
                    if (index < registrationPage.effectiveStepIndex()) {
                        if (index === 0) registrationPage.currentStep = 0;
                        else if (index === 1) registrationPage.currentStep = 1;
                    }
                }
            }

            // ════════════════════════════════════════
            // STEP 0: Server Selection
            // ════════════════════════════════════════

            SegmentedButton {
                visible: registrationPage.currentStep === 0
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                currentIndex: registrationPage.serverTab
                model: [
                    { text: qsTr("Public servers") },
                    { text: qsTr("Custom server") }
                ]
                onActivated: function(index) {
                    registrationPage.serverTab = index;
                    registrationPage.selectedServer = "";
                    registrationPage.selectedServerDomain = "";
                    registrationPage.selectedServerVanillaReg = true;
                    registrationPage.selectedServerRegLink = "";
                }
            }

            // Tab description
            Label {
                visible: registrationPage.currentStep === 0
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingSmall
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                text: registrationPage.serverTab === 0
                    ? qsTr("Community-operated servers with open registration")
                    : qsTr("Enter any Matrix homeserver address")
                color: palette.buttonText
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            // Attribution for public server list
            Text {
                visible: registrationPage.currentStep === 0 && registrationPage.serverTab === 0
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingSmall
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pointSize: Settings.uiFontSizePt * 0.85
                color: palette.buttonText
                text: "<style>a { color: " + palette.highlight + "; }</style>" +
                      qsTr("Based on <a href=\"https://servers.joinmatrix.org/\">servers.joinmatrix.org</a>, curated by the Komai team")
                onLinkActivated: function(link) { Qt.openUrlExternally(link); }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            // ── Public server list ──
            Repeater {
                model: (registrationPage.currentStep === 0 && registrationPage.serverTab === 0)
                    ? reg.serverList : []

                delegate: ItemDelegate {
                    id: serverDelegate
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    Layout.leftMargin: Komai.paddingMedium
                    Layout.rightMargin: Komai.paddingMedium
                    // Collapse: hide non-selected entries when a server is selected
                    visible: registrationPage.selectedServer.length === 0
                        || registrationPage.selectedServer === modelData.name

                    readonly property bool isSelected: registrationPage.selectedServer === modelData.name
                    property color backgroundColor: palette.window
                    property color textColor: palette.text
                    property color secondaryTextColor: palette.buttonText

                    HoverHandler { cursorShape: Qt.PointingHandCursor }

                    background: Rectangle {
                        radius: Komai.paddingSmall
                        color: serverDelegate.backgroundColor
                    }

                    states: [
                        State {
                            name: "hover"
                            when: serverDelegate.hovered && !serverDelegate.isSelected

                            PropertyChanges {
                                serverDelegate {
                                    backgroundColor: palette.dark
                                    textColor: palette.brightText
                                    secondaryTextColor: palette.brightText
                                }
                            }
                        },
                        State {
                            name: "selected"
                            when: serverDelegate.isSelected

                            PropertyChanges {
                                serverDelegate {
                                    backgroundColor: palette.highlight
                                    textColor: palette.highlightedText
                                    secondaryTextColor: palette.highlightedText
                                }
                            }
                        }
                    ]

                    onClicked: {
                        if (serverDelegate.isSelected) {
                            // Deselect — re-expand the list
                            registrationPage.selectedServer = "";
                            registrationPage.selectedServerDomain = "";
                            registrationPage.selectedServerVanillaReg = true;
                            registrationPage.selectedServerRegLink = "";
                        } else {
                            registrationPage.selectedServer = modelData.name;
                            registrationPage.selectedServerDomain = modelData.clientDomain;
                            registrationPage.selectedServerVanillaReg = modelData.usingVanillaReg;
                            registrationPage.selectedServerRegLink = modelData.regLink || "";
                        }
                    }

                    contentItem: ColumnLayout {
                        spacing: 2

                        RowLayout {
                            spacing: Komai.paddingSmall

                            Label {
                                text: modelData.name
                                font.pointSize: Settings.uiFontSizePt * 1.05
                                font.bold: true
                                color: serverDelegate.textColor
                            }

                            // ── Badges ──
                            Rectangle {
                                id: webBadge
                                visible: !modelData.usingVanillaReg
                                readonly property color badgeColor: palette.highlight
                                implicitWidth: webBadgeLabel.implicitWidth + Komai.paddingSmall * 2
                                implicitHeight: webBadgeLabel.implicitHeight + Komai.paddingSmall * 0.5
                                radius: Komai.paddingSmall
                                color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
                                border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
                                border.width: 1

                                Label {
                                    id: webBadgeLabel
                                    anchors.centerIn: parent
                                    text: qsTr("Web")
                                    color: webBadge.badgeColor
                                    font.pointSize: Settings.uiFontSizePt * 0.8
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                    onContainsMouseChanged: {
                                        if (containsMouse) {
                                            registrationPage.hoveredBadge = webBadge;
                                            registrationPage.hoveredBadgeText = qsTr("Registration is completed in your browser on the server's website.");
                                        } else if (registrationPage.hoveredBadge === webBadge) {
                                            registrationPage.hoveredBadge = null;
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: demoBadge
                                visible: modelData.category === "demo"
                                readonly property color badgeColor: Komai.theme.warning
                                implicitWidth: demoBadgeLabel.implicitWidth + Komai.paddingSmall * 2
                                implicitHeight: demoBadgeLabel.implicitHeight + Komai.paddingSmall * 0.5
                                radius: Komai.paddingSmall
                                color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
                                border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
                                border.width: 1

                                Label {
                                    id: demoBadgeLabel
                                    anchors.centerIn: parent
                                    text: qsTr("Demo")
                                    color: demoBadge.badgeColor
                                    font.pointSize: Settings.uiFontSizePt * 0.8
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                    onContainsMouseChanged: {
                                        if (containsMouse) {
                                            registrationPage.hoveredBadge = demoBadge;
                                            registrationPage.hoveredBadgeText = qsTr("A server for demonstration and testing purposes. Not suitable for real use.");
                                        } else if (registrationPage.hoveredBadge === demoBadge) {
                                            registrationPage.hoveredBadge = null;
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: captchaBadge
                                visible: modelData.captcha
                                readonly property color badgeColor: serverDelegate.secondaryTextColor
                                implicitWidth: captchaBadgeLabel.implicitWidth + Komai.paddingSmall * 2
                                implicitHeight: captchaBadgeLabel.implicitHeight + Komai.paddingSmall * 0.5
                                radius: Komai.paddingSmall
                                color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
                                border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
                                border.width: 1

                                Label {
                                    id: captchaBadgeLabel
                                    anchors.centerIn: parent
                                    text: qsTr("CAPTCHA")
                                    color: captchaBadge.badgeColor
                                    font.pointSize: Settings.uiFontSizePt * 0.8
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                    onContainsMouseChanged: {
                                        if (containsMouse) {
                                            registrationPage.hoveredBadge = captchaBadge;
                                            registrationPage.hoveredBadgeText = qsTr("The registration flow requires completing a CAPTCHA challenge in the browser.");
                                        } else if (registrationPage.hoveredBadge === captchaBadge) {
                                            registrationPage.hoveredBadge = null;
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: emailBadge
                                visible: modelData.email
                                readonly property color badgeColor: serverDelegate.secondaryTextColor
                                implicitWidth: emailBadgeLabel.implicitWidth + Komai.paddingSmall * 2
                                implicitHeight: emailBadgeLabel.implicitHeight + Komai.paddingSmall * 0.5
                                radius: Komai.paddingSmall
                                color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
                                border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
                                border.width: 1

                                Label {
                                    id: emailBadgeLabel
                                    anchors.centerIn: parent
                                    text: qsTr("Email")
                                    color: emailBadge.badgeColor
                                    font.pointSize: Settings.uiFontSizePt * 0.8
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                    onContainsMouseChanged: {
                                        if (containsMouse) {
                                            registrationPage.hoveredBadge = emailBadge;
                                            registrationPage.hoveredBadgeText = qsTr("The registration flow requires confirming a valid email address.");
                                        } else if (registrationPage.hoveredBadge === emailBadge) {
                                            registrationPage.hoveredBadge = null;
                                        }
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.editorial || modelData.description
                            textFormat: Text.MarkdownText
                            font.pointSize: Settings.uiFontSizePt * 0.9
                            color: serverDelegate.secondaryTextColor
                            linkColor: serverDelegate.isSelected ? palette.highlightedText : palette.highlight
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            onLinkActivated: function(link) { Qt.openUrlExternally(link); }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                        }
                    }
                }
            }

            // ── Custom server input ──
            Rectangle {
                visible: registrationPage.currentStep === 0 && registrationPage.serverTab === 1
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.paddingMedium
                color: palette.window
                radius: 8
                implicitHeight: customServerField.implicitHeight + Komai.paddingMedium * 2

                KomaiTextField {
                    id: customServerField
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    font.pointSize: Settings.uiFontSizePt * 1.1
                    placeholderText: qsTr("e.g. example.com or https://matrix.example.com")
                    onTextChanged: {
                        registrationPage.selectedServer = text.trim();
                        registrationPage.selectedServerDomain = text.trim();
                    }
                }
            }

            // ── Probe error ──
            MatrixText {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: reg.error
                visible: text && registrationPage.currentStep === 0
                wrapMode: TextEdit.Wrap
            }

            // ── Probing spinner ──
            Item {
                Layout.preferredHeight: Komai.listIconSize
                Layout.fillWidth: true
                visible: reg.probing

                Spinner {
                    height: Komai.listIconSize
                    anchors.centerIn: parent
                    visible: running
                    running: parent.visible
                    foreground: palette.mid
                }
            }

            // ── Web-only registration notice ──
            Rectangle {
                visible: registrationPage.currentStep === 0
                    && registrationPage.selectedServer.length > 0
                    && !registrationPage.selectedServerVanillaReg
                    && !reg.probing
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.paddingMedium
                color: palette.window
                radius: 8
                implicitHeight: webRegCol.implicitHeight + Komai.paddingMedium * 2

                ColumnLayout {
                    id: webRegCol
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingMedium

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("This server handles registration on its website.")
                        color: palette.text
                        wrapMode: Text.Wrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Create your account there, then come back and sign in with Login.")
                        color: palette.buttonText
                        wrapMode: Text.Wrap
                    }

                    KomaiButton {
                        text: qsTr("Open registration page")
                        icon.source: "qrc:/icons/icons/ui/forward.svg"
                        highlighted: true
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        onClicked: {
                            var url = registrationPage.selectedServerRegLink;
                            if (!url) {
                                // Fallback: try the homepage
                                for (var i = 0; i < reg.serverList.length; i++) {
                                    if (reg.serverList[i].name === registrationPage.selectedServer) {
                                        url = reg.serverList[i].homepage;
                                        break;
                                    }
                                }
                            }
                            if (url) Qt.openUrlExternally(url);
                        }
                    }
                }
            }

            // ── Step 0 Continue (in-app registration) ──
            KomaiButton {
                visible: registrationPage.currentStep === 0
                    && !reg.probing
                    && registrationPage.selectedServerVanillaReg
                enabled: registrationPage.selectedServer.length > 0
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingMedium
                text: qsTr("Continue")
                icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
                highlighted: true
                font.pointSize: Settings.uiFontSizePt * 1.3
                onClicked: {
                    reg.probeServer(registrationPage.selectedServerDomain || registrationPage.selectedServer);
                }
            }

            // Watch for probe completion → advance to step 1
            Connections {
                target: reg
                function onProbedChanged() {
                    if (reg.probed && registrationPage.currentStep === 0) {
                        registrationPage.currentStep = 1;
                        registrationPage.currentStageIndex = 0;
                    }
                }
            }

            // ════════════════════════════════════════
            // STEP 1: Account (Username + Password + Device Name)
            // ════════════════════════════════════════

            Label {
                visible: registrationPage.currentStep >= 1
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingLarge
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                text: qsTr("Create your account on %1").arg(registrationPage.selectedServer)
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.1
                wrapMode: Text.Wrap
            }

            // Username
            Item {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: usernameRow.implicitHeight
                visible: registrationPage.currentStep >= 1

                HoverHandler { id: usernameHover; blocking: false }
                Rectangle { anchors.fill: usernameRow; color: usernameHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: usernameRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.preferredWidth: registrationPage.fieldLabelWidth
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Username")
                        color: usernameHover.hovered ? palette.brightText : palette.text
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    Item {
                        id: usernameStatusSlot
                        Layout.preferredWidth: usernameField.height / 2
                        Layout.preferredHeight: usernameField.height / 2
                        Layout.alignment: Qt.AlignVCenter

                        Spinner {
                            anchors.fill: parent
                            visible: running
                            running: reg.checkingUsername
                            foreground: palette.mid
                        }

                        Image {
                            anchors.fill: parent
                            visible: !reg.checkingUsername && usernameField.text.length > 0 && reg.usernameAvailable
                            source: "image://colorimage/:/icons/icons/ui/checkmark.svg?" + Komai.theme.success
                            sourceSize.width: width
                            sourceSize.height: height
                        }

                        Image {
                            id: usernameUnavailableIcon
                            anchors.fill: parent
                            visible: !reg.checkingUsername && usernameField.text.length > 0 && !reg.usernameAvailable
                            source: "image://colorimage/:/icons/icons/ui/dismiss.svg?" + Komai.theme.error
                            sourceSize.width: width
                            sourceSize.height: height

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                                onContainsMouseChanged: {
                                    if (containsMouse) {
                                        registrationPage.hoveredBadge = usernameUnavailableIcon;
                                        registrationPage.hoveredBadgeText = qsTr("This username is not available. Try a different one.");
                                    } else if (registrationPage.hoveredBadge === usernameUnavailableIcon) {
                                        registrationPage.hoveredBadge = null;
                                    }
                                }
                            }
                        }
                    }

                    KomaiTextField {
                        id: usernameField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        placeholderText: qsTr("Choose a username")
                        readOnly: registrationPage.currentStep > 1

                        onTextChanged: usernameDebounce.restart()

                        Timer {
                            id: usernameDebounce
                            interval: 500
                            onTriggered: {
                                if (usernameField.text.trim().length > 0)
                                    reg.checkUsername(usernameField.text.trim());
                            }
                        }
                    }
                }
            }

            // Password
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: passwordRow.implicitHeight
                visible: registrationPage.currentStep >= 1

                HoverHandler { id: passwordHover; blocking: false }
                Rectangle { anchors.fill: passwordRow; color: passwordHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: passwordRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.preferredWidth: registrationPage.fieldLabelWidth
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Password")
                        color: passwordHover.hovered ? palette.brightText : palette.text
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    ImageButton {
                        Layout.preferredWidth: passwordField.height / 2
                        Layout.preferredHeight: passwordField.height / 2
                        Layout.alignment: Qt.AlignVCenter
                        buttonTextColor: passwordHover.hovered ? palette.brightText : palette.buttonText
                        image: passwordField.echoMode === TextInput.Password ? ":/icons/icons/ui/eye-show.svg" : ":/icons/icons/ui/eye-hide.svg"
                        toolTipVisible: hovered
                        toolTipText: qsTr("Show/Hide Password")
                        onClicked: passwordField.echoMode = passwordField.echoMode === TextInput.Normal ? TextInput.Password : TextInput.Normal
                    }

                    KomaiTextField {
                        id: passwordField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        echoMode: TextInput.Password
                        placeholderText: qsTr("Choose a password")
                        readOnly: registrationPage.currentStep > 1
                    }
                }
            }

            // Confirm password
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: confirmCol.implicitHeight
                visible: registrationPage.currentStep >= 1

                HoverHandler { id: confirmHover; blocking: false }
                Rectangle { anchors.fill: confirmCol; color: confirmHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                ColumnLayout {
                    id: confirmCol
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall

                        Label {
                            Layout.preferredWidth: registrationPage.fieldLabelWidth
                            Layout.margins: Komai.paddingMedium
                            text: qsTr("Confirm")
                            color: confirmHover.hovered ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt * 1.1
                        }

                        Item {
                            Layout.preferredWidth: confirmField.height / 2
                            Layout.preferredHeight: confirmField.height / 2
                            Layout.alignment: Qt.AlignVCenter

                            Image {
                                anchors.fill: parent
                                visible: confirmField.text.length > 0 && passwordField.text === confirmField.text
                                source: "image://colorimage/:/icons/icons/ui/checkmark.svg?" + Komai.theme.success
                                sourceSize.width: width
                                sourceSize.height: height
                            }
                        }

                        KomaiTextField {
                            id: confirmField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 300
                            Layout.topMargin: Komai.paddingSmall
                            Layout.bottomMargin: Komai.paddingSmall
                            Layout.rightMargin: Komai.paddingSmall
                            font.pointSize: Settings.uiFontSizePt * 1.1
                            echoMode: TextInput.Password
                            placeholderText: qsTr("Confirm password")
                            readOnly: registrationPage.currentStep > 1
                        }
                    }

                    Label {
                        visible: confirmField.text.length > 0 && passwordField.text !== confirmField.text
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingSmall
                        text: qsTr("Passwords do not match")
                        color: Komai.theme.error
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            // Device name
            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                implicitHeight: deviceCol.implicitHeight
                visible: registrationPage.currentStep >= 1

                HoverHandler { id: deviceHover; blocking: false }
                Rectangle { anchors.fill: deviceCol; color: deviceHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                ColumnLayout {
                    id: deviceCol
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall

                        Label {
                            Layout.preferredWidth: registrationPage.fieldLabelWidth
                            Layout.margins: Komai.paddingMedium
                            text: qsTr("Device name")
                            color: deviceHover.hovered ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt * 1.1
                        }

                        // Spacer matching the spinner slot in other rows
                        Item {
                            Layout.preferredWidth: regDeviceField.height / 2
                            Layout.preferredHeight: regDeviceField.height / 2
                            Layout.alignment: Qt.AlignVCenter
                        }

                        KomaiTextField {
                            id: regDeviceField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 300
                            Layout.topMargin: Komai.paddingSmall
                            Layout.bottomMargin: Komai.paddingSmall
                            Layout.rightMargin: Komai.paddingSmall
                            font.pointSize: Settings.uiFontSizePt * 1.1
                            text: reg.deviceNameOS()
                            readOnly: registrationPage.currentStep > 1
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        text: qsTr("Choose a recognizable name. Others can see it too.")
                        color: deviceHover.hovered ? palette.brightText : palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignRight
                        lineHeight: 1.3
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingSmall
                        layoutDirection: Qt.RightToLeft
                        spacing: Komai.paddingSmall

                        KomaiButton {
                            enabled: registrationPage.currentStep === 1
                            icon.source: "qrc:/icons/icons/ui/arrow-clockwise.svg"
                            display: AbstractButton.IconOnly
                            topPadding: Komai.paddingSmall * 0.5
                            bottomPadding: Komai.paddingSmall * 0.5
                            leftPadding: Komai.paddingMedium
                            rightPadding: Komai.paddingMedium
                            toolTipText: qsTr("Generate another random name")
                            onClicked: regSuggestRandomBtn.randomName = reg.deviceNameRandom()
                        }

                        KomaiButton {
                            id: regSuggestRandomBtn
                            enabled: registrationPage.currentStep === 1
                            property string randomName: reg.deviceNameRandom()
                            text: randomName
                            font.pointSize: Settings.uiFontSizePt
                            topPadding: Komai.paddingSmall * 0.5
                            bottomPadding: Komai.paddingSmall * 0.5
                            leftPadding: Komai.paddingSmall
                            rightPadding: Komai.paddingSmall
                            highlighted: regDeviceField.text === text
                            onClicked: regDeviceField.text = randomName
                            width: regRandomNameMaxMetrics.advanceWidth + leftPadding + rightPadding
                            TextMetrics { id: regRandomNameMaxMetrics; font: regSuggestRandomBtn.font; text: reg.deviceNameRandomMax() }
                        }

                        KomaiButton {
                            enabled: registrationPage.currentStep === 1
                            text: reg.deviceNameHostname()
                            font.pointSize: Settings.uiFontSizePt
                            topPadding: Komai.paddingSmall * 0.5
                            bottomPadding: Komai.paddingSmall * 0.5
                            leftPadding: Komai.paddingSmall
                            rightPadding: Komai.paddingSmall
                            highlighted: regDeviceField.text === text
                            onClicked: regDeviceField.text = text
                        }

                        KomaiButton {
                            id: regSuggestOsBtn
                            enabled: registrationPage.currentStep === 1
                            text: reg.deviceNameOS()
                            font.pointSize: Settings.uiFontSizePt
                            topPadding: Komai.paddingSmall * 0.5
                            bottomPadding: Komai.paddingSmall * 0.5
                            leftPadding: Komai.paddingSmall
                            rightPadding: Komai.paddingSmall
                            highlighted: regDeviceField.text === text
                            onClicked: regDeviceField.text = text
                        }

                        Label {
                            text: qsTr("Suggestions:")
                            color: deviceHover.hovered ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            height: regSuggestOsBtn.height
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // Step 1 error
            MatrixText {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: reg.error
                visible: text && registrationPage.currentStep === 1
                wrapMode: TextEdit.Wrap
            }

            // Step 1 Continue
            KomaiButton {
                visible: registrationPage.currentStep === 1
                enabled: usernameField.text.trim().length > 0
                    && passwordField.text.length > 0
                    && passwordField.text === confirmField.text
                    && reg.usernameAvailable
                    && !reg.registering
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingMedium
                text: qsTr("Continue")
                icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
                highlighted: true
                font.pointSize: Settings.uiFontSizePt * 1.3
                onClicked: {
                    registrationPage.currentStep = 2;
                    registrationPage.autoSubmitNextStage();
                }
            }

            // ════════════════════════════════════════
            // STEP 2+: UIAA Stages
            // ════════════════════════════════════════

            // ── Email identity stage ──
            Rectangle {
                visible: registrationPage.currentStep >= 2 && registrationPage.currentStageType() === "m.login.email.identity"
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.paddingLarge
                color: palette.window
                radius: 8
                implicitHeight: emailCol.implicitHeight + Komai.paddingMedium * 2

                ColumnLayout {
                    id: emailCol
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingMedium

                    Label {
                        text: qsTr("Email verification required")
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        font.bold: true
                        color: palette.text
                    }

                    KomaiTextField {
                        id: emailField
                        Layout.fillWidth: true
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        placeholderText: qsTr("your@email.com")
                        visible: reg.emailSid.length === 0
                    }

                    KomaiButton {
                        visible: reg.emailSid.length === 0 && !reg.requestingEmail
                        enabled: emailField.text.trim().length > 0
                        text: qsTr("Send verification email")
                        highlighted: true
                        onClicked: reg.requestEmailToken(emailField.text.trim())
                    }

                    Item {
                        Layout.preferredHeight: Komai.listIconSize
                        Layout.fillWidth: true
                        visible: reg.requestingEmail

                        Spinner {
                            height: Komai.listIconSize
                            anchors.centerIn: parent
                            visible: running
                            running: parent.visible
                            foreground: palette.mid
                        }
                    }

                    Label {
                        visible: reg.emailSid.length > 0
                        Layout.fillWidth: true
                        text: qsTr("Check your email and click the verification link, then click Continue below.")
                        color: palette.text
                        wrapMode: Text.Wrap
                    }

                    RowLayout {
                        visible: reg.emailSid.length > 0
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        KomaiButton {
                            text: qsTr("Resend")
                            icon.source: "qrc:/icons/icons/ui/arrow-clockwise.svg"
                            enabled: !reg.requestingEmail
                            onClicked: reg.requestEmailToken(emailField.text.trim())
                        }

                        Item { Layout.fillWidth: true }

                        KomaiButton {
                            text: qsTr("Continue")
                            icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
                            highlighted: true
                            enabled: !reg.registering
                            onClicked: {
                                reg.submitStage(usernameField.text.trim(), passwordField.text,
                                    regDeviceField.text.trim(), "m.login.email.identity",
                                    "", reg.emailSid, reg.emailClientSecret);
                            }
                        }
                    }
                }
            }

            // ── Terms stage ──
            Rectangle {
                visible: registrationPage.currentStep >= 2 && registrationPage.currentStageType() === "m.login.terms"
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.paddingLarge
                color: palette.window
                radius: 8
                implicitHeight: termsCol.implicitHeight + Komai.paddingMedium * 2

                ColumnLayout {
                    id: termsCol
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingMedium

                    Label {
                        text: qsTr("Terms of Service")
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        font.bold: true
                        color: palette.text
                    }

                    Repeater {
                        model: reg.termsPolicies

                        delegate: Text {
                            required property var modelData
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            wrapMode: Text.Wrap
                            color: palette.text
                            text: "<style>a { color: " + palette.highlight + "; }</style>" +
                                  "<a href=\"" + modelData.url + "\">" + modelData.name + " (v" + modelData.version + ")</a>"
                            onLinkActivated: function(link) { Qt.openUrlExternally(link); }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                        }
                    }

                    KomaiButton {
                        text: qsTr("I accept the terms")
                        highlighted: true
                        enabled: !reg.registering
                        onClicked: {
                            reg.submitStage(usernameField.text.trim(), passwordField.text,
                                regDeviceField.text.trim(), "m.login.terms", "", "", "");
                        }
                    }
                }
            }

            // ── Registration token stage ──
            Rectangle {
                visible: registrationPage.currentStep >= 2 && registrationPage.currentStageType() === "m.login.registration_token"
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.paddingLarge
                color: palette.window
                radius: 8
                implicitHeight: tokenCol.implicitHeight + Komai.paddingMedium * 2

                ColumnLayout {
                    id: tokenCol
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingMedium

                    Label {
                        text: qsTr("Registration token required")
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        font.bold: true
                        color: palette.text
                    }

                    KomaiTextField {
                        id: tokenField
                        Layout.fillWidth: true
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        placeholderText: qsTr("Enter your registration token")
                    }

                    KomaiButton {
                        text: qsTr("Continue")
                        highlighted: true
                        enabled: tokenField.text.trim().length > 0 && !reg.registering
                        onClicked: {
                            reg.submitStage(usernameField.text.trim(), passwordField.text,
                                regDeviceField.text.trim(), "m.login.registration_token",
                                tokenField.text.trim(), "", "");
                        }
                    }
                }
            }

            // ── Browser fallback stage (recaptcha, SSO, unknown) ──
            Rectangle {
                visible: registrationPage.currentStep >= 2
                    && registrationPage.currentStageType() !== ""
                    && registrationPage.currentStageType() !== "m.login.dummy"
                    && registrationPage.currentStageType() !== "m.login.email.identity"
                    && registrationPage.currentStageType() !== "m.login.terms"
                    && registrationPage.currentStageType() !== "m.login.registration_token"
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.paddingLarge
                color: palette.window
                radius: 8
                implicitHeight: fallbackCol.implicitHeight + Komai.paddingMedium * 2

                ColumnLayout {
                    id: fallbackCol
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingMedium

                    Label {
                        text: registrationPage.currentStageType() === "m.login.recaptcha"
                            ? qsTr("CAPTCHA verification required")
                            : qsTr("Additional verification required")
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        font.bold: true
                        color: palette.text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Complete the verification in your browser, then click Confirm below.")
                        color: palette.buttonText
                        wrapMode: Text.Wrap
                    }

                    KomaiButton {
                        text: qsTr("Open verification")
                        icon.source: "qrc:/icons/icons/ui/forward.svg"
                        highlighted: true
                        onClicked: {
                            var url = reg.homeserverUrl + "/_matrix/client/v3/auth/"
                                + encodeURIComponent(registrationPage.currentStageType())
                                + "/fallback/web?session=" + encodeURIComponent(reg.flowStages.length > 0 ? reg.homeserverUrl : "");
                            Qt.openUrlExternally(url);
                        }
                    }

                    KomaiButton {
                        text: qsTr("I've completed the verification")
                        enabled: !reg.registering
                        onClicked: {
                            reg.submitStage(usernameField.text.trim(), passwordField.text,
                                regDeviceField.text.trim(), registrationPage.currentStageType(),
                                "", "", "");
                        }
                    }
                }
            }

            // ── Stage error ──
            MatrixText {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingSmall
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: reg.error
                visible: text && registrationPage.currentStep >= 2
                wrapMode: TextEdit.Wrap
            }

            // ── Stage spinner ──
            Item {
                Layout.preferredHeight: Komai.listIconSize
                Layout.fillWidth: true
                visible: reg.registering

                Spinner {
                    height: Komai.listIconSize
                    anchors.centerIn: parent
                    visible: running
                    running: parent.visible
                    foreground: palette.mid
                }
            }
        }

        AttributionFooter { showSponsor: false }
    }
}
