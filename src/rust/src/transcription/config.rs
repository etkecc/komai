// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Resolution of the effective transcription config for a given room.

use crate::settings::config::{
    ConfigIntegrationsTranscription, ConfigIntegrationsTranscriptionProviderToken,
};

/// Provider type. Mirrors `ConfigIntegrationsTranscriptionProviderToken` but
/// kept as a separate enum so the transcription module doesn't have to
/// depend on the storage/token machinery beyond a one-way conversion.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TranscriptionProvider {
    OpenaiBatch,
    OpenaiRealtime,
}

impl TranscriptionProvider {
    pub fn from_token(token: &ConfigIntegrationsTranscriptionProviderToken) -> Self {
        match token {
            ConfigIntegrationsTranscriptionProviderToken::OpenaiBatch => Self::OpenaiBatch,
            ConfigIntegrationsTranscriptionProviderToken::OpenaiRealtime => Self::OpenaiRealtime,
        }
    }
}

/// Effective transcription config for a single transcription job. Built by
/// merging the global `ConfigIntegrationsTranscription` with the per-room
/// override entry (if any) and the per-room or global api_key from the
/// secrets backend.
///
/// The composer-side master toggle (`composer.input.transcription.enabled`)
/// is consulted independently by the QML hook — it is *not* part of this
/// config. See `var/plans/composer-voice-transcription.md` § "Config shape".
#[derive(Debug, Clone)]
pub struct ResolvedTranscriptionConfig {
    pub provider: TranscriptionProvider,
    pub api_url: String,
    pub api_key: Option<String>,
    pub model: String,
    /// Empty string means "auto" (let the server detect).
    pub language: String,
    /// Empty string means "no prompt".
    pub prompt: String,
}

impl ResolvedTranscriptionConfig {
    /// Whether this config carries everything we need to make a request.
    /// `api_key` is treated as optional (some local servers don't require
    /// auth); set `require_api_key = true` to enforce its presence.
    pub fn is_ready(&self, require_api_key: bool) -> bool {
        if self.api_url.trim().is_empty() {
            return false;
        }
        if self.model.trim().is_empty() {
            return false;
        }
        if require_api_key && self.api_key.as_deref().unwrap_or("").is_empty() {
            return false;
        }
        true
    }

    /// Heuristic: does this URL point to a provider that requires
    /// authentication? Used by the UI to decide whether to nag for an api_key.
    pub fn url_likely_requires_api_key(api_url: &str) -> bool {
        let lowered = api_url.to_ascii_lowercase();
        // Any well-known cloud host requires auth. Local servers usually don't.
        lowered.contains("api.openai.com")
            || lowered.contains("openai.azure.com")
            || lowered.contains("api.deepgram.com")
            || lowered.contains("api.assemblyai.com")
            || lowered.contains("api.groq.com")
    }
}

/// Default model when none is configured for batch mode. `whisper-1` is the
/// OpenAI cloud model id and is also accepted by every OpenAI-compatible
/// local server we've surveyed (whisper.cpp `whisper-server`, Lemonade,
/// LocalAI, vLLM). Bumping this constant transparently upgrades users who
/// haven't pinned a model in their config.
const DEFAULT_BATCH_MODEL: &str = "whisper-1";

/// Default model when none is configured for realtime mode. `whisper-1`
/// doesn't actually stream — it returns the full transcript at the end —
/// so streaming users would get nothing useful from it. `gpt-4o-mini-transcribe`
/// is OpenAI's cheaper streaming model and is what compatible local
/// servers (Lemonade etc.) tend to alias to their own streaming model.
const DEFAULT_REALTIME_MODEL: &str = "gpt-4o-mini-transcribe";

/// Default api_url when none is configured. Empty so the UI clearly shows
/// "needs configuration" rather than silently aiming at OpenAI cloud.
const DEFAULT_API_URL: &str = "";

fn default_model_for(provider: TranscriptionProvider) -> &'static str {
    match provider {
        TranscriptionProvider::OpenaiBatch => DEFAULT_BATCH_MODEL,
        TranscriptionProvider::OpenaiRealtime => DEFAULT_REALTIME_MODEL,
    }
}

