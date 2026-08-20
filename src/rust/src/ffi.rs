// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub(crate) use crate::composer_format::{
    toggle_block_prefix as composer_toggle_block_prefix,
    toggle_code as composer_toggle_code, toggle_inline_wrap as composer_toggle_inline_wrap,
    toggle_link as composer_toggle_link,
};
pub(crate) use crate::composer_mentions::composer_extract_mentions;
pub(crate) use crate::composer_trigger::trigger_at_word_boundary as composer_trigger_at_word_boundary;
pub(crate) use crate::emoji::emoji_only_visual_count;
pub(crate) use crate::logging::{init_logging, log_from_cpp};
pub(crate) use crate::matrix_backend::ffi::*;
pub(crate) use crate::settings::ffi::*;
pub(crate) use crate::settings::profile::SettingsProfileHandle;
pub(crate) use crate::spellcheck::{
    spellcheck_add_word, spellcheck_check_block, spellcheck_discover_dictionaries,
    spellcheck_register_builtin_dictionary, spellcheck_set_config, spellcheck_suggest,
};
pub(crate) use crate::syntax_highlight::{highlight_formatted_code_blocks, highlight_raw_json};
pub(crate) use crate::transcription::ffi::{
    transcription_clear_global_api_key, transcription_clear_room_api_key,
    transcription_load_global_api_key, transcription_load_room_api_key,
    transcription_realtime_cancel, transcription_realtime_commit,
    transcription_realtime_drain_events, transcription_realtime_push_audio,
    transcription_realtime_sample_rate_hz, transcription_resolve_for_room,
    transcription_run_batch, transcription_run_realtime, transcription_save_global_api_key,
    transcription_save_room_api_key,
};

pub(crate) fn html_sanitize(html: &str) -> String {
    crate::html_processor::sanitize_html(html)
}

pub(crate) fn html_linkify(html: &str) -> String {
    crate::html_processor::linkify_html(html)
}

pub(crate) fn html_mark_search_matches(html: &str, query: &str) -> String {
    crate::html_processor::mark_search_matches(html, query)
}

pub(crate) fn format_body_html(
    body: &str,
    formatted_body: &str,
    pill_avatars: &Vec<HtmlPillAvatar>,
    pill_avatar_size: u32,
    code_background: &str,
    syntax_highlight: bool,
) -> String {
    crate::html_processor::format_body_html(
        body,
        formatted_body,
        pill_avatars,
        pill_avatar_size,
        code_background,
        syntax_highlight,
    )
}
pub(crate) use crate::image_ops::lanczos_resize_rgba;
pub(crate) use crate::serverlist::entries as serverlist_entries;
pub(crate) use crate::theme::base16::parse_base16_yaml as theme_parse_base16_yaml;
pub(crate) use crate::theme::builtins::builtin_themes as theme_builtin_themes;
pub(crate) use crate::theme::external::parse_external_theme as theme_parse_external_theme;

pub(crate) fn blurhash_decode(hash: &str, width: u32, height: u32) -> Vec<u8> {
    blurhash::decode(hash, width, height, 1.0).unwrap_or_default()
}

#[cxx::bridge(namespace = "komai::rust")]
mod bridge {
    enum MatrixFfiBlockingThreadPolicy {
        AllowUiThread,
        RequireWorkerThread,
    }

    enum MatrixFfiCallerThread {
        AppUiThread,
        WorkerThread,
    }

    struct MatrixFfiBlockingContext {
        thread_policy: MatrixFfiBlockingThreadPolicy,
        caller_thread: MatrixFfiCallerThread,
    }

    struct ResolveResult {
        base_url: String,
    }

    struct MatrixSdkPaths {
        profile_data_root: String,
        profile_cache_root: String,
        matrix_data_root: String,
        matrix_cache_root: String,
        state_store_root: String,
        cache_root: String,
        event_cache_root: String,
        media_cache_root: String,
    }

    struct MatrixRestorePreview {
        has_session: bool,
        session_source: String,
        auth_type: String,
        homeserver_url: String,
        user_id: String,
        device_id: String,
        state_store_root: String,
        cache_root: String,
    }

    struct SettingsStartupSnapshot {
        ui_scale_factor: f32,
    }

    struct SettingsConfigOverview {
        ui_scale_factor: f32,
        theme_slug: String,
        uses_file_secrets_provider: bool,
    }

    struct SettingsProfileOverview {
        theme_slug: String,
        uses_file_secrets_provider: bool,
        user_id: String,
        homeserver: String,
    }

    struct HtmlPillAvatar {
        user_id: String,
        // Most recent mxc:// avatar URL captured for this sender. Empty when
        // the latest event we have for them carries no avatar (e.g. they
        // never set one, or they cleared it). When empty, `fallback_url`
        // is used to render a default avatar instead.
        mxc_url: String,
        // Fully-formed `image://default-avatar/{userid}?radius=...&...`
        // URL prepared on the C++ side, where the per-user colour, theme,
        // and avatar-style settings are all accessible. Rust appends an
        // `&avatarSize=N` query at decoration time.
        fallback_url: String,
    }

    struct SettingsOptionalString {
        has_value: bool,
        value: String,
    }

    #[derive(Clone, Debug, PartialEq, Eq)]
    struct SettingsStringMapEntry {
        key: String,
        value: String,
    }

    #[derive(Debug, PartialEq, Eq)]
    struct SettingsStringListMapEntry {
        key: String,
        values: Vec<String>,
    }

    #[derive(Debug, PartialEq, Eq)]
    struct SettingsBoolMapEntry {
        key: String,
        value: bool,
    }

    struct SettingsSecretsPayload {
        access_token: String,
        secrets: Vec<SettingsStringMapEntry>,
        had_stale_values: bool,
    }

    /// Mirror of `transcription::TranscriptionErrorCode`. Includes a
    /// no-error sentinel `Ok` so callers can branch off a single value
    /// without the result-success bool.
    enum TranscriptionErrorCodeFfi {
        Ok,
        NotConfigured,
        Network,
        Unauthorized,
        ServerError,
        InvalidResponse,
        InvalidAudio,
        Internal,
    }

    struct TranscriptionBatchResult {
        success: bool,
        text: String,
        error_code: TranscriptionErrorCodeFfi,
        error_message: String,
    }

    /// Outcome of starting a realtime session. `job_id == 0` means the
    /// session could not even be started (typically because the config is
    /// not ready) and the C++ side should surface the error immediately
    /// without setting up a poll loop.
    struct TranscriptionRealtimeStartResult {
        job_id: i64,
        accepted: bool,
        error_code: TranscriptionErrorCodeFfi,
        error_message: String,
    }

    /// Discriminator for [`TranscriptionRealtimeEvent`]. The C++ side
    /// branches on this when draining the event queue.
    enum TranscriptionRealtimeEventKindFfi {
        /// Incremental tentative text for the current utterance. For
        /// `gpt-4o-(mini-)transcribe` deltas are append-only per turn; for
        /// `whisper-1` the first delta carries the full final text in one
        /// shot.
        Delta,
        /// Polished final transcript for ONE utterance. With server VAD
        /// active, multiple `Completed` events can land in a single
        /// session (one per VAD-detected segment). Composer replaces the
        /// current tentative range with the polished text and prepares a
        /// fresh range for any subsequent deltas — it does NOT use this
        /// event to decide the session has ended (see `Closed`).
        Completed,
        /// Session ended in failure. Composer surfaces the error in the
        /// banner. No further events follow this for the same job id.
        Failed,
        /// Session ended cleanly. Always the last event of a successful
        /// session. Composer uses this to clear any leftover tentative
        /// range and reset state to idle.
        Closed,
    }

    struct TranscriptionRealtimeEvent {
        kind: TranscriptionRealtimeEventKindFfi,
        /// Holds the delta text (kind = Delta) or the final transcript
        /// (kind = Completed). Empty for Failed.
        text: String,
        /// Only meaningful when kind = Failed.
        error_code: TranscriptionErrorCodeFfi,
        /// Only meaningful when kind = Failed.
        error_message: String,
    }

    /// Effective transcription config for a given room. Used by the UI to
    /// decide whether long-press Space activates recording or surfaces the
    /// "needs configuration" hint.
    ///
    /// The composer-side master toggle (`composer.input.transcription.enabled`)
    /// is consulted independently by QML — it is *not* part of this struct.
    struct TranscriptionResolvedConfig {
        provider: String,
        api_url: String,
        has_api_key: bool,
        /// Heuristic: is this URL a cloud provider that almost certainly
        /// needs an api key? (OpenAI cloud, Deepgram, etc.) Local servers
        /// usually return `false` here even when no key is set.
        needs_api_key: bool,
        model: String,
        language: String,
        prompt: String,
        is_ready: bool,
    }

    struct SettingsConfigUiSection {
        scale_factor: f32,
        theme_slug: String,
        theme_mode: String,
        font_size_pt: f64,
        font_family: String,
        font_emoji_family: String,
        motion_animations_enabled: bool,
        layout_density: String,
        avatars_circular: bool,
        scrollbar_policy: String,
        default_avatar_style: String,
        language: String,
    }

