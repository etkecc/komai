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

`MxcMediaProxy::startDownload()` currently downloads the full file before playback begins:

1. **Cached media**: loaded directly from disk into a `QBuffer`.
2. **Unencrypted remote media**: downloaded via `http::client()->download()`, saved to cache, loaded into `QBuffer`.
3. **Encrypted remote media**: downloaded, decrypted via `mtx::crypto::decrypt_file`, loaded into `QBuffer`.

All paths call `setSourceDevice(&buffer)` once the data is ready.

The `encrypted` Q_PROPERTY lets QML distinguish encrypted from unencrypted media (e.g. for future progress indicators).

### Future: HTTP streaming

HTTP streaming for unencrypted media is not yet possible because `QMediaPlayer::setSource(QUrl)` cannot set custom HTTP headers, and the Matrix authenticated media endpoint (`/_matrix/client/v1/media/download/`) requires an `Authorization: Bearer` header. The `?access_token=` query parameter fallback is not supported by all servers. See the project memory for alternative approaches (streaming `QIODevice`, local proxy, GStreamer pipeline, or mtxclient patch).

### Timeline video delegate

`PlayableMediaMessage.qml` shows a thumbnail with a dark scrim and a large centered play button for videos.
Tapping opens the media overlay rather than playing inline (audio messages still play inline).
The existing `MxcMedia` + `VideoOutput` remain in the delegate for potential future inline playback.

## Signal flow

```
User clicks video in timeline
    │
    ▼
PlayableMediaMessage.TapHandler
    │ calls TimelineManager.openMediaOverlayWithContext(room, url, eventId, width, height, type, duration, thumbnailUrl, timeline, timelineView)
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
