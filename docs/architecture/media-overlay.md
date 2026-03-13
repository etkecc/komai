# Media Overlay

The media overlay is a full-screen viewer for images and videos opened from the timeline.
It replaces an earlier image-only overlay with a unified viewer that handles all visual media types.

## Overview

When a user clicks an image or video in the timeline, a full-screen `Window` opens showing the media with an action bar (Forward, Open, Copy, Save, Close), gallery navigation (prev/next arrows), and -- for videos -- playback controls (play/pause, seek, volume).

Gallery navigation walks through **all** visual media in the room (images, stickers, and videos) rather than images-only.

## Architecture

### QML layer

| File | Role |
|------|------|
| `resources/qml/dialogs/media/ImageOverlay.qml` | Main overlay window. Handles both images and videos via the `isVideo` property. |
| `resources/qml/dialogs/media/components/ImageOverlayActionButton.qml` | Reusable action button for the action bar. |
| `resources/qml/delegates/PlayableMediaMessage.qml` | Timeline delegate for video/audio messages. Tapping a video opens the media overlay; tapping audio keeps inline playback. |
| `resources/qml/delegates/ImageMessage.qml` | Timeline delegate for images. Opens the overlay via `openImageOverlayWithContext`. |
| `resources/qml/ui/media/MediaControls.qml` | Shared playback controls (seek bar, play/pause, volume, time display). Used both in the overlay and in-timeline for audio. |
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
3. **Encrypted remote media**: full download, decrypted via `mtx::crypto::decrypt_file`, loaded into `QBuffer`, then `setSourceDevice(&buffer)`. Streaming is not possible because AES-256-CTR + HMAC-SHA256 requires the complete ciphertext for HMAC verification.

The `encrypted` Q_PROPERTY lets QML distinguish encrypted from unencrypted media (e.g. for progress indicators).

### Local media proxy (`MediaProxyServer`)

`QMediaPlayer::setSource(QUrl)` uses Qt's internal `QNetworkAccessManager` for HTTP URLs and provides no API to inject custom HTTP headers. The Matrix authenticated media endpoint (`/_matrix/client/v1/media/download/`) requires an `Authorization: Bearer` header. To bridge this gap, `MediaProxyServer` runs a local HTTP server.

**Architecture:**

```
Any consumer ──HTTP GET──> localhost:PORT/m/{per-media-token}.ext
(QMediaPlayer,                      │
 VLC, mpv, etc.)            MediaProxyServer
                                    │  looks up token → (server, media_id)
                                    │
                     ┌──────────────┼──────────────┐
                     │  Adds Authorization header  │
                     │  Streams to consumer        │
                     └──────────────┼──────────────┘
                                    │
                          Homeserver ──> media data
```

**Binding and network safety**: The server binds to `localhost`, which resolves to the loopback interface only — `::1` (IPv6) or `127.0.0.1` (IPv4) depending on the system. This means the proxy is never reachable from the network. URLs also use `localhost` so client and server resolve consistently. httplib uses `getaddrinfo` internally, so both IPv4 and IPv6 loopback work transparently.

**URL scheme**: Each `urlForMxc()` call generates a random token and stores a `token → (server, media_id)` mapping. The proxy URL is `http://localhost:PORT/m/{random-token}.ext` — the mxc URL components never appear in the proxy URL. When a MIME type is provided, the corresponding file extension is appended (e.g. `.mp4`, `.webm`) so that the OS can identify the media type; the extension is cosmetic and ignored by the route handler.

**Token eviction policy: never evict.** Evicting after first use would break seeking (QMediaPlayer makes multiple Range requests) and external players that reconnect. Evicting on overlay close would break external players that outlive the overlay. Tokens accumulate for the session but the memory cost is negligible (a few hundred entries at most). The map resets on logout/exit with the rest of the proxy.

**Lifecycle**: The proxy starts lazily on first media request and runs for the entire logged-in session. It stops on logout / app exit. External players (VLC, mpv) may hold the URL long after the overlay closes, so idle-timeout or stop-on-close would break them.

**Upstream streaming**: Uses `libcurl` directly (`curl_easy_*`) for the upstream HTTPS fetch. For full GET requests, a HEAD request first determines Content-Length; the body is then streamed chunk-by-chunk via httplib's `set_content_provider` (known size, enables seeking) or `set_chunked_content_provider` (unknown size).

**Request handling — multi-layer cache**: Every request (Range or full GET) checks caches before touching the network:

1. **In-memory cache** (per-token `cachedBody` in the token map) — fastest path
2. **Disk cache** (same cache used by `MxcMediaProxy` for in-overlay playback, looked up via `app_paths::cache::mediaMediaFileForMxc`) — avoids re-downloading files already on disk
3. **Upstream fetch** — only when both caches miss