    struct SettingsConfigNavigationRoomListSection {
        show_last_message_time: bool,
        last_message_preview: String,
        show_unread_indicators: bool,
        sort: String,
        opening_policy: String,
    }

    struct SettingsConfigNavigationCommunitiesSection {
        show_unread_indicators: bool,
        filter_favourites: bool,
        filter_people: bool,
        filter_bots: bool,
        filter_groups: bool,
        filter_server_notices: bool,
        filter_low_priority: bool,
    }

    struct SettingsConfigNavigationTabsSection {
        auto_hide_with_single_tab: bool,
        show_pin_button: String,
        pinned_tab_label: String,
        tab_label: String,
        preferred_width_px: i32,
        minimum_width_px: i32,
        max_recently_closed_timelines: i32,
    }

    struct SettingsConfigNavigationSection {
        room_list: SettingsConfigNavigationRoomListSection,
        communities: SettingsConfigNavigationCommunitiesSection,
        tabs: SettingsConfigNavigationTabsSection,
    }

    struct SettingsConfigTimelineHiddenEventsSection {
        global: Vec<String>,
        by_room: Vec<SettingsStringListMapEntry>,
    }

    struct SettingsConfigTimelineMessagesSection {
        style: String,
        layout_positioning: String,
        user_color_coding_policy: String,
        layout_avatar_size: String,
        layout_show_own_avatar: bool,
        layout_max_width_percent: i32,
        layout_adaptive_positioning_breakpoint_px: i32,
        sender_username: String,
        emoji_only_enlarge: bool,
        hover_highlight: bool,
        drag_select: bool,
    }

    struct SettingsConfigTimelineFormattedSection {
        code_syntax_highlighting: bool,
    }

    struct SettingsConfigTimelineTypingSection {
        show_enabled: bool,
    }

    struct SettingsConfigTimelineReadReceiptsSection {
        // Whether to send a *public* read receipt as you read messages by
        // default. When false, a private (`m.read.private`) receipt is sent
        // instead, so the room still clears its unread state for the user
        // but other participants don't see the receipt.  Per-room overrides
        // live in `by_room`; the resolved value falls back to this global
        // when a room is not in the map.
        global: bool,
        by_room: Vec<SettingsBoolMapEntry>,
    }

    struct SettingsConfigTimelineMessageActionsSection {
        activation_policy: String,
        pinned_reactions: String,
    }

    struct SettingsConfigTimelineMediaSection {
        effects_enabled: bool,
        animate_on_hover: bool,
        image_display: String,
        open_images_external: bool,
        open_videos_external: bool,
        autoplay_gif_videos: bool,
        open_audio_external: bool,
        default_audio_playback_speed: f64,
    }

    struct SettingsConfigTimelineThreadsSection {
        collapse_replies_global: bool,
        collapse_replies_by_room: Vec<SettingsBoolMapEntry>,
    }

    struct SettingsConfigTimelineDateDividersSection {
        enabled: bool,
    }

    struct SettingsConfigTimelineRoomHeaderSection {
        button_labels: String,
    }

    struct SettingsConfigTimelineSection {
        messages: SettingsConfigTimelineMessagesSection,
        formatted: SettingsConfigTimelineFormattedSection,
        typing: SettingsConfigTimelineTypingSection,
        read_receipts: SettingsConfigTimelineReadReceiptsSection,
        message_actions: SettingsConfigTimelineMessageActionsSection,
        media: SettingsConfigTimelineMediaSection,
        hidden_events: SettingsConfigTimelineHiddenEventsSection,
        threads: SettingsConfigTimelineThreadsSection,
        date_dividers: SettingsConfigTimelineDateDividersSection,
        room_header: SettingsConfigTimelineRoomHeaderSection,
    }

    struct SettingsConfigSecretsSection {
        provider: String,
    }

    struct SettingsConfigDesktopNotificationsSection {
        enabled: bool,
        attention_on_incoming: bool,
        message_content_policy: String,
    }

    struct SettingsConfigDesktopAttentionWindowTitleSection {
        enabled: bool,
    }

    struct SettingsConfigDesktopAttentionAppBadgeSection {
        enabled: bool,
    }

    struct SettingsConfigDesktopAttentionSection {
        window_title: SettingsConfigDesktopAttentionWindowTitleSection,
        app_badge: SettingsConfigDesktopAttentionAppBadgeSection,
    }

    struct SettingsConfigDesktopSystemTraySection {
        enabled: bool,
        autostart: bool,
        icon_style: String,
    }

    struct SettingsConfigDesktopWindowFocusBlurSection {
        enabled: bool,
        delay_seconds: i32,
    }

    struct SettingsConfigDesktopSection {
        notifications: SettingsConfigDesktopNotificationsSection,
        attention: SettingsConfigDesktopAttentionSection,
        system_tray: SettingsConfigDesktopSystemTraySection,
        window_focus_blur: SettingsConfigDesktopWindowFocusBlurSection,
    }

    struct SettingsConfigNetworkEncryptionSection {
        only_verified_users: bool,
        share_with_trusted: bool,
        key_backup: bool,
    }

    struct SettingsConfigCallsLegacySection {
        enabled: bool,
    }

    struct SettingsConfigCallsElementSection {
        enabled: bool,
    }

    struct SettingsConfigCallsRelaySection {
        use_fallback_server: bool,
    }

    struct SettingsConfigCallsDevicesSection {
        microphone: String,
        camera: String,
        camera_resolution: String,
        camera_frame_rate: String,
    }

    struct SettingsConfigCallsAudioSection {
        ringtone: String,
    }

    struct SettingsConfigCallsScreenshareSection {
        frame_rate: i32,
        picture_in_picture: bool,
        include_remote_video: bool,
        show_cursor: bool,
    }

    struct SettingsConfigCallsSection {
        legacy: SettingsConfigCallsLegacySection,
        element: SettingsConfigCallsElementSection,
        relay: SettingsConfigCallsRelaySection,
        devices: SettingsConfigCallsDevicesSection,
        audio: SettingsConfigCallsAudioSection,
        screenshare: SettingsConfigCallsScreenshareSection,
    }

    struct SettingsConfigNetworkSection {
        encryption: SettingsConfigNetworkEncryptionSection,
        presence_status_policy: String,
        tls_enable_certificate_validation: bool,
        mrs_enabled: bool,
        mrs_server_name: String,
        http3_enabled: bool,
    }

    struct SettingsConfigIntegrationsSection {
        dbus_api_access: String,
        browser_command: String,
        // Non-secret transcription fields. `api_key` is intentionally NOT
        // here — it lives in the secrets backend, never in `config.yml`.
        // Storage form: `transcription_provider` is a token string
        // (e.g. "openai_batch"); the rest are raw strings, empty meaning
        // "absent / fall back to default".
        transcription_provider: String,
        transcription_api_url: String,
        transcription_model: String,
        transcription_language: String,
        transcription_prompt: String,
        // Per-room transcription overrides. Each entry's `has_*` flag
        // distinguishes "field overridden to empty" (e.g. `language: ""`
        // = autodetect) from "field not overridden / inherit global". An
        // entry with no `has_*` set is dropped from the YAML round-trip.
        transcription_by_room: Vec<SettingsConfigTranscriptionByRoomEntry>,
    }

    /// Per-room transcription override snapshot entry. `key` is the raw
    /// Matrix room id. Each field is gated by `has_*` so the round-trip
    /// can preserve "set to empty" vs "not set".
    struct SettingsConfigTranscriptionByRoomEntry {
        key: String,
        has_provider: bool,
        provider: String,
        has_api_url: bool,
        api_url: String,
        has_model: bool,
        model: String,
        has_language: bool,
        language: String,
        has_prompt: bool,
        prompt: String,
    }

    struct SettingsConfigComposerSection {
        input_markdown_to_html_enabled: bool,
        input_send_key: String,
        input_auto_replace_emoji: String,
        input_emoji_preferred_gender: String,
        input_emoji_preferred_skin_tone: String,
        input_inline_emoji_picker_enabled: bool,
        input_inline_room_picker_enabled: bool,
        input_inline_user_picker_enabled: bool,
        input_selection_formatting_toolbar_enabled: bool,
        input_transcription_enabled: bool,
        input_spellcheck_enabled: bool,
        input_spellcheck_languages: Vec<String>,
        attachments_strip_image_metadata: bool,
        // Whether to send a typing notice to other users by default.
        // Per-room overrides live in `typing_send_by_room`; the resolved
        // value falls back to this global when a room is not in the map.
        typing_send_global: bool,
        typing_send_by_room: Vec<SettingsBoolMapEntry>,
    }

    struct SettingsConfigSnapshot {
        ui: SettingsConfigUiSection,
        navigation: SettingsConfigNavigationSection,
        timeline: SettingsConfigTimelineSection,
        secrets: SettingsConfigSecretsSection,
        desktop: SettingsConfigDesktopSection,
        calls: SettingsConfigCallsSection,
        network: SettingsConfigNetworkSection,
        integrations: SettingsConfigIntegrationsSection,
        composer: SettingsConfigComposerSection,
    }

