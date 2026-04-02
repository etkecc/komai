// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub trait StorageToken: Clone + Default {
    fn from_storage_str(value: &str) -> Self;
}

macro_rules! storage_token_enum {
    ($name:ident { $($variant:ident => $token:literal),+ $(,)? }) => {
        #[derive(Clone, Debug, PartialEq, Eq)]
        pub enum $name {
            $($variant,)+
            Raw(String),
        }

        impl Default for $name {
            fn default() -> Self {
                Self::Raw(String::new())
            }
        }

        impl $name {
            pub fn to_storage_string(&self) -> String {
                match self {
                    $(Self::$variant => $token.to_owned(),)+
                    Self::Raw(value) => value.clone(),
                }
            }
        }

        impl StorageToken for $name {
            fn from_storage_str(value: &str) -> Self {
                match value.trim() {
                    $($token => Self::$variant,)+
                    raw => Self::Raw(raw.to_owned()),
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

storage_token_enum!(ConfigUiInputModeToken {
    Text => "text",
    Touch => "touch",
});

storage_token_enum!(ConfigUiScrollbarPolicyToken {
    WhenNeeded => "when_needed",
    Never => "never",
    Always => "always",
});

storage_token_enum!(ConfigUiDefaultAvatarStyleToken {
    BoringAvatarsBauhaus => "boring_avatars_bauhaus",
    BoringAvatarsBeam => "boring_avatars_beam",
    BoringAvatarsMarble => "boring_avatars_marble",
    LetterInitial => "letter_initial",
    UserIcon => "user_icon",
});

storage_token_enum!(ConfigSidebarsRoomListLastMessagePreviewToken {
    Always => "always",
    OnlyUnencrypted => "only_unencrypted",
    Never => "never",
});

storage_token_enum!(ConfigSidebarsRoomListSortToken {
    UnreadFirstRecent => "unread_first_recent",
    UnreadFirstAlpha => "unread_first_alpha",
    Recent => "recent",
    Alphabetical => "alphabetical",
});

storage_token_enum!(ConfigSidebarsRoomListUnreadDetectionPolicyToken {
    AnyEvent => "any_event",
    MessagesOnly => "messages_only",
});

storage_token_enum!(ConfigTimelineMessagesStyleToken {
    Plain => "plain",
    Bubbles => "bubbles",
});

storage_token_enum!(ConfigTimelineMessagesPositioningToken {
    OpposingBySender => "opposing_by_sender",
    AllLeft => "all_left",
    AllRight => "all_right",
});

storage_token_enum!(ConfigTimelineUserColorCodingPolicyToken {
    AdaptiveByRoomSize => "adaptive_by_room_size",
    MeVsOthers => "me_vs_others",
});

storage_token_enum!(ConfigTimelineMessagesSenderUsernameToken {
    Always => "always",
    OnlyInLargeRooms => "only_in_large_rooms",
    Never => "never",
});

storage_token_enum!(ConfigTimelineMessageActionsActivationPolicyToken {
    OnHover => "on_message_hover",
    ActionsButton => "on_button_click",
    Never => "never",
});

storage_token_enum!(ConfigTimelineMediaImageDisplayToken {
    Always => "always",
    OnlyPrivate => "only_private",
    Never => "never",
});

storage_token_enum!(ConfigSecretsProviderToken {
    File => "file",
    SecretService => "secret_service",
});

storage_token_enum!(ConfigNotificationsMessageContentPolicyToken {
    Never => "never",
    UnencryptedOnly => "unencrypted_only",
    WheneverAvailable => "whenever_available",
});

storage_token_enum!(ConfigNetworkPresenceStatusPolicyToken {
    Automatic => "automatic",
    Online => "online",
    Unavailable => "unavailable",
    Offline => "offline",
});

storage_token_enum!(ConfigIntegrationsDbusApiAccessToken {
    Full => "full",
    ReadOnly => "read_only",
    None => "none",
});

storage_token_enum!(ConfigComposerInputSendKeyToken {
    Enter => "enter",
    CtrlEnter => "ctrl_enter",
    ShiftEnter => "shift_enter",
});

storage_token_enum!(ConfigComposerInputAutoReplaceEmojiToken {
    Never => "never",
    CompleteWord => "complete_word",
    CompleteWordAndColon => "complete_word_and_colon",
});

storage_token_enum!(ConfigComposerEmojiPreferredGenderToken {
    Neutral => "neutral",
    Woman => "woman",
    Man => "man",
});

storage_token_enum!(ConfigComposerEmojiPreferredSkinToneToken {
    Neutral => "neutral",
    Light => "light",
    MediumLight => "medium_light",
    Medium => "medium",
    MediumDark => "medium_dark",
    Dark => "dark",
});
