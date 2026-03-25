// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import "../../components"
import "../"
import QtMultimedia
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: root

    required property var room
    required property string eventId
    required property int duration
    required property string body
    required property string filename
    required property string filesize
    required property string mimetype

    property bool autoPlayPending: false
    property real desiredVolume: 1.0
    property real lastNonZeroVolume: 1.0
    property bool muted: false
    property bool loadingIndicatorVisible: false
    property bool loadingIndicatorHidePending: false
    readonly property real configuredDefaultPlaybackRate: Settings.timelineMediaDefaultAudioPlaybackSpeed
    readonly property bool inlinePlaybackEnabled: !Settings.timelineMediaOpenAudioExternal
    readonly property bool loading: autoPlayPending
        || mediaPlayer.mediaStatus === MediaPlayer.LoadingMedia
        || mediaPlayer.mediaStatus === MediaPlayer.BufferingMedia
    readonly property bool playing: mediaPlayer.playbackState === MediaPlayer.PlayingState
    readonly property bool mediaReady: mediaPlayer.loaded
    readonly property var allPlaybackRates: [0.5, 1.0, 1.5, 2.0, 2.5, 3.0]
    readonly property real normalizedDefaultPlaybackRate: {
        let bestRate = allPlaybackRates[0];
        let bestDistance = Math.abs(bestRate - configuredDefaultPlaybackRate);
        for (let i = 1; i < allPlaybackRates.length; ++i) {
            const candidate = allPlaybackRates[i];
            const distance = Math.abs(candidate - configuredDefaultPlaybackRate);
            if (distance < bestDistance || (distance === bestDistance && candidate < bestRate)) {
                bestRate = candidate;
                bestDistance = distance;
            }
        }
        return bestRate;
    }
    readonly property var visiblePlaybackRates: root.computeVisiblePlaybackRates()

    color: palette.alternateBase
    radius: Komai.paddingMedium
    border.color: Qt.rgba(palette.buttonText.r, palette.buttonText.g, palette.buttonText.b, 0.25)
    border.width: 1
    implicitWidth: 500
    implicitHeight: contentLayout.implicitHeight + Komai.paddingMedium * 2
    activeFocusOnTab: false

    Keys.onPressed: event => {
        if (!root.inlinePlaybackEnabled || event.modifiers !== Qt.NoModifier)
            return;

        if (event.key === Qt.Key_Space) {
            event.accepted = true;
            root.togglePlayback();
            return;
        }

        if (event.key === Qt.Key_Left) {
            event.accepted = true;
            root.seekBy(-5000);
            return;
        }

        if (event.key === Qt.Key_Right) {
            event.accepted = true;
            root.seekBy(5000);
        }
    }

    function formatDuration(durationMs)
    {
        const safeDuration = Math.max(0, Math.floor(durationMs / 1000));
        const seconds = safeDuration % 60;
        const minutes = Math.floor(safeDuration / 60) % 60;
        const hours = Math.floor(safeDuration / 3600);

        function withLeadingZero(value) {
            return value < 10 ? "0" + value : value.toString();
        }

        if (hours > 0)
            return hours.toString() + ":" + withLeadingZero(minutes) + ":" + withLeadingZero(seconds);

        return minutes.toString() + ":" + withLeadingZero(seconds);
    }

    function rateText(rate)
    {
        return (Math.round(rate) === rate ? rate.toFixed(0) : rate.toFixed(1)) + "x";
    }

    function computeVisiblePlaybackRates()
    {
        const defaultRate = normalizedDefaultPlaybackRate;
        const defaultIndex = allPlaybackRates.indexOf(defaultRate);
        const maxStart = allPlaybackRates.length - 4;
        const start = Math.max(0, Math.min(Math.floor(Math.max(0, defaultIndex) / 2), maxStart));
        const visibleRates = allPlaybackRates.slice(start, start + 4);

        if (visibleRates.indexOf(1.0) !== -1)
            return visibleRates;

        visibleRates.push(1.0);
        visibleRates.sort((left, right) => left - right);

        const pinnedRates = [1.0, defaultRate];
        while (visibleRates.length > 4) {
            let dropIndex = -1;
            let dropDistance = -1;

            for (let i = 0; i < visibleRates.length; ++i) {
                const rate = visibleRates[i];
                if (pinnedRates.indexOf(rate) !== -1)
                    continue;

                const distance = Math.abs(rate - defaultRate);
                if (distance > dropDistance
                    || (distance === dropDistance && dropIndex >= 0 && rate < visibleRates[dropIndex])) {
                    dropIndex = i;
                    dropDistance = distance;
                }
            }

            if (dropIndex < 0)
                break;

            visibleRates.splice(dropIndex, 1);
        }

        return visibleRates;
    }

    function compactFilename(name)
    {
        const maxLength = 40;
        if (!name || name.length <= maxLength)
            return name;

        const lastDot = name.lastIndexOf(".");
        const hasExtension = lastDot > 0 && lastDot < (name.length - 1) && (name.length - lastDot) <= 8;
        if (!hasExtension)
            return name.slice(0, maxLength - 3) + "...";

        const extension = name.slice(lastDot);
        const stemLength = Math.max(1, maxLength - extension.length - 3);
        return name.slice(0, stemLength) + "..." + extension;
    }

    function displayLabel()
    {
        const safeFilename = compactFilename(filename);
        if (body.length > 0 && safeFilename.length > 0 && body !== filename)
            return body + " (" + safeFilename + ")";

        return safeFilename.length > 0 ? safeFilename : body;
    }

    function updatePlaybackRate(rate)
    {
        focusPlayer();
        mediaPlayer.playbackRate = rate;
    }

    function applyConfiguredPlaybackRate()
    {
        mediaPlayer.playbackRate = normalizedDefaultPlaybackRate;
    }

    function focusPlayer()
    {
        if (inlinePlaybackEnabled)
            root.forceActiveFocus(Qt.MouseFocusReason);
    }

    function seekBy(deltaMs)
    {
        if (!inlinePlaybackEnabled || !mediaReady)
            return;

        const maxDuration = Math.max(root.duration, mediaPlayer.duration, mediaPlayer.position, 0);
        mediaPlayer.position = Math.max(0, Math.min(maxDuration, mediaPlayer.position + deltaMs));
    }

    function toggleMuted()
    {
        focusPlayer();
        if (muted || desiredVolume <= 0) {
            const restoredVolume = lastNonZeroVolume > 0 ? lastNonZeroVolume : 1.0;
            desiredVolume = restoredVolume;
            muted = false;
            return;
        }

        lastNonZeroVolume = desiredVolume;
        desiredVolume = 0;
        muted = true;
    }

    function togglePlayback()
    {
        if (!inlinePlaybackEnabled) {
            room.openMedia(eventId);
            return;
        }

        focusPlayer();
        if (mediaReady) {
            if (playing) {
                autoPlayPending = false;
                mediaPlayer.pause();
                InlineAudioPlaybackRegistry.clearIfCurrent(root);
            } else {
                if (mediaPlayer.duration > 0
                    && mediaPlayer.position >= Math.max(0, mediaPlayer.duration - 250)) {
                    mediaPlayer.position = 0;
                }
                InlineAudioPlaybackRegistry.activate(root);
                mediaPlayer.ensureAudioReady();
                mediaPlayer.play();
            }
            return;
        }

        InlineAudioPlaybackRegistry.activate(root);
        autoPlayPending = true;
        mediaPlayer.ensureAudioReady();
        mediaPlayer.startDownload();
    }

    function deactivateForAnotherPlayback()
    {
        autoPlayPending = false;
        if (mediaPlayer.playbackState === MediaPlayer.PlayingState)
            mediaPlayer.pause();
    }

    Component.onCompleted: root.applyConfiguredPlaybackRate()
    Component.onDestruction: InlineAudioPlaybackRegistry.clearIfCurrent(root)
    onConfiguredDefaultPlaybackRateChanged: root.applyConfiguredPlaybackRate()
    onDesiredVolumeChanged: {
        if (desiredVolume > 0)
            lastNonZeroVolume = desiredVolume;
    }
    onLoadingChanged: {
        if (loading) {
            loadingIndicatorHidePending = false;
            if (loadingIndicatorVisible)
                loadingIndicatorMinimumTimer.restart();
            else
                loadingIndicatorDelayTimer.restart();
        } else {
            loadingIndicatorDelayTimer.stop();
            if (!loadingIndicatorVisible) {
                loadingIndicatorHidePending = false;
                loadingIndicatorMinimumTimer.stop();
            } else if (loadingIndicatorMinimumTimer.running) {
                loadingIndicatorHidePending = true;
            } else {
                loadingIndicatorVisible = false;
            }
        }
    }

    Timer {
        id: loadingIndicatorDelayTimer

        interval: 180
        repeat: false
        onTriggered: {
            if (!root.loading)
                return;
            root.loadingIndicatorVisible = true;
            root.loadingIndicatorHidePending = false;
            loadingIndicatorMinimumTimer.restart();
        }
    }

    Timer {
        id: loadingIndicatorMinimumTimer

        interval: 220
        repeat: false
        onTriggered: {
            if (!root.loading && root.loadingIndicatorHidePending) {
                root.loadingIndicatorVisible = false;
                root.loadingIndicatorHidePending = false;
            }
        }
    }

    MxcMedia {
        id: mediaPlayer

        roomm: root.room
        eventId: root.eventId
        mimeTypeHint: root.mimetype
        muted: root.muted
        volume: root.desiredVolume

        onMediaStatusChanged: {
            if (!root.autoPlayPending)
                return;

            if (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia) {
                root.autoPlayPending = false;
                mediaPlayer.ensureAudioReady();
                mediaPlayer.play();
            }
        }
        onPlaybackStateChanged: {
            if (playbackState === MediaPlayer.PlayingState) {
                root.autoPlayPending = false;
                InlineAudioPlaybackRegistry.activate(root);
            } else if (!root.autoPlayPending) {
                InlineAudioPlaybackRegistry.clearIfCurrent(root);
            }
        }
        onErrorOccurred: {
            if (mediaPlayer.recoveringFromStreamingFallback)
                return;
            root.autoPlayPending = false;
            InlineAudioPlaybackRegistry.clearIfCurrent(root);
        }
    }

    ColumnLayout {
        id: contentLayout

        anchors.fill: parent
        anchors.margins: Komai.paddingMedium
        spacing: Komai.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: Komai.paddingMedium

            Item {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                implicitHeight: headerContent.implicitHeight

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.togglePlayback()
                }

                RowLayout {
                    id: headerContent

                    anchors.fill: parent
                    spacing: Komai.paddingMedium

                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        radius: width / 2
                        color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.14)

                        Spinner {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            running: root.loading
                            visible: root.loadingIndicatorVisible
                        }

                        Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            visible: !root.loadingIndicatorVisible
                            source: "image://colorimage/:/icons/icons/ui/music.svg?" + root.palette.highlight
                            sourceSize.width: width * Screen.devicePixelRatio
                            sourceSize.height: height * Screen.devicePixelRatio
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: root.displayLabel()
                            color: palette.text
                            elide: Text.ElideMiddle
                            maximumLineCount: 1
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Komai.paddingSmall

                            Label {
                                text: root.filesize
                                color: palette.buttonText
                            }

                            Label {
                                visible: !root.inlinePlaybackEnabled
                                text: qsTr("External player")
                                color: palette.buttonText
                            }

                        }
                    }
                }
            }

            RowLayout {
                spacing: Komai.paddingSmall
                opacity: root.inlinePlaybackEnabled ? 1.0 : 0.55

                Repeater {
                    model: root.visiblePlaybackRates

                    delegate: Rectangle {
                        required property real modelData
                        readonly property bool selected: Math.abs(mediaPlayer.playbackRate - modelData) < 0.001
                        readonly property bool hovered: hoverHandler.hovered

                        Layout.preferredWidth: speedText.implicitWidth + Komai.paddingMedium * 2
                        Layout.preferredHeight: speedText.implicitHeight + Komai.paddingSmall * 2
                        radius: Komai.paddingSmall
                        color: selected
                            ? (hovered
                                ? Qt.rgba((root.palette.dark.r * 0.78) + (root.palette.highlight.r * 0.22),
                                          (root.palette.dark.g * 0.78) + (root.palette.highlight.g * 0.22),
                                          (root.palette.dark.b * 0.78) + (root.palette.highlight.b * 0.22),
                                          1)
                                : Qt.rgba(root.palette.highlight.r, root.palette.highlight.g,
                                          root.palette.highlight.b, 0.16))
                            : (hovered
                                ? root.palette.dark
                                : Qt.rgba(root.palette.base.r, root.palette.base.g,
                                          root.palette.base.b, 0.9))
                        border.color: selected
                            ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g,
                                      root.palette.highlight.b, hovered ? 0.72 : 0.5)
                            : (hovered
                                ? Qt.rgba(root.palette.brightText.r, root.palette.brightText.g,
                                          root.palette.brightText.b, 0.22)
                                : Qt.rgba(root.palette.buttonText.r, root.palette.buttonText.g,
                                          root.palette.buttonText.b, 0.3))
                        border.width: 1

                        HoverHandler {
                            id: hoverHandler

                            enabled: root.inlinePlaybackEnabled
                        }

                        KomaiCursorShape {
                            anchors.fill: parent
                            cursorShape: root.inlinePlaybackEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }

                        Text {
                            id: speedText

                            anchors.centerIn: parent
                            text: root.rateText(modelData)
                            color: parent.selected
                                ? (parent.hovered ? root.palette.brightText : root.palette.highlight)
                                : (parent.hovered ? root.palette.brightText : root.palette.text)
                            font.pointSize: Math.max(9, Qt.application.font.pointSize * 0.9)
                        }

                        TapHandler {
                            enabled: root.inlinePlaybackEnabled
                            onTapped: root.updatePlaybackRate(parent.modelData)
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Komai.paddingSmall

            Rectangle {
                id: playButtonBackground

                readonly property bool hovered: playButtonHover.hovered
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: width / 2
                color: !root.inlinePlaybackEnabled
                    ? Qt.rgba(palette.base.r, palette.base.g, palette.base.b, 0.9)
                    : (root.playing
                        ? Qt.darker(palette.highlight, 1.06)
                        : palette.highlight)
                border.color: root.inlinePlaybackEnabled
                    ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.85)
                    : Qt.rgba(palette.buttonText.r, palette.buttonText.g, palette.buttonText.b, 0.3)
                border.width: 1
                scale: hovered && root.inlinePlaybackEnabled ? 1.05 : 1.0

                Behavior on scale {
                    NumberAnimation {
                        duration: 150
                    }
                }

                HoverHandler {
                    id: playButtonHover

                    enabled: root.inlinePlaybackEnabled
                }

                ImageButton {
                    anchors.fill: parent
                    leftPadding: Komai.paddingSmall
                    rightPadding: Komai.paddingSmall
                    topPadding: Komai.paddingSmall
                    bottomPadding: Komai.paddingSmall
                    buttonTextColor: root.inlinePlaybackEnabled ? palette.highlightedText : palette.text
                    changeColorOnHover: true
                    highlightColor: root.inlinePlaybackEnabled ? palette.highlightedText : palette.highlight
                    image: !root.inlinePlaybackEnabled
                        ? ":/icons/icons/ui/open-externally.svg"
                        : (root.playing
                            ? ":/icons/icons/ui/pause-symbol.svg"
                            : ":/icons/icons/ui/play-sign.svg")
                    onClicked: root.togglePlayback()
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: root.loading
                    visible: running
                    width: 20
                    height: 20
                }
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: root.formatDuration(mediaPlayer.position)
                color: palette.buttonText
            }

            KomaiSlider {
                Layout.fillWidth: true
                Layout.minimumWidth: 120
                Layout.alignment: Qt.AlignVCenter
                enabled: root.inlinePlaybackEnabled && root.mediaReady
                focusPolicy: Qt.NoFocus
                value: mediaPlayer.position
                sliderRadius: 14
                onMoved: {
                    root.focusPlayer();
                    mediaPlayer.position = value;
                }
                from: 0
                to: Math.max(root.duration, mediaPlayer.duration, 1)
                alwaysShowSlider: true
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: root.formatDuration(Math.max(root.duration, mediaPlayer.duration))
                color: palette.buttonText
            }

            ImageButton {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                enabled: root.inlinePlaybackEnabled
                opacity: enabled ? 1.0 : 0.55
                buttonTextColor: palette.text
                image: (root.muted || root.desiredVolume <= 0)
                    ? ":/icons/icons/ui/volume-off-indicator.svg"
                    : ":/icons/icons/ui/volume-up.svg"
                onClicked: root.toggleMuted()
            }

            KomaiSlider {
                Layout.preferredWidth: 84
                Layout.alignment: Qt.AlignVCenter
                enabled: root.inlinePlaybackEnabled
                focusPolicy: Qt.NoFocus
                sliderRadius: 12
                opacity: enabled ? 1.0 : 0.55
                value: root.desiredVolume
                from: 0
                to: 1
                alwaysShowSlider: true
                onMoved: {
                    root.focusPlayer();
                    root.desiredVolume = value;
                    root.muted = !(value > 0);
                }
            }
        }
    }
}