    struct SettingsLoadedConfig {
        ui: SettingsConfigUiSection,
        navigation: SettingsConfigNavigationSection,
        timeline: SettingsConfigTimelineSection,
        secrets: SettingsConfigSecretsSection,
        desktop: SettingsConfigDesktopSection,
        calls: SettingsConfigCallsSection,
        network: SettingsConfigNetworkSection,
        integrations: SettingsConfigIntegrationsSection,
        composer: SettingsConfigComposerSection,
        source_exists: bool,
        source_version: i32,
        migrated_version: i32,
        had_future_version: bool,
        had_unsupported_path: bool,
        should_write_back: bool,
        serialized_yaml: String,
    }

    struct SettingsLoadedSession {
        user_id: String,
        device_id: String,
        homeserver: String,
        source_exists: bool,
        source_version: i32,
        migrated_version: i32,
        had_future_version: bool,
        had_unsupported_path: bool,
        should_write_back: bool,
        serialized_yaml: String,
    }

    struct SettingsLoadedState {
        window_width: i32,
        window_height: i32,
        navigation_room_list_width_px: i32,
        navigation_communities_width_px: i32,
        current_filter_id: String,
        current_room_id: String,
        global_excludes: Vec<String>,
        unread_indicators_hidden_filters: Vec<String>,
        hidden_pins: Vec<String>,
        hidden_widgets: Vec<String>,
        collapsed_spaces: Vec<String>,
        hidden_spaces: Vec<String>,
        open_tabs: Vec<String>,
        pinned_tabs: Vec<String>,
        composer_drafts_by_room: Vec<SettingsStringMapEntry>,
        sponsoring_status: String,
        desktop_system_tray_first_close_prompted: bool,
        source_exists: bool,
        source_version: i32,
        migrated_version: i32,
        had_future_version: bool,
        had_unsupported_path: bool,
        should_write_back: bool,
        serialized_yaml: String,
    }

    struct SettingsLoadedProfile {
        config: SettingsLoadedConfig,
        session: SettingsLoadedSession,
        state: SettingsLoadedState,
        secrets: SettingsSecretsPayload,
        uses_file_secrets_provider: bool,
        startup_secrets_provider_changed: bool,
        secrets_provider_fallback_warning_visible: bool,
    }

    struct SettingsProfileFlushResult {
        config_attempted: bool,
        config_saved: bool,
        session_attempted: bool,
        session_saved: bool,
        secrets_attempted: bool,
        secrets_saved: bool,
        state_attempted: bool,
        state_saved: bool,
    }

    struct SettingsStateSnapshot {
        window_width: i32,
        window_height: i32,
        navigation_room_list_width_px: i32,
        navigation_communities_width_px: i32,
        current_filter_id: String,
        current_room_id: String,
        global_excludes: Vec<String>,
        unread_indicators_hidden_filters: Vec<String>,
        hidden_pins: Vec<String>,
        hidden_widgets: Vec<String>,
        collapsed_spaces: Vec<String>,
        hidden_spaces: Vec<String>,
        open_tabs: Vec<String>,
        pinned_tabs: Vec<String>,
        composer_drafts_by_room: Vec<SettingsStringMapEntry>,
        sponsoring_status: String,
        desktop_system_tray_first_close_prompted: bool,
    }

    struct ThemeUserColorSlotData {
        background: String,
        text: String,
        secondary_text: String,
        link: String,
    }

    struct ThemePaletteData {
        window: String,
        window_text: String,
        base: String,
        alternate_base: String,
        text: String,
        bright_text: String,
        button: String,
        button_text: String,
        light: String,
        mid: String,
        dark: String,
        highlight: String,
        highlighted_text: String,
        link: String,
        tool_tip_base: String,
        tool_tip_text: String,
        attention: String,
        attention_text: String,
        success: String,
        warning: String,
        error: String,
    }

    struct ThemeExternalDefinition {
        name: String,
        variant: String,
        palette: ThemePaletteData,
        user_color_self: ThemeUserColorSlotData,
        user_color_others: Vec<ThemeUserColorSlotData>,
    }

    struct ThemeExternalParseResult {
        has_theme: bool,
        error_message: String,
        theme: ThemeExternalDefinition,
    }

    struct ThemeBuiltinEntry {
        slug: String,
        sort_order: i32,
        theme: ThemeExternalDefinition,
    }

    struct ThemeBuiltinListResult {
        themes: Vec<ThemeBuiltinEntry>,
        errors: Vec<String>,
    }

    struct ThemeBase16PaletteData {
        base00: String,
        base01: String,
        base02: String,
        base03: String,
        base04: String,
        base05: String,
        base06: String,
        base07: String,
        base08: String,
        base09: String,
        base0a: String,
        base0b: String,
        base0c: String,
        base0d: String,
        base0e: String,
        base0f: String,
    }

    struct ThemeBase16ParseResult {
        has_document: bool,
        error_message: String,
        name: String,
        author: String,
        palette: ThemeBase16PaletteData,
    }

    struct ServerListEntry {
        name: String,
        client_domain: String,
        description: String,
        homepage: String,
        using_vanilla_reg: bool,
        languages: Vec<String>,
        software: String,
        staff_jur: String,
        rules: String,
        privacy: String,
        captcha: bool,
        email: bool,
        features: Vec<String>,
        sliding_sync: bool,
        reg_link: String,
        reg_note: String,
        rank: i32,
        category: String,
        editorial: String,
        featured: bool,
    }

    struct ServerListResult {
        entries: Vec<ServerListEntry>,
        error_message: String,
    }

    // ----- Spell checking (see crate::spellcheck) -----------------------

    struct SpellcheckDictionaryEntry {
        // Normalised locale code, e.g. "en_US", "bg_BG", "de_DE".
        code: String,
        // Filesystem path of the .dic file; empty for the bundled dictionary.
        path: String,
        // True for the bundled en_US dictionary (not removable in the UI).
        builtin: bool,
    }

    // A misspelled span within a checked block, in UTF-16 code units from the
    // start of the block — the units QString / QTextCursor index by.
    struct SpellcheckRange {
        start_utf16: u32,
        length_utf16: u32,
    }

    struct SpellcheckBlockResult {
        ranges: Vec<SpellcheckRange>,
        // Whether the document is still inside an unterminated triple-backtick
        // fenced code block after this block. The C++ highlighter threads this
        // through QSyntaxHighlighter block state into the next block's
        // `in_code_fence_before`.
        in_code_fence_after: bool,
    }

    struct SpellcheckSuggestionGroup {
        // Locale code of the dictionary the suggestions came from; C++ maps it
        // to a human language name and only shows a header when there are >=2
        // groups.
        language_code: String,
        suggestions: Vec<String>,
    }

    // A user mention recovered from composer draft text. `source` is the link
    // substring as it appears in the text, so the composer can prune the
    // mention when that text is edited away.
    struct ComposerMentionMatch {
        user_id: String,
        source: String,
    }

    // Result of a composer formatting toggle (bold / italic / code / quote /
    // link). Indices are UTF-16 code units — the units QString/TextArea index
    // by. `applied = false` is the no-op sentinel.
    struct ComposerTransformResult {
        applied: bool,
        replace_start_utf16: u32,
        replace_end_utf16: u32,
        replacement_text: String,
        new_sel_start_utf16: u32,
        new_sel_end_utf16: u32,
    }

    struct RegistrationFlowStages {
        stages: Vec<String>,
    }

    struct RegistrationTermsPolicy {
        id: String,
        version: String,
        name: String,
        url: String,
    }

    struct RegistrationProbeResult {
        registration_id: u64,
        homeserver_url: String,
        session: String,
        chosen_flow_stages: Vec<String>,
        all_flows: Vec<RegistrationFlowStages>,
        terms_policies: Vec<RegistrationTermsPolicy>,
    }

    struct RegistrationUsernameResult {
        available: bool,
    }

    struct RegistrationSubmitResult {
        completed: bool,
        user_id: String,
        access_token: String,
        device_id: String,
        homeserver_url: String,
        session: String,
        remaining_stages: Vec<String>,
        completed_stages: Vec<String>,
        terms_policies: Vec<RegistrationTermsPolicy>,
    }

    struct RegistrationEmailTokenResult {
        sid: String,
    }

    struct MatrixBackendHandleInfo {
        handle_id: u64,
        has_session: bool,
        auth_type: String,
        homeserver_url: String,
        user_id: String,
        device_id: String,
    }

    /// One room state event's content, as raw JSON. `exists` is false when the
    /// room has no such state, which is an answer rather than an error.
    struct MatrixRoomStateEvent {
        exists: bool,
        content_json: String,
    }

    struct MatrixOwnProfile {
        display_name: String,
        avatar_url: String,
    }

    struct MatrixOwnPresence {
        state: String,
        status_message: String,
    }

    struct MatrixMediaDownloadProgress {
        received_bytes: u64,
        // 0 while the total is unknown (no Content-Length and no event info size).
        total_bytes: u64,
    }

    struct MatrixTurnServerInfo {
        username: String,
        password: String,
        uris: Vec<String>,
        ttl_seconds: u64,
    }

    struct MatrixRecoveryStatus {
        state: String,
        has_devices_to_verify_against: bool,
        own_device_is_verified: bool,
        has_unverified_own_devices: bool,
    }

