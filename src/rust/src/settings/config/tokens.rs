// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub trait StorageToken: Clone + Default {
    fn from_storage_str(value: &str) -> Self;
}

macro_rules! storage_token_enum {
    ($name:ident, $default:ident { $($variant:ident => $token:literal),+ $(,)? }) => {
        #[derive(Clone, Debug, PartialEq, Eq)]
        pub enum $name {
            $($variant,)+
        }

        impl Default for $name {
            fn default() -> Self {
                Self::$default
            }
        }

        impl $name {
            pub fn to_storage_string(&self) -> String {
                match self {
                    $(Self::$variant => $token.to_owned(),)+
                }
            }
        }

        impl StorageToken for $name {
            fn from_storage_str(value: &str) -> Self {
                match value.trim() {
                    $($token => Self::$variant,)+
                    _ => Self::default(),
                }
            }
        }

        impl From<&str> for $name {
            fn from(value: &str) -> Self {
                <Self as StorageToken>::from_storage_str(value)
            }
        }

        impl From<String> for $name {
            fn from(value: String) -> Self {
                <Self as StorageToken>::from_storage_str(&value)
            }
        }
    };
}

storage_token_enum!(ConfigUiThemeModeToken, Auto {
    Light => "light",
    Dark => "dark",
    Auto => "auto",
});

storage_token_enum!(ConfigUiScrollbarPolicyToken, WhenNeeded {
    WhenNeeded => "when_needed",
    Never => "never",
    Always => "always",
});

storage_token_enum!(ConfigUiDefaultAvatarStyleToken, BoringAvatarsBauhaus {
    BoringAvatarsBauhaus => "boring_avatars_bauhaus",
    BoringAvatarsBeam => "boring_avatars_beam",
    BoringAvatarsMarble => "boring_avatars_marble",
    LetterInitial => "letter_initial",
    UserIcon => "user_icon",
});

storage_token_enum!(ConfigUiLayoutDensityToken, Spacious {
    Spacious => "spacious",
    Compact => "compact",
    Dense => "dense",
});

storage_token_enum!(ConfigNavigationRoomListLastMessagePreviewToken, Always {
    Always => "always",
    OnlyUnencrypted => "only_unencrypted",
    Never => "never",
});

storage_token_enum!(ConfigNavigationRoomListSortToken, UnreadFirstRecent {
    UnreadFirstRecent => "unread_first_recent",
    UnreadFirstAlpha => "unread_first_alpha",
    Recent => "recent",
    Alphabetical => "alphabetical",
});

storage_token_enum!(ConfigNavigationRoomListOpeningPolicyToken, ReuseActiveTab {
    ReuseActiveTab => "reuse_active_tab",
    OpenNewTab => "open_new_tab",
});

storage_token_enum!(ConfigNavigationTabsPinButtonVisibilityToken, Never {
    Always => "always",
    Never => "never",
});

storage_token_enum!(ConfigNavigationTabsLabelDisplayToken, AvatarOnly {
    AvatarAndLabel => "avatar_and_label",
    AvatarOnly => "avatar_only",
});

storage_token_enum!(ConfigTimelineMessagesStyleToken, Bubbles {
    Plain => "plain",
    Bubbles => "bubbles",
});

storage_token_enum!(ConfigTimelineMessagesPositioningToken, Adaptive {
    Adaptive => "adaptive",
    OpposingBySender => "opposing_by_sender",
    AllLeft => "all_left",
    AllRight => "all_right",
});

storage_token_enum!(ConfigTimelineUserColorCodingPolicyToken, AdaptiveByRoomSize {
    AdaptiveByRoomSize => "adaptive_by_room_size",
    MeVsOthers => "me_vs_others",
});

storage_token_enum!(ConfigTimelineMessagesLayoutAvatarSizeToken, Regular {
    Regular => "regular",
    Small => "small",
    Hidden => "hidden",
});

storage_token_enum!(ConfigTimelineMessagesSenderUsernameToken, OnlyInLargeRooms {
    Always => "always",
    OnlyInLargeRooms => "only_in_large_rooms",
    Never => "never",
});

storage_token_enum!(ConfigTimelineMessageActionsActivationPolicyToken, ActionsButton {
    OnHover => "on_message_hover",
    ActionsButton => "on_button_click",
    Never => "never",
});

storage_token_enum!(ConfigTimelineMediaImageDisplayToken, Always {
    Always => "always",
    OnlyPrivate => "only_private",
    Never => "never",
});

storage_token_enum!(ConfigTimelineRoomHeaderButtonLabelsToken, Adaptive {
    Adaptive => "adaptive",
    Never => "never",
});

storage_token_enum!(ConfigDesktopSystemTrayIconStyleToken, Colorized {
    Colorized => "colorized",
    MonochromeLight => "monochrome_light",
    MonochromeDark => "monochrome_dark",
});

storage_token_enum!(ConfigSecretsProviderToken, SecretService {
    File => "file",
    SecretService => "secret_service",
});

storage_token_enum!(ConfigNotificationsMessageContentPolicyToken, WheneverAvailable {
    Never => "never",
    UnencryptedOnly => "unencrypted_only",
    WheneverAvailable => "whenever_available",
});

storage_token_enum!(ConfigNetworkPresenceStatusPolicyToken, AutomaticPresence {
    AutomaticPresence => "automatic_presence",
    Online => "online",
    Unavailable => "unavailable",
    Offline => "offline",
});

storage_token_enum!(ConfigIntegrationsDbusApiAccessToken, None {
    ReadWrite => "read_write",
    ReadOnly => "read_only",
    None => "none",
});

storage_token_enum!(ConfigComposerInputSendKeyToken, Enter {
    Enter => "enter",
    CtrlEnter => "ctrl_enter",
    ShiftEnter => "shift_enter",
});

storage_token_enum!(ConfigComposerInputAutoReplaceEmojiToken, Always {
    Always => "always",
    OnlyAtEnd => "only_at_end",
    Never => "never",
});

storage_token_enum!(ConfigComposerEmojiPreferredGenderToken, NoPreference {
    NoPreference => "no_preference",
    Woman => "woman",
    Man => "man",
});

storage_token_enum!(ConfigComposerEmojiPreferredSkinToneToken, NoPreference {
    NoPreference => "no_preference",
    Light => "light",
    MediumLight => "medium_light",
    Medium => "medium",
    MediumDark => "medium_dark",
    Dark => "dark",
});

storage_token_enum!(ConfigIntegrationsTranscriptionProviderToken, OpenaiBatch {
    OpenaiBatch => "openai_batch",
    OpenaiRealtime => "openai_realtime",
});
