// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Canonical defaults for all Option<T> config fields.
//
// Every optional setting resolved by the FFI layer uses exactly one of these
// constants.  C++ no longer carries its own fallback values — it unconditionally
// trusts whatever the Rust FFI snapshot provides.

// -- ui -----------------------------------------------------------------------
pub const SCALE_FACTOR: f32 = 1.0;
pub const FONT_SIZE_PT: f64 = 13.0;
pub const MOTION_ANIMATIONS_ENABLED: bool = true;
pub const AVATARS_CIRCULAR: bool = false;

// -- navigation ---------------------------------------------------------------
pub const SHOW_LAST_MESSAGE_TIME: bool = true;
pub const SHOW_ROOM_LIST_UNREAD_INDICATORS: bool = true;
pub const SHOW_COMMUNITIES_UNREAD_INDICATORS: bool = true;
pub const COMMUNITIES_FILTER_FAVOURITES: bool = true;
pub const COMMUNITIES_FILTER_PEOPLE: bool = true;
pub const COMMUNITIES_FILTER_BOTS: bool = true;
pub const COMMUNITIES_FILTER_GROUPS: bool = true;
pub const COMMUNITIES_FILTER_SERVER_NOTICES: bool = true;
pub const COMMUNITIES_FILTER_LOW_PRIORITY: bool = true;
pub const TABS_AUTO_HIDE_WITH_SINGLE_TAB: bool = false;
pub const TABS_PREFERRED_WIDTH_PX: i32 = 200;
pub const TABS_MINIMUM_WIDTH_PX: i32 = 120;
pub const TABS_MAX_RECENTLY_CLOSED_TIMELINES: i32 = 3;

// -- timeline -----------------------------------------------------------------
pub const HIDDEN_TIMELINE_EVENT_TYPES: &[&str] = &[
    "m.reaction",
    "m.call.candidates",
    "m.call.select_answer",
    "m.call.negotiate",
];
pub const LAYOUT_SHOW_OWN_AVATAR: bool = true;
pub const LAYOUT_MAX_WIDTH_PERCENT: i32 = 80;
pub const LAYOUT_ADAPTIVE_POSITIONING_BREAKPOINT_PX: i32 = 1600;
pub const EMOJI_ONLY_ENLARGE: bool = true;
pub const HOVER_HIGHLIGHT: bool = true;
pub const DRAG_SELECT: bool = true;
pub const CODE_SYNTAX_HIGHLIGHTING: bool = true;
pub const TYPING_SHOW_ENABLED: bool = true;
pub const READ_RECEIPTS_GLOBAL: bool = true;
pub const MEDIA_EFFECTS_ENABLED: bool = true;
pub const DATE_DIVIDERS_ENABLED: bool = true;
pub const MEDIA_ANIMATE_ON_HOVER: bool = false;
pub const MEDIA_OPEN_IMAGES_EXTERNAL: bool = false;
pub const MEDIA_OPEN_VIDEOS_EXTERNAL: bool = false;
pub const MEDIA_AUTOPLAY_GIF_VIDEOS: bool = true;
pub const MEDIA_OPEN_AUDIO_EXTERNAL: bool = false;
pub const THREADS_COLLAPSE_REPLIES: bool = false;
pub const MEDIA_DEFAULT_AUDIO_PLAYBACK_SPEED: f64 = 1.0;

// -- desktop ------------------------------------------------------------------
pub const NOTIFICATIONS_ENABLED: bool = true;
pub const NOTIFICATIONS_ATTENTION_ON_INCOMING: bool = false;
pub const ATTENTION_WINDOW_TITLE_ENABLED: bool = true;
pub const ATTENTION_APP_BADGE_ENABLED: bool = true;
pub const SYSTEM_TRAY_ENABLED: bool = false;
pub const SYSTEM_TRAY_AUTOSTART: bool = false;
pub const WINDOW_FOCUS_BLUR_ENABLED: bool = false;
pub const WINDOW_FOCUS_BLUR_DELAY_SECONDS: i32 = 0;

// -- network ------------------------------------------------------------------
pub const ENCRYPTION_ONLY_VERIFIED_USERS: bool = false;
pub const ENCRYPTION_SHARE_WITH_TRUSTED: bool = false;
pub const ENCRYPTION_KEY_BACKUP: bool = true;
pub const TLS_ENABLE_CERTIFICATE_VALIDATION: bool = true;
pub const MRS_ENABLED: bool = true;
pub const HTTP3_ENABLED: bool = false;

// -- calls --------------------------------------------------------------------
pub const CALLS_LEGACY_ENABLED: bool = false;
pub const CALLS_RELAY_USE_FALLBACK_SERVER: bool = false;
pub const SCREENSHARE_FRAME_RATE: i32 = 30;
pub const SCREENSHARE_PICTURE_IN_PICTURE: bool = true;
pub const SCREENSHARE_INCLUDE_REMOTE_VIDEO: bool = false;
pub const SCREENSHARE_SHOW_CURSOR: bool = true;

// -- composer -----------------------------------------------------------------
pub const INPUT_MARKDOWN_TO_HTML_ENABLED: bool = true;
pub const INPUT_INLINE_EMOJI_PICKER_ENABLED: bool = true;
pub const INPUT_INLINE_ROOM_PICKER_ENABLED: bool = true;
pub const INPUT_INLINE_USER_PICKER_ENABLED: bool = true;
pub const INPUT_SELECTION_FORMATTING_TOOLBAR_ENABLED: bool = true;
pub const INPUT_TRANSCRIPTION_ENABLED: bool = true;
pub const INPUT_SPELLCHECK_ENABLED: bool = true;
pub const ATTACHMENTS_STRIP_IMAGE_METADATA: bool = true;
pub const TYPING_SEND_GLOBAL: bool = true;