    struct MatrixSetupRecoveryResult {
        recovery_key: String,
    }

    struct MatrixRoomKeyImportCounts {
        imported: u64,
        total: u64,
    }

    struct MatrixResetEncryptionIdentityResult {
        completed: bool,
        auth_type: String,
        approval_url: String,
    }

    struct MatrixDeviceSignOutResult {
        completed: bool,
        auth_type: String,
        approval_url: String,
    }

    struct MatrixVerificationSession {
        flow_id: String,
        user_id: String,
        device_id: String,
        state: String,
        error: String,
        sender: bool,
        is_self_verification: bool,
        is_multi_device_verification: bool,
        sas_numbers: Vec<u16>,
    }

    struct MatrixUserDevice {
        device_id: String,
        display_name: String,
        verification_state: String,
        last_seen_ip: String,
        last_seen_ts: u64,
    }

    struct MatrixUserVerificationState {
        has_master_key: bool,
        user_trust: String,
        devices: Vec<MatrixUserDevice>,
    }

    struct MatrixUserProfile {
        display_name: String,
        avatar_url: String,
    }

    struct MatrixDirectoryUser {
        display_name: String,
        user_id: String,
        avatar_url: String,
    }

    struct MatrixPublicRoomDirectoryEntry {
        room_id: String,
        room_server_name: String,
        display_name: String,
        avatar_url: String,
        topic: String,
        canonical_alias: String,
        member_count: u64,
        is_world_readable: bool,
        is_space: bool,
    }

    struct MatrixPublicRoomDirectoryPage {
        rooms: Vec<MatrixPublicRoomDirectoryEntry>,
        next_batch: String,
        total_room_count_estimate: i32,
    }

    struct MatrixRoomSummary {
        room_id: String,
        latest_event_id: String,
        display_name: String,
        avatar_url: String,
        topic: String,
        room_alias: String,
        last_message: String,
        last_message_kind: String,
        last_message_sender_id: String,
        last_message_sender_display_name: String,
        tags: Vec<String>,
        parent_space_room_ids: Vec<String>,
        direct_chat_other_user_id: String,
        is_invite: bool,
        inviter_user_id: String,
        inviter_display_name: String,
        inviter_avatar_url: String,
        invite_reason: String,
        is_space: bool,
        is_direct: bool,
        is_bot_room: bool,
        is_encrypted: bool,
        is_public: bool,
        member_count: u64,
        unread_message_count: u64,
        notification_count: u64,
        highlight_count: u64,
        is_marked_unread: bool,
        has_active_call: bool,
        active_call_participant_count: u64,
        timestamp: u64,
    }

    struct MatrixRoomPreviewUpdate {
        room_id: String,
        latest_event_id: String,
        last_message: String,
        last_message_kind: String,
        last_message_sender_id: String,
        last_message_sender_display_name: String,
        timestamp: u64,
    }

    struct MatrixNotificationRequest {
        room_id: String,
        event_id: String,
    }

    struct MatrixNotificationItem {
        room_id: String,
        event_id: String,
        replacement_event_id: String,
        room_name: String,
        avatar_url: String,
        sender_display_name: String,
        notification_kind: String,
        plain_body: String,
        formatted_body: String,
        media_mxc_url: String,
        is_reply: bool,
        is_emote: bool,
        is_encrypted: bool,
        contains_spoiler: bool,
        has_inline_image: bool,
        play_sound: bool,
    }

    struct MatrixImagePackImage {
        shortcode: String,
        body: String,
        url: String,
        is_emote: bool,
        is_sticker: bool,
    }

    struct MatrixImagePack {
        source_room_id: String,
        state_key: String,
        display_name: String,
        avatar_url: String,
        attribution: String,
        is_emote_pack: bool,
        is_sticker_pack: bool,
        from_space: bool,
        is_globally_enabled: bool,
        images: Vec<MatrixImagePackImage>,
    }

    struct MatrixRoomSettings {
        room_id: String,
        room_name: String,
        room_topic: String,
        room_avatar_url: String,
        room_version: String,
        member_count: u64,
        notifications: i32,
        join_rule: String,
        history_visibility: String,
        allowed_room_ids: Vec<String>,
        parent_space_room_ids: Vec<String>,
        guest_access: bool,
        is_encrypted: bool,
        can_change_name: bool,
        can_change_topic: bool,
        can_change_avatar: bool,
        can_change_join_rules: bool,
        can_change_history_visibility: bool,
        can_change_encryption: bool,
        can_upgrade_room: bool,
    }

    struct MatrixRoomVersionsCapability {
        default_version: String,
        stable: Vec<String>,
    }

    struct MatrixRoomAliases {
        canonical_alias: String,
        alt_aliases: Vec<String>,
        published_aliases: Vec<String>,
    }

    struct MatrixRoomMember {
        user_id: String,
        display_name: String,
        avatar_url: String,
        power_level: i64,
        is_invited: bool,
    }

    struct MatrixPowerLevelEntry {
        key: String,
        level: i64,
    }

    struct MatrixRoomPowerLevels {
        room_version: String,
        creators: Vec<String>,
        events: Vec<MatrixPowerLevelEntry>,
        users: Vec<MatrixPowerLevelEntry>,
        ban: i64,
        events_default: i64,
        invite: i64,
        kick: i64,
        redact: i64,
        state_default: i64,
        users_default: i64,
    }

    struct MatrixChildSpaceEntry {
        room_id: String,
        display_name: String,
        avatar_url: String,
        power_levels: MatrixRoomPowerLevels,
    }

    struct MatrixRoomRedactionPermissions {
        can_redact_own: bool,
        can_redact_other: bool,
    }

    struct MatrixRawEventDialogData {
        cleartext_json: String,
        cleartext_error: String,
        wire_json: String,
        wire_error: String,
        wire_matches_cleartext: bool,
        body: String,
        formatted_body: String,
    }

    struct MatrixEventContentForForwarding {
        event_type: String,
        content_json: String,
    }

    struct MatrixCallSessionDescription {
        sdp: String,
        /// "offer" or "answer"
        sdp_type: String,
    }

    struct MatrixCallIceCandidate {
        sdp_mid: String,
        sdp_m_line_index: u16,
        candidate: String,
    }

