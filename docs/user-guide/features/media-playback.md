# 🎬 Media Playback

This page covers how Komai handles media playback on the timeline.

See a [🖼️ Screenshot of the timeline media viewer](../screenshots/timeline-media-viewer.webp).


## 🎞️ Inline GIF Video Playback

Komai can automatically play short video clips that behave like GIFs directly on the timeline,
without requiring a click. These play muted and looped, just like animated GIFs.

Clicking a GIF video opens it in the Media Overlay (or external player), like any other video.


### 🔍 Detection

A video message is treated as a GIF-like video when it is small enough (at most 1 MB) and
at least one of the following is true:

- The file name starts with `gif-` — a common naming convention used by bridges
- The reported duration is zero or at most 3 seconds


### ⚙️ Setting

The feature is controlled by the **Inline auto-play of GIF videos** toggle in
**Settings → Timeline → Video handling**. It is enabled by default.

Config key: `timeline.media.autoplay_gif_videos`


### 🔇 Audio

GIF videos play silently — no audio output is allocated.
