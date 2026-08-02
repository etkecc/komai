// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../emoji"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai 1.0

Rectangle {
    id: inputBar

    required property var room
    required property var timelineRoot
    required property var selectionModeRoot
    property var inputController: room && room.input ? room.input : null
    property bool allowCalls: true
    property bool allowStickers: true
    property bool allowCommandCompleter: true
    property bool attachmentsEnabled: true
    property bool showAllButtons: width > 450 || (messageInput.length == 0 && !messageInput.inputMethodComposing)
    property bool walkModeActive: false
    property string _draftRoomId: ""
    property string _draftText: ""
    onInputControllerChanged: {
        // Save draft for the room we are leaving, using the last captured text
        if (_draftSaveTimer.running)
            _draftSaveTimer.stop();
        if (_draftRoomId)
            Rooms.persistDraftForRoom(_draftRoomId, _draftText);
        _draftRoomId = "";
        _draftText = "";
        // Cancel any in-flight transcription gesture; the audio + result
        // belong to the previous room.
        if (transcriptionState !== "idle")
            _cancelTranscriptionGesture();
    }
    readonly property string text: messageInput.text
    readonly property bool textInputActiveFocus: messageInput.activeFocus
    readonly property bool hasUploads: !!(inputController && inputController.uploads && inputController.uploads.length > 0)
    readonly property bool hasVoiceRecording: VoiceRecorder.recording || VoiceRecorder.paused || VoiceRecorder.hasRecording
    onHasVoiceRecordingChanged: {
        if (hasVoiceRecording && voiceButton.visible)
            voiceButton.forceActiveFocus(Qt.OtherFocusReason);
    }
    onHasUploadsChanged: {
        // If the staged attachment was removed via UploadBox, also discard the voice recording
        if (!hasUploads && hasVoiceRecording)
            VoiceRecorder.discardRecording();
    }
    readonly property bool composerEnabled: !hasUploads && !hasVoiceRecording
    readonly property bool hasSendableContent: messageInput.length > 0 || hasUploads
    readonly property bool _rtlLayout: inputBar.LayoutMirroring.enabled
        || Qt.application.layoutDirection === Qt.RightToLeft
        || inputBar._languageCodeIsRtl(Settings.uiLanguage)

    // Thread-view tinting: when a thread is open, both the bar background and
    // the send-button active state pick up the thread's user color so the
    // composer reads as part of the thread surface.
    readonly property string _threadEventId: TimelineManager.matrixTimelineThreadEventId
    readonly property bool _threadActive: _threadEventId.length > 0
    readonly property color _threadTintColor: _threadActive
        ? TimelineManager.userColor(_threadEventId, palette.base)
        : palette.buttonText
    readonly property color _threadBackgroundTint: Qt.tint(palette.window,
        Qt.hsla(_threadTintColor.hslHue, 0.7,
                _threadTintColor.hslLightness, 0.1))

    // Transcription gesture state. Two trigger surfaces share this state
    // machine (Space long-press in the textarea, and the composer
    // microphone button). `transcriptionTriggerKind` records which surface
    // started the current gesture — it determines what release/click
    // counts as "stop and dispatch" and what banner copy to show.
    //  States:
    //  - "idle"         : nothing happening
    //  - "armed"        : trigger pressed, waiting for the long-press threshold
    //  - "recording"    : threshold passed (or click-toggle entered), capturing audio
    //  - "transcribing" : audio submitted, waiting for the API result
    //  - "error"        : last attempt failed (banner shows the message)
    //  Trigger kinds:
    //  - "space"         : long-press Space in the textarea; release commits
    //  - "button-hold"   : button mouse-held; release commits
    //  - "button-toggle" : button clicked (no hold); next click commits
    property string transcriptionState: "idle"
    property string transcriptionTriggerKind: ""
    // Effective provider for the in-flight gesture (resolved from
    // Transcription.resolveForRoom at gesture start so per-room overrides
    // apply). Drives banner copy: "Recording…" for batch (transcription
    // happens at release) vs "Recording & transcribing…" for realtime
    // (transcription happens live). Empty when state is idle.
    property string transcriptionEffectiveProvider: ""
    property string transcriptionLastError: ""
    property int _transcriptionJobId: 0
    // Realtime job id from `Transcription.runRealtimeAsync`. Distinct
    // type (qint64 in C++) but harmless to store as a JS number here as
    // long as we only compare/pass through. 0 means "no realtime session".
    property var _transcriptionRealtimeJobId: 0
    // Tentative range bookkeeping for realtime mode. While the user is
    // speaking, deltas land in `[tentativeStart, tentativeStart + tentativeLength)`
    // styled muted/italic. On each `completed`, the tentative range is
    // replaced with the polished utterance (in normal style) and
    // `tentativeStart` advances past it so subsequent deltas land
    // after — see `_consolidateTranscriptionTentative`. -1 / 0 when no
    // tentative range is alive.
    property int _transcriptionTentativeStart: -1
    property int _transcriptionTentativeLength: 0
    // Position where ALL realtime-inserted text begins (origin of the
    // current session). Unlike `tentativeStart` this DOES NOT advance
    // when an utterance is consolidated — it pins the start of the
    // session's contribution, so Esc/cancel can wipe everything that
    // was dictated in one shot. -1 when no session is active.
    property int _transcriptionRealtimeOriginStart: -1
    // Accumulator for the current realtime turn's tentative text. We use
    // the accumulator (rather than reading the live editor contents) so
    // user typing inside the tentative range — should it happen — does
    // not corrupt our delta book-keeping.
    property string _transcriptionTentativeText: ""
    // Position where the provisional space landed when arming a Space
    // gesture. Captured at keydown so we can pull it back if the long-press
    // threshold elapses into recording, even if the cursor moved while
    // Space was held. -1 when no Space gesture is in flight.
    property int _transcriptionGestureSpacePos: -1
    readonly property bool _transcriptionIsRealtime:
        transcriptionEffectiveProvider === "openai_realtime"
    readonly property bool transcriptionGestureEligible:
        Settings.composerInputTranscriptionEnabled === true
        && composerEnabled
        && !popup.opened
        && !walkModeActive

    Timer {
        id: transcriptionLongPressTimer
        interval: 350
        repeat: false
        onTriggered: inputBar._beginTranscriptionRecording()
    }

    Connections {
        // Toggling the master toggle off mid-gesture must cleanly cancel —
        // no banner left over, no API call fired after the flip.
        target: Settings
        function onComposerInputTranscriptionEnabledChanged() {
            if (Settings.composerInputTranscriptionEnabled !== true
                && inputBar.transcriptionState !== "idle")
                inputBar._cancelTranscriptionGesture();
        }
    }

    Connections {
        target: Transcription
        function onBatchFinished(jobId, text) {
            if (jobId !== inputBar._transcriptionJobId)
                return;
            inputBar._transcriptionJobId = 0;
            inputBar.transcriptionState = "idle";
            inputBar.transcriptionTriggerKind = "";
            inputBar.transcriptionEffectiveProvider = "";
            inputBar.transcriptionLastError = "";
            TranscriptionAudioCapture.discardRecording();
            if (text && text.length > 0)
                inputBar._insertTranscribedText(text);
        }
        function onBatchFailed(jobId, errorCode, errorMessage) {
            if (jobId !== inputBar._transcriptionJobId)
                return;
            inputBar._transcriptionJobId = 0;
            inputBar.transcriptionState = "error";
            inputBar.transcriptionTriggerKind = "";
            inputBar.transcriptionEffectiveProvider = "";
            inputBar.transcriptionLastError = errorMessage && errorMessage.length > 0
                ? errorMessage
                : errorCode;
            TranscriptionAudioCapture.discardRecording();
        }

        function onRealtimeDelta(jobId, delta) {
            if (jobId !== inputBar._transcriptionRealtimeJobId)
                return;
            if (!delta || delta.length === 0)
                return;
            inputBar._appendTranscriptionTentative(delta);
        }
        function onRealtimeCompleted(jobId, transcript) {
            // Server VAD can fire `completed` mid-session (one per
            // detected utterance). Treat each as "consolidate the
            // current tentative range into final text and start a fresh
            // tentative range for any subsequent deltas". The session
            // itself ends on `realtimeClosed`, not here.
            if (jobId !== inputBar._transcriptionRealtimeJobId)
                return;
            inputBar._consolidateTranscriptionTentative(transcript || "");
        }
        function onRealtimeClosed(jobId) {
            if (jobId !== inputBar._transcriptionRealtimeJobId)
                return;
            inputBar._transcriptionRealtimeJobId = 0;
            // Anything still tentative at close time never got
            // consolidated by a `completed` (e.g., the session timed out
            // post-commit). Drop the styling — there's no polished form
            // coming for it — but keep the text in the buffer so the
            // user doesn't lose what they dictated.
            inputBar._demoteTentativeToFinal();
            inputBar._transcriptionRealtimeOriginStart = -1;
            if (TranscriptionAudioCapture.streaming)
                TranscriptionAudioCapture.stopStreaming();
            inputBar.transcriptionState = "idle";
            inputBar.transcriptionTriggerKind = "";
            inputBar.transcriptionEffectiveProvider = "";
            inputBar.transcriptionLastError = "";
        }
        function onRealtimeFailed(jobId, errorCode, errorMessage) {
            if (jobId !== 0 && jobId !== inputBar._transcriptionRealtimeJobId)
                return;
            // jobId === 0 means the session could not even be started
            // (e.g. config not ready). Surface as a regular failure.
            if (jobId !== 0)
                inputBar._transcriptionRealtimeJobId = 0;
            // Wipe everything realtime added — we have no polished form
            // to fall back to, and surfacing half-finalised italicised
            // bits next to an error banner would be confusing.
            inputBar._removeAllRealtimeText();
            // If the streaming source is still feeding the (now-dead)
            // session, stop it so we don't keep the mic hot.
            if (TranscriptionAudioCapture.streaming)
                TranscriptionAudioCapture.stopStreaming();
            inputBar.transcriptionState = "error";
            inputBar.transcriptionTriggerKind = "";
            inputBar.transcriptionEffectiveProvider = "";
            inputBar.transcriptionLastError = errorMessage && errorMessage.length > 0
                ? errorMessage
                : errorCode;
        }
    }

    Connections {
        target: TranscriptionAudioCapture
        function onRecordingFinished(filePath) {
            // Only react when we are mid-flight from the gesture (i.e. the
            // user released Space and we asked the recorder to stop). Other
            // callers of stopRecording should set state themselves.
            if (inputBar.transcriptionState !== "transcribing")
                return;
            if (!filePath || !inputBar.room) {
                inputBar._abortTranscriptionInFlight();
                return;
            }
            inputBar._transcriptionJobId =
                Transcription.runBatchAsync(inputBar.room.roomId, filePath);
        }
        function onPcmChunkReady(bytes) {
            if (inputBar._transcriptionRealtimeJobId === 0)
                return;
            Transcription.pushRealtimeAudio(inputBar._transcriptionRealtimeJobId, bytes);
        }
        function onErrorOccurred(message) {
            if (inputBar.transcriptionState === "idle")
                return;
            inputBar._abortTranscriptionInFlight();
            inputBar.transcriptionState = "error";
            inputBar.transcriptionLastError = message;
        }
    }
    readonly property int minimumBarHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property bool composerExpanded: textInput.effectiveTextAreaHeight > textInput.singleLineHeight

    // ── Per-room input-area resize state ───────────────────────────────
    // The composer is a single instance reused across tabs; `room` rebinds
    // when the user switches rooms. A drag-resize of the input area should:
    //   - survive switching tabs and coming back;
    //   - not bleed across rooms.
    // Modelled as a roomId → desired textarea height (px) map. 0 / no entry
    // means "no user preference": the textarea sizes to its content exactly
    // as before. The desired height acts as a floor — content taller than it
    // still grows (and scrolls) the same way it does today. No persistence
    // to disk — app exit drops the map.
    property var _inputResizeByRoom: ({})
    readonly property string _inputResizeRoomKey: room ? String(room.roomId || "") : ""
    readonly property real userDesiredInputHeight: _inputResizeRoomKey.length > 0
        ? (_inputResizeByRoom[_inputResizeRoomKey] || 0)
        : 0
    readonly property real maxUserInputHeight: Math.max(0,
        (inputBar.Window.window
            ? inputBar.Window.window.height * 0.5
            : 400))

    function _setUserDesiredInputHeight(value) {
        if (_inputResizeRoomKey.length === 0)
            return;
        const clamped = Math.max(0, Math.min(maxUserInputHeight, value));
        const newMap = Object.assign({}, _inputResizeByRoom);
        newMap[_inputResizeRoomKey] = clamped;
        _inputResizeByRoom = newMap;
    }
    readonly property bool commandPickerVisible: popup.opened && completer.completerType === "command"
    readonly property int permissionsRevision: room && room.permissions ? room.permissions.revision : 0
    readonly property bool canSendCurrentRoom: {
        const _ = permissionsRevision;
        return room ? room.permissions.canSend(room.isEncrypted ? MtxEvent.Encrypted : MtxEvent.TextMessage) : false;
    }
    readonly property bool canSendTextMessages: {
        const _ = permissionsRevision;
        return room ? room.permissions.canSend(MtxEvent.TextMessage) : false;
    }
    signal composerInteractionRequested()

    function focusTextInput() {
        if (walkModeActive) {
            composerInteractionRequested();
            return false;
        }

        messageInput.forceActiveFocus();
        return !!messageInput.activeFocus;
    }

    function focusTextInputIfAllowed() {
        if (walkModeActive)
            return false;

        messageInput.forceActiveFocus();
        return !!messageInput.activeFocus;
    }

    function appendText(text) {
        if (!text)
            return false;

        messageInput.forceActiveFocus();
        messageInput.insert(messageInput.cursorPosition, text);
        return true;
    }

    function replaceText(text) {
        const value = String(text || "");
        messageInput.text = value;
        messageInput.cursorPosition = value.length;
        return true;
    }

    // Transcription gesture helpers ---------------------------------------
    // Each entry point arms the gesture with a `triggerKind` that the rest
    // of the state machine threads through:
    //  - "space" / "button-hold": the long-press timer arms recording;
    //    short-release hands control back ("space" types one literal space,
    //    "button-hold" promotes to "button-toggle" and starts recording).
    //  - "button-toggle": skip the long-press wait; recording starts
    //    immediately and stays running until the user clicks again.
    //
    // A pre-flight readiness check (api_url + api_key present where required)
    // gates every entry point: when the user attempts the gesture but the
    // Integrations side is not configured, we surface the not-configured
    // banner with an "Open Settings" link instead of recording.
    function _transcriptionResolved() {
        if (!room || !room.roomId)
            return null;
        return Transcription.resolveForRoom(room.roomId);
    }
    function _languageCodeIsRtl(code) {
        const primaryCode = String(code || "").split(/[_-]/)[0];
        return primaryCode === "ar"
            || primaryCode === "fa"
            || primaryCode === "he"
            || primaryCode === "ur";
    }
    function _textStartsRtl(text) {
        const value = String(text || "");
        for (let i = 0; i < value.length; ++i) {
            const c = value.charCodeAt(i);
            if ((c >= 0x0590 && c <= 0x08ff) || (c >= 0xfb1d && c <= 0xfdff) || (c >= 0xfe70 && c <= 0xfeff))
                return true;
            if ((c >= 0x0041 && c <= 0x005a) || (c >= 0x0061 && c <= 0x007a))
                return false;
        }
        return false;
    }

    function _armTranscriptionGesture(triggerKind) {
        // Readiness is *not* checked here. A short tap must look indistinguishable
        // from typing a space, which means we can't surface the not-configured
        // banner until the long-press threshold actually elapses. Checking up
        // front would flash the banner on every plain Space press.
        if (transcriptionState !== "idle" && transcriptionState !== "error")
            return;
        transcriptionLastError = "";
        transcriptionTriggerKind = triggerKind;
        transcriptionEffectiveProvider = "";
        transcriptionState = "armed";
        transcriptionLongPressTimer.restart();
    }

    function _beginTranscriptionRecording() {
        if (transcriptionState !== "armed")
            return;
        var resolved = _transcriptionResolved();
        if (!resolved || !resolved.isReady) {
            transcriptionEffectiveProvider = "";
            transcriptionState = "not-configured";
            // Provisional space stays — user effectively typed it and
            // we're showing a banner, not recording.
            _transcriptionGestureSpacePos = -1;
            return;
        }
        transcriptionEffectiveProvider = resolved.provider || "";
        transcriptionState = "recording";
        // Long-press confirmed — pull back the provisional space that Qt
        // inserted at keydown. Defensive: only remove if it's still a
        // space at the captured position (text could have been cleared
        // by Send, mutated by Backspace, etc.).
        if (transcriptionTriggerKind === "space"
            && _transcriptionGestureSpacePos >= 0
            && _transcriptionGestureSpacePos < messageInput.length
            && messageInput.getText(_transcriptionGestureSpacePos,
                                    _transcriptionGestureSpacePos + 1) === " ") {
            messageInput.remove(_transcriptionGestureSpacePos,
                                _transcriptionGestureSpacePos + 1);
        }
        _transcriptionGestureSpacePos = -1;
        if (_transcriptionIsRealtime)
            _startRealtimeCapture();
        else
            TranscriptionAudioCapture.startRecording();
    }

    function _beginTranscriptionInToggleMode() {
        // Called when the button is clicked (released before the hold
        // threshold). Cancels the armed timer and starts recording right
        // away in click-to-stop mode.
        if (transcriptionState !== "idle"
            && transcriptionState !== "error"
            && transcriptionState !== "armed") {
            return;
        }
        transcriptionLongPressTimer.stop();
        transcriptionLastError = "";
        transcriptionTriggerKind = "button-toggle";
        var resolved = _transcriptionResolved();
        if (!resolved || !resolved.isReady) {
            transcriptionEffectiveProvider = "";
            transcriptionState = "not-configured";
            return;
        }
        transcriptionEffectiveProvider = resolved.provider || "";
        transcriptionState = "recording";
        if (_transcriptionIsRealtime)
            _startRealtimeCapture();
        else
            TranscriptionAudioCapture.startRecording();
    }

    function _startRealtimeCapture() {
        // Realtime path: open the WebSocket session first (synchronous —
        // returns 0 on a config-readiness failure that we already
        // pre-checked, so this should normally succeed) and only flip on
        // the streaming mic source if the session was accepted. The
        // tentative range is anchored at the current cursor position so
        // deltas land in-place even if the user pre-typed content.
        if (!room || !room.roomId) {
            _abortTranscriptionInFlight();
            return;
        }
        var jobId = Transcription.runRealtimeAsync(room.roomId);
        if (!jobId || jobId === 0) {
            // `realtimeFailed(0, …)` was emitted synchronously and has
            // already moved us to the `error` state; nothing more to do.
            return;
        }
        _transcriptionRealtimeJobId = jobId;
        _transcriptionRealtimeOriginStart = messageInput.cursorPosition;
        _transcriptionTentativeStart = messageInput.cursorPosition;
        _transcriptionTentativeLength = 0;
        _transcriptionTentativeText = "";
        TranscriptionAudioCapture.startStreaming();
    }

    function _commitTranscriptionGesture() {
        // Generic release/commit. Behavior depends on trigger kind:
        //  - "space" + armed: short tap. Qt already inserted the space at
        //    keydown; nothing to do here besides clearing state.
        //  - "button-hold" + armed: short tap, promote to toggle mode and
        //    start recording (caller decides whether to do this).
        //  - any + not-configured: dismiss the hint. For "space" the user
        //    effectively typed a space and Qt already inserted it.
        //  - any + recording (batch): stop recorder, wait for file flush.
        //  - any + recording (realtime): stop the streaming mic, send
        //    `commit` to the server, wait for the polished `completed`.
        if (transcriptionState === "armed" || transcriptionState === "not-configured") {
            transcriptionLongPressTimer.stop();
            transcriptionState = "idle";
            transcriptionTriggerKind = "";
            transcriptionEffectiveProvider = "";
            _transcriptionGestureSpacePos = -1;
            return;
        }
        if (transcriptionState === "recording") {
            if (_transcriptionIsRealtime) {
                transcriptionState = "transcribing";
                if (TranscriptionAudioCapture.streaming)
                    TranscriptionAudioCapture.stopStreaming();
                if (_transcriptionRealtimeJobId !== 0)
                    Transcription.commitRealtime(_transcriptionRealtimeJobId);
                return;
            }
            transcriptionState = "transcribing";
            TranscriptionAudioCapture.stopRecording();
        }
    }

    function _cancelTranscriptionGesture() {
        if (transcriptionState === "idle")
            return;
        transcriptionLongPressTimer.stop();
        if (_transcriptionRealtimeJobId !== 0) {
            Transcription.cancelRealtime(_transcriptionRealtimeJobId);
            _transcriptionRealtimeJobId = 0;
        }
        if (TranscriptionAudioCapture.streaming)
            TranscriptionAudioCapture.stopStreaming();
        if (transcriptionState === "recording" || transcriptionState === "transcribing")
            TranscriptionAudioCapture.discardRecording();
        _removeAllRealtimeText();
        _transcriptionJobId = 0;
        transcriptionState = "idle";
        transcriptionTriggerKind = "";
        transcriptionEffectiveProvider = "";
        transcriptionLastError = "";
        // Cancellation kills the gesture, not the user's typing — leave
        // the provisional space (if any) in the buffer.
        _transcriptionGestureSpacePos = -1;
    }

    function _abortTranscriptionInFlight() {
        if (_transcriptionRealtimeJobId !== 0) {
            Transcription.cancelRealtime(_transcriptionRealtimeJobId);
            _transcriptionRealtimeJobId = 0;
        }
        if (TranscriptionAudioCapture.streaming)
            TranscriptionAudioCapture.stopStreaming();
        _removeAllRealtimeText();
        _transcriptionJobId = 0;
        transcriptionState = "idle";
        transcriptionTriggerKind = "";
        transcriptionEffectiveProvider = "";
        _transcriptionGestureSpacePos = -1;
    }

    function dismissTranscriptionError() {
        if (transcriptionState === "error") {
            transcriptionState = "idle";
            transcriptionTriggerKind = "";
            transcriptionEffectiveProvider = "";
            transcriptionLastError = "";
        }
    }

    function dismissTranscriptionNotConfigured() {
        if (transcriptionState === "not-configured") {
            transcriptionState = "idle";
            transcriptionTriggerKind = "";
            transcriptionEffectiveProvider = "";
        }
    }

    function openTranscriptionSettings() {
        // Same call path the SettingsContent in-app dispatcher uses for
        // `komai://settings/integrations/transcription`. Dismiss the hint
        // first so it does not linger in the composer after navigation.
        dismissTranscriptionNotConfigured();
        MainWindow.showUserSettingsPage(UserSettingsModel.TabIntegrations,
                                        "transcription");
    }

    // Inject a transcription result at the cursor.
    //
    // Auto-prepends a space when joining a fresh transcript onto existing
    // non-whitespace content so consecutive dictations and mixed
    // type-and-dictate flows aren't mashed together. Skipped when the
    // preceding character is in a CJK code-point range — Japanese,
    // Chinese, Korean don't typically separate tokens with spaces, so an
    // automatic space there would just be wrong. The transcribed text
    // itself is left otherwise untouched.
    function _insertTranscribedText(text) {
        if (!text)
            return;
        messageInput.forceActiveFocus();
        const pos = messageInput.cursorPosition;
        let prefix = "";
        if (pos > 0 && !/^\s/.test(text)) {
            const prevChar = messageInput.getText(pos - 1, pos);
            if (prevChar.length > 0
                && !/^\s$/.test(prevChar)
                && !_isCJKCharacter(prevChar)) {
                prefix = " ";
            }
        }
        messageInput.insert(pos, prefix + text);
    }

    // Realtime "tentative range" book-keeping ----------------------------
    //
    // While a realtime session is live, deltas accumulate inside a range
    // `[tentativeStart, tentativeStart + tentativeLength)`. On `completed`,
    // the range is replaced by the polished transcript using the same
    // auto-space-when-joining heuristic batch mode uses. On Esc / cancel
    // the range is removed entirely.
    //
    // (Inline italic/muted styling for the tentative range would require
    // switching messageInput to RichText mode, which conflicts with
    // markdown handling. The composer banner — "Recording & transcribing…"
    // — already signals to the user that the in-place text is provisional.
    // Inline styling can land as a follow-up if desired.)

    function _appendTranscriptionTentative(delta) {
        if (!delta || delta.length === 0)
            return;
        if (_transcriptionTentativeStart < 0)
            return;
        var insertPos = _transcriptionTentativeStart + _transcriptionTentativeLength;
        if (insertPos < 0)
            insertPos = 0;
        if (insertPos > messageInput.length)
            insertPos = messageInput.length;
        // Auto-prepend a space the first time we land tentative text into
        // a non-whitespace context, mirroring `_insertTranscribedText`'s
        // batch heuristic so realtime and batch insertion feel the same.
        var prefix = "";
        if (_transcriptionTentativeLength === 0
            && insertPos > 0
            && !/^\s/.test(delta)) {
            var prevChar = messageInput.getText(insertPos - 1, insertPos);
            if (prevChar.length > 0
                && !/^\s$/.test(prevChar)
                && !_isCJKCharacter(prevChar)) {
                prefix = " ";
            }
        }
        var combined = prefix + delta;
        messageInput.insert(insertPos, combined);
        // Pull any auto-space *into* the tentative range so a cancel
        // takes it back out cleanly.
        _transcriptionTentativeLength += combined.length;
        _transcriptionTentativeText += combined;
        // Mark the whole tentative range italic + muted via the underlying
        // QTextDocument. Re-applied on every delta because Qt's TextEdit
        // can fan out the format at the boundary; mergeCharFormat is
        // cheap and idempotent.
        Transcription.applyTentativeFormat(messageInput.textDocument,
                                           _transcriptionTentativeStart,
                                           _transcriptionTentativeLength);
    }

    // Mid-session consolidation: replace the current tentative range
    // with the polished `finalText` (in normal style), then anchor a
    // fresh empty tentative range right after it for the next utterance.
    // Used when server VAD emits a `completed` while the user is still
    // holding Space — multiple utterances accumulate cleanly across the
    // same gesture.
    function _consolidateTranscriptionTentative(finalText) {
        if (_transcriptionTentativeStart < 0) {
            // No active tentative range — drop the polished text in at
            // the current cursor position (same auto-space heuristic as
            // batch). This is the path for "completed arrived without
            // preceding deltas", e.g. extremely short utterances.
            if (finalText && finalText.length > 0) {
                var cursorBefore = messageInput.cursorPosition;
                _insertTranscribedText(finalText);
                _transcriptionTentativeStart = messageInput.cursorPosition;
                _transcriptionTentativeLength = 0;
                _transcriptionTentativeText = "";
                // cursorBefore unused; _insertTranscribedText already
                // moved the cursor to the end of the inserted text,
                // which is where new deltas should land.
                void cursorBefore;
            }
            return;
        }
        var start = _transcriptionTentativeStart;
        var end = Math.min(messageInput.length, start + _transcriptionTentativeLength);
        if (end > start)
            messageInput.remove(start, end);
        _transcriptionTentativeLength = 0;
        _transcriptionTentativeText = "";
        // Insert polished text at the previous tentative-start position.
        // Apply the same auto-space-when-joining heuristic batch mode
        // uses: VAD-split utterances arrive WITHOUT leading whitespace
        // and the previous utterance ends in punctuation, so without
        // this you get "pizza.And just" mashed together.
        if (finalText && finalText.length > 0) {
            var prefix = "";
            if (start > 0 && !/^\s/.test(finalText)) {
                var prevChar = messageInput.getText(start - 1, start);
                if (prevChar.length > 0
                    && !/^\s$/.test(prevChar)
                    && !_isCJKCharacter(prevChar)) {
                    prefix = " ";
                }
            }
            var combined = prefix + finalText;
            messageInput.insert(start, combined);
            _transcriptionTentativeStart = start + combined.length;
        } else {
            _transcriptionTentativeStart = start;
        }
        messageInput.cursorPosition = _transcriptionTentativeStart;
    }

    // Drop the italic/muted styling on whatever tentative text is still
    // sitting in the buffer at session-close time. Keeps the text — the
    // user dictated it; we just have no polished form to swap in. Used
    // by `onRealtimeClosed` for the rare "session ended without a final
    // completed" case (e.g. post-commit timeout).
    function _demoteTentativeToFinal() {
        if (_transcriptionTentativeStart < 0 || _transcriptionTentativeLength <= 0) {
            _transcriptionTentativeStart = -1;
            _transcriptionTentativeLength = 0;
            _transcriptionTentativeText = "";
            return;
        }
        Transcription.clearTextFormat(messageInput.textDocument,
                                      _transcriptionTentativeStart,
                                      _transcriptionTentativeLength);
        messageInput.cursorPosition =
            _transcriptionTentativeStart + _transcriptionTentativeLength;
        _transcriptionTentativeStart = -1;
        _transcriptionTentativeLength = 0;
        _transcriptionTentativeText = "";
    }

    function _removeTranscriptionTentative() {
        if (_transcriptionTentativeStart < 0) {
            _transcriptionTentativeLength = 0;
            _transcriptionTentativeText = "";
            return;
        }
        var start = _transcriptionTentativeStart;
        var end = Math.min(messageInput.length, start + _transcriptionTentativeLength);
        if (end > start)
            messageInput.remove(start, end);
        _transcriptionTentativeStart = -1;
        _transcriptionTentativeLength = 0;
        _transcriptionTentativeText = "";
    }

    // Wipe the entire realtime contribution to the buffer — both
    // already-consolidated final utterances AND the live tentative
    // range. Used by Esc / cancel so an aborted dictation doesn't leave
    // half a session's worth of text behind.
    function _removeAllRealtimeText() {
        if (_transcriptionRealtimeOriginStart < 0) {
            // Either no realtime session was active, or this is the
            // batch path; fall back to just clearing the tentative
            // range.
            _removeTranscriptionTentative();
            return;
        }
        var start = _transcriptionRealtimeOriginStart;
        var end = _transcriptionTentativeStart >= 0
            ? _transcriptionTentativeStart + _transcriptionTentativeLength
            : start;
        end = Math.min(messageInput.length, Math.max(start, end));
        if (end > start)
            messageInput.remove(start, end);
        _transcriptionRealtimeOriginStart = -1;
        _transcriptionTentativeStart = -1;
        _transcriptionTentativeLength = 0;
        _transcriptionTentativeText = "";
    }

    function _isCJKCharacter(ch) {
        if (!ch || ch.length === 0)
            return false;
        const code = ch.charCodeAt(0);
        return (code >= 0x3000 && code <= 0x303F)   // CJK Symbols & Punctuation
            || (code >= 0x3040 && code <= 0x309F)   // Hiragana
            || (code >= 0x30A0 && code <= 0x30FF)   // Katakana
            || (code >= 0x3400 && code <= 0x4DBF)   // CJK Unified Ideographs Ext A
            || (code >= 0x4E00 && code <= 0x9FFF)   // CJK Unified Ideographs
            || (code >= 0xAC00 && code <= 0xD7AF)   // Hangul Syllables
            || (code >= 0xF900 && code <= 0xFAFF)   // CJK Compatibility Ideographs
            || (code >= 0xFF00 && code <= 0xFFEF);  // Halfwidth & Fullwidth Forms
    }

    function toggleEmojiPicker() {
        if (emojiPopup.visible) {
            emojiPopup.close();
            return;
        }

        emojiPopup.show(emojiButton, room ? room.roomId : "", function (plaintext, markdown) {
            messageInput.insert(messageInput.cursorPosition, markdown);
            TimelineManager.focusMessageInput();
        }, inputBar);
    }

    function eventMatchesLatinKey(event, latinKey) {
        if (!event)
            return false;

        return LayoutAgnosticKeys.matchesLatinKey(latinKey,
                                                  event.key,
                                                  event.nativeScanCode);
    }

    function isBackwardTabEvent(event) {
        if (!event)
            return false;

        const modifiers = Number(event.modifiers);
        if ((modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) !== 0)
            return false;

        return event.key === Qt.Key_Backtab
            || (event.key === Qt.Key_Tab && (modifiers & Qt.ShiftModifier) !== 0);
    }

    function isForwardTabEvent(event) {
        if (!event)
            return false;

        return event.key === Qt.Key_Tab && event.modifiers === Qt.NoModifier;
    }

    function isComposerTabEvent(event) {
        return isForwardTabEvent(event) || isBackwardTabEvent(event);
    }

    function requestSelectionModeEntry() {
        if (!selectionModeRoot || typeof selectionModeRoot.enterWalkModeFromBottomMostVisible !== "function")
            return false;

        return selectionModeRoot.enterWalkModeFromBottomMostVisible();
    }

    function requestSelectionModeOlderChunk() {
        if (!selectionModeRoot || typeof selectionModeRoot.enterWalkModeAndMoveTowardOlderEventsByChunk !== "function")
            return false;

        return selectionModeRoot.enterWalkModeAndMoveTowardOlderEventsByChunk();
    }

    function roomHeaderBacktabTarget() {
        if (!selectionModeRoot || typeof selectionModeRoot.lastRoomHeaderActionButtonTarget !== "function")
            return null;

        return selectionModeRoot.lastRoomHeaderActionButtonTarget();
    }

    function composerBacktabTarget() {
        if (transcriptionButton.visible && transcriptionButton.enabled)
            return transcriptionButton;
        if (voiceButton.visible && voiceButton.enabled)
            return voiceButton;
        if (attachButton.visible && attachButton.enabled)
            return attachButton;
        if (callButton.visible && callButton.enabled)
            return callButton;
        return roomHeaderBacktabTarget();
    }

    function composerTabTarget() {
        if (stickerButton.visible && stickerButton.enabled)
            return stickerButton;
        if (emojiButton.visible && emojiButton.enabled)
            return emojiButton;
        if (sendButton.visible && sendButton.enabled)
            return sendButton;
        return null;
    }

    function focusComposerBacktabTarget() {
        const target = composerBacktabTarget();
        if (!target)
            return false;

        target.forceActiveFocus(Qt.TabFocusReason);
        return true;
    }

    function focusComposerTabTarget() {
        const target = composerTabTarget();
        if (!target)
            return false;

        target.forceActiveFocus(Qt.TabFocusReason);
        return true;
    }

    Layout.fillWidth: true
    implicitHeight: Math.max(minimumBarHeight, row.implicitHeight)
    Layout.minimumHeight: minimumBarHeight
    Layout.preferredHeight: implicitHeight
    color: inputBar.hasUploads || (room && !inputBar.canSendTextMessages)
        ? palette.alternateBase
        : (inputBar._threadActive ? inputBar._threadBackgroundTint : palette.window)

    Shortcut {
        sequence: "Ctrl+R"
        enabled: inputBar.canSendCurrentRoom
        onActivated: {
            if (VoiceRecorder.recording)
                VoiceRecorder.pauseRecording();
            else if (VoiceRecorder.paused)
                VoiceRecorder.resumeRecording();
            else if (!VoiceRecorder.hasRecording && !inputBar.hasUploads) {
                VoiceRecorder.startRecording();
                TimelineManager.stageVoiceRecording(VoiceRecorder.filePath);
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+."
        enabled: inputBar.composerEnabled && !inputBar.hasVoiceRecording && emojiButton.visible
        onActivated: inputBar.toggleEmojiPicker()
    }

    Shortcut {
        sequences: {
            // Plain Enter always works for voice (no newline concept).
            // Also honour the user's configured send-key combo.
            var seqs = ["Return", "Enter"];
            if (Settings.composerInputSendKey == Settings.SendMessageKey.ShiftEnter)
                seqs.push("Shift+Return", "Shift+Enter");
            else if (Settings.composerInputSendKey == Settings.SendMessageKey.CtrlEnter)
                seqs.push("Ctrl+Return", "Ctrl+Enter");
            return seqs;
        }
        enabled: inputBar.hasVoiceRecording && inputBar.inputController
        onActivated: inputBar.inputController.send()
    }

    Shortcut {
        sequence: "Tab"
        enabled: inputBar.hasVoiceRecording
        onActivated: {
            // Cycle: voiceButton → play/finalize → trash → sendButton → voiceButton
            var voicePreview = voicePreviewLoader.item;
            var previewBtn = voicePreview ? voicePreview.focusableButton : null;
            var trashBtn = voicePreview ? voicePreview.deleteButton : null;
            if (voiceButton.activeFocus && previewBtn)
                previewBtn.forceActiveFocus(Qt.TabFocusReason);
            else if ((voiceButton.activeFocus || (previewBtn && previewBtn.activeFocus)) && trashBtn)
                trashBtn.forceActiveFocus(Qt.TabFocusReason);
            else if (trashBtn && trashBtn.activeFocus)
                sendButton.forceActiveFocus(Qt.TabFocusReason);
            else if (sendButton.activeFocus && voiceButton.visible)
                voiceButton.forceActiveFocus(Qt.TabFocusReason);
            else
                sendButton.forceActiveFocus(Qt.TabFocusReason);
        }
    }

    Shortcut {
        sequence: "Shift+Tab"
        enabled: inputBar.hasVoiceRecording
        onActivated: {
            var voicePreview = voicePreviewLoader.item;
            var previewBtn = voicePreview ? voicePreview.focusableButton : null;
            var trashBtn = voicePreview ? voicePreview.deleteButton : null;
            if (sendButton.activeFocus && trashBtn)
                trashBtn.forceActiveFocus(Qt.TabFocusReason);
            else if (trashBtn && trashBtn.activeFocus && previewBtn)
                previewBtn.forceActiveFocus(Qt.TabFocusReason);
            else if ((previewBtn && previewBtn.activeFocus) && voiceButton.visible)
                voiceButton.forceActiveFocus(Qt.TabFocusReason);
            else if (voiceButton.activeFocus)
                sendButton.forceActiveFocus(Qt.TabFocusReason);
            else if (voiceButton.visible)
                voiceButton.forceActiveFocus(Qt.TabFocusReason);
            else
                sendButton.forceActiveFocus(Qt.TabFocusReason);
        }
    }

    Timer {
        id: _draftSaveTimer

        interval: 300
        onTriggered: {
            if (inputBar._draftRoomId)
                Rooms.persistDraftForRoom(inputBar._draftRoomId, messageInput.text);
        }
    }

    readonly property bool roomIsSpace: room ? !!room.isSpace : false

    RowLayout {
        id: row

        anchors.fill: parent
        spacing: 0
        visible: inputBar.canSendCurrentRoom && !inputBar.roomIsSpace

        ComposerCallButton {
            id: callButton

            Layout.alignment: inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter
            Layout.leftMargin: Komai.paddingMedium
            KeyNavigation.backtab: inputBar.roomHeaderBacktabTarget()
            KeyNavigation.tab: attachButton.visible ? attachButton : messageInput
            room: inputBar.room
            timelineRoot: inputBar.timelineRoot
            showAllButtons: inputBar.showAllButtons && inputBar.allowCalls
        }
        ComposerAttachButton {
            id: attachButton

            Layout.alignment: inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter
            Layout.leftMargin: callButton.visible ? 0 : Komai.paddingMedium
            enabled: inputBar.attachmentsEnabled && !inputBar.hasVoiceRecording
            KeyNavigation.backtab: callButton.visible ? callButton : inputBar.roomHeaderBacktabTarget()
            KeyNavigation.tab: voiceButton.visible ? voiceButton : messageInput
            room: inputBar.room
            showAllButtons: inputBar.showAllButtons
        }
        ComposerVoiceButton {
            id: voiceButton

            Layout.alignment: inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter
            KeyNavigation.backtab: attachButton.visible ? attachButton : (callButton.visible ? callButton : inputBar.roomHeaderBacktabTarget())
            KeyNavigation.tab: transcriptionButton.visible
                ? transcriptionButton
                : (inputBar.hasVoiceRecording ? sendButton : messageInput)
            showAllButtons: inputBar.showAllButtons
            composerHasText: messageInput.length > 0 || inputBar.hasUploads
        }
        ComposerTranscriptionButton {
            id: transcriptionButton

            Layout.alignment: inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter
            KeyNavigation.backtab: voiceButton.visible
                ? voiceButton
                : (attachButton.visible ? attachButton : (callButton.visible ? callButton : inputBar.roomHeaderBacktabTarget()))
            KeyNavigation.tab: messageInput
            showAllButtons: inputBar.showAllButtons
            inputBar: inputBar
        }
        Loader {
            id: voicePreviewLoader

            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.leftMargin: Komai.paddingSmall
            Layout.rightMargin: Komai.paddingSmall
            active: inputBar.hasVoiceRecording
            visible: active
            sourceComponent: ComposerVoicePreview {}
        }
        // Transcription status banner is mounted by MatrixRoomComposerPane
        // (sibling of ReplyPopup), not here — keeps the textarea visible
        // during recording / transcribing / error / not-configured.
        ScrollView {
            id: textInput

            readonly property int singleLineHeight: Math.ceil(fontMetrics.lineSpacing + messageInput.topPadding + messageInput.bottomPadding)
            readonly property int targetTextAreaHeight: Math.max(singleLineHeight,
                                                                 Math.ceil(messageInput.contentHeight + messageInput.topPadding + messageInput.bottomPadding))
            // User drag-resize floor (re-clamped at read time so a window
            // shrink after the drag can't leave the composer oversized).
            readonly property int userDesiredHeight: Math.ceil(Math.min(inputBar.userDesiredInputHeight,
                                                                        inputBar.maxUserInputHeight))
            readonly property int effectiveTextAreaHeight: Math.max(targetTextAreaHeight, userDesiredHeight)
            readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
            readonly property bool hasVerticalOverflow: targetTextAreaHeight > height
            readonly property bool scrollbarVisible: {
                switch (scrollbarPolicy) {
                case Settings.ScrollbarPolicy.Always:
                    return true;
                case Settings.ScrollbarPolicy.Never:
                    return false;
                case Settings.ScrollbarPolicy.WhenNeeded:
                default:
                    return hasVerticalOverflow;
                }
            }
            // Reserve based on the user's policy (a static setting), not on the
            // live `scrollbarVisible` — that one feeds back through the
            // TextArea's padding → contentHeight → hasVerticalOverflow and
            // closes a binding loop. For WhenNeeded we reserve unconditionally
            // so the textarea width stays stable as the user types. Use
            // `implicitWidth` only; the live `width` is 0 when the bar's policy
            // is AlwaysOff, which depends on `scrollbarVisible` and reopens the
            // same loop.
            readonly property real reservedScrollbarWidth: scrollbarPolicy === Settings.ScrollbarPolicy.Never
                ? 0
                : ScrollBar.vertical.implicitWidth
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            // Content alone still caps at a quarter of the window; a user
            // drag-resize may push the cap further (userDesiredHeight is
            // already clamped to half the window).
            Layout.maximumHeight: Math.max(inputBar.Window.height / 4, userDesiredHeight)
            Layout.minimumHeight: visible ? effectiveTextAreaHeight : 0
            Layout.preferredHeight: visible ? effectiveTextAreaHeight : 0
            visible: !inputBar.hasVoiceRecording
            // Fade out (rather than hide) during uploads so Layout.fillWidth
            // stays active and the button row doesn't redistribute; the draft
            // lives on in messageInput.text and reappears once uploads clear.
            opacity: inputBar.hasUploads ? 0 : 1
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: scrollbarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            contentWidth: availableWidth
            implicitHeight: effectiveTextAreaHeight
            padding: 0

            TextArea {
                id: messageInput

                property int completerTriggeredAt: 0
                // Position of the `:` whose emoji picker we auto-suppressed
                // because the typed token was a complete emoticon shortcut
                // (`:)`, `:D`, ...). If the user keeps typing into the same
                // token so it's no longer an emoticon (`:D` → `:Dog`), we
                // reopen the picker at this position.
                property int emoticonSuppressedTriggerAt: -1
                // Position of the `:` whose emoji picker we explicitly closed
                // because the user pressed Space (Keys.onPressed, before the
                // space is inserted) while it was open, win or lose (`:thin`
                // or `:thin` with zero results alike). Kept separate from
                // emoticonSuppressedTriggerAt: that one resets as soon as its
                // tracked token contains any whitespace, which would discard
                // this immediately -- here the whitespace IS the closing
                // trigger, so backspacing it back out must still reopen.
                property int emojiSpaceClosedAt: -1
                readonly property int horizontalTextPadding: inputBar.showAllButtons ? Komai.paddingSmall : 8
                property string lastChar
                property string placeholderLabelText: qsTr("Write a message, or press ↑ to select messages.")
                property int previousTextLength: 0

                function currentCompleterSearchString() {
                    if (completer.completerType === "command" && inputBar.inputController) {
                        const prefix = messageInput.getText(0, cursorPosition) + messageInput.preeditText;
                        return inputBar.inputController.commandCompletionSearchString(prefix,
                                                                                      cursorPosition + messageInput.preeditText.length);
                    }

                    return messageInput.getText(completerTriggeredAt, cursorPosition) + messageInput.preeditText;
                }
                function hasNestedCompleterTrigger(searchString) {
                    const suffix = String(searchString || "").slice(1);
                    if (suffix.length === 0)
                        return false;

                    switch (completer.completerType) {
                    case "user":
                    case "user-mxid":
                        return suffix.indexOf('@') >= 0 || suffix.indexOf('＠') >= 0;
                    case "emoji":
                        return suffix.indexOf(':') >= 0 || suffix.indexOf('：') >= 0;
                    case "roomAliases":
                        return suffix.indexOf('#') >= 0 || suffix.indexOf('＃') >= 0;
                    case "customEmoji":
                        return suffix.indexOf('~') >= 0 || suffix.indexOf('～') >= 0;
                    default:
                        return false;
                    }
                }
                function looksLikeAbandonedEmojiSearch(query) {
                    // True once a character other than a letter/digit or
                    // whitespace follows the `:` trigger (e.g. `:)?`, `:D!`)
                    // -- the user has moved on to punctuation in their
                    // sentence, not continuing an emoji-name search, so
                    // showing the picker would just flash empty results.
                    // Whitespace is deliberately excluded: a trailing space
                    // just ends the search with no matches (normal, and
                    // already handled by the empty-results state) -- it must
                    // NOT engage this same suppress/remember-to-reopen
                    // tracking, which is for punctuation specifically and
                    // doesn't reliably recover on backspace for a plain
                    // space (e.g. `:thin` + space then backspace must still
                    // show the picker again for `:thin`). An empty query
                    // (just typed `:`) is not considered abandoned yet.
                    return query.length > 0 && /[^A-Za-z0-9\s]$/.test(query);
                }
                function refreshCompleterSearchString() {
                    // Gate on completerType (set synchronously by openCompleter)
                    // rather than popup.opened. The popup's 100ms enter
                    // transition leaves popup.opened=false for a frame or two
                    // after a fresh open, which would let an `:emoticon` typed
                    // immediately after `:` slip past the auto-close path.
                    if (!completer.completer || completer.completerType === "")
                        return;

                    const searchString = messageInput.currentCompleterSearchString();
                    if (messageInput.hasNestedCompleterTrigger(searchString)) {
                        completer.completerType = "";
                        popup.close();
                        return;
                    }

                    // Once the typed token is itself a complete emoticon
                    // shortcut (e.g. `:)` or `:D`), or once it's been
                    // extended with punctuation instead of more letters
                    // (`:)?`, `:D!`), the picker's lookup for that string is
                    // noise — auto-conversion will turn a real shortcut into
                    // an emoji on send anyway, and punctuation isn't a name
                    // search. Remember the trigger so we can reopen if the
                    // user extends the token with more letters instead
                    // (`:D` → `:Dog`).
                    if (completer.completerType === "emoji"
                            && Settings.composerInputAutoReplaceEmoji !== Settings.AutoReplaceEmoji.Never
                            && (Komai.isEmoticonShortcut(searchString)
                                || messageInput.looksLikeAbandonedEmojiSearch(searchString.slice(1)))) {
                        emoticonSuppressedTriggerAt = completerTriggeredAt;
                        completer.completerType = "";
                        popup.close();
                        return;
                    }

                    completer.completer.setSearchString(searchString);
                }
                function maybeReopenAfterEmoticonSuppression() {
                    // Same rationale as in refreshCompleterSearchString: use
                    // completerType, not popup.opened, so we don't get
                    // confused mid-transition.
                    if (completer.completerType !== "" || emoticonSuppressedTriggerAt < 0)
                        return;
                    const trigger = emoticonSuppressedTriggerAt;
                    if (cursorPosition <= trigger || cursorPosition > text.length) {
                        emoticonSuppressedTriggerAt = -1;
                        return;
                    }
                    const token = text.substring(trigger, cursorPosition) + messageInput.preeditText;
                    if (token.length === 0 || /\s/.test(token)) {
                        emoticonSuppressedTriggerAt = -1;
                        return;
                    }
                    if (Komai.isEmoticonShortcut(token))
                        return; // still a shortcut — keep the picker dismissed
                    if (messageInput.looksLikeAbandonedEmojiSearch(token.slice(1)))
                        return; // extended with punctuation, not more letters — stay dismissed
                    messageInput.openCompleter(trigger, "emoji");
                }
                function maybeReopenAfterSpaceClose() {
                    if (completer.completerType !== "" || emojiSpaceClosedAt < 0)
                        return;
                    const trigger = emojiSpaceClosedAt;
                    if (cursorPosition <= trigger || cursorPosition > text.length) {
                        emojiSpaceClosedAt = -1;
                        return;
                    }
                    const token = text.substring(trigger, cursorPosition) + messageInput.preeditText;
                    if (token.length === 0) {
                        emojiSpaceClosedAt = -1;
                        return;
                    }
                    if (/\s/.test(token))
                        return; // the space (or more) is still there — wait for it to be removed, keep tracking
                    emojiSpaceClosedAt = -1;
                    messageInput.openCompleter(trigger, "emoji");
                }
                function insertCompletion(completion, activeType, activeUserid) {
                    if (activeType === undefined)
                        activeType = completer.completerType;
                    if (activeUserid === undefined)
                        activeUserid = completer.currentUserid();

                    if (activeType === "command" && inputBar.inputController) {
                        const updatedText = inputBar.inputController.applyCommandCompletion(messageInput.text,
                                                                                            cursorPosition,
                                                                                            completion);
                        const updatedCursorPosition = inputBar.inputController.commandCompletionCursorPosition(messageInput.text,
                                                                                                               cursorPosition,
                                                                                                               completion);
                        messageInput.text = updatedText;
                        messageInput.cursorPosition = updatedCursorPosition;
                        return;
                    }

                    const isUserMxidPick = activeType === "user-mxid";
                    const textToInsert = isUserMxidPick && activeUserid
                        ? String(activeUserid)
                        : completion;
                    let replaceEnd = cursorPosition;
                    messageInput.remove(completerTriggeredAt, replaceEnd);
                    messageInput.insert(completerTriggeredAt, textToInsert);
                    messageInput.cursorPosition = completerTriggeredAt + textToInsert.length;
                    if (activeUserid && inputBar.inputController && !isUserMxidPick)
                        inputBar.inputController.addMention(activeUserid, completion);
                }
                function openCompleter(pos, type) {
                    emoticonSuppressedTriggerAt = -1;
                    completerTriggeredAt = pos;
                    completer.completerType = type;
                    if (!popup.opened) {
                        popup._positionRefreshTick++;
                        popup.open();
                    }
                    messageInput.refreshCompleterSearchString();
                }
                function completerTypeForTrigger(trigger, tokenStart) {
                    if ((trigger === '@' || trigger === '＠') && Settings.composerInputInlineUserPickerEnabled) {
                        if (inputBar.inputController
                                && typeof inputBar.inputController.commandExpectsUserIdAt === "function"
                                && inputBar.inputController.commandExpectsUserIdAt(messageInput.text, tokenStart)) {
                            return "user-mxid";
                        }
                        return "user";
                    }
                    if ((trigger === ':' || trigger === '：') && Settings.composerInputInlineEmojiPickerEnabled)
                        return "emoji";
                    if ((trigger === '#' || trigger === '＃') && Settings.composerInputInlineRoomPickerEnabled)
                        return "roomAliases";
                    if (trigger === '~' || trigger === '～')
                        return "customEmoji";
                    if (inputBar.allowCommandCompleter
                            && (trigger === '/' || trigger === '／')
                            && tokenStart === 0)
                        return "command";
                    return "";
                }
                function isCursorOnTopLine() {
                    return currentVisualLineStartPosition() === 0;
                }
                function isCursorOnBottomLine() {
                    return currentVisualLineEndPosition() === messageInput.length;
                }
                function currentVisualLineStartPosition() {
                    return positionAt(0, cursorRectangle.y + cursorRectangle.height / 2);
                }
                function currentVisualLineEndPosition() {
                    return positionAt(width, cursorRectangle.y + cursorRectangle.height / 2);
                }
                function isCursorAtSelectionModeBoundary() {
                    if (text.length === 0 && cursorPosition === 0)
                        return true;

                    if (!messageInput.isCursorOnTopLine())
                        return false;

                    return cursorPosition === currentVisualLineStartPosition();
                }
                function maybeOpenCompleterForTrailingTokenAfterBulkInsert() {
                    if (popup.opened || cursorPosition !== text.length)
                        return;

                    var tokenStart = cursorPosition - 1;
                    while (tokenStart >= 0) {
                        const c = text.charAt(tokenStart);
                        if (c === ' ' || c === '\t' || c === '\n')
                            break;
                        tokenStart = tokenStart - 1;
                    }
                    tokenStart = tokenStart + 1;

                    if (tokenStart < 0 || tokenStart >= cursorPosition)
                        return;

                    const tokenLength = cursorPosition - tokenStart;
                    if (tokenLength < 2)
                        return;

                    const trigger = text.charAt(tokenStart);
                    const type = messageInput.completerTypeForTrigger(trigger, tokenStart);
                    if (type !== "")
                        messageInput.openCompleter(tokenStart, type);
                }
                function applyComposerFormat(kind) {
                    const result = Komai.composerApplyFormat(messageInput.text,
                                                              messageInput.selectionStart,
                                                              messageInput.selectionEnd,
                                                              kind);
                    if (!result || !result.applied)
                        return false;
                    Komai.composerReplaceRange(messageInput.textDocument,
                                                result.replaceStart,
                                                result.replaceEnd,
                                                result.replacement);
                    if (result.selectionStart === result.selectionEnd)
                        messageInput.cursorPosition = result.selectionStart;
                    else
                        messageInput.select(result.selectionStart, result.selectionEnd);
                    messageInput.forceActiveFocus();
                    return true;
                }

                KeyNavigation.backtab: transcriptionButton.visible
                    ? transcriptionButton
                    : (voiceButton.visible
                        ? voiceButton
                        : (attachButton.visible
                            ? attachButton
                            : (callButton.visible ? callButton : inputBar.roomHeaderBacktabTarget())))
                background: null
                bottomPadding: Komai.composerTextAreaPadding
                color: palette.text
                enabled: inputBar.composerEnabled
                // Lock typing while a realtime session is mid-flight so
                // user keystrokes don't shift the tentative range out
                // from under us. Esc still cancels (handled separately
                // via Keys.onShortcutOverride), and Space release still
                // commits because the gesture state machine intercepts
                // before readOnly applies.
                readOnly: inputBar._transcriptionIsRealtime
                    && (inputBar.transcriptionState === "recording"
                        || inputBar.transcriptionState === "transcribing")
                focus: true
                horizontalAlignment: inputBar._rtlLayout ? Text.AlignRight : Text.AlignLeft
                LayoutMirroring.enabled: false
                padding: 0
                leftPadding: (inputBar._rtlLayout ? 0 : horizontalTextPadding) + (inputBar._rtlLayout ? textInput.reservedScrollbarWidth : 0)
                rightPadding: (inputBar._rtlLayout ? horizontalTextPadding : 0) + (inputBar._rtlLayout ? 0 : textInput.reservedScrollbarWidth)
                font.pointSize: Settings.uiFontSizePt
                placeholderText: ""
                placeholderTextColor: palette.buttonText
                Accessible.name: qsTr("Message")
                Accessible.description: placeholderLabelText
                selectByMouse: true
                // Keep the selection alive across focus blinks so a click on
                // the formatting toolbar can apply a transform to the
                // selection that was just visible.
                persistentSelection: true
                topPadding: Komai.composerTextAreaPadding
                // When the user has dragged the input area taller than its
                // content, pin the text to the top (centred text floating in
                // the middle of a tall empty editor reads wrong). The default
                // content-sized state keeps its centred single-line look.
                verticalAlignment: textInput.effectiveTextAreaHeight > textInput.targetTextAreaHeight
                    ? TextEdit.AlignTop
                    : TextEdit.AlignVCenter
                // Fill the viewport when drag-resized taller than the content
                // so clicks in the extra space still land in the editor.
                implicitHeight: textInput.effectiveTextAreaHeight
                width: textInput.width
                wrapMode: TextEdit.Wrap

                Label {
                    readonly property bool rtlText: inputBar._rtlLayout || inputBar._textStartsRtl(text)
                    readonly property real availableWidth: Math.max(0, messageInput.width - messageInput.leftPadding - messageInput.rightPadding)

                    color: messageInput.placeholderTextColor
                    enabled: false
                    font: messageInput.font
                    height: Math.max(0, messageInput.height - messageInput.topPadding - messageInput.bottomPadding)
                    horizontalAlignment: rtlText ? Text.AlignRight : Text.AlignLeft
                    LayoutMirroring.enabled: false
                    text: messageInput.placeholderLabelText
                    textFormat: Text.PlainText
                    width: Math.min(implicitWidth, availableWidth)
                    x: rtlText
                        ? Math.max(0, messageInput.width - messageInput.rightPadding - width)
                        : messageInput.leftPadding
                    y: messageInput.topPadding
                    z: 1
                    verticalAlignment: messageInput.verticalAlignment === TextEdit.AlignTop
                        ? Text.AlignTop
                        : Text.AlignVCenter
                    visible: messageInput.length === 0 && !messageInput.inputMethodComposing
                }

                Keys.onPressed: event => {
                    if (event.modifiers === (Qt.ControlModifier | Qt.ShiftModifier) && event.key === Qt.Key_V) {
                        // Ctrl+Shift+V: paste as plain text (Qt doesn't handle this natively,
                        // and the unhandled key event produces a control character / tofu square)
                        var clipText = inputBar.inputController ? inputBar.inputController.clipboardText() : "";
                        if (clipText)
                            messageInput.insert(messageInput.cursorPosition, clipText);
                        event.accepted = true;
                    } else if (event.matches(StandardKey.Paste)) {
                        event.accepted = inputBar.inputController
                            ? inputBar.inputController.tryPasteAttachment(false)
                            : false;
                    } else if (event.key == Qt.Key_Space) {
                        // close popup if user enters space after colon
                        if (cursorPosition == completerTriggeredAt + 1)
                            popup.close();
                        if (popup.opened && completer.count <= 0)
                            popup.close();
                        // A space always ends an emoji-name search, matched
                        // results or not (unlike other completer types,
                        // which stay open here) -- remember the trigger so
                        // backspacing the space back out reopens it right
                        // where it was, via maybeReopenAfterSpaceClose().
                        if (popup.opened && completer.completerType === "emoji") {
                            emojiSpaceClosedAt = completerTriggeredAt;
                            completer.completerType = "";
                            popup.close();
                        }
                        // Long-press Space → voice transcription. Initial
                        // keydown arms the gesture and captures the cursor
                        // position so we can pull the space back if the
                        // long-press threshold elapses into recording. The
                        // event is *not* accepted — Qt inserts the space
                        // immediately, keeping typing WYSIWYG. Autorepeats
                        // while the gesture (or its sticky banner) is alive
                        // get swallowed so a held Space past the long-press
                        // threshold doesn't leak a stream of spaces.
                        // Button-driven gestures leave Space alone.
                        if ((event.modifiers & ~Qt.KeypadModifier) === 0
                            && inputBar.transcriptionGestureEligible) {
                            if (!event.isAutoRepeat
                                && (inputBar.transcriptionState === "idle"
                                    || inputBar.transcriptionState === "error")) {
                                inputBar._transcriptionGestureSpacePos = messageInput.cursorPosition;
                                inputBar._armTranscriptionGesture("space");
                            } else if (event.isAutoRepeat
                                && inputBar.transcriptionState !== "idle"
                                && inputBar.transcriptionTriggerKind === "space") {
                                event.accepted = true;
                            }
                        }
                    } else if (event.modifiers == Qt.ControlModifier && inputBar.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.U)) {
                        event.accepted = inputBar.requestSelectionModeOlderChunk();
                    } else if (event.modifiers == Qt.ControlModifier && event.key == Qt.Key_P) {
                        messageInput.text = inputBar.inputController
                            ? inputBar.inputController.previousText()
                            : messageInput.text;
                    } else if (event.modifiers == Qt.ControlModifier && event.key == Qt.Key_N) {
                        messageInput.text = inputBar.inputController
                            ? inputBar.inputController.nextText()
                            : messageInput.text;
                    } else if (event.modifiers === Qt.ControlModifier && event.key === Qt.Key_B
                               && !messageInput.readOnly
                               && messageInput.preeditText.length === 0
                               && !popup.opened
                               && messageInput.selectionStart !== messageInput.selectionEnd) {
                        if (messageInput.applyComposerFormat(Komai.ComposerFormatKind.Bold))
                            event.accepted = true;
                    } else if (event.modifiers === Qt.ControlModifier && event.key === Qt.Key_I
                               && !messageInput.readOnly
                               && messageInput.preeditText.length === 0
                               && !popup.opened
                               && messageInput.selectionStart !== messageInput.selectionEnd) {
                        if (messageInput.applyComposerFormat(Komai.ComposerFormatKind.Italic))
                            event.accepted = true;
                    } else if (event.modifiers === Qt.ControlModifier && event.key === Qt.Key_E
                               && !messageInput.readOnly
                               && messageInput.preeditText.length === 0
                               && !popup.opened
                               && messageInput.selectionStart !== messageInput.selectionEnd) {
                        if (messageInput.applyComposerFormat(Komai.ComposerFormatKind.InlineCode))
                            event.accepted = true;
                    } else if (event.modifiers === (Qt.ControlModifier | Qt.ShiftModifier)
                               && (event.key === Qt.Key_Greater
                                   || event.key === Qt.Key_Period)
                               && !messageInput.readOnly
                               && messageInput.preeditText.length === 0
                               && !popup.opened
                               && messageInput.selectionStart !== messageInput.selectionEnd) {
                        // Ctrl+Shift+> (US layout produces Key_Greater; some
                        // layouts deliver Key_Period with Shift instead).
                        if (messageInput.applyComposerFormat(Komai.ComposerFormatKind.Quote))
                            event.accepted = true;
                    } else if (event.modifiers === (Qt.ControlModifier | Qt.ShiftModifier) && event.key === Qt.Key_L
                               && !messageInput.readOnly
                               && messageInput.preeditText.length === 0
                               && !popup.opened) {
                        // Link tolerates an empty selection — it inserts
                        // `[]()` and parks the cursor inside the brackets.
                        if (messageInput.applyComposerFormat(Komai.ComposerFormatKind.Link))
                            event.accepted = true;
                    } else if (event.key == Qt.Key_Escape && popup.opened) {
                        completer.completerType = "";
                        popup.close();
                        event.accepted = true;
                    } else if (event.key == Qt.Key_Escape
                        && inputBar.transcriptionState !== "idle") {
                        // Esc during recording / transcription cancels the
                        // gesture. Discards audio without inserting text.
                        inputBar._cancelTranscriptionGesture();
                        event.accepted = true;
                    } else if (event.key == Qt.Key_Escape) {
                        if (TimelineManager.matrixTimelineReplyEventId.length > 0) {
                            TimelineManager.clearActiveMatrixReply();
                            event.accepted = true;
                        } else if (TimelineManager.matrixTimelineEditEventId.length > 0) {
                            TimelineManager.clearActiveMatrixEdit();
                            event.accepted = true;
                        } else if (TimelineManager.matrixTimelineThreadEventId.length > 0) {
                            TimelineManager.clearActiveMatrixThread();
                            event.accepted = true;
                        }
                    } else if (event.matches(StandardKey.SelectAll) && popup.opened) {
                        completer.completerType = "";
                        popup.close();
                    } else if ((event.key == Qt.Key_Enter || event.key == Qt.Key_Return)
                               && inputBar._transcriptionIsRealtime
                               && (inputBar.transcriptionState === "recording"
                                   || inputBar.transcriptionState === "transcribing")) {
                        // Don't let Enter send a half-transcribed message
                        // while a realtime session is mid-flight. The user
                        // can finish the gesture (release Space / click
                        // Stop) and then send.
                        event.accepted = true;
                    } else if (event.key == Qt.Key_Enter || event.key == Qt.Key_Return) {
                        // If popup is open and user has selected a completion, insert it.
                        if (popup.opened &&
                            (event.modifiers == Qt.NoModifier
                            || event.modifiers == Qt.ShiftModifier
                            || event.modifiers == Qt.ControlModifier)
                        ) {
                            var currentCompletion = completer.currentCompletion();
                            let userid = completer.currentUserid();
                            const capturedType = completer.completerType;

                            // Close after insertCompletion: while popup.opened
                            // is still true, the bulk-insert trigger detection
                            // in maybeOpenCompleterForTrailingTokenAfterBulkInsert
                            // short-circuits, so insertions that happen to begin
                            // with a trigger char (e.g. raw MXIDs from "user-mxid"
                            // completion) don't immediately re-open the popup.
                            if (currentCompletion) {
                                messageInput.insertCompletion(currentCompletion, capturedType, userid);
                                if (userid && capturedType === "user") {
                                    if (inputBar.inputController)
                                        inputBar.inputController.addMention(userid, currentCompletion);
                                }
                                event.accepted = true;
                            }

                            completer.completerType = "";
                            popup.close();
                            // Nothing selected: popup closed, fall through to send/newline.
                        }
                        // Send message Enter key combination event.
                        if (!event.accepted && (
                            Settings.composerInputSendKey == Settings.SendMessageKey.Enter && event.modifiers == Qt.NoModifier
                              || Settings.composerInputSendKey == Settings.SendMessageKey.ShiftEnter && event.modifiers == Qt.ShiftModifier
                              || Settings.composerInputSendKey == Settings.SendMessageKey.CtrlEnter && event.modifiers == Qt.ControlModifier)
                        ) {
                            if (inputBar.inputController)
                                inputBar.inputController.send();
                            event.accepted = true;
                        }
                        // Add newline Enter key combination event.
                        else if (!event.accepted && (
                            Settings.composerInputSendKey == Settings.SendMessageKey.Enter && event.modifiers == Qt.ShiftModifier
                              || Settings.composerInputSendKey == Settings.SendMessageKey.ShiftEnter && event.modifiers == Qt.NoModifier
                              || Settings.composerInputSendKey == Settings.SendMessageKey.CtrlEnter && event.modifiers == Qt.ShiftModifier)
                        ) {
                            messageInput.insert(messageInput.cursorPosition, "\n");
                            event.accepted = true;
                        }
                        // Any other Enter key combo is ignored here.
                    } else if (inputBar.isComposerTabEvent(event)) {
                        if (!popup.opened && messageInput.length === 0 && !messageInput.inputMethodComposing) {
                            event.accepted = true;
                            if (inputBar.isBackwardTabEvent(event))
                                inputBar.focusComposerBacktabTarget();
                            else
                                inputBar.focusComposerTabTarget();
                            return;
                        }

                        event.accepted = true;
                        if (popup.opened) {
                            if (inputBar.isBackwardTabEvent(event))
                                completer.down();
                            else
                                completer.up();
                        } else {
                            var pos = cursorPosition - 1;
                            while (pos > -1) {
                                var t = messageInput.getText(pos, pos + 1);
                                console.log('"' + t + '"');
                                const type = messageInput.completerTypeForTrigger(t, pos);
                                if (type !== "") {
                                    messageInput.openCompleter(pos, type);
                                    return;
                                } else if (t == ' ' || t == '\t') {
                                    messageInput.openCompleter(pos + 1, "user");
                                    return;
                                }
                                pos = pos - 1;
                            }
                            // At start of input
                            messageInput.openCompleter(0, "user");
                        }
                    } else if (event.key == Qt.Key_Backspace
                               && event.modifiers === Qt.NoModifier
                               && messageInput.preeditText.length === 0
                               && messageInput.selectionStart === messageInput.selectionEnd
                               && messageInput.cursorPosition > 0) {
                        // Qt's default Backspace deletes one UTF-16 code unit, which
                        // breaks emoji clusters: base + VS16 leaves a bare text-default
                        // glyph rendered B&W (e.g. ☺️ becomes ☺), and ZWJ sequences
                        // degrade one component at a time. Delete the full previous
                        // grapheme cluster instead.
                        const pos = messageInput.cursorPosition;
                        const prev = Komai.previousGraphemeBoundary(messageInput.text, pos);
                        if (prev < pos - 1) {
                            messageInput.remove(prev, pos);
                            event.accepted = true;
                        }
                    } else if (event.key == Qt.Key_Delete
                               && event.modifiers === Qt.NoModifier
                               && messageInput.preeditText.length === 0
                               && messageInput.selectionStart === messageInput.selectionEnd
                               && messageInput.cursorPosition < messageInput.length) {
                        const pos = messageInput.cursorPosition;
                        const next = Komai.nextGraphemeBoundary(messageInput.text, pos);
                        if (next > pos + 1) {
                            messageInput.remove(pos, next);
                            event.accepted = true;
                        }
                    } else if (event.key == Qt.Key_Up && popup.opened) {
                        event.accepted = true;
                        completer.up();
                    } else if (event.key == Qt.Key_Down && popup.opened) {
                        event.accepted = true;
                        completer.down();
                    } else if (event.key == Qt.Key_Up && (event.modifiers == Qt.NoModifier || event.modifiers == Qt.KeypadModifier)) {
                        if (messageInput.isCursorAtSelectionModeBoundary()) {
                            event.accepted = inputBar.requestSelectionModeEntry();
                        } else if (messageInput.isCursorOnTopLine()) {
                            event.accepted = true;
                            cursorPosition = messageInput.currentVisualLineStartPosition();
                        }
                    } else if (event.key == Qt.Key_Down && (event.modifiers == Qt.NoModifier || event.modifiers == Qt.KeypadModifier)) {
                        if (messageInput.isCursorOnBottomLine()) {
                            const lineEnd = messageInput.currentVisualLineEndPosition();
                            if (cursorPosition !== lineEnd) {
                                event.accepted = true;
                                cursorPosition = lineEnd;
                            }
                        }
                    }
                }
                Keys.onReleased: event => {
                    if (event.key !== Qt.Key_Space || event.isAutoRepeat)
                        return;
                    if (inputBar.transcriptionTriggerKind === "space"
                        && (inputBar.transcriptionState === "armed"
                            || inputBar.transcriptionState === "recording")) {
                        inputBar._commitTranscriptionGesture();
                        event.accepted = true;
                    }
                }
                // Ensure that we get escape key press events first.
                Keys.onShortcutOverride: event => {
                    if (inputBar.isComposerTabEvent(event)) {
                        event.accepted = true;
                        return;
                    }

                    let escapeHandled = event.key === Qt.Key_Escape
                        && (popup.opened
                            || inputBar.transcriptionState !== "idle"
                            || TimelineManager.matrixTimelineReplyEventId.length > 0
                            || TimelineManager.matrixTimelineEditEventId.length > 0
                            || TimelineManager.matrixTimelineThreadEventId.length > 0);

                    event.accepted = escapeHandled
                        || (popup.opened
                            && (event.key === Qt.Key_Tab
                                || event.key === Qt.Key_Backtab
                                || event.key === Qt.Key_Enter
                                || event.key === Qt.Key_Space));
                }
                onCursorPositionChanged: {
                    if (!inputBar.inputController)
                        return;
                    inputBar.inputController.updateState(selectionStart, selectionEnd, cursorPosition, text);
                    if (popup.opened && cursorPosition <= completerTriggeredAt)
                        popup.close();
                    if (emoticonSuppressedTriggerAt >= 0 && cursorPosition <= emoticonSuppressedTriggerAt)
                        emoticonSuppressedTriggerAt = -1;
                    messageInput.refreshCompleterSearchString();
                }
                onPreeditTextChanged: {
                    messageInput.refreshCompleterSearchString();
                }
                onSelectionEndChanged: {
                    if (inputBar.inputController)
                        inputBar.inputController.updateState(selectionStart, selectionEnd, cursorPosition, text);
                }
                onSelectionStartChanged: {
                    if (inputBar.inputController)
                        inputBar.inputController.updateState(selectionStart, selectionEnd, cursorPosition, text);
                }
                onTextChanged: {
                    const insertedLength = text.length - previousTextLength;
                    // Live emoticon replacement: convert a completed shortcut
                    // (e.g. `:)`, `:D`) to its emoji the moment the user types
                    // the space that follows it -- so what's shown is what
                    // will be sent, instead of only converting at send time.
                    // Gated to exactly one typed space character (not paste,
                    // not IME composition) so it never fires mid-word or on
                    // bulk-inserted text; pasted shortcuts still convert at
                    // send time via the existing replaceEmoticons() fallback.
                    if (insertedLength === 1 && cursorPosition > 0
                            && !messageInput.inputMethodComposing
                            && Settings.composerInputAutoReplaceEmoji !== Settings.AutoReplaceEmoji.Never
                            && text.charAt(cursorPosition - 1) === " ") {
                        const spacePos = cursorPosition - 1;
                        let tokenStart = spacePos;
                        while (tokenStart > 0 && !/\s/.test(text.charAt(tokenStart - 1)))
                            tokenStart--;
                        const token = text.substring(tokenStart, spacePos);
                        if (token.length > 0) {
                            const replacement = Komai.emoticonReplacementFor(token);
                            if (replacement.length > 0) {
                                messageInput.remove(tokenStart, spacePos);
                                messageInput.insert(tokenStart, replacement);
                                messageInput.cursorPosition = tokenStart + replacement.length + 1;
                                // The token just consumed may have reopened the
                                // emoji picker along the way (e.g. typing the
                                // "?" in ":)?" no longer matches an exact
                                // shortcut, so maybeReopenAfterEmoticonSuppression()
                                // reopens it) -- now that we've converted it,
                                // any picker tracking that span is stale.
                                completer.completerType = "";
                                popup.close();
                                emoticonSuppressedTriggerAt = -1;
                            }
                        }
                    }
                    if (inputBar.inputController)
                        inputBar.inputController.updateState(selectionStart, selectionEnd, cursorPosition, text);
                    // A bulk insertion (paste, drag-drop) can carry matrix links
                    // the composer never saw picked, so derive mentions from them.
                    // Single-character typing is skipped, and derivation respects
                    // dismissals, so a mention dismissed mid-typing is not undone.
                    if (insertedLength > 1 && inputBar.inputController
                            && typeof inputBar.inputController.deriveMentionsFromText === "function")
                        inputBar.inputController.deriveMentionsFromText(text);
                    inputBar.focusTextInputIfAllowed();
                    if (cursorPosition > 0)
                        lastChar = text.charAt(cursorPosition - 1);
                    else
                        lastChar = '';
                    const triggerPos = selectionStart - 1;
                    const type = messageInput.completerTypeForTrigger(lastChar, triggerPos);
                    if (type !== "") {
                        // Don't open mid-word (e.g. inside `user@example.com`
                        // or `test:value`). The boundary check lives in Rust
                        // so it can handle whitespace, punctuation, and
                        // emoji-cluster surrogates uniformly: typing `:`
                        // immediately after an inserted emoji must still
                        // open the picker.
                        if (type === "command"
                            || Komai.composerTriggerAtWordBoundary(text, triggerPos))
                            messageInput.openCompleter(triggerPos, type);
                    } else if (insertedLength > 1) {
                        messageInput.maybeOpenCompleterForTrailingTokenAfterBulkInsert();
                    } else {
                        messageInput.maybeReopenAfterEmoticonSuppression();
                        messageInput.maybeReopenAfterSpaceClose();
                    }
                    previousTextLength = text.length;
                    inputBar._draftText = text;
                    if (inputBar._draftRoomId) {
                        if (text.length === 0) {
                            _draftSaveTimer.stop();
                            Rooms.persistDraftForRoom(inputBar._draftRoomId, "");
                        } else {
                            _draftSaveTimer.restart();
                        }
                    }
                    if (inputBar.room && inputBar.room.isActiveMatrixTimelineRoom
                            && !inputBar.walkModeActive
                            && Settings.resolvedComposerTypingSendEnabled(inputBar.room.roomId)) {
                        const shouldType = text.length >= 4 && !text.startsWith("/");
                        TimelineManager.sendActiveMatrixTypingNotice(shouldType);
                    }
                }

                Connections {
                    function onRoomPreviewChanged() {
                        if (TimelineManager.perfUiFlagEnabled("disable_composer"))
                            return;

                        // Save draft for the room we are leaving. Mentions are
                        // per-room draft state too, so snapshot them now — before
                        // clear() below prunes the live mention list — keyed to
                        // the same leaving-room id as the text draft.
                        if (_draftSaveTimer.running)
                            _draftSaveTimer.stop();
                        if (inputBar._draftRoomId) {
                            Rooms.persistDraftForRoom(inputBar._draftRoomId, inputBar._draftText);
                            if (inputBar.inputController
                                    && typeof inputBar.inputController.saveMentionsForRoom === "function")
                                inputBar.inputController.saveMentionsForRoom(inputBar._draftRoomId);
                        }
                        // Blank _draftRoomId before clear() so onTextChanged doesn't clobber the saved draft
                        inputBar._draftRoomId = "";

                        messageInput.clear();
                        if (inputBar.inputController && inputBar.inputController.text)
                            messageInput.append(inputBar.inputController.text);

                        // Restore draft for the new room
                        inputBar._draftRoomId = room ? room.roomId : "";
                        if (inputBar._draftRoomId && messageInput.length === 0) {
                            var draft = Rooms.composerDraftForRoom(inputBar._draftRoomId);
                            if (draft)
                                messageInput.append(draft);
                        }

                        // Restore the entering room's mentions: its saved
                        // snapshot (preserving dismissals) if we have one this
                        // session, otherwise derived from the restored text. This
                        // is the mention half of the per-room draft handoff.
                        if (inputBar.inputController
                                && typeof inputBar.inputController.loadMentionsForRoom === "function")
                            inputBar.inputController.loadMentionsForRoom(inputBar._draftRoomId,
                                                                         messageInput.text);

                        completer.completerType = "";
                        inputBar.focusTextInputIfAllowed();
                        if (room) {
                            const roomId = room.roomId;
                            TimelineManager.markRoomSwitchPhase(roomId, "qml.message_input.room_changed");
                            Qt.callLater(function () {
                                TimelineManager.markRoomSwitchPhase(roomId, "qml.message_input.next_tick");
                            });
                        }
                    }

                    target: timelineView
                }
                Connections {
                    function onCompletionClicked(completion) {
                        messageInput.insertCompletion(completion);
                        completer.completerType = "";
                        popup.close();
                    }
                    function onCountChanged() {
                        // When the async search settles with zero results and the
                        // typed text already contains a space, close the popup.
                        // This handles the race where the Space key handler fires
                        // before the search has updated (Qt::QueuedConnection),
                        // while still allowing multi-word searches that DO produce
                        // results (e.g. ":bearded woman").
                        if (popup.opened && completer.completerType !== "command" && completer.count <= 0) {
                            var raw = messageInput.getText(messageInput.completerTriggeredAt, messageInput.cursorPosition);
                            if (raw.indexOf(' ') >= 0)
                                popup.close();
                        }
                    }
                    function onDismissed() {
                        completer.completerType = "";
                        popup.close();
                    }

                    target: completer
                }
                ComposerFormattingBar {
                    id: formattingBar

                    messageInput: messageInput
                    popupOpen: popup.opened
                }
                Popup {
                    id: popup

                    // Bumped before each open() to force the x/y/width bindings to
                    // re-run inputBar.mapToItem(). mapToItem isn't a tracked binding
                    // dep, so when an ancestor's transform changes (e.g. after a
                    // StackView push/pop transition leaves the chat page transform
                    // mid-flight) the cached binding value goes stale and the popup
                    // ends up positioned off-screen. Reading this counter inside the
                    // *RectInOverlay helpers makes them tracked.
                    property int _positionRefreshTick: 0

                    readonly property real popupMargin: Komai.paddingSmall
                    readonly property bool composerIsTall: textInput.height > textInput.singleLineHeight * 2.5

                    function clamp(value, minValue, maxValue) {
                        return Math.max(minValue, Math.min(value, maxValue));
                    }
                    function inputBarRectInOverlay() {
                        const _ = popup._positionRefreshTick;
                        if (!popup.parent)
                            return Qt.rect(0, 0, inputBar.width, inputBar.height);

                        const topLeft = inputBar.mapToItem(popup.parent, 0, 0);
                        return Qt.rect(topLeft.x, topLeft.y, inputBar.width, inputBar.height);
                    }
                    function anchorRectInOverlay() {
                        const _ = popup._positionRefreshTick;
                        const cursorRect = messageInput.positionToRectangle(messageInput.completerTriggeredAt);
                        if (!popup.parent)
                            return cursorRect;

                        const topLeft = messageInput.mapToItem(popup.parent, cursorRect.x, cursorRect.y);
                        return Qt.rect(topLeft.x, topLeft.y, cursorRect.width, cursorRect.height);
                    }
                    function preferredInlineWidth(availableWidth) {
                        const minWidth = completer.emptyStateMinWidth;
                        switch (completer.completerType) {
                        case "roomAliases":
                            return availableWidth;
                        case "user":
                        case "user-mxid":
                            if (availableWidth <= Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 40)))
                                return availableWidth;
                            return Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 34));
                        case "emoji":
                        case "customEmoji":
                            if (availableWidth <= Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 40)))
                                return availableWidth;
                            return Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 30));
                        case "command":
                            return availableWidth;
                        default:
                            return Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 30));
                        }
                    }

                    background: null
                    padding: 0
                    parent: Overlay.overlay
                    width: {
                        const containerRect = popup.inputBarRectInOverlay();
                        const availableWidth = Math.max(0, containerRect.width - popup.popupMargin * 2);
                        const preferredWidth = popup.preferredInlineWidth(availableWidth);
                        return availableWidth > 0 ? Math.min(preferredWidth, availableWidth) : preferredWidth;
                    }
                    x: {
                        const containerRect = popup.inputBarRectInOverlay();
                        const anchorRect = popup.anchorRectInOverlay();
                        const minX = containerRect.x + popup.popupMargin;
                        const maxX = containerRect.x + containerRect.width - popup.popupMargin - popup.width;
                        return Math.round(popup.clamp(anchorRect.x, minX, Math.max(minX, maxX)));
                    }
                    y: {
                        const anchorRect = popup.composerIsTall ? popup.anchorRectInOverlay() : popup.inputBarRectInOverlay();
                        const popupHeight = Math.max(popup.height, popup.implicitHeight);
                        const overlayHeight = popup.parent ? popup.parent.height : textInput.Window.height;
                        const minY = popup.popupMargin;
                        const maxY = Math.max(minY, overlayHeight - popupHeight - popup.popupMargin);
                        const gap = popup.composerIsTall ? 0 : popup.popupMargin;
                        const aboveY = anchorRect.y - popupHeight - gap;
                        const belowY = anchorRect.y + anchorRect.height;
                        const canOpenAbove = aboveY >= minY;
                        const canOpenBelow = belowY + popupHeight <= overlayHeight - popup.popupMargin;

                        if (!canOpenAbove && canOpenBelow)
                            return Math.round(belowY);

                        return Math.round(popup.clamp(aboveY, minY, maxY));
                    }

                    enter: Transition {
                        NumberAnimation {
                            duration: 100
                            from: 0
                            property: "opacity"
                            to: 1
                        }
                    }
                    exit: Transition {
                        NumberAnimation {
                            duration: 100
                            from: 1
                            property: "opacity"
                            to: 0
                        }
                    }

                    onClosed: completer.completerType = ""

                    contentItem: Completer {
                        id: completer

                        // The emoji picker's best match should read top-down
                        // like a normal dropdown, not bottom-anchored -- the
                        // other completer types (mentions, rooms, commands)
                        // keep the default bottomToTop behavior, unchanged.
                        bottomToTop: completerType !== "emoji"
                        commandValidationMessage: inputBar.inputController ? inputBar.inputController.commandValidationMessage : ""
                        commandValidationState: inputBar.inputController ? inputBar.inputController.commandValidationState : "none"
                        rowMargin: 2
                        rowSpacing: 0
                        roomId: room ? room.roomId : ""
                        roomAvatarUrl: room ? room.roomAvatarUrl : ""
                    }
                }
                Connections {
                    function onTextChanged(newText) {
                        const updatedText = newText !== undefined
                            ? String(newText)
                            : String((inputBar.inputController && inputBar.inputController.text) || "");
                        if (messageInput.text === updatedText)
                            return;
                        // Wholesale assignment to messageInput.text (or
                        // setPlainText under the hood) wipes the QTextDocument
                        // undo stack, so Ctrl+Z stops working from then on.
                        // Apply through the atomic QTextCursor path instead so
                        // external setText/updateState round-trips preserve
                        // prior typing history.
                        Komai.composerReplaceRange(messageInput.textDocument,
                                                    0, messageInput.length, updatedText);
                        messageInput.cursorPosition = updatedText.length;
                    }

                    ignoreUnknownSignals: true
                    target: inputBar.inputController
                }
                Connections {
                    function onEditChanged() {
                        inputBar.focusTextInput();
                    }
                    function onReplyChanged() {
                        inputBar.focusTextInput();
                    }
                    function onThreadChanged() {
                        inputBar.focusTextInput();
                    }

                    ignoreUnknownSignals: true
                    target: room
                }
                Connections {
                    function onFocusInput() {
                        inputBar.focusTextInput();
                    }

                    target: TimelineManager
                }
                MouseArea {
                    // Middle-click: paste-attachment workaround. Right-click:
                    // the spell-check context menu (intercepted here, ahead of
                    // the TextArea's own context menu). Left clicks aren't in
                    // acceptedButtons, so they fall through to the text editor
                    // for selection/cursor placement as usual.
                    acceptedButtons: Qt.MiddleButton | Qt.RightButton
                    // workaround for wrong cursor shape on some platforms
                    anchors.fill: parent
                    cursorShape: Qt.IBeamCursor

                    onPressed: mouse => {
                        if (mouse.button === Qt.RightButton) {
                            // Claim the press so the TextArea's own context menu
                            // doesn't fire; we pop ours on release (below) so
                            // there's no press-drag-release that could
                            // accidentally activate a menu item.
                            mouse.accepted = true;
                            return;
                        }
                        mouse.accepted = inputBar.inputController
                            ? inputBar.inputController.tryPasteAttachment(true)
                            : false;
                    }
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            composerSpellcheckCtx.show(Qt.point(mouse.x, mouse.y));
                    }
                }

                // Spell checking: draws the squiggles for the composer.
                SpellChecker {
                    id: composerSpellChecker

                    document: messageInput.textDocument
                    underlineColor: Komai.theme.error

                    // `textDocument` is a CONSTANT property — if it wasn't ready
                    // when the binding above first evaluated, re-assign once the
                    // component tree is complete.
                    Component.onCompleted: if (!composerSpellChecker.document) composerSpellChecker.document = messageInput.textDocument
                }

                SpellcheckContextMenu {
                    id: composerSpellcheckCtx
                    target: messageInput
                    spellChecker: composerSpellChecker
                }
            }
        }
        ComposerToolbarButton {
            id: stickerButton

            Layout.alignment: Qt.AlignRight | (inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter)
            KeyNavigation.backtab: messageInput
            KeyNavigation.tab: emojiButton.visible ? emojiButton : sendButton
            toolTipText: qsTr("Stickers")
            image: ":/icons/icons/ui/sticky-note-solid.svg"
            visible: showAllButtons && inputBar.allowStickers && !inputBar.hasVoiceRecording

            onClicked: {
                if (!inputBar.inputController || typeof inputBar.inputController.sticker !== "function")
                    return;

                stickerPopup.visible ? stickerPopup.close() : stickerPopup.show(stickerButton, room.roomId, function (row) {
                        inputBar.inputController.sticker(row);
                        TimelineManager.focusMessageInput();
                    }, inputBar);
            }

            StickerPicker {
                id: stickerPopup

                emoji: false
            }
        }
        ComposerToolbarButton {
            id: emojiButton

            Layout.alignment: Qt.AlignRight | (inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter)
            KeyNavigation.backtab: stickerButton.visible ? stickerButton : messageInput
            KeyNavigation.tab: sendButton
            toolTipText: qsTr("Emoji [Ctrl+.]")
            image: ":/icons/icons/ui/smile.svg"
            visible: inputBar.composerEnabled && !inputBar.hasVoiceRecording

            onClicked: inputBar.toggleEmojiPicker()

            StickerPicker {
                id: emojiPopup

                emoji: true
            }
        }
        ComposerToolbarButton {
            id: sendButton

            Layout.alignment: Qt.AlignRight | (inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter)
            Layout.rightMargin: Komai.paddingMedium
            KeyNavigation.backtab: emojiButton.visible ? emojiButton : (stickerButton.visible ? stickerButton : (inputBar.hasVoiceRecording && voiceButton.visible ? voiceButton : messageInput))
            toolTipText: qsTr("Send [%1]").arg(Settings.composerInputSendKeyLabel)
            // In a thread view, the active-state colour swaps to the thread's
            // user color so the composer reinforces the "you're sending into a
            // thread" cue that the timeline tint provides.
            buttonTextColor: inputBar.hasSendableContent
                ? (inputBar._threadActive ? inputBar._threadTintColor : palette.highlight)
                : palette.buttonText
            image: ":/icons/icons/ui/send.svg"
            mirrorImage: inputBar.LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft

            SequentialAnimation {
                id: shakeAnimation

                NumberAnimation { target: sendButton; property: "rotation"; to: -15; duration: 50 }
                NumberAnimation { target: sendButton; property: "rotation"; to: 15; duration: 80 }
                NumberAnimation { target: sendButton; property: "rotation"; to: -10; duration: 70 }
                NumberAnimation { target: sendButton; property: "rotation"; to: 10; duration: 60 }
                NumberAnimation { target: sendButton; property: "rotation"; to: 0; duration: 50 }
            }

            Timer {
                id: shakeTimer

                interval: 500
                repeat: false
                onTriggered: {
                    if (inputBar.hasSendableContent && Settings.uiMotionAnimationsEnabled)
                        shakeAnimation.start();
                }
            }

            Connections {
                target: messageInput
                function onTextChanged() {
                    if (messageInput.length > 0 && Settings.uiMotionAnimationsEnabled)
                        shakeTimer.restart();
                    else
                        shakeTimer.stop();
                }
            }

            Connections {
                target: inputBar.inputController
                ignoreUnknownSignals: true
                function onUploadsChanged() {
                    if (inputBar.hasUploads && Settings.uiMotionAnimationsEnabled)
                        shakeTimer.restart();
                }
            }

            onClicked: {
                if (inputBar.inputController)
                    inputBar.inputController.send();
            }
        }
    }

    // ── Top-edge resize handle ──
    // A thin grab strip at the top edge of the input bar (right under the
    // separator line). Dragging it up grows the input area so long
    // multi-paragraph drafts can be read at a glance; dragging back down
    // shrinks toward the natural content-driven size. Modelled on the
    // "Replying to ..." popup's resize strip — no visible affordance beyond
    // the cursor change. Taps pass through to the textarea underneath; the
    // handler only grabs once an actual drag starts.
    Item {
        id: inputResizeStrip

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 8
        z: 10
        visible: row.visible && textInput.visible && !inputBar.hasUploads

        KomaiCursorShape {
            anchors.fill: parent
            cursorShape: Qt.SizeVerCursor
        }

        DragHandler {
            id: inputResizeDrag

            property real startHeight: 0

            target: null
            xAxis.enabled: false
            yAxis.enabled: true
            grabPermissions: PointerHandler.CanTakeOverFromAnything
                | PointerHandler.ApprovesTakeOverByHandlersOfSameType

            onActiveChanged: {
                if (active)
                    startHeight = textInput.height;
            }
            onTranslationChanged: {
                if (!active)
                    return;
                // Dragging up (translation.y < 0) grows the input area.
                inputBar._setUserDesiredInputHeight(startHeight - translation.y);
            }
        }
    }

    Label {
        anchors.centerIn: parent
        color: palette.buttonText
        text: qsTr("You don't have permission to send messages in this room")
        visible: room ? (!inputBar.canSendTextMessages && !inputBar.roomIsSpace) : false
    }
    Label {
        anchors.centerIn: parent
        color: palette.buttonText
        text: qsTr("This room is a space (a collection of other rooms). Sending messages is disabled.")
        visible: inputBar.roomIsSpace
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        width: parent.width - Komai.paddingMedium * 2
    }
    Label {
        anchors.centerIn: parent
        color: palette.buttonText
        text: qsTr("Attach more files or send the upload")
        visible: inputBar.hasUploads && !inputBar.hasVoiceRecording
    }

    // Right-click on empty composer space shows the settings shortcut.
    // The textarea keeps its native cut/copy/paste menu, and buttons consume
    // the right-click on press so the handler only fires on bare areas.
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: composerSettingsMenu.popup()
    }

    Menu {
        id: composerSettingsMenu

        Component.onCompleted: {
            if (composerSettingsMenu.popupType != undefined)
                composerSettingsMenu.popupType = 2;
        }

        MenuItem {
            text: qsTr("Settings...") // Keep short: Qt may clip/elide longer menu item text
            icon.source: "qrc:/icons/icons/ui/settings.svg"

            onTriggered: MainWindow.showUserSettingsPage(UserSettingsModel.TabComposer)
        }
    }
}