/// Merge globals + per-room override into a fully-populated config.
///
/// `global_api_key` and `room_api_key` come from the secrets backend
/// (loaded separately from the YAML config); per-room wins over global,
/// and an empty string is treated as "absent".
pub fn resolve_for_room(
    global: &ConfigIntegrationsTranscription,
    room_id: &str,
    global_api_key: Option<String>,
    room_api_key: Option<String>,
) -> ResolvedTranscriptionConfig {
    let room_override = global.by_room.get(room_id);

    let pick_str = |room: Option<&Option<String>>, fallback: Option<&String>| -> String {
        if let Some(Some(value)) = room {
            if !value.is_empty() {
                return value.clone();
            }
        }
        fallback.cloned().unwrap_or_default()
    };

    let pick_provider = || -> TranscriptionProvider {
        if let Some(room) = room_override.as_ref() {
            if let Some(token) = room.provider.as_ref() {
                return TranscriptionProvider::from_token(token);
            }
        }
        if let Some(token) = global.provider.as_ref() {
            return TranscriptionProvider::from_token(token);
        }
        TranscriptionProvider::OpenaiBatch
    };

    let api_url = pick_str(
        room_override.map(|r| &r.api_url),
        global.api_url.as_ref(),
    );
    let api_url = if api_url.is_empty() {
        DEFAULT_API_URL.to_owned()
    } else {
        api_url
    };

    let provider = pick_provider();
    let model = pick_str(
        room_override.map(|r| &r.model),
        global.model.as_ref(),
    );
    let model = if model.is_empty() {
        default_model_for(provider).to_owned()
    } else {
        model
    };

    let language = pick_str(
        room_override.map(|r| &r.language),
        global.language.as_ref(),
    );
    let prompt = pick_str(
        room_override.map(|r| &r.prompt),
        global.prompt.as_ref(),
    );

    let api_key = match (
        room_api_key.as_deref().filter(|s| !s.is_empty()),
        global_api_key.as_deref().filter(|s| !s.is_empty()),
    ) {
        (Some(value), _) => Some(value.to_owned()),
        (None, Some(value)) => Some(value.to_owned()),
        (None, None) => None,
    };

    ResolvedTranscriptionConfig {
        provider,
        api_url,
        api_key,
        model,
        language,
        prompt,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::settings::config::ConfigIntegrationsTranscriptionOverrides;
    use std::collections::BTreeMap;

    fn empty_globals() -> ConfigIntegrationsTranscription {
        ConfigIntegrationsTranscription {
            provider: None,
            api_url: None,
            api_key: None,
            model: None,
            language: None,
            prompt: None,
            by_room: BTreeMap::new(),
        }
    }

    #[test]
    fn defaults_are_sane() {
        let resolved = resolve_for_room(&empty_globals(), "!room:server", None, None);
        assert_eq!(resolved.provider, TranscriptionProvider::OpenaiBatch);
        assert!(resolved.api_url.is_empty());
        assert!(resolved.api_key.is_none());
        assert_eq!(resolved.model, "whisper-1");
        assert!(resolved.language.is_empty());
        assert!(resolved.prompt.is_empty());
    }

    #[test]
    fn room_override_wins_over_global() {
        let mut globals = empty_globals();
        globals.api_url = Some("https://api.openai.com/v1".to_owned());
        globals.model = Some("whisper-1".to_owned());
        globals.language = Some("en".to_owned());

        let mut overrides = ConfigIntegrationsTranscriptionOverrides::default();
        overrides.model = Some("gpt-4o-mini-transcribe".to_owned());
        overrides.language = Some("bg".to_owned());
        globals
            .by_room
            .insert("!room:server".to_owned(), overrides);

        let resolved = resolve_for_room(&globals, "!room:server", None, None);
        assert_eq!(resolved.api_url, "https://api.openai.com/v1");
        assert_eq!(resolved.model, "gpt-4o-mini-transcribe");
        assert_eq!(resolved.language, "bg");
    }

    #[test]
    fn unknown_room_falls_back_to_global() {
        let mut globals = empty_globals();
        globals.api_url = Some("https://global".to_owned());
        let resolved = resolve_for_room(&globals, "!unknown:server", None, None);
        assert_eq!(resolved.api_url, "https://global");
    }

    #[test]
    fn room_api_key_wins_over_global_api_key() {
        let resolved = resolve_for_room(
            &empty_globals(),
            "!room:server",
            Some("global-key".to_owned()),
            Some("room-key".to_owned()),
        );
        assert_eq!(resolved.api_key.as_deref(), Some("room-key"));
    }

    #[test]
    fn empty_room_api_key_falls_back_to_global() {
        let resolved = resolve_for_room(
            &empty_globals(),
            "!room:server",
            Some("global-key".to_owned()),
            Some(String::new()),
        );
        assert_eq!(resolved.api_key.as_deref(), Some("global-key"));
    }

    #[test]
    fn is_ready_requires_url_and_model() {
        let mut globals = empty_globals();
        let resolved = resolve_for_room(&globals, "!room:server", None, None);
        // model has a default so the only missing piece is api_url
        assert!(!resolved.is_ready(false));

        globals.api_url = Some("https://server".to_owned());
        let resolved = resolve_for_room(&globals, "!room:server", None, None);
        assert!(resolved.is_ready(false));
        assert!(!resolved.is_ready(true)); // api_key required by caller
    }

    #[test]
    fn url_heuristic_recognises_openai_cloud() {
        assert!(ResolvedTranscriptionConfig::url_likely_requires_api_key(
            "https://api.openai.com/v1"
        ));
        assert!(!ResolvedTranscriptionConfig::url_likely_requires_api_key(
            "http://localhost:8080/v1"
        ));
    }
}
