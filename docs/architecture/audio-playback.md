# Audio Playback

Audio messages stay inline in the timeline. They do not use the full-screen media overlay.

## Overview

The inline audio player is designed around two goals:

- one-click playback for both cached and uncached audio
- a stable layout that does not jump around as controls appear or disappear

The first tap on an uncached audio message starts buffering and then begins playback inline.
When audio is already loaded, tapping the inline player toggles play/pause.

Audio playback remains room-local in the first implementation. If you switch rooms, playback stops
instead of following you elsewhere in the app. A future enhancement may move active playback into a
small persistent bar beneath the room header, but that is explicitly out of scope for v1.

## Architecture

### QML layer

Audio and video now use separate timeline delegates:

- `resources/qml/delegates/AudioMessage.qml`
  Hosts the inline audio player entry point used for audio and voice messages.
- `resources/qml/delegates/VideoMessage.qml`
  Keeps the timeline video preview path that opens the media overlay or an external player.
- `resources/qml/ui/media/`
  Hosts the inline audio controls and the lightweight registry/helper used to ensure only one clip
  plays at a time.

The inline player keeps its full control surface visible from first render:

- play/pause
- progress/seek
- time display
- optional volume/mute controls if enabled in the final UI
- playback-speed buttons

Keeping these controls visible from the start allows speed changes before playback begins and avoids
message reflow caused by controls popping in later.

### C++ layer

- `src/ui/MxcMediaProxy.h/.cpp`
  Provides the Matrix-aware playback backend. It wraps `QMediaPlayer` and handles cache lookup,
  decryption, HTTP proxy streaming, and fallback to buffered playback.
- `src/timeline/media/TimelineMediaController.cpp`
  Handles opening audio externally when the user chooses that behavior.

The implementation continues to use the existing Qt media player stack rather than introducing a new
decoder or custom playback backend.

## Playback behavior

### One-click inline playback

When inline playback is enabled:

- first tap on uncached audio starts buffering and auto-plays when ready
- tap on loaded audio toggles play/pause

When the separate audio external-open setting is enabled:

- tapping the audio message opens it in the system's default external audio player instead

### Single active clip

Only one audio clip should play at a time. Starting a second clip stops or pauses the currently active
one before the new clip starts.

This does not imply a full app-wide media session manager. For v1, a lightweight coordination layer is
enough as long as the user never hears overlapping inline clips.

## Playback speed

Audio playback supports a persisted default playback speed plus per-player temporary overrides.

### Default setting

The settings UI exposes a `Default playback speed` control:

- default: `1.0x`
- allowed values: `0.5x`, `1.0x`, `1.5x`, `2.0x`, `2.5x`, `3.0x`
- step: `0.5x`

This default is applied to inline audio players and updates open ones immediately.

### Per-player controls

Each inline player shows exactly four speed buttons, derived from the saved default speed.
The row always includes `1.0x` and the configured default.

Runtime speed changes:

- are local to that player
- are not persisted
- do not reshuffle the visible button row during normal per-player speed changes

Changing the saved default speed is different: existing inline players immediately adopt the new
default speed and regenerate their four-button row.

Examples:

- default `0.5x` or `1.0x` -> `0.5x`, `1.0x`, `1.5x`, `2.0x`
- default `1.5x` or `2.0x` -> `1.0x`, `1.5x`, `2.0x`, `2.5x`
- default `2.5x` or `3.0x` -> `1.5x`, `2.0x`, `2.5x`, `3.0x`

## Keyboard behavior

Inline audio keyboard controls are intentionally scoped to the active inline player, not the whole app.

- `Space` toggles play/pause
- `Left` jumps back 5 seconds
- `Right` jumps forward 5 seconds

The player takes focus when you interact with it, and its sliders avoid stealing focus. This keeps the
shortcuts available for the active clip without conflicting with normal typing in the message composer
or with the separate key handling used by the media overlay.

## External player behavior

Audio now has its own external-open setting rather than piggybacking on the video setting.

When enabled, tapping audio opens it in the default external audio player.
When disabled, tapping audio keeps playback inline in the timeline.

For unencrypted audio, external-open prefers the HTTP media proxy when the homeserver supports Range
requests. This avoids forcing a full download before launching the external player.

Encrypted or non-seekable audio falls back to the local cache path instead.

If direct launcher resolution fails, Komai should return to the caller's local-file fallback rather
than opening the proxy URL in the browser.

## Relationship to Media Overlay

The media overlay remains focused on visual media only:

- images
- stickers
- video

Audio intentionally stays outside that surface because the overlay's gallery navigation, full-screen
layout, and action model are all built around visual content rather than compact inline playback.

See also: [Media Overlay](media-overlay.md)
