# Media Overlay

The media overlay is a full-screen viewer for images and videos opened from the timeline.
It replaces an earlier image-only overlay with a unified viewer that handles all visual media types.
Audio intentionally stays inline in the timeline; see [Audio Playback](audio-playback.md).

## Overview

When a user clicks an image or video in the timeline, a full-screen `Window` opens showing the media with an action bar (Forward, Open, Copy, Save, Close), gallery navigation (prev/next arrows), and -- for videos -- playback controls (play/pause, seek, volume).

Gallery navigation walks through **all** visual media in the room (images, stickers, and videos) rather than images-only.

## Architecture

### QML layer

| File | Role |
|------|------|
| `resources/qml/dialogs/media/MediaOverlay.qml` | Main overlay window. Handles both images and videos via the `isVideo` property. |
| `resources/qml/dialogs/media/components/ImageOverlayActionButton.qml` | Reusable action button for the action bar. |
| `resources/qml/delegates/VideoMessage.qml` | Timeline delegate for videos. Tapping a video opens the media overlay or the external player, depending on settings. |
| `resources/qml/delegates/ImageMessage.qml` | Timeline delegate for images. Opens the overlay via `openImageOverlayWithContext`. |
| `resources/qml/ui/media/MediaControls.qml` | Playback controls used by the overlay video player. |
| `resources/qml/shell/components/RootEventRouter.qml` | Routes `showImageOverlay` and `showMediaOverlay` signals to overlay window creation. |

### C++ layer

| File | Role |
|------|------|
| `src/timeline/navigation/TimelineModelNavigation.cpp` | `adjacentMediaEvent()` and `countNearbyMedia()` -- walk the event store to find nearby images, stickers, and videos for gallery navigation. |
| `src/timeline/TimelineModel.h` | Declares the above as `Q_INVOKABLE` methods on the room model. |
| `src/timeline/TimelineViewManager.h` | Declares `openMediaOverlay[WithContext]()` and the `showMediaOverlay` signal. |
| `src/timeline/view/TimelineViewManagerMedia.cpp` | Implements the overlay-launch methods. Also contains legacy `openImageOverlay` for non-timeline callers (avatar clicks, link previews). |
| `src/ui/MxcMediaProxy.h/.cpp` | `MxcMedia` QML type. Wraps `QMediaPlayer` for Matrix media. Handles download, decryption, buffering, and HTTP streaming. |

## Media type detection

The overlay receives a `mediaType` property (integer matching `MtxEvent.VideoMessage`, `MtxEvent.ImageMessage`, etc.).
The readonly `isVideo` property switches the display mode:

- **Image mode** (`isVideo == false`): shows `Image` + `MxcAnimatedImage` (for GIFs), with zoom/pan/pinch support.
- **Video mode** (`isVideo == true`): shows a thumbnail with a centered play button overlay, a `VideoOutput` for playback, and `MediaControls` at the bottom.

## Gallery navigation

`adjacentMediaEvent(eventId, direction)` in `TimelineModelNavigation.cpp` linearly scans the event store (up to 10 000 events) for the next image, sticker, or video in the given direction.

The returned `QVariantMap` contains:

| Key | Type | Description |
|-----|------|-------------|
| `eventId` | string | Matrix event ID |
| `url` | string | `mxc://` media URL |
| `originalWidth` | uint64 | Media width in pixels |
| `proportionalHeight` | double | height / width ratio |
| `type` | int | `MtxEvent` enum value (ImageMessage, VideoMessage, Sticker) |
| `duration` | uint64 | Duration in milliseconds (0 for images) |
| `thumbnailUrl` | string | `mxc://` thumbnail URL |

When navigating away from a playing video, `navigateTo()` stops playback before switching.

`countNearbyMedia()` counts available media in each direction to trigger server backpagination when the gallery reserve drops below 5.

## Video playback

### In-overlay playback

The overlay contains its own `MxcMedia` instance (`overlayMediaPlayer`) and `VideoOutput`.
The play button overlay starts download/playback on click.
`MediaControls` at the bottom provides seek, play/pause, volume, and time display.

### Streaming vs. buffered playback

`MxcMediaProxy::startDownload()` handles three cases:

1. **Cached media**: loaded directly from disk into a `QBuffer`, then `setSourceDevice(&buffer)`.
2. **Unencrypted remote media**: streamed via the local media proxy (see below). `QMediaPlayer::setSource()` receives a `http://localhost:PORT/m/{token}.ext` URL, enabling HTTP buffering and seeking without downloading the full file first.
3. **Runtime-backed full download**: fetched through `MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(...)`, loaded into `QBuffer`, then `setSourceDevice(&buffer)`. This is the path used when proxy streaming is not used or when playback falls back to buffered media. Encrypted media also belongs in this full-download class because it cannot be streamed incrementally.

The `encrypted` Q_PROPERTY lets QML distinguish encrypted from unencrypted media (e.g. for progress indicators).

### Local media proxy (Rust-side)

`QMediaPlayer::setSource(QUrl)` uses Qt's internal `QNetworkAccessManager` for HTTP URLs and provides no API to inject custom HTTP headers. The Matrix authenticated media endpoint (`/_matrix/client/v1/media/download/`) requires an `Authorization: Bearer` header. To bridge this gap, a Rust-side media proxy runs a local HTTP server using hyper.

**Architecture:**