    struct MatrixCallInviteEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        lifetime: u32,
        invitee: String,
        offer: MatrixCallSessionDescription,
    }

    struct MatrixCallCandidatesEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        candidates: Vec<MatrixCallIceCandidate>,
    }

    struct MatrixCallAnswerEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        answer: MatrixCallSessionDescription,
    }

    struct MatrixCallHangUpEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        /// One of: "ice_failed", "invite_timeout", "ice_timeout",
        /// "user_hangup", "user_media_failed", "user_busy",
        /// "unknown_error", "user", or "" for default (UserHangUp)
        reason: String,
    }

    struct MatrixCallSelectAnswerEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        selected_party_id: String,
    }

    struct MatrixCallRejectEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
    }

    struct MatrixCallNegotiateEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        lifetime: u32,
        description: MatrixCallSessionDescription,
    }

    // MatrixRTC (MSC4075) incoming call notification, forwarded to the C++
    // ElementCallController which owns the ring decision + UI.
    struct MatrixRtcNotificationEvent {
        room_id: String,
        // The notification event id (referenced when sending an m.rtc.decline).
        event_id: String,
        sender_id: String,
        // "ring" (audible) or "notification" (silent); other values are ignored.
        notification_type: String,
        // The notification was sent by our own user (do not ring ourselves).
        is_self: bool,
        // We are addressed by the notification's m.mentions (listed personally
        // OR via a room-wide @room), or it had none.
        mentions_me: bool,
        // How long the notification rings for, in milliseconds (MSC4075 lifetime).
        lifetime_ms: u64,
        // Unix-epoch ms at which ringing should stop (lifetime applied to the
        // sender/server timestamp), already clock-skew corrected.
        expires_at_ms: u64,
        // The room's notification mode: 0 = mute, 1 = mentions/keywords only,
        // 2 = all messages. Lets the desktop-notification decision honour the
        // room's notify setting for silent (group) call notifications.
        notification_mode: i32,
    }

    // MatrixRTC (MSC4310) call decline, forwarded so our other devices stop
    // ringing once one device declines.
    struct MatrixRtcDeclineEvent {
        room_id: String,
        // The m.rtc.notification this decline references.
        notification_event_id: String,
        sender_id: String,
        // The decline was sent by our own user (possibly another device).
        is_self: bool,
    }

    struct MatrixReadReceiptEntry {
        user_id: String,
        display_name: String,
        avatar_url: String,
        timestamp: u64,
    }

    struct MatrixTimelineItem {
        item_id: String,
        event_id: String,
        transaction_id: String,
        delivery_state: String,
        send_error: String,
        is_recoverable: bool,
        thread_id: String,
        is_thread_root: bool,
        thread_reply_count: u32,
        sender_id: String,
        sender_display_name: String,
        sender_avatar_url: String,
        body: String,
        formatted_body: String,
        reply_event_id: String,
        reply_sender_id: String,
        reply_sender_display_name: String,
        reply_item_kind: String,
        reply_matrix_event_type: String,
        reply_body: String,
        reply_formatted_body: String,
        reply_media_url: String,
        reply_thumbnail_url: String,
        reply_file_name: String,
        reply_mime_type: String,
        reply_media_width: u64,
        reply_media_height: u64,
        reply_media_duration_ms: u64,
        reply_media_size_bytes: u64,
        reply_blurhash: String,
        reactions: Vec<MatrixReactionSummary>,
        reactions_summary: String,
        special_effect_names: Vec<String>,
        item_kind: String,
        membership_change_kind: String,
        matrix_event_type: String,
        is_edited: bool,
        media_url: String,
        thumbnail_url: String,
        file_name: String,
        mime_type: String,
        media_width: u64,
        media_height: u64,
        media_duration_ms: u64,
        media_size_bytes: u64,
        blurhash: String,
        media_is_encrypted: bool,
        thumbnail_is_encrypted: bool,
        is_voice_message: bool,
        waveform: Vec<f32>,
        timestamp: u64,
        is_own: bool,
        state_event_target_user: String,
        state_event_target_user_id: String,
        state_event_detail: String,
        state_event_reason: String,
        state_event_has_sender: bool,
        utd_cause: String,
        is_encrypted_event: bool,
        shield_color: String,
        shield_code: String,
        power_level_changes: Vec<MatrixPowerLevelChange>,
        server_acl_allowed_added: Vec<String>,
        server_acl_allowed_removed: Vec<String>,
        server_acl_denied_added: Vec<String>,
        server_acl_denied_removed: Vec<String>,
        /// 0 = unchanged, 1 = now allowed, 2 = now denied
        server_acl_ip_literals_change: u8,
        /// For `m.room.tombstone` state events: the room id the tombstone
        /// points at.  Empty for every other event type.
        tombstone_replacement_room_id: String,
    }

    struct MatrixPowerLevelChange {
        user_id: String,
        old_level: i64,
        new_level: i64,
    }

    struct MatrixReactionSummary {
        key: String,
        users: String,
        user_ids: Vec<String>,
        self_reacted_event: String,
        count: u64,
    }

    struct MatrixChatExportEvent {
        item: MatrixTimelineItem,
        /// "" | "annotation" | "replacement"
        relation_kind: String,
        relates_to_event_id: String,
        annotation_key: String,
    }

    struct MatrixChatExportBatch {
        /// Newest → oldest within the batch.
        events: Vec<MatrixChatExportEvent>,
        /// Pagination token for the next call; empty when done.
        next_token: String,
        reached_start: bool,
    }

    struct MatrixJoinRoomResult {
        ok: bool,
        room_id: String,
        error: String,
        matrix_errcode: String,
    }

    struct MatrixLoginResult {
        user_id: String,
        access_token: String,
        device_id: String,
        homeserver_url: String,
    }

    struct MatrixLoginIdentityProvider {
        id: String,
        name: String,
        icon: String,
        brand: String,
    }

    struct MatrixLoginFlows {
        homeserver_url: String,
        password_supported: bool,
        sso_supported: bool,
        oauth_supported: bool,
        identity_providers: Vec<MatrixLoginIdentityProvider>,
    }

    struct MatrixSsoCallbackServer {
        listener_id: u64,
        callback_url: String,
    }

    struct MatrixSsoCallbackStatus {
        ready: bool,
        success: bool,
        login_token: String,
        callback_query: String,
    }

    struct MatrixOauthLoginStartResult {
        login_id: u64,
        login_url: String,
    }

    struct MatrixPersistedSessionSecrets {
        store_passphrase: String,
        homeserver_url: String,
        serialized_session: String,
    }

    struct MatrixThreadRootItem {
        event_id: String,
        sender_id: String,
        sender_display_name: String,
        sender_avatar_url: String,
        body: String,
        timestamp: u64,
        reply_count: u32,
    }

    struct MatrixThreadRootsResult {
        items: Vec<MatrixThreadRootItem>,
        next_batch_token: String,
    }

    unsafe extern "C++" {
        include!("matrix/backend/MatrixBackendBridge.h");

        #[namespace = "komai::rust_bridge"]
        fn matrix_profile_data_root(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn matrix_profile_cache_root(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn settings_profile_directory(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn settings_secure_store_key(profile_id: &str, key_name: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn settings_read_secure_value(key: &str) -> SettingsOptionalString;
        #[namespace = "komai::rust_bridge"]
        fn settings_write_secure_value(key: &str, value: &str);
        #[namespace = "komai::rust_bridge"]
        fn settings_write_secure_value_blocking(key: &str, value: &str) -> bool;
        #[namespace = "komai::rust_bridge"]
        fn settings_delete_secure_value(key: &str);
        #[namespace = "komai::rust_bridge"]
        fn settings_delete_secure_value_blocking(key: &str) -> bool;
        #[namespace = "komai::rust_bridge"]
        fn settings_read_text_file(path: &str, label: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn settings_path_exists(path: &str) -> bool;
        #[namespace = "komai::rust_bridge"]
        fn settings_remove_path(path: &str) -> bool;
        #[namespace = "komai::rust_bridge"]
        fn settings_write_text_file(
            path: &str,
            content: &str,
            owner_read_write_only: bool,
        ) -> bool;
        #[namespace = "komai::rust_bridge"]
        fn settings_delete_all_profile_secrets_from_store(
            profile_id: &str,
            uses_file_secrets_provider: bool,
        ) -> bool;
        #[namespace = "komai::rust_bridge"]
        fn matrix_load_session_secrets(profile_id: &str) -> MatrixPersistedSessionSecrets;
        #[namespace = "komai::rust_bridge"]
        fn matrix_save_session_secrets(
            profile_id: &str,
            store_passphrase: &str,
            homeserver_url: &str,
            serialized_session: &str,
        ) -> bool;
        #[namespace = "komai::rust_bridge"]
        fn matrix_clear_session_secrets(profile_id: &str);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_room_list_snapshot_updated(
            handle_id: u64,
            room_list: Vec<MatrixRoomSummary>,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_room_previews_backfilled(
            handle_id: u64,
            updates: Vec<MatrixRoomPreviewUpdate>,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_ignored_user_list_updated(handle_id: u64, user_ids: Vec<String>);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_initial_sync_ready(handle_id: u64);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_sync_connection_state_changed(handle_id: u64, is_connected: bool);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_room_timeline_snapshot_updated(handle_id: u64, room_id: &str);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_room_timeline_pagination_state(
            handle_id: u64,
            room_id: &str,
            in_progress: bool,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_room_pinned_events_changed(
            handle_id: u64,
            room_id: &str,
            event_ids: Vec<String>,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_thread_timeline_snapshot_updated(
            handle_id: u64,
            room_id: &str,
            thread_root_id: &str,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_notification_received(handle_id: u64, room_id: &str, event_id: &str);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_notification_item_received(handle_id: u64, item: MatrixNotificationItem);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_invite_received(handle_id: u64, event: MatrixCallInviteEvent);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_candidates_received(
            handle_id: u64,
            event: MatrixCallCandidatesEvent,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_answer_received(handle_id: u64, event: MatrixCallAnswerEvent);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_hangup_received(handle_id: u64, event: MatrixCallHangUpEvent);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_select_answer_received(
            handle_id: u64,
            event: MatrixCallSelectAnswerEvent,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_reject_received(handle_id: u64, event: MatrixCallRejectEvent);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_negotiate_received(
            handle_id: u64,
            event: MatrixCallNegotiateEvent,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_sync_stopped(handle_id: u64, reason: &str, is_auth_error: bool);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_typing_users_updated(
            handle_id: u64,
            room_id: &str,
            display_names: Vec<String>,
        );
        // Element Call widget driver -> webview bridge. Routed by session_id to
        // the matching ElementCallWidgetSession on the GUI thread (these fire
        // from tokio worker threads). Always provided by the C++ side even in
        // -DELEMENT_CALL=OFF builds (they become no-ops there) since the Rust
        // widget driver is compiled unconditionally.
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_element_call_widget_url_ready(session_id: u64, url: &str);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_element_call_widget_message(session_id: u64, message: &str);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_element_call_widget_stopped(session_id: u64, reason: &str);
        // MatrixRTC ring/notify (MSC4075) + decline (MSC4310) -> the always-built
        // ElementCallController on the GUI thread (these fire from tokio worker
        // threads). Always provided by C++ even in -DELEMENT_CALL=OFF builds (the
        // controller exists there too and ignores them when EC is unsupported)
        // since the Rust RTC handlers are compiled unconditionally.
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_rtc_notification(handle_id: u64, event: MatrixRtcNotificationEvent);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_rtc_decline(handle_id: u64, event: MatrixRtcDeclineEvent);
    }

    extern "Rust" {
        type SettingsProfileHandle;

        fn init_logging(
            level: &str,
            to_stderr: bool,
            enable_debug: bool,
        );
        fn log_from_cpp(component: &str, level: &str, message: &str);
        fn settings_load_startup_snapshot_for_profile(profile_id: &str) -> SettingsStartupSnapshot;
        fn settings_load_config_overview_for_profile(profile_id: &str) -> SettingsConfigOverview;
        fn settings_load_profile_overview_for_profile(profile_id: &str) -> SettingsProfileOverview;
        fn settings_encode_string_map_yaml(entries: &Vec<SettingsStringMapEntry>) -> String;
        fn settings_decode_string_map_yaml(serialized: &str) -> Vec<SettingsStringMapEntry>;
        fn settings_encode_persisted_secrets_map_yaml(
            access_token: &str,
            entries: &Vec<SettingsStringMapEntry>,
        ) -> String;
        fn settings_decode_persisted_secrets_map_yaml(
            serialized: &str,
        ) -> SettingsSecretsPayload;
        fn settings_encode_named_string_map_yaml(
            root_key: &str,
            entries: &Vec<SettingsStringMapEntry>,
        ) -> String;
        fn settings_decode_named_string_map_yaml(
            serialized: &str,
            root_key: &str,
        ) -> Vec<SettingsStringMapEntry>;
        fn settings_load_persisted_secrets_file_for_profile(
            profile_id: &str,
        ) -> SettingsSecretsPayload;
        fn settings_write_persisted_secrets_file_for_profile(
            profile_id: &str,
            access_token: &str,
            entries: &Vec<SettingsStringMapEntry>,
            owner_read_write_only: bool,
        ) -> bool;
        fn settings_remove_persisted_secrets_file_for_profile(profile_id: &str) -> bool;
        fn settings_load_matrix_sdk_secrets_for_profile(
            profile_id: &str,
        ) -> Vec<SettingsStringMapEntry>;
        fn settings_write_matrix_sdk_secrets_for_profile(
            profile_id: &str,
            entries: &Vec<SettingsStringMapEntry>,
            owner_read_write_only: bool,
        ) -> bool;
        fn settings_remove_matrix_sdk_secrets_file_for_profile(profile_id: &str) -> bool;
        fn settings_load_persisted_matrix_session_secrets_for_profile(
            profile_id: &str,
        ) -> MatrixPersistedSessionSecrets;
        fn settings_save_persisted_matrix_session_secrets_for_profile(
            profile_id: &str,
            store_passphrase: &str,
            homeserver_url: &str,
            serialized_session: &str,
        ) -> bool;
        fn settings_clear_persisted_matrix_session_secrets_for_profile(profile_id: &str) -> bool;
        fn settings_load_config_snapshot(config_text: &str) -> SettingsLoadedConfig;
        fn settings_open_profile_handle_for_profile(
            profile_id: &str,
            include_session: bool,
        ) -> Box<SettingsProfileHandle>;
        fn settings_profile_snapshot(handle: &SettingsProfileHandle) -> SettingsLoadedProfile;
        fn settings_profile_prepare_for_load(
            handle: Pin<&mut SettingsProfileHandle>,
            full_load: bool,
            secure_backend_available: bool,
        );
        fn settings_profile_replace_config_snapshot(
            handle: Pin<&mut SettingsProfileHandle>,
            snapshot: &SettingsConfigSnapshot,
        );
        fn settings_profile_replace_session_identity(
            handle: Pin<&mut SettingsProfileHandle>,
            user_id: &str,
            homeserver: &str,
            device_id: &str,
        );
        fn settings_profile_replace_state_snapshot(
            handle: Pin<&mut SettingsProfileHandle>,
            snapshot: &SettingsStateSnapshot,
        );
        fn settings_profile_replace_secrets_payload(
            handle: Pin<&mut SettingsProfileHandle>,
            access_token: &str,
            entries: &Vec<SettingsStringMapEntry>,
        );
        fn settings_profile_clear_secrets(handle: Pin<&mut SettingsProfileHandle>) -> bool;
        fn settings_profile_clear_auth(handle: Pin<&mut SettingsProfileHandle>) -> bool;
        fn settings_profile_flush(
            handle: Pin<&mut SettingsProfileHandle>,
            write_config: bool,
            write_session: bool,
            write_secrets: bool,
            write_state: bool,
        ) -> SettingsProfileFlushResult;
        fn settings_load_session_snapshot(session_text: &str) -> SettingsLoadedSession;
        fn settings_load_state_snapshot(state_text: &str) -> SettingsLoadedState;
        fn theme_builtin_themes() -> ThemeBuiltinListResult;
        fn theme_parse_external_theme(theme_text: &str) -> ThemeExternalParseResult;
        fn theme_parse_base16_yaml(theme_text: &str) -> ThemeBase16ParseResult;

        fn serverlist_entries(locale: &str) -> ServerListResult;

        fn matrix_registration_probe(
            context: MatrixFfiBlockingContext,
            server_name_or_url: &str,
            verify_certificates: bool,
        ) -> Result<RegistrationProbeResult>;
        fn matrix_registration_check_username(
            context: MatrixFfiBlockingContext,
            registration_id: u64,
            username: &str,
        ) -> Result<RegistrationUsernameResult>;
        fn matrix_registration_submit_stage(
            context: MatrixFfiBlockingContext,
            registration_id: u64,
            username: &str,
            password: &str,
            device_name: &str,
            stage_type: &str,
            token: &str,
            email_sid: &str,
            email_client_secret: &str,
        ) -> Result<RegistrationSubmitResult>;
        fn matrix_registration_request_email_token(
            context: MatrixFfiBlockingContext,
            registration_id: u64,
            email: &str,
            client_secret: &str,
            send_attempt: u64,
        ) -> Result<RegistrationEmailTokenResult>;
        fn matrix_registration_cancel(registration_id: u64) -> Result<()>;

        fn blurhash_decode(hash: &str, width: u32, height: u32) -> Vec<u8>;

        // Lanczos3-downscale a packed RGBA8888 buffer (resample only; the caller
        // colour-manages to sRGB first). Empty result => caller keeps the source.
        fn lanczos_resize_rgba(
            pixels: &[u8],
            src_w: u32,
            src_h: u32,
            dst_w: u32,
            dst_h: u32,
        ) -> Vec<u8>;

        fn highlight_formatted_code_blocks(html: &str, code_background: &str) -> String;
        fn highlight_raw_json(raw_json: &str, code_background: &str) -> String;

        // ----- Spell checking (crate::spellcheck) -----------------------
        fn spellcheck_register_builtin_dictionary(code: &str, aff: &str, dic: &str);
        fn spellcheck_discover_dictionaries() -> Vec<SpellcheckDictionaryEntry>;
        fn spellcheck_set_config(
            data_dir: &str,
            master_enabled: bool,
            enabled_codes: &Vec<String>,
        );
        fn spellcheck_check_block(text: &str, in_code_fence_before: bool) -> SpellcheckBlockResult;
        fn spellcheck_suggest(word: &str) -> Vec<SpellcheckSuggestionGroup>;
        fn spellcheck_add_word(word: &str);

        fn emoji_only_visual_count(body: &str) -> i32;

        fn composer_trigger_at_word_boundary(text: &str, trigger_byte_pos: usize) -> bool;

        fn composer_extract_mentions(text: &str) -> Vec<ComposerMentionMatch>;

        fn composer_toggle_inline_wrap(
            text: &str,
            sel_start_utf16: u32,
            sel_end_utf16: u32,
            marker: &str,
        ) -> ComposerTransformResult;
        fn composer_toggle_block_prefix(
            text: &str,
            sel_start_utf16: u32,
            sel_end_utf16: u32,
            prefix: &str,
        ) -> ComposerTransformResult;
        fn composer_toggle_code(
            text: &str,
            sel_start_utf16: u32,
            sel_end_utf16: u32,
        ) -> ComposerTransformResult;
        fn composer_toggle_link(
            text: &str,
            sel_start_utf16: u32,
            sel_end_utf16: u32,
        ) -> ComposerTransformResult;

        fn html_sanitize(html: &str) -> String;
        fn html_linkify(html: &str) -> String;
        fn html_mark_search_matches(html: &str, query: &str) -> String;

        fn format_body_html(
            body: &str,
            formatted_body: &str,
            pill_avatars: &Vec<HtmlPillAvatar>,
            pill_avatar_size: u32,
            code_background: &str,
            syntax_highlight: bool,
        ) -> String;

        fn resolve_server(
            context: MatrixFfiBlockingContext,
            server_name: &str,
        ) -> Result<ResolveResult>;
        fn matrix_sdk_paths(profile_id: &str) -> MatrixSdkPaths;
        fn matrix_restore_session_preview(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
        ) -> Result<MatrixRestorePreview>;
        fn matrix_start_restored_backend(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
        ) -> Result<MatrixBackendHandleInfo>;
        fn matrix_logout_backend(context: MatrixFfiBlockingContext, handle_id: u64) -> Result<()>;
        fn matrix_stop_backend(handle_id: u64) -> Result<()>;
        fn matrix_start_media_proxy(handle_id: u64) -> Result<u16>;
        fn matrix_is_timeline_media_encrypted(handle_id: u64, item_id: &str) -> bool;
        fn matrix_register_timeline_media_proxy_url(
            handle_id: u64,
            item_id: &str,
            file_extension: &str,
        ) -> Result<String>;
        fn matrix_stop_media_proxy(handle_id: u64);
        fn matrix_start_backend_sync(handle_id: u64) -> Result<()>;
        fn matrix_join_room(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id_or_alias: &str,
            via: &Vec<String>,
            reason: &str,
        ) -> MatrixJoinRoomResult;
        fn matrix_knock_room(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id_or_alias: &str,
            via: &Vec<String>,
            reason: &str,
        ) -> Result<String>;
        fn matrix_create_room(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            name: &str,
            topic: &str,
            room_alias_localpart: &str,
            invite_user_ids: &Vec<String>,
            preset: &str,
            is_direct: bool,
            is_encrypted: bool,
            is_space: bool,
            is_public: bool,
            room_version: &str,
            power_level_content_override_json: &str,
            initial_state_json: &str,
            creation_content_json: &str,
        ) -> Result<String>;
        fn matrix_leave_room(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_toggle_room_tag(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            tag: &str,
            enabled: bool,
        ) -> Result<()>;
        fn matrix_set_room_is_direct(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            is_direct: bool,
        ) -> Result<()>;
        fn matrix_invite_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_kick_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_ban_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_unban_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_upgrade_room(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            new_version: &str,
            additional_creators: Vec<String>,
        ) -> Result<String>;
        fn matrix_set_user_power_level(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            power_level: i64,
        ) -> Result<()>;
        fn matrix_fetch_own_profile(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixOwnProfile>;
        fn matrix_fetch_own_presence(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixOwnPresence>;
        fn matrix_fetch_recovery_status(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixRecoveryStatus>;
        fn matrix_setup_recovery(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            use_ssss: bool,
            passphrase: &str,
            encryption_backup_online_enabled: bool,
        ) -> Result<MatrixSetupRecoveryResult>;
        fn matrix_recover_encryption_secrets(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            key_or_passphrase: &str,
        ) -> Result<()>;
        fn matrix_export_room_keys(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            path: &str,
            passphrase: &str,
        ) -> Result<u64>;
        fn matrix_import_room_keys(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            path: &str,
            passphrase: &str,
        ) -> Result<MatrixRoomKeyImportCounts>;
        fn matrix_fetch_chat_export_batch(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            from_token: &str,
            limit: u32,
        ) -> Result<MatrixChatExportBatch>;
        fn matrix_start_reset_encryption_identity(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixResetEncryptionIdentityResult>;
        fn matrix_continue_reset_encryption_identity_with_password(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            password: &str,
        ) -> Result<()>;
        fn matrix_continue_reset_encryption_identity_after_approval(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<()>;
        fn matrix_cancel_reset_encryption_identity(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<()>;
        fn matrix_start_sign_out_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            device_id: &str,
        ) -> Result<MatrixDeviceSignOutResult>;
        fn matrix_continue_sign_out_device_with_password(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            password: &str,
        ) -> Result<()>;
        fn matrix_rename_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            device_id: &str,
            display_name: &str,
        ) -> Result<()>;
        fn matrix_start_self_verification(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_start_user_verification(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_start_device_verification(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
            device_id: &str,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_unverify_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
            device_id: &str,
        ) -> Result<()>;
        fn matrix_block_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
            device_id: &str,
        ) -> Result<()>;
        fn matrix_unblock_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
            device_id: &str,
        ) -> Result<()>;
        fn matrix_fetch_user_verification_state(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<MatrixUserVerificationState>;
        fn matrix_take_pending_verification_flow_ids(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<Vec<String>>;
        fn matrix_fetch_verification_session(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            flow_id: &str,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_clear_verification_session(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            flow_id: &str,
        ) -> Result<()>;
        fn matrix_advance_verification_session(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            flow_id: &str,
        ) -> Result<()>;
        fn matrix_cancel_verification_session(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            flow_id: &str,
            mismatch: bool,
        ) -> Result<()>;
        fn matrix_fetch_user_profile(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<MatrixUserProfile>;
        fn matrix_fetch_room_member_profile(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
        ) -> Result<MatrixUserProfile>;
        fn matrix_search_users(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            search_term: &str,
            limit: u64,
        ) -> Result<Vec<MatrixDirectoryUser>>;
        fn matrix_fetch_public_room_directory_page(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            search_term: &str,
            limit: u64,
            since: &str,
            server: &str,
            room_type_filter: &str,
        ) -> Result<MatrixPublicRoomDirectoryPage>;
        fn matrix_set_own_display_name(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            display_name: &str,
        ) -> Result<()>;
        fn matrix_set_own_presence(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            presence_state: &str,
            status_message: &str,
        ) -> Result<()>;
        fn matrix_set_own_room_display_name(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            display_name: &str,
        ) -> Result<()>;
        fn matrix_upload_own_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            file_path: &str,
            mime_type: &str,
        ) -> Result<()>;
        fn matrix_remove_own_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<()>;
        fn matrix_upload_own_room_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            file_path: &str,
            mime_type: &str,
        ) -> Result<()>;
        fn matrix_remove_own_room_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<()>;
        fn matrix_ignore_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<()>;
        fn matrix_unignore_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<()>;
        fn matrix_fetch_room_list(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<Vec<MatrixRoomSummary>>;
        fn matrix_fetch_notification_items(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            requests: &Vec<MatrixNotificationRequest>,
        ) -> Result<Vec<MatrixNotificationItem>>;
        fn matrix_fetch_account_notifications_enabled(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<bool>;
        fn matrix_fetch_turn_server_info(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixTurnServerInfo>;
        // Element Call widget driver. start returns a session id immediately;
        // the generated webview URL arrives later via
        // matrix_notify_element_call_widget_url_ready. These are non-blocking
        // (start only validates the room synchronously, then spawns the driver),
        // so they take no MatrixFfiBlockingContext.
        fn matrix_element_call_start_session(
            handle_id: u64,
            room_id: &str,
            base_url: &str,
            lang: &str,
            theme: &str,
        ) -> Result<u64>;
        fn matrix_element_call_send_message(session_id: u64, message: &str) -> Result<()>;
        fn matrix_element_call_stop_session(session_id: u64);
        // Decline an incoming MatrixRTC call notification (sends m.rtc.decline,
        // MSC4310). Non-blocking: validates the room synchronously, then spawns
        // the send.
        fn matrix_element_call_decline(
            handle_id: u64,
            room_id: &str,
            notification_event_id: &str,
        ) -> Result<()>;
        fn matrix_set_account_notifications_enabled(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            enabled: bool,
        ) -> Result<()>;
        fn matrix_fetch_image_packs(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<Vec<MatrixImagePack>>;
        fn matrix_save_image_pack(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            state_key: &str,
            previous_state_key: &str,
            has_previous_state_key: bool,
            pack: MatrixImagePack,
        ) -> Result<()>;
        fn matrix_remove_image_pack(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            state_key: &str,
        ) -> Result<()>;
        fn matrix_set_image_pack_globally_enabled(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            state_key: &str,
            enabled: bool,
        ) -> Result<()>;
        fn matrix_fetch_room_settings(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<MatrixRoomSettings>;
        fn matrix_fetch_room_versions_capability(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixRoomVersionsCapability>;
        fn matrix_fetch_room_aliases(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<MatrixRoomAliases>;
        fn matrix_apply_room_aliases(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            aliases: MatrixRoomAliases,
        ) -> Result<()>;
        fn matrix_fetch_room_members(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<Vec<MatrixRoomMember>>;
        fn matrix_fetch_room_power_levels(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<MatrixRoomPowerLevels>;
        fn matrix_apply_room_power_levels(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            power_levels: MatrixRoomPowerLevels,
        ) -> Result<()>;
        fn matrix_fetch_room_child_spaces(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<Vec<MatrixChildSpaceEntry>>;
        fn matrix_fetch_media_content(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            mxc_uri: &str,
            width: i32,
            height: i32,
            crop: bool,
        ) -> Result<Vec<u8>>;
        fn matrix_set_room_notification_mode(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            mode: i32,
        ) -> Result<()>;
        fn matrix_set_room_name(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            name: &str,
        ) -> Result<()>;
        fn matrix_set_room_topic(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            topic: &str,
        ) -> Result<()>;
        fn matrix_fetch_room_state_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_type: &str,
            state_key: &str,
        ) -> Result<MatrixRoomStateEvent>;
        fn matrix_send_room_state_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_type: &str,
            state_key: &str,
            content_json: &str,
        ) -> Result<String>;
        fn matrix_upload_room_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            file_path: &str,
            mime_type: &str,
            width: i32,
            height: i32,
        ) -> Result<String>;
        fn matrix_remove_room_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<()>;
        fn matrix_enable_room_encryption(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<()>;
        fn matrix_set_room_history_visibility(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            history_visibility: &str,
        ) -> Result<()>;
        fn matrix_set_room_access_rules(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            join_rule_kind: &str,
            guest_access: bool,
            allowed_room_ids: &Vec<String>,
        ) -> Result<()>;
        fn matrix_set_active_room_timeline_initial_page_size(
            handle_id: u64,
            page_size: u16,
        ) -> Result<()>;
        fn matrix_select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<()>;
        fn matrix_subscribe_to_room(handle_id: u64, room_id: &str) -> Result<()>;
        fn matrix_unsubscribe_from_room(handle_id: u64, room_id: &str) -> Result<()>;
        fn matrix_fetch_active_room_timeline(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<Vec<MatrixTimelineItem>>;
        fn matrix_fetch_room_timeline(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            limit: u16,
        ) -> Result<Vec<MatrixTimelineItem>>;
        fn matrix_stop_room_timeline(handle_id: u64, room_id: &str) -> Result<()>;
        fn matrix_fetch_room_timeline_snapshot(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<Vec<MatrixTimelineItem>>;
        fn matrix_paginate_active_room_timeline_backwards(
            handle_id: u64,
            page_size: u16,
        ) -> Result<()>;
        fn matrix_fetch_active_room_timeline_media_content(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            item_id: &str,
            width: i32,
            height: i32,
            crop: bool,
        ) -> Result<Vec<u8>>;
        fn matrix_fetch_active_room_timeline_media_content_with_progress(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            item_id: &str,
        ) -> Result<Vec<u8>>;
        fn matrix_active_timeline_media_download_progress(
            handle_id: u64,
            item_id: &str,
        ) -> MatrixMediaDownloadProgress;
        fn matrix_send_typing_notice(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            typing: bool,
        ) -> Result<()>;
        fn matrix_send_room_message(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            body: &str,
            use_markdown_formatting: bool,
            message_kind: &str,
            mention_user_ids: &str,
            mentions_room: bool,
            use_send_queue: bool,
        ) -> Result<String>;
        fn matrix_send_room_message_like_event_json(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_type: &str,
            content_json: &str,
        ) -> Result<()>;
        fn matrix_send_call_invite(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            lifetime: u32,
            invitee: &str,
            offer_sdp: &str,
            offer_type: &str,
        ) -> Result<()>;
        fn matrix_send_call_candidates(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            candidates: Vec<MatrixCallIceCandidate>,
        ) -> Result<()>;
        fn matrix_send_call_answer(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            answer_sdp: &str,
            answer_type: &str,
        ) -> Result<()>;
        fn matrix_send_call_hangup(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_send_call_select_answer(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            selected_party_id: &str,
        ) -> Result<()>;
        fn matrix_send_call_reject(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
        ) -> Result<()>;
        fn matrix_send_call_negotiate(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            lifetime: u32,
            description_sdp: &str,
            description_type: &str,
        ) -> Result<()>;
        fn matrix_send_room_reply_message(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            replied_to_event_id: &str,
            body: &str,
            use_markdown_formatting: bool,
            message_kind: &str,
            thread_id: &str,
            mention_user_ids: &str,
            mentions_room: bool,
        ) -> Result<()>;
        fn matrix_send_room_edit_message(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            target_event_id: &str,
            body: &str,
            use_markdown_formatting: bool,
            message_kind: &str,
            mention_user_ids: &str,
            mentions_room: bool,
        ) -> Result<()>;
        fn matrix_toggle_room_reaction(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            reaction_key: &str,
        ) -> Result<()>;
        fn matrix_redact_room_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            reason: &str,
        ) -> Result<String>;
        fn matrix_cancel_room_local_echo(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            transaction_id: &str,
        ) -> Result<bool>;
        fn matrix_retry_room_local_echo(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            transaction_id: &str,
        ) -> Result<()>;
        fn matrix_mark_room_event_as_read(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            public_receipt: bool,
        ) -> Result<()>;
        fn matrix_mark_room_as_read(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            public_receipt: bool,
        ) -> Result<()>;
        fn matrix_mark_room_unread(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            unread: bool,
        ) -> Result<()>;
        fn matrix_report_room_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_fetch_room_thread_roots(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            include: &str,
            from: &str,
            limit: u32,
        ) -> Result<MatrixThreadRootsResult>;
        fn matrix_subscribe_to_thread_timeline(
            handle_id: u64,
            room_id: &str,
            thread_root_id: &str,
        ) -> Result<()>;
        fn matrix_unsubscribe_from_thread_timeline(handle_id: u64) -> Result<()>;
        fn matrix_refresh_thread_timeline(handle_id: u64) -> Result<()>;
        fn matrix_fetch_thread_timeline_snapshot(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<Vec<MatrixTimelineItem>>;
        fn matrix_paginate_thread_timeline_backwards(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            num_events: u16,
        ) -> Result<bool>;
        fn matrix_fetch_room_frequent_reactions(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            lookback_days: i32,
            max_results: u32,
            max_scanned_events: u64,
        ) -> Result<Vec<String>>;
        fn matrix_pin_room_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<()>;
        fn matrix_unpin_room_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<()>;
        fn matrix_fetch_active_room_raw_event_dialog_data(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<MatrixRawEventDialogData>;
        fn matrix_fetch_active_room_event_content_for_forwarding(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<MatrixEventContentForForwarding>;
        fn matrix_fetch_room_read_receipts(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<Vec<MatrixReadReceiptEntry>>;
        fn matrix_fetch_room_redaction_permissions(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<MatrixRoomRedactionPermissions>;
        fn matrix_send_room_attachment(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            file_path: &str,
            filename: &str,
            caption: &str,
            use_markdown_formatting: bool,
            reply_event_id: &str,
            thread_id: &str,
            mime_type: &str,
            duration_ms: u64,
            is_voice: bool,
            waveform: &[f32],
            strip_image_metadata: bool,
        ) -> Result<String>;
        fn matrix_upload_media(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            file_path: &str,
            mime_type: &str,
            strip_image_metadata: bool,
        ) -> Result<String>;
        fn matrix_send_room_image(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            mxc_uri: &str,
            body: &str,
            filename: &str,
            info_json: &str,
            use_send_queue: bool,
        ) -> Result<String>;
        fn matrix_discover_login_flows(
            context: MatrixFfiBlockingContext,
            server_name_or_url: &str,
            verify_certificates: bool,
        ) -> Result<MatrixLoginFlows>;
        fn matrix_get_sso_login_url(
            context: MatrixFfiBlockingContext,
            homeserver_url: &str,
            redirect_url: &str,
            identity_provider_id: &str,
            verify_certificates: bool,
        ) -> Result<String>;
        fn matrix_start_sso_callback_server(
            success_html: &str,
            failure_html: &str,
            timeout_ms: u32,
        ) -> Result<MatrixSsoCallbackServer>;
        fn matrix_poll_sso_callback_server(listener_id: u64) -> Result<MatrixSsoCallbackStatus>;
        fn matrix_stop_sso_callback_server(listener_id: u64) -> Result<()>;
        fn matrix_start_oauth_login(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
            homeserver_url: &str,
            redirect_url: &str,
            user_id_hint: &str,
            device_id: &str,
            initial_device_display_name: &str,
            verify_certificates: bool,
        ) -> Result<MatrixOauthLoginStartResult>;
        fn matrix_finish_oauth_login(
            context: MatrixFfiBlockingContext,
            login_id: u64,
            callback_query: &str,
        ) -> Result<MatrixLoginResult>;
        fn matrix_cancel_oauth_login(login_id: u64) -> Result<()>;
        fn matrix_login_password(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
            homeserver_url: &str,
            user_id: &str,
            password: &str,
            device_id: &str,
            initial_device_display_name: &str,
            verify_certificates: bool,
        ) -> Result<MatrixLoginResult>;
        fn matrix_login_token(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
            homeserver_url: &str,
            login_token: &str,
            device_id: &str,
            initial_device_display_name: &str,
            verify_certificates: bool,
        ) -> Result<MatrixLoginResult>;

        // Voice transcription. See `src/rust/src/transcription/`.
        fn transcription_resolve_for_room(
            profile_id: &str,
            room_id: &str,
        ) -> TranscriptionResolvedConfig;
        fn transcription_run_batch(
            profile_id: &str,
            room_id: &str,
            audio_path: &str,
        ) -> TranscriptionBatchResult;
        fn transcription_load_global_api_key(profile_id: &str) -> SettingsOptionalString;
        fn transcription_save_global_api_key(profile_id: &str, value: &str);
        fn transcription_clear_global_api_key(profile_id: &str);
        fn transcription_load_room_api_key(
            profile_id: &str,
            room_id: &str,
        ) -> SettingsOptionalString;
        fn transcription_save_room_api_key(profile_id: &str, room_id: &str, value: &str);
        fn transcription_clear_room_api_key(profile_id: &str, room_id: &str);

        // Realtime / streaming transcription. The session lives in a
        // background tokio task; the C++ side drives it by pushing PCM16
        // chunks, then calling commit on user release (or cancel on Esc),
        // and polls `drain_events` on a Qt timer to surface deltas /
        // completed / failures into the composer banner.
        //
        // The audio format is fixed: little-endian PCM16, mono, at the
        // sample rate returned by `transcription_realtime_sample_rate_hz`.
        // The C++ side is responsible for resampling its mic capture to
        // that rate.
        fn transcription_realtime_sample_rate_hz() -> u32;
        fn transcription_run_realtime(
            profile_id: &str,
            room_id: &str,
        ) -> TranscriptionRealtimeStartResult;
        fn transcription_realtime_push_audio(job_id: i64, pcm16_bytes: &[u8]);
        fn transcription_realtime_commit(job_id: i64);
        fn transcription_realtime_cancel(job_id: i64);
        fn transcription_realtime_drain_events(job_id: i64) -> Vec<TranscriptionRealtimeEvent>;
    }
}

pub(crate) use bridge::*;