**Range requests**: The proxy forwards the client's `Range` header to upstream. If the server returns **206** (supports Range), the partial content is served directly via a chunked content provider — bypassing httplib's built-in Range processing which would double-process the header and produce 416. If the server returns **200** (no Range support, common on Matrix homeservers), the download is **aborted immediately** and the proxy returns **416** to the client. This signals "Range not satisfiable" so the client falls back to a plain GET (streaming without seeking). The `noRangeSupport` flag is cached per-token so subsequent Range requests return 416 immediately without probing upstream again.

**QMediaPlayer fallback**: QMediaPlayer/FFmpeg does not recover from 416 — it errors out instead of retrying with a plain GET. `MxcMediaProxy` detects this error (`errorOccurred` while `streaming_` is true) and automatically falls back to a full download via `http::client()->download()`, then plays from a local `QBuffer`. This is the same path used for encrypted media. The fallback is attempted only once per playback session (`streamingFallbackAttempted_` flag) to avoid retry loops.

**Streaming GET**: Used for plain GET requests (including client fallback after 416). A HEAD request first determines Content-Length and Content-Type; the body is then streamed chunk-by-chunk via httplib's `set_content_provider` (known size) or `set_chunked_content_provider` (unknown size). This lets `QMediaPlayer` start playback before the full file is downloaded.

**External player Range probe**: Before launching an external player, `openInExternalPlayer()` sends a `Range: bytes=0-0` probe to upstream. If the server responds with **206**, Range is supported and the player is launched with the proxy URL — seeking works. If the server responds with anything else (typically **200**), Range is not supported. In that case, `openInExternalPlayer()` returns `false` and the caller falls back to download-to-cache → open local file. This fallback is necessary because MP4 files (and most container formats) store the moov atom at the end of the file; without Range/seek support, external players cannot read the file index and refuse to play.

**Opening in external players**: `QDesktopServices::openUrl(http://...)` opens the browser because it dispatches on the `http://` URL scheme rather than the media MIME type.  To open media in the correct application, `openInExternalPlayer()` uses platform-specific strategies:

**Linux/FreeBSD:**

1. `xdg-mime query default <mimetype>` — finds the `.desktop` file for the default handler (e.g. `vlc.desktop`).  This is the freedesktop.org-standard query and respects KDE/GNOME/etc. default application settings via `mimeapps.list`.
2. `gio launch <full-desktop-path> <proxy-url>` — preferred launcher.  `gio` is part of `glib2`, which is a near-universal dependency (Qt/KDE apps depend on it transitively).
3. `gtk-launch <desktop-name> <proxy-url>` — fallback.  Part of `gtk3`, which may not be installed on KDE-only systems.
4. `QDesktopServices::openUrl()` — last resort, opens in the browser.

**macOS:**

1. Launch Services lookup — converts the MIME type to a UTI (Uniform Type Identifier) via `UTTypeCreatePreferredIdentifierForTag`, then queries the default viewer application via `LSCopyDefaultApplicationURLForContentType`.  Launches the discovered app with `open -a <app-path> <proxy-url>`.
2. `QDesktopServices::openUrl()` — last resort, opens in the browser.

**Windows:**

1. `AssocQueryStringW` — queries the default executable for the file extension (e.g. `.mp4`) derived from the MIME type, then launches it directly with the proxy URL.
2. `QDesktopServices::openUrl()` — last resort, opens in the browser.

**Images skip the proxy**: Images don't benefit from streaming and player-launch would open the wrong application. When `openMedia()` is called for an image, it bypasses the proxy entirely and uses the download-to-cache → `QDesktopServices::openUrl(file://...)` path, which lets the OS open the default image viewer.

**Key files:**
- `src/ui/MediaProxyServer.h/.cpp` — proxy server implementation
- `src/ui/MxcMediaProxy.cpp` — uses proxy for unencrypted streaming
- `src/timeline/media/TimelineMediaController.cpp` — uses proxy for external player "open"
- `src/timeline/view/TimelineViewManagerMedia.cpp` — uses proxy for `openMedia(mxcUrl)` path

### Timeline video delegate

`PlayableMediaMessage.qml` shows a thumbnail with a dark scrim and a large centered play button for videos.
Tapping checks the `timelineMediaOpenVideosExternal` setting: if enabled, opens directly in the external player via `room.openMedia(eventId)`; otherwise, opens the media overlay.  Audio messages use the same setting to choose between external player and inline controls.
The existing `MxcMedia` + `VideoOutput` remain in the delegate for potential future inline playback.

## Signal flow

```
User clicks video in timeline
    │
    ├─ Settings.timelineMediaOpenVideosExternal == true
    │      │ calls room.openMedia(eventId)
    │      │ → TimelineMediaController::openMedia()
    │      │ → images: download to cache, open local file
    │      │ → video/audio: MediaProxyServer::openInExternalPlayer()
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
    ImageOverlay opens full-screen
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