```
Any consumer ──HTTP GET──> localhost:PORT/m/{per-media-token}.ext
(QMediaPlayer,                      │
 VLC, mpv, etc.)             media proxy (Rust/hyper)
                                    │  looks up token → (server_name, media_id)
                                    │  reads fresh access_token from matrix-sdk Client
                                    │
                     ┌──────────────┼──────────────┐
                     │  Adds Authorization header  │
                     │  Streams response via reqwest│
                     └──────────────┼──────────────┘
                                    │
                          Homeserver ──> media data
```

**Binding and network safety**: The server binds to `127.0.0.1` (IPv4 loopback), so the proxy is never reachable from the network. URLs use `localhost` so that Qt's `QNetworkAccessManager` resolves via `getaddrinfo` — if `::1` fails (no IPv6 server), it falls back to `127.0.0.1` transparently.

**URL scheme**: Each `registerTimelineMediaProxyUrl()` call generates a random 24-character alphanumeric token and stores a `token → (server_name, media_id)` mapping. The proxy URL is `http://localhost:PORT/m/{random-token}.ext` — the mxc URL components never appear in the proxy URL. The file extension is cosmetic (derived from the MIME type) and ignored by the route handler.

**Token eviction policy: never evict.** Evicting after first use would break seeking (QMediaPlayer makes multiple Range requests) and external players that reconnect. Evicting on overlay close would break external players that outlive the overlay. Tokens accumulate for the session but the memory cost is negligible (a few hundred entries at most). The registry is destroyed on logout/exit with the rest of the proxy.

**Lifecycle**: The proxy starts during backend initialization (`MatrixBackendRuntimeService::startMediaProxy()`) and runs for the entire logged-in session. It stops on logout / app exit via `stopMediaProxy()`, and is also auto-stopped as a safety net during `stop_backend()`.

**Upstream streaming**: Uses `reqwest` (already a dependency for matrix-sdk) for the upstream HTTPS fetch. The response body is streamed chunk-by-chunk via `bytes_stream()` and forwarded through hyper's `StreamBody` — the proxy never buffers the full file in memory.

**Range requests**: The proxy forwards the client's `Range` header to upstream as-is. If the server returns **206** (partial content), the proxy forwards the partial response — enabling seeking. If the server returns **200** despite the Range header (no Range support), the proxy forwards the full response and lets QMediaPlayer cope. On streaming errors, `MxcMediaProxy` automatically falls back to the full-download buffer path.

**QMediaPlayer fallback**: When streaming fails (error while `streaming_` is true), `MxcMediaProxy` automatically falls back to the runtime-backed full-download path, then plays from a local `QBuffer`. This is the same path used for encrypted media. The fallback is attempted only once per playback session (`streamingFallbackAttempted_` flag) to avoid retry loops.

**Key files:**
- `src/rust/src/matrix_backend/runtime_media_proxy.rs` — proxy server implementation (Rust)
- `src/ui/MxcMediaProxy.cpp` — uses proxy for unencrypted streaming, fallback to buffer
- `src/matrix/backend/MatrixBackendRuntimeService.h/.cpp` — C++ FFI wrappers for proxy start/register/stop

### Timeline video delegate

`VideoMessage.qml` shows a thumbnail with a dark scrim and a large centered play button for videos.
Tapping checks the `timelineMediaOpenVideosExternal` setting: if enabled, opens directly in the external player via `room.openMedia(eventId)`; otherwise, opens the media overlay.
The existing `MxcMedia` + `VideoOutput` remain in the delegate for potential future inline playback.

Audio messages are handled separately by the inline audio player and do not enter the overlay path.
See [Audio Playback](audio-playback.md).

## Signal flow

```
User clicks video in timeline
    │
    ├─ Settings.timelineMediaOpenVideosExternal == true
    │      │ calls room.openMedia(eventId)
    │      │ → TimelineMediaController::openMedia()
    │      │ → images: download to cache, open local file
    │      │ → video: MediaProxyServer::openInExternalPlayer()
    │      │     → platform-specific player launch (see above) / browser fallback
    │
    └─ Settings.timelineMediaOpenVideosExternal == false (default)
           │ calls TimelineManager.openMediaOverlayWithContext(...)
           │
           ▼
    TimelineViewManager::openMediaOverlayWithContext()  (C++)
           │ emits showMediaOverlay signal
           │
           ▼
    RootEventRouter.onShowMediaOverlay()  (QML)
           │ creates ImageOverlay window via ComponentCatalog
           │ sets mediaType, mediaDuration, thumbnailUrl properties
           │
           ▼
    MediaOverlay opens full-screen
           │ isVideo == true → shows video thumbnail + play button
           │ User clicks play → overlayMediaPlayer.startDownload()
           │ MxcMediaProxy streams or buffers depending on encryption
           │ VideoOutput renders frames, MediaControls show progress
           │
           ▼
    Gallery navigation:
        room.adjacentMediaEvent(eventId, ±1) → walks event store
        returns QVariantMap with type/duration/thumbnailUrl
        navigateTo() stops current video, switches display mode
```

## Future: minimize/collapse (picture-in-picture)

Not yet implemented. The idea: add a "Minimize" button to the action bar that collapses the full-screen overlay into a small floating window (e.g. 320x180) anchored to a corner. The mini-player would show the video with a small expand/close button. This avoids the overlay blocking the whole app while a video plays in the background.

Implementation approach: create a secondary `Window` with `Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint`, transfer the `MxcMedia` and `VideoOutput` to it, and hide the full overlay. Expanding would reverse the process.

## Legacy image overlay path

The original `openImageOverlay` / `showImageOverlay` signal path is preserved for callers that don't have video metadata (avatar clicks in `UserProfile.qml`, image links in `MatrixText.qml`, VoIP bars).
These open the overlay with `mediaType = -1`, which defaults to image mode.
