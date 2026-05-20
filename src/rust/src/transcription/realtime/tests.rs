// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use crate::transcription::config::TranscriptionProvider;

fn cfg(model: &str, language: &str, prompt: &str) -> ResolvedTranscriptionConfig {
    ResolvedTranscriptionConfig {
        provider: TranscriptionProvider::OpenaiRealtime,
        api_url: "https://api.openai.com/v1".to_owned(),
        api_key: Some("sk-test".to_owned()),
        model: model.to_owned(),
        language: language.to_owned(),
        prompt: prompt.to_owned(),
    }
}

#[test]
fn ws_url_is_derived_from_https_base() {
    assert_eq!(
        build_ws_url("https://api.openai.com/v1").unwrap(),
        "wss://api.openai.com/v1/realtime?intent=transcription"
    );
    assert_eq!(
        build_ws_url("https://api.openai.com/v1/").unwrap(),
        "wss://api.openai.com/v1/realtime?intent=transcription"
    );
    assert_eq!(
        build_ws_url("http://localhost:8000/v1").unwrap(),
        "ws://localhost:8000/v1/realtime?intent=transcription"
    );
}

#[test]
fn ws_url_passes_through_existing_realtime_path() {
    assert_eq!(
        build_ws_url("https://example.com/v1/realtime").unwrap(),
        "wss://example.com/v1/realtime?intent=transcription"
    );
    assert_eq!(
        build_ws_url("wss://example.com/v1/realtime").unwrap(),
        "wss://example.com/v1/realtime?intent=transcription"
    );
    assert_eq!(
        build_ws_url("wss://example.com/v1").unwrap(),
        "wss://example.com/v1/realtime?intent=transcription"
    );
}

#[test]
fn ws_url_preserves_existing_query_params() {
    assert_eq!(
        build_ws_url("wss://example.com/v1/realtime?foo=bar").unwrap(),
        "wss://example.com/v1/realtime?foo=bar&intent=transcription"
    );
}

#[test]
fn ws_url_does_not_duplicate_intent_param() {
    assert_eq!(
        build_ws_url("wss://example.com/v1/realtime?intent=transcription").unwrap(),
        "wss://example.com/v1/realtime?intent=transcription"
    );
}

#[test]
fn ws_url_rejects_unknown_scheme() {
    assert!(build_ws_url("ftp://nope").is_err());
    assert!(build_ws_url("").is_err());
}

#[test]
fn session_update_uses_ga_nested_shape() {
    let payload = build_session_update(&cfg("gpt-4o-mini-transcribe", "en", "vocab"));
    assert_eq!(payload["type"], "session.update");
    assert_eq!(payload["session"]["type"], "transcription");
    let input = &payload["session"]["audio"]["input"];
    assert_eq!(input["format"]["type"], "audio/pcm");
    assert_eq!(input["format"]["rate"], REALTIME_SAMPLE_RATE_HZ);
    assert_eq!(input["transcription"]["model"], "gpt-4o-mini-transcribe");
    assert_eq!(input["transcription"]["language"], "en");
    assert_eq!(input["transcription"]["prompt"], "vocab");
    assert_eq!(input["turn_detection"]["type"], "server_vad");
    assert_eq!(
        input["turn_detection"]["silence_duration_ms"],
        VAD_SILENCE_DURATION_MS
    );
}

#[test]
fn session_update_omits_turn_detection_for_gpt_realtime_whisper() {
    let payload = build_session_update(&cfg("gpt-realtime-whisper", "en", ""));
    let input = &payload["session"]["audio"]["input"];
    assert!(input.get("turn_detection").is_none());
    assert_eq!(input["transcription"]["model"], "gpt-realtime-whisper");
}

#[test]
fn session_update_omits_empty_language_and_prompt() {
    let payload = build_session_update(&cfg("gpt-4o-mini-transcribe", "", ""));
    let transcription = &payload["session"]["audio"]["input"]["transcription"];
    assert!(transcription.get("language").is_none());
    assert!(transcription.get("prompt").is_none());
}

#[test]
fn session_update_omits_auto_language() {
    let payload = build_session_update(&cfg("gpt-4o-mini-transcribe", "auto", ""));
    let transcription = &payload["session"]["audio"]["input"]["transcription"];
    assert!(transcription.get("language").is_none());
}

#[test]
fn handle_delta_event_emits_delta() {
    let (tx, mut rx) = mpsc::unbounded_channel();
    let event = r#"{"type":"conversation.item.input_audio_transcription.delta","item_id":"i1","delta":"Hel"}"#;
    let outcome = handle_server_event(event, &tx);
    assert!(matches!(outcome, ServerEventOutcome::Continue));
    match rx.try_recv().unwrap() {
        SessionEvent::Delta(s) => assert_eq!(s, "Hel"),
        other => panic!("unexpected event: {other:?}"),
    }
}

#[test]
fn handle_completed_event_emits_completed_and_signals_end() {
    let (tx, mut rx) = mpsc::unbounded_channel();
    let event = r#"{"type":"conversation.item.input_audio_transcription.completed","item_id":"i1","transcript":"Hello, world."}"#;
    let outcome = handle_server_event(event, &tx);
    assert!(matches!(outcome, ServerEventOutcome::Completed));
    match rx.try_recv().unwrap() {
        SessionEvent::Completed(s) => assert_eq!(s, "Hello, world."),
        other => panic!("unexpected event: {other:?}"),
    }
}

#[test]
fn handle_error_event_classifies_unauthorized() {
    let (tx, mut rx) = mpsc::unbounded_channel();
    let event = r#"{"type":"error","error":{"code":"invalid_api_key","message":"Invalid API key"}}"#;
    let outcome = handle_server_event(event, &tx);
    assert!(matches!(outcome, ServerEventOutcome::Failed));
    match rx.try_recv().unwrap() {
        SessionEvent::Failed(err) => {
            assert_eq!(err.code, super::super::TranscriptionErrorCode::Unauthorized);
            assert!(err.message.contains("Invalid API key"));
        }
        other => panic!("unexpected event: {other:?}"),
    }
}

#[test]
fn handle_error_event_falls_back_to_server_error() {
    let (tx, mut rx) = mpsc::unbounded_channel();
    let event = r#"{"type":"error","error":{"code":"rate_limit_exceeded","message":"Slow down"}}"#;
    let outcome = handle_server_event(event, &tx);
    assert!(matches!(outcome, ServerEventOutcome::Failed));
    match rx.try_recv().unwrap() {
        SessionEvent::Failed(err) => {
            assert_eq!(err.code, super::super::TranscriptionErrorCode::ServerError);
            assert!(err.message.contains("Slow down"));
        }
        other => panic!("unexpected event: {other:?}"),
    }
}

#[test]
fn handle_unknown_event_is_ignored() {
    let (tx, mut rx) = mpsc::unbounded_channel();
    let event = r#"{"type":"session.created","session":{"id":"sess_1"}}"#;
    let outcome = handle_server_event(event, &tx);
    assert!(matches!(outcome, ServerEventOutcome::Continue));
    assert!(rx.try_recv().is_err());
}

#[test]
fn handle_invalid_json_is_ignored() {
    let (tx, mut rx) = mpsc::unbounded_channel();
    let outcome = handle_server_event("not json", &tx);
    assert!(matches!(outcome, ServerEventOutcome::Continue));
    assert!(rx.try_recv().is_err());
}
