// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

OverlayDialog {
    id: inviteDialogRoot

    property InviteesModel invitees
    readonly property int selectedCount: invitees ? invitees.count : 0

    // Card row metrics shared by the results and staging panels.
    readonly property int cardRowHeight: Komai.iconSize + Komai.paddingMedium * 2
    readonly property int listSpacing: Komai.paddingSmall

    // The dialog parents onto Overlay.overlay, which fills the window; use the
    // viewport height (never content height) to budget the list panels.
    readonly property real windowHeight: overlayDialogViewport ? overlayDialogViewport.height : 800

    // Scrollbar handling, honouring the user's tri-state scrollbar policy.
    readonly property int scrollbarPolicySetting: Settings.uiScrollbarPolicy
    // Reserve a gutter whenever a scrollbar can appear, so it never paints over
    // the rows and the row width stays stable when it shows/hides.
    readonly property bool scrollbarsReserveGutter: scrollbarPolicySetting !== Settings.ScrollbarPolicy.Never

    // Fixed reserved height for a list panel: a function of row count and the
    // window only, never of the content, so the panels keep a constant size and
    // nothing shifts under the pointer while picking users.
    function reservedListHeight(rows) {
        const rowH = cardRowHeight + listSpacing;
        const cap = Math.round(windowHeight * 0.28);
        return Math.max(rowH * 2, Math.min(rowH * rows, cap));
    }

    // Height budget for the scrollable middle section (search + both list
    // panels): whatever's left below the dialog's fixed y position and above
    // the pinned Invite button, so a short window scrolls that section
    // instead of pushing the Invite button off-screen. Mirrors the same fix
    // in ForwardCompleter.qml.
    readonly property real inviteScrollChromeHeight: overlayDialogChromeHeight
        + Komai.paddingMedium + inviteButton.implicitHeight
    readonly property real availableScrollHeight: Math.max(
        cardRowHeight * 2,
        windowHeight - y - Komai.paddingLarge - inviteScrollChromeHeight)

    title: invitees && invitees.roomName.length > 0
        ? qsTr("Invite users to %1").arg(invitees.roomName)
        : qsTr("Invite users")
    titleIcon: ":/icons/icons/ui/plus-circle.svg"
    initialFocusItem: inviteeEntry
    overlayDialogMinWidth: 760

    onOpened: {
        // Full reset every open: fresh directory search, empty query.
        inviteeEntry.clear();
        userDirectory.setSearchString("");
        searchResults.currentIndex = searchResults.count > 0 ? 0 : -1;
        inviteeEntry.forceActiveFocus();
    }
    onClosed: {
        inviteeEntry.clear();
        userDirectory.setSearchString("");
    }

    function resetSearch()
    {
        inviteeEntry.clear();
        userDirectory.setSearchString("");
        searchResults.currentIndex = -1;
        inviteeEntry.forceActiveFocus();
    }

    // Stage one user. When keepSearch is true the query and result list stay put
    // so several people can be staged in a row without re-aiming; the staged rows
    // remain visible with an "added" treatment. The direct-MXID path resets,
    // since inviting the typed id consumes the field.
    function addInvite(mxid, displayName, avatarUrl, keepSearch)
    {
        const trimmedMxid = (mxid || "").trim();
        if (!trimmedMxid.match("@.+?:.{3,}"))
            return false;
        if (invitees.containsUser(trimmedMxid))
            return false;

        invitees.addUser(trimmedMxid, displayName || "", avatarUrl || "");
        if (!keepSearch)
            resetSearch();
        return true;
    }

    // Stage the highlighted (or, if none, the first) directory result.
    function stageHighlightedResult(keepSearch)
    {
        if (searchResults.count === 0)
            return false;

        let index = searchResults.currentIndex;
        if (index < 0 || index >= searchResults.count)
            index = 0;

        const item = searchResults.itemAtIndex(index);
        if (item && item.enabled)
            return addInvite(item.userIdText, item.displayNameText, item.avatarUrlText, keepSearch);

        return false;
    }

    // Enter from the search field: invite the typed MXID directly, else stage the
    // highlighted result (keeping the search so more can be staged).
    function stageCurrentResult()
    {
        if (inviteeEntry.isValidMxid)
            return addInvite(inviteeEntry.resolvedMxid, directInviteCard.resolvedDisplayName, directInviteCard.resolvedAvatarUrl, false);
        return stageHighlightedResult(true);
    }

    // Pull in whatever is pending before committing, then close.
    function addCurrentInvite()
    {
        if (inviteeEntry.isValidMxid)
            return addInvite(inviteeEntry.resolvedMxid, directInviteCard.resolvedDisplayName, directInviteCard.resolvedAvatarUrl, false);
        return stageHighlightedResult(false);
    }

    function cleanUpAndClose() {
        addCurrentInvite();
        if (invitees.count === 0)
            return;
        invitees.accept();
        close();
    }

    function selectedHeadingText() {
        if (selectedCount > 0)
            return qsTr("Selected users (%1)").arg(selectedCount);
        return qsTr("Selected users");
    }

    // Ctrl+Enter invites everyone staged, from anywhere in the dialog.
    Shortcut {
        sequences: ["Ctrl+Return", "Ctrl+Enter"]
        enabled: inviteDialogRoot.visible && inviteDialogRoot.selectedCount > 0
        onActivated: inviteDialogRoot.cleanUpAndClose()
    }

    // Scrollable middle section: search, direct-invite card and both list
    // panels. Capped to availableScrollHeight so on a short window this
    // section scrolls instead of the Invite button running off the bottom.
    Flickable {
        id: inviteScrollArea

        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(inviteScrollColumn.implicitHeight, inviteDialogRoot.availableScrollHeight)
        contentWidth: width
        contentHeight: inviteScrollColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        FlickableWheelBooster {
            flickable: inviteScrollArea
        }

        ScrollBar.vertical: ScrollBar {
            id: inviteScrollBar

            policy: {
                switch (inviteDialogRoot.scrollbarPolicySetting) {
                case Settings.ScrollbarPolicy.Always:
                    return ScrollBar.AlwaysOn;
                case Settings.ScrollbarPolicy.Never:
                    return ScrollBar.AlwaysOff;
                default:
                    return inviteScrollArea.contentHeight > inviteScrollArea.height
                        ? ScrollBar.AlwaysOn
                        : ScrollBar.AlwaysOff;
                }
            }
        }

        ColumnLayout {
            id: inviteScrollColumn

            width: inviteScrollArea.width
            spacing: Komai.paddingMedium

            // --- Search ---
            KomaiTextField {
                id: inviteeEntry

                readonly property string localHomeserver: {
                    const uid = Settings.userId;
                    const colonIdx = uid.indexOf(":");
                    return colonIdx >= 0 ? uid.substring(colonIdx + 1) : "";
                }

                function normalizedMxid(input) {
                    var t = input.trim();
                    if (t.length === 0)
                        return "";
                    if (t.charAt(0) !== "@")
                        t = "@" + t;
                    if (t.indexOf(":") < 0 && localHomeserver.length > 0)
                        t = t + ":" + localHomeserver;
                    return t;
                }

                property string resolvedMxid: normalizedMxid(text)
                property bool isValidMxid: resolvedMxid.match("@.+?:.{3,}")

                Layout.fillWidth: true
                placeholderText: qsTr("Search by name or @user:example.com")
                font.pixelSize: Math.ceil(Komai.fontPixelSize * 1.2)

                Keys.onShortcutOverride: (event) => {
                    // Claim navigation / staging keys before the platform turns them
                    // into edit operations (Ctrl+U "clear line", etc.).
                    if (event.key === Qt.Key_Up || event.key === Qt.Key_Down
                            || ((event.key === Qt.Key_D || event.key === Qt.Key_U)
                                && (event.modifiers & Qt.ControlModifier))
                            || (event.matches(StandardKey.InsertParagraphSeparator)
                                && !(event.modifiers & Qt.ControlModifier)))
                        event.accepted = true;
                }
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Up) {
                        searchResults.moveSelection(-1);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Down) {
                        searchResults.moveSelection(1);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_U && (event.modifiers & Qt.ControlModifier)) {
                        searchResults.moveSelection(-searchResults.pageStep);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_D && (event.modifiers & Qt.ControlModifier)) {
                        searchResults.moveSelection(searchResults.pageStep);
                        event.accepted = true;
                    } else if (event.matches(StandardKey.InsertParagraphSeparator)
                               && !(event.modifiers & Qt.ControlModifier)) {
                        inviteDialogRoot.stageCurrentResult();
                        event.accepted = true;
                    }
                }
                onTextChanged: {
                    if (text.trim().length === 0)
                        userDirectory.setSearchString("");
                    else
                        searchTimer.restart();
                }

                Timer {
                    id: searchTimer

                    interval: 350
                    onTriggered: {
                        if (inviteeEntry.text.trim().length > 0)
                            userDirectory.setSearchString(inviteeEntry.text.trim());
                    }
                }
            }

            // Direct MXID invite card — shown when input is a valid MXID
            AbstractButton {
                id: directInviteCard

                readonly property bool activeState: hovered || pressed
                readonly property bool alreadyAdded: inviteDialogRoot.selectedCount > 0
                    && inviteDialogRoot.invitees.containsUser(inviteeEntry.resolvedMxid)
                property string resolvedDisplayName: ""
                property string resolvedAvatarUrl: ""
                property string resolvedMxid: ""

                function resetResolved() {
                    resolvedDisplayName = "";
                    resolvedAvatarUrl = "";
                    resolvedMxid = "";
                }

                Layout.fillWidth: true
                activeFocusOnTab: false
                visible: inviteeEntry.isValidMxid && !alreadyAdded
                implicitHeight: directInviteRow.implicitHeight + Komai.paddingSmall * 2
                onClicked: inviteDialogRoot.addInvite(inviteeEntry.resolvedMxid,
                    directInviteCard.resolvedDisplayName, directInviteCard.resolvedAvatarUrl, false)

                Connections {
                    target: inviteeEntry
                    function onResolvedMxidChanged() {
                        directInviteCard.resetResolved();
                        if (inviteeEntry.isValidMxid)
                            userDirectory.resolveUser(inviteeEntry.resolvedMxid);
                    }
                }

                Connections {
                    target: userDirectory
                    function onUserResolved(mxid, displayName, avatarUrl) {
                        if (mxid === inviteeEntry.resolvedMxid) {
                            directInviteCard.resolvedMxid = mxid;
                            directInviteCard.resolvedDisplayName = displayName;
                            directInviteCard.resolvedAvatarUrl = avatarUrl;
                        }
                    }
                }

                background: Rectangle {
                    radius: Komai.paddingMedium
                    color: directInviteCard.activeState ? palette.dark : palette.window
                    border.color: Komai.theme.separator
                    border.width: 1
                }

                contentItem: RowLayout {
                    id: directInviteRow

                    spacing: Komai.paddingMedium

                    Avatar {
                        Layout.preferredWidth: Komai.iconSize
                        Layout.preferredHeight: Komai.iconSize
                        Layout.alignment: Qt.AlignVCenter
                        Layout.leftMargin: Komai.paddingMedium
                        userid: inviteeEntry.resolvedMxid
                        displayName: directInviteCard.resolvedDisplayName
                        url: (directInviteCard.resolvedAvatarUrl || "").replace("mxc://", "image://MxcImage/")
                        enabled: false
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall

                        Label {
                            Layout.fillWidth: true
                            text: directInviteCard.resolvedDisplayName || qsTr("Invite directly")
                            color: directInviteCard.activeState ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt
                            font.italic: !directInviteCard.resolvedDisplayName
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: inviteeEntry.resolvedMxid
                            color: directInviteCard.activeState ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt * 0.9
                            elide: Text.ElideRight
                        }
                    }

                    Image {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        Layout.rightMargin: Komai.paddingMedium
                        fillMode: Image.PreserveAspectFit
                        source: "image://colorimage/:/icons/icons/ui/plus-circle.svg?"
                            + (directInviteCard.activeState ? palette.brightText : palette.buttonText)
                    }
                }

                KomaiCursorShape {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                }
            }

            // --- Results (directory matches) ---
            Label {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingSmall
                text: qsTr("Users")
                color: palette.text
                font.bold: true
                font.pointSize: Settings.uiFontSizePt * 1.1
            }

            ListView {
                id: searchResults

                // How far Ctrl+D / Ctrl+U jump through the result list.
                readonly property int pageStep: 5
                // Reserve the scrollbar gutter so rows never sit under the bar.
                readonly property real scrollGutter: inviteDialogRoot.scrollbarsReserveGutter
                    ? (searchScrollBar.implicitWidth + Komai.paddingSmall)
                    : 0

                function moveSelection(delta) {
                    if (count === 0) {
                        currentIndex = -1;
                        return;
                    }
                    let next = (currentIndex < 0 ? 0 : currentIndex) + delta;
                    next = Math.max(0, Math.min(count - 1, next));
                    currentIndex = next;
                    positionViewAtIndex(next, ListView.Contain);
                }

                Layout.fillWidth: true
                Layout.preferredHeight: inviteDialogRoot.reservedListHeight(6)
                model: userDirectory
                clip: true
                spacing: inviteDialogRoot.listSpacing
                boundsBehavior: Flickable.StopAtBounds
                highlightFollowsCurrentItem: true
                keyNavigationEnabled: false

                onCountChanged: {
                    if (count > 0 && (currentIndex < 0 || currentIndex >= count))
                        currentIndex = 0;
                    else if (count === 0)
                        currentIndex = -1;
                }

                FlickableWheelBooster { flickable: searchResults }

                ScrollBar.vertical: ScrollBar {
                    id: searchScrollBar

                    policy: {
                        switch (inviteDialogRoot.scrollbarPolicySetting) {
                        case Settings.ScrollbarPolicy.Always:
                            return ScrollBar.AlwaysOn;
                        case Settings.ScrollbarPolicy.Never:
                            return ScrollBar.AlwaysOff;
                        default:
                            return searchResults.contentHeight > searchResults.height
                                ? ScrollBar.AlwaysOn
                                : ScrollBar.AlwaysOff;
                        }
                    }
                }

                delegate: AbstractButton {
                    id: resultDelegate

                    readonly property bool alreadySelected: inviteDialogRoot.selectedCount > 0
                        && inviteDialogRoot.invitees.containsUser(model.userid)
                    readonly property bool current: model.index === searchResults.currentIndex
                    property string userIdText: model.userid
                    property string displayNameText: model.displayName
                    property string avatarUrlText: model.avatarUrl

                    width: searchResults.width - searchResults.scrollGutter
                    implicitHeight: resultRow.implicitHeight + Komai.paddingSmall * 2
                    hoverEnabled: true
                    enabled: !alreadySelected
                    activeFocusOnTab: false
                    onClicked: {
                        searchResults.currentIndex = model.index;
                        inviteDialogRoot.addInvite(userIdText, displayNameText, avatarUrlText, true);
                    }
                    onHoveredChanged: {
                        if (hovered && enabled)
                            searchResults.currentIndex = model.index;
                    }

                    background: Rectangle {
                        radius: Komai.paddingMedium
                        color: {
                            if (resultDelegate.current && resultDelegate.enabled)
                                return palette.highlight;
                            if (resultDelegate.alreadySelected)
                                return Qt.tint(palette.window,
                                               Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.22));
                            return palette.window;
                        }
                        border.width: 1
                        border.color: resultDelegate.alreadySelected
                            ? palette.highlight
                            : Komai.theme.separator
                    }

                    contentItem: RowLayout {
                        id: resultRow

                        spacing: Komai.paddingMedium

                        Avatar {
                            Layout.preferredWidth: Komai.iconSize
                            Layout.preferredHeight: Komai.iconSize
                            Layout.alignment: Qt.AlignVCenter
                            Layout.leftMargin: Komai.paddingMedium
                            userid: resultDelegate.userIdText
                            url: (resultDelegate.avatarUrlText || "").replace("mxc://", "image://MxcImage/")
                            displayName: resultDelegate.displayNameText
                            enabled: false
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Komai.paddingSmall

                            Label {
                                Layout.fillWidth: true
                                text: resultDelegate.displayNameText || qsTr("Unknown display name")
                                color: resultDelegate.current && resultDelegate.enabled
                                    ? palette.highlightedText
                                    : resultDelegate.alreadySelected
                                        ? palette.buttonText
                                        : resultDelegate.displayNameText ? palette.text : palette.buttonText
                                font.pointSize: Settings.uiFontSizePt
                                font.italic: !resultDelegate.displayNameText
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: resultDelegate.userIdText
                                color: resultDelegate.current && resultDelegate.enabled
                                    ? palette.highlightedText
                                    : palette.buttonText
                                font.pointSize: Settings.uiFontSizePt * 0.9
                                elide: Text.ElideRight
                            }
                        }

                        Image {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: visible ? 18 : 0
                            Layout.preferredHeight: 18
                            Layout.rightMargin: Komai.paddingMedium
                            visible: resultDelegate.alreadySelected
                            fillMode: Image.PreserveAspectFit
                            source: visible
                                ? "image://colorimage/:/icons/icons/ui/double-checkmark.svg?" + palette.highlight
                                : ""
                        }
                    }

                    KomaiCursorShape {
                        anchors.fill: parent
                        cursorShape: resultDelegate.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: searchResults.count === 0 && inviteeEntry.text.trim().length === 0
                    text: qsTr("Type a search query. Results will appear here.")
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt * 0.95
                }

                Column {
                    anchors.centerIn: parent
                    spacing: Komai.paddingSmall
                    visible: searchResults.count === 0
                        && inviteeEntry.text.trim().length > 0
                        && !searchTimer.running
                        && !userDirectory.searchingUsers

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("No matching users found.")
                        color: palette.buttonText
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: inviteeEntry.isValidMxid
                        text: qsTr("Use the suggestion above to invite by Matrix ID.")
                        color: palette.buttonText
                        font.pointSize: Settings.uiFontSizePt
                    }
                }

                Spinner {
                    anchors.centerIn: parent
                    height: 48
                    running: searchResults.count === 0
                        && inviteeEntry.text.trim().length > 0
                        && (searchTimer.running || userDirectory.searchingUsers)
                    visible: running
                }
            }

            // --- Staging (selected users) ---
            Label {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingSmall
                text: inviteDialogRoot.selectedHeadingText()
                color: palette.text
                font.bold: true
                font.pointSize: Settings.uiFontSizePt * 1.1
            }

            ListView {
                id: selectedInvitees

                readonly property real scrollGutter: inviteDialogRoot.scrollbarsReserveGutter
                    ? (selectedScrollBar.implicitWidth + Komai.paddingSmall)
                    : 0

                Layout.fillWidth: true
                Layout.preferredHeight: inviteDialogRoot.reservedListHeight(4)
                clip: true
                model: inviteDialogRoot.invitees
                spacing: inviteDialogRoot.listSpacing
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    id: selectedScrollBar

                    policy: {
                        switch (inviteDialogRoot.scrollbarPolicySetting) {
                        case Settings.ScrollbarPolicy.Always:
                            return ScrollBar.AlwaysOn;
                        case Settings.ScrollbarPolicy.Never:
                            return ScrollBar.AlwaysOff;
                        default:
                            return selectedInvitees.contentHeight > selectedInvitees.height
                                ? ScrollBar.AlwaysOn
                                : ScrollBar.AlwaysOff;
                        }
                    }
                }

                delegate: Rectangle {
                    width: selectedInvitees.width - selectedInvitees.scrollGutter
                    implicitHeight: selectedInviteeRow.implicitHeight + Komai.paddingSmall * 2
                    color: palette.window
                    radius: Komai.paddingMedium
                    border.color: Komai.theme.separator
                    border.width: 1

                    RowLayout {
                        id: selectedInviteeRow

                        anchors.fill: parent
                        anchors.margins: Komai.paddingSmall
                        spacing: Komai.paddingMedium

                        AvatarUserFlipButton {
                            Layout.preferredWidth: Komai.iconSize
                            Layout.preferredHeight: Komai.iconSize
                            Layout.alignment: Qt.AlignVCenter
                            avatarButtonSize: Komai.iconSize
                            cleanFront: true
                            avatarDisplayName: model.displayName
                            avatarUrl: (model.avatarUrl || "").replace("mxc://", "image://MxcImage/")
                            avatarUserId: model.mxid
                            badgeIconSource: ":/icons/icons/ui/person.svg"
                            onLeftClicked: TimelineManager.openGlobalUserProfile(model.mxid)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Komai.paddingSmall

                            Label {
                                Layout.fillWidth: true
                                text: model.displayName || qsTr("Unknown display name")
                                color: model.displayName ? palette.text : palette.buttonText
                                font.pointSize: Settings.uiFontSizePt
                                font.italic: !model.displayName
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: model.mxid
                                color: palette.buttonText
                                font.pointSize: Settings.uiFontSizePt * 0.9
                                elide: Text.ElideRight
                            }
                        }

                        KomaiButton {
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Remove")
                            icon.source: "qrc:/icons/icons/ui/delete.svg"
                            onClicked: inviteDialogRoot.invitees.removeUser(model.mxid)
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: parent.width - Komai.paddingLarge * 2
                    visible: selectedInvitees.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Choose one or more users to invite.")
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt * 0.95
                }
            }
        } // inviteScrollColumn
    } // inviteScrollArea

    KomaiButton {
        id: inviteButton

        Layout.alignment: Qt.AlignRight
        activeFocusOnTab: true
        focusPolicy: Qt.StrongFocus
        text: qsTr("Invite")
        highlighted: true
        enabled: invitees.count > 0
        onClicked: inviteDialogRoot.cleanUpAndClose()
        Keys.onEnterPressed: event => {
            inviteDialogRoot.cleanUpAndClose();
            event.accepted = true;
        }
        Keys.onReturnPressed: event => {
            inviteDialogRoot.cleanUpAndClose();
            event.accepted = true;
        }
    }
}
