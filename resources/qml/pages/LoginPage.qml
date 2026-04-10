// SPDX-FileCopyrightText: Nheko Contributors
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

Item {
    id: loginPage
    property int maxExpansion: 800
    required property var loginController

    readonly property var login: loginController
    property string error: login.error

    StackView.onActivated: matrixIdField.forceActiveFocus()

    // Step management (0-based)
    property int currentStep: 0
    readonly property int stepCount: 3

    // Step 1 completion: homeserver is validated
    readonly property bool step1Complete: login.homeserverValid && !step1HasPendingInput
    // Step 2 completion: device name is provided (or will use fallback)
    // Always completeable — empty field uses OS default
    readonly property bool step2Complete: currentStep >= 2
    // Step 3 completion: login succeeded (handled by loginOk signal)

    // Step 1 state
    property bool isBareUsername: {
        const text = matrixIdField.text.trim();
        if (text.length === 0) return false;
        // A bare username has no '@' prefix or no ':server' part
        return !text.startsWith("@") || text.indexOf(":") < 0;
    }
    property bool step1HasPendingInput: {
        if (isBareUsername) {
            // Bare username: pending until server is filled and effective ID matches
            return serverField.text.trim().length === 0 || effectiveMatrixId() !== login.mxid;
        } else {
            // Full @user:server: pending until MXID is submitted
            return matrixIdField.text !== login.mxid;
        }
    }
    property bool step1LoginEnabled: login.homeserverValid && !step1HasPendingInput

    // Shared label width for consistent row alignment.
    // Uses max of all three label widths so translations are safe.
    readonly property int fieldLabelWidth: Math.max(
        mxidLabelMetrics.advanceWidth,
        serverLabelMetrics.advanceWidth,
        deviceLabelMetrics.advanceWidth,
        methodLabelMetrics.advanceWidth,
        passwordLabelMetrics.advanceWidth
    ) + Komai.paddingMedium * 2
    TextMetrics { id: mxidLabelMetrics; font.pointSize: Settings.uiFontSizePt * 1.1; text: qsTr("Matrix ID") }
    TextMetrics { id: serverLabelMetrics; font.pointSize: Settings.uiFontSizePt * 1.1; text: qsTr("Server") }
    TextMetrics { id: deviceLabelMetrics; font.pointSize: Settings.uiFontSizePt * 1.1; text: qsTr("Device name") }
    TextMetrics { id: methodLabelMetrics; font.pointSize: Settings.uiFontSizePt * 1.1; text: qsTr("Sign in method") }
    TextMetrics { id: passwordLabelMetrics; font.pointSize: Settings.uiFontSizePt * 1.1; text: qsTr("Password") }

    // Step 3 state: sign-in method index for "both available" (0 = browser/SSO, 1 = password)
    property int signInMethodIndex: 0
    readonly property bool showPasswordField: signInMethodIndex === 1

    // Effective Matrix ID: construct from bare username + server if needed
    function effectiveMatrixId() {
        const mxid = matrixIdField.text.trim();
        if (mxid.startsWith("@") && mxid.indexOf(":") > 0)
            return mxid;
        // Bare username — construct full ID from server field
        const server = serverField.text.trim();
        if (server.length === 0) return mxid;
        const local = mxid.startsWith("@") ? mxid.substring(1) : mxid;
        // Extract hostname from URL if needed
        let hostname = server;
        if (hostname.indexOf("://") >= 0) {
            try {
                const url = new URL(hostname);
                hostname = url.hostname;
                if (url.port) hostname += ":" + url.port;
            } catch(e) {}
        }
        return "@" + local + ":" + hostname;
    }

    // Effective device name
    function effectiveDeviceName() {
        const name = deviceNameField.text.trim();
        return name.length > 0 ? name : login.initialDeviceName();
    }

    function advanceToStep(step) {
        if (step >= 0 && step < stepCount)
            currentStep = step;
    }

    Onboarding.OnboardingScrollPage {
        id: scroll
        anchors.fill: parent
        maxContentWidth: loginPage.maxExpansion
        topSpacerHeight: Komai.paddingLarge * 2

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/logos/komai.svg"
                sourceSize.width: 128
                sourceSize.height: 128
                Layout.preferredHeight: 128
                Layout.preferredWidth: 128
                fillMode: Image.PreserveAspectFit
            }

            Label {
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingMedium
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: qsTr("Sign in to Matrix")
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.5
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            // ── Step Indicator ──
            StepIndicator {
                Layout.fillWidth: true
                Layout.bottomMargin: Komai.paddingLarge
                currentIndex: loginPage.currentStep
                model: [
                    { text: qsTr("Account") },
                    { text: qsTr("Device") },
                    { text: qsTr("Sign in") }
                ]
                onActivated: function(index) {
                    if (index < loginPage.currentStep) {
                        loginPage.currentStep = index;
                        // Reset step 3 method choice when going back
                        if (index < 2)
                            loginPage.signInMethodIndex = 0;
                    }
                }
            }

            // ════════════════════════════════════════
            // STEP 1: Account
            // ════════════════════════════════════════

            // ── Matrix ID ──
            Item {
                Layout.fillWidth: true
                implicitHeight: mxidRow.implicitHeight
                visible: loginPage.currentStep >= 0

                HoverHandler { id: mxidHover; blocking: false }
                Rectangle { anchors.fill: mxidRow; color: mxidHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: mxidRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.preferredWidth: loginPage.fieldLabelWidth
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Matrix ID")
                        color: mxidHover.hovered ? palette.brightText : palette.text
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    Item {
                        Layout.preferredWidth: matrixIdField.height / 2
                        Layout.preferredHeight: matrixIdField.height / 2
                        Layout.alignment: Qt.AlignVCenter

                        Spinner {
                            anchors.fill: parent
                            visible: running
                            running: login.lookingUpHs
                            foreground: palette.mid
                        }
                    }

                    KomaiTextField {
                        id: matrixIdField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        placeholderText: qsTr("e.g. @user:example.com or user")
                        readOnly: loginPage.currentStep !== 0
                        Keys.onReturnPressed: if (loginPage.step1Complete) loginPage.advanceToStep(1)
                        Keys.onEnterPressed: if (loginPage.step1Complete) loginPage.advanceToStep(1)

                        onTextChanged: mxidDebounce.restart()

                        Timer {
                            id: mxidDebounce
                            interval: 350
                            onTriggered: {
                                if (!loginPage.isBareUsername)
                                    login.mxid = matrixIdField.text;
                            }
                        }
                    }
                }
            }

            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: login.mxidError
                visible: text && loginPage.currentStep === 0
                wrapMode: TextEdit.Wrap
            }

            // ── Matrix ID hint (always visible on step 1) ──
            Label {
                Layout.fillWidth: true
                visible: !login.mxidError
                text: qsTr("Accounts live on a server. Enter a full ID or just a username with the server below.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignRight
                lineHeight: 1.3
            }

            // ── Server field ──
            Item {
                Layout.fillWidth: true
                implicitHeight: serverRow.implicitHeight
                visible: loginPage.currentStep >= 0

                HoverHandler { id: serverHover; blocking: false }
                Rectangle { anchors.fill: serverRow; color: serverHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: serverRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.preferredWidth: loginPage.fieldLabelWidth
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Server")
                        color: serverHover.hovered ? palette.brightText : palette.text
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    Item {
                        Layout.preferredWidth: serverField.height / 2
                        Layout.preferredHeight: serverField.height / 2
                        Layout.alignment: Qt.AlignVCenter

                        Spinner {
                            anchors.fill: parent
                            visible: running
                            running: login.lookingUpHs
                            foreground: palette.mid
                        }
                    }

                    KomaiTextField {
                        id: serverField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        placeholderText: qsTr("e.g. example.com or https://matrix.example.com")
                        text: login.homeserver
                        readOnly: loginPage.currentStep !== 0
                        Keys.onReturnPressed: if (loginPage.step1Complete) loginPage.advanceToStep(1)
                        Keys.onEnterPressed: if (loginPage.step1Complete) loginPage.advanceToStep(1)

                        onTextChanged: serverDebounce.restart()

                        Timer {
                            id: serverDebounce
                            interval: 350
                            onTriggered: {
                                // For bare username, construct the full @user:server ID
                                // and set it before the homeserver — so that
                                // checkHomeserverVersion() can parse it.
                                if (loginPage.isBareUsername && serverField.text.trim().length > 0)
                                    login.mxid = loginPage.effectiveMatrixId();
                                else
                                    login.homeserver = serverField.text;
                            }
                        }
                    }
                }
            }

            // ── Server help text ──
            Label {
                Layout.fillWidth: true
                text: qsTr("Both a server name and a full homeserver URL work.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignRight
            }

            // ── Step 1 status messages ──
            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: loginPage.error
                visible: text && loginPage.currentStep === 0
                wrapMode: TextEdit.Wrap
            }

            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: palette.buttonText
                visible: loginPage.currentStep === 0 && !loginPage.error && login.lookingUpHs
                text: qsTr("Checking server...")
                wrapMode: TextEdit.Wrap
            }

            // ── Step 1 Continue button ──
            KomaiButton {
                visible: loginPage.currentStep === 0
                enabled: loginPage.step1Complete
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingMedium
                text: qsTr("Continue")
                icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
                highlighted: true
                font.pointSize: Settings.uiFontSizePt * 1.3
                onClicked: loginPage.advanceToStep(1)
                Keys.onReturnPressed: if (enabled) loginPage.advanceToStep(1)
                Keys.onEnterPressed: if (enabled) loginPage.advanceToStep(1)
            }

            // ════════════════════════════════════════
            // STEP 2: Device Name
            // ════════════════════════════════════════

            // ── Device name field ──
            Item {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingLarge
                implicitHeight: deviceRow.implicitHeight
                visible: loginPage.currentStep >= 1

                HoverHandler { id: deviceHover; blocking: false }
                Rectangle { anchors.fill: deviceRow; color: deviceHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: deviceRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.preferredWidth: loginPage.fieldLabelWidth
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Device name")
                        color: deviceHover.hovered ? palette.brightText : palette.text
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    // Spacer matching the spinner slot in other rows
                    Item {
                        Layout.preferredWidth: deviceNameField.height / 2
                        Layout.preferredHeight: deviceNameField.height / 2
                        Layout.alignment: Qt.AlignVCenter
                    }

                    KomaiTextField {
                        id: deviceNameField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        text: login.deviceNameOS()
                        readOnly: loginPage.currentStep !== 1
                        Keys.onReturnPressed: loginPage.advanceToStep(2)
                        Keys.onEnterPressed: loginPage.advanceToStep(2)
                    }
                }
            }

            // ── Device name explanation (below field) ──
            Label {
                Layout.fillWidth: true
                visible: loginPage.currentStep >= 1
                text: qsTr("Multiple Matrix chat apps and devices can be disorienting. Pick a name that you and others can recognize.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignRight
                lineHeight: 1.3
            }

            // ── Device name suggestions ──
            Flow {
                Layout.maximumWidth: parent.width - Komai.paddingMedium * 2
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: Komai.paddingMedium
                spacing: Komai.paddingSmall
                visible: loginPage.currentStep >= 1

                Label {
                    text: qsTr("Suggestions:")
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                    height: suggestOsBtn.height
                    verticalAlignment: Text.AlignVCenter
                }

                KomaiButton {
                    id: suggestOsBtn
                    enabled: loginPage.currentStep === 1
                    text: login.deviceNameOS()
                    font.pointSize: Settings.uiFontSizePt
                    topPadding: Komai.paddingSmall * 0.5
                    bottomPadding: Komai.paddingSmall * 0.5
                    leftPadding: Komai.paddingSmall
                    rightPadding: Komai.paddingSmall
                    highlighted: deviceNameField.text === text
                    onClicked: deviceNameField.text = text
                }

                KomaiButton {
                    enabled: loginPage.currentStep === 1
                    text: login.deviceNameHostname()
                    font.pointSize: Settings.uiFontSizePt
                    topPadding: Komai.paddingSmall * 0.5
                    bottomPadding: Komai.paddingSmall * 0.5
                    leftPadding: Komai.paddingSmall
                    rightPadding: Komai.paddingSmall
                    highlighted: deviceNameField.text === text
                    onClicked: deviceNameField.text = text
                }

                KomaiButton {
                    id: suggestRandomBtn
                    enabled: loginPage.currentStep === 1
                    property string randomName: login.deviceNameRandom()
                    text: randomName
                    font.pointSize: Settings.uiFontSizePt
                    topPadding: Komai.paddingSmall * 0.5
                    bottomPadding: Komai.paddingSmall * 0.5
                    leftPadding: Komai.paddingSmall
                    rightPadding: Komai.paddingSmall
                    highlighted: deviceNameField.text === text
                    onClicked: deviceNameField.text = randomName
                }

                KomaiButton {
                    enabled: loginPage.currentStep === 1
                    icon.source: "qrc:/icons/icons/ui/arrow-clockwise.svg"
                    display: AbstractButton.IconOnly
                    topPadding: Komai.paddingSmall * 0.5
                    bottomPadding: Komai.paddingSmall * 0.5
                    leftPadding: Komai.paddingMedium
                    rightPadding: Komai.paddingMedium
                    toolTipText: qsTr("Generate another random name")
                    onClicked: suggestRandomBtn.randomName = login.deviceNameRandom()
                }
            }

            // ── Step 2 Continue button ──
            KomaiButton {
                id: step2ContinueBtn
                visible: loginPage.currentStep === 1
                onVisibleChanged: if (visible) forceActiveFocus()
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingMedium
                text: qsTr("Continue")
                icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
                highlighted: true
                font.pointSize: Settings.uiFontSizePt * 1.3
                onClicked: loginPage.advanceToStep(2)
                Keys.onReturnPressed: loginPage.advanceToStep(2)
                Keys.onEnterPressed: loginPage.advanceToStep(2)
            }

            // ════════════════��═══════════════════════
            // STEP 3: Sign In
            // ════════════════════════════════════════

            // ── Logging in spinner ──
            Item {
                Layout.preferredHeight: login.loggingIn ? Komai.listIconSize : 0
                Layout.fillWidth: true
                visible: loginPage.currentStep === 2

                Spinner {
                    height: Komai.listIconSize
                    anchors.centerIn: parent
                    visible: running
                    running: login.loggingIn
                    foreground: palette.mid
                }
            }

            // ── Step 3 error ──
            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: loginPage.error
                visible: text && loginPage.currentStep === 2
                wrapMode: TextEdit.Wrap
            }

            // ── SSO-only: single button ──
            KomaiButton {
                visible: loginPage.currentStep === 2 && login.ssoSupported && !login.passwordSupported
                    && login.identityProviders.length <= 1
                onVisibleChanged: if (visible) forceActiveFocus()
                enabled: !login.loggingIn
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingMedium
                text: qsTr("Continue in browser (SSO)")
                icon.source: "qrc:/icons/icons/ui/forward.svg"
                highlighted: true
                font.pointSize: Settings.uiFontSizePt * 1.3
                function doSsoLogin() {
                    const idpId = login.identityProviders.length > 0 ? login.identityProviders[0].id : "";
                    login.onLoginButtonClicked(Login.SSO, loginPage.effectiveMatrixId(), idpId, loginPage.effectiveDeviceName());
                }
                onClicked: doSsoLogin()
                Keys.onReturnPressed: if (enabled) doSsoLogin()
                Keys.onEnterPressed: if (enabled) doSsoLogin()
            }

            // ── SSO-only with multiple identity providers ──
            Repeater {
                id: ssoOnlyRepeater
                model: (loginPage.currentStep === 2 && login.ssoSupported && !login.passwordSupported
                    && login.identityProviders.length > 1) ? login.identityProviders : []

                delegate: KomaiButton {
                    id: ssoIdpBtn
                    required property int index
                    required property var modelData
                    Component.onCompleted: if (index === 0) forceActiveFocus()
                    enabled: !login.loggingIn
                    Layout.alignment: Qt.AlignHCenter
                    text: modelData.name
                    icon.source: modelData.avatarUrl ? modelData.avatarUrl.replace("mxc://", "image://MxcImage/") : ""
                    highlighted: true
                    font.pointSize: Settings.uiFontSizePt * 1.3
                    function doSsoLogin() {
                        login.onLoginButtonClicked(Login.SSO, loginPage.effectiveMatrixId(), modelData.id, loginPage.effectiveDeviceName());
                    }
                    onClicked: doSsoLogin()
                    Keys.onReturnPressed: if (enabled) doSsoLogin()
                    Keys.onEnterPressed: if (enabled) doSsoLogin()
                }
            }

            // ── Password-only: password field + sign in ──
            Item {
                Layout.fillWidth: true
                implicitHeight: pwRow.implicitHeight
                visible: loginPage.currentStep === 2 && login.passwordSupported && !login.ssoSupported
                onVisibleChanged: if (visible) pwOnlyField.forceActiveFocus()

                HoverHandler { id: pwOnlyHover; blocking: false }
                Rectangle { anchors.fill: pwRow; color: pwOnlyHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: pwRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Password")
                        color: pwOnlyHover.hovered ? palette.brightText : palette.text
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    ImageButton {
                        Layout.preferredWidth: Math.round(Settings.uiFontSizePt * 2)
                        Layout.preferredHeight: Math.round(Settings.uiFontSizePt * 2)
                        Layout.alignment: Qt.AlignVCenter
                        buttonTextColor: pwOnlyHover.hovered ? palette.brightText : palette.buttonText
                        image: pwOnlyField.echoMode === TextInput.Password ? ":/icons/icons/ui/eye-show.svg" : ":/icons/icons/ui/eye-hide.svg"
                        toolTipVisible: hovered
                        toolTipText: qsTr("Show/Hide Password")
                        onClicked: {
                            pwOnlyField.echoMode = pwOnlyField.echoMode === TextInput.Normal
                                ? TextInput.Password : TextInput.Normal;
                        }
                    }

                    KomaiTextField {
                        id: pwOnlyField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        echoMode: TextInput.Password
                        Keys.onReturnPressed: if (pwOnlyBtn.enabled) pwOnlyBtn.doPwLogin()
                        Keys.onEnterPressed: if (pwOnlyBtn.enabled) pwOnlyBtn.doPwLogin()
                    }
                }
            }

            KomaiButton {
                id: pwOnlyBtn
                visible: loginPage.currentStep === 2 && login.passwordSupported && !login.ssoSupported
                enabled: !login.loggingIn
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingMedium
                text: qsTr("Sign in")
                icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
                highlighted: true
                font.pointSize: Settings.uiFontSizePt * 1.3
                function doPwLogin() {
                    login.onLoginButtonClicked(Login.Password, loginPage.effectiveMatrixId(), pwOnlyField.text, loginPage.effectiveDeviceName());
                }
                onClicked: doPwLogin()
                Keys.onReturnPressed: if (enabled) doPwLogin()
                Keys.onEnterPressed: if (enabled) doPwLogin()
            }

            // ── Both available: sign-in method selector ──
            Item {
                Layout.fillWidth: true
                implicitHeight: methodRow.implicitHeight
                visible: loginPage.currentStep === 2 && login.passwordSupported && login.ssoSupported

                HoverHandler { id: methodHover; blocking: false }
                Rectangle { anchors.fill: methodRow; color: methodHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: methodRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.preferredWidth: loginPage.fieldLabelWidth
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Sign in method")
                        color: methodHover.hovered ? palette.brightText : palette.text
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    // Spacer matching the spinner slot in other rows
                    Item {
                        Layout.preferredWidth: signInMethodSegment.height / 2
                        Layout.preferredHeight: signInMethodSegment.height / 2
                        Layout.alignment: Qt.AlignVCenter
                    }

                    SegmentedButton {
                        id: signInMethodSegment
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        currentIndex: loginPage.signInMethodIndex
                        model: [
                            { text: qsTr("Browser (SSO)"), value: "sso" },
                            { text: qsTr("Password"), value: "password" }
                        ]
                        onActivated: function(index) {
                            loginPage.signInMethodIndex = index;
                        }
                    }
                }
            }

            // ── Both: SSO provider buttons (when browser/SSO selected) ──
            Repeater {
                model: (loginPage.currentStep === 2 && login.passwordSupported && login.ssoSupported && !loginPage.showPasswordField)
                    ? login.identityProviders : []

                delegate: KomaiButton {
                    id: ssoBothBtn
                    required property int index
                    required property var modelData
                    Component.onCompleted: if (index === 0) forceActiveFocus()
                    enabled: !login.loggingIn
                    Layout.alignment: Qt.AlignHCenter
                    text: modelData.name
                    icon.source: modelData.avatarUrl ? modelData.avatarUrl.replace("mxc://", "image://MxcImage/") : "qrc:/icons/icons/ui/forward.svg"
                    highlighted: true
                    font.pointSize: Settings.uiFontSizePt * 1.3
                    function doSsoLogin() {
                        login.onLoginButtonClicked(Login.SSO, loginPage.effectiveMatrixId(), modelData.id, loginPage.effectiveDeviceName());
                    }
                    onClicked: doSsoLogin()
                    Keys.onReturnPressed: if (enabled) doSsoLogin()
                    Keys.onEnterPressed: if (enabled) doSsoLogin()
                }
            }

            // ── Both: Password field (when password selected) ──
            Item {
                Layout.fillWidth: true
                implicitHeight: pwBothRow.implicitHeight
                visible: loginPage.currentStep === 2 && login.passwordSupported && login.ssoSupported && loginPage.showPasswordField
                onVisibleChanged: if (visible) pwBothField.forceActiveFocus()

                HoverHandler { id: pwBothHover; blocking: false }
                Rectangle { anchors.fill: pwBothRow; color: pwBothHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: pwBothRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.preferredWidth: loginPage.fieldLabelWidth
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Password")
                        color: pwBothHover.hovered ? palette.brightText : palette.text
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    // Spacer matching the spinner slot in other rows
                    Item {
                        Layout.preferredWidth: pwBothField.height / 2
                        Layout.preferredHeight: pwBothField.height / 2
                        Layout.alignment: Qt.AlignVCenter
                    }

                    ImageButton {
                        Layout.preferredWidth: Math.round(Settings.uiFontSizePt * 2)
                        Layout.preferredHeight: Math.round(Settings.uiFontSizePt * 2)
                        Layout.alignment: Qt.AlignVCenter
                        buttonTextColor: pwBothHover.hovered ? palette.brightText : palette.buttonText
                        image: pwBothField.echoMode === TextInput.Password ? ":/icons/icons/ui/eye-show.svg" : ":/icons/icons/ui/eye-hide.svg"
                        toolTipVisible: hovered
                        toolTipText: qsTr("Show/Hide Password")
                        onClicked: {
                            pwBothField.echoMode = pwBothField.echoMode === TextInput.Normal
                                ? TextInput.Password : TextInput.Normal;
                        }
                    }

                    KomaiTextField {
                        id: pwBothField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        echoMode: TextInput.Password
                        Keys.onReturnPressed: if (pwBothBtn.enabled) pwBothBtn.doPwLogin()
                        Keys.onEnterPressed: if (pwBothBtn.enabled) pwBothBtn.doPwLogin()
                    }
                }
            }

            KomaiButton {
                id: pwBothBtn
                visible: loginPage.currentStep === 2 && login.passwordSupported && login.ssoSupported && loginPage.showPasswordField
                enabled: !login.loggingIn
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingMedium
                text: qsTr("Sign in")
                icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
                highlighted: true
                font.pointSize: Settings.uiFontSizePt * 1.3
                function doPwLogin() {
                    login.onLoginButtonClicked(Login.Password, loginPage.effectiveMatrixId(), pwBothField.text, loginPage.effectiveDeviceName());
                }
                onClicked: doPwLogin()
                Keys.onReturnPressed: if (enabled) doPwLogin()
                Keys.onEnterPressed: if (enabled) doPwLogin()
            }
    }

    ImageButton {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Komai.paddingMedium
        width: Komai.listIconSize
        height: Komai.listIconSize
        image: ":/icons/icons/ui/angle-arrow-left.svg"
        toolTipVisible: hovered
        toolTipText: qsTr("Back")
        onClicked: mainWindow.pop()
    }
}
