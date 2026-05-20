# Notifications

This page documents how Komai constructs and dispatches desktop notifications, with emphasis on the Linux freedesktop path (the most moving parts).

## Scope

- Applies to system-level notifications: the freedesktop dbus `Notify` (Linux), WinToast (Windows), `UNUserNotification` (macOS).
- In-app indicators (unread badges, attention pulses, window-title counts) are out of scope. For user-facing toggles see [Notifications feature page](../user-guide/features/notifications.md).

## Pipeline Overview

From "Matrix event arrives" to "system notification appears":

1. `runtime_notifications.rs::notification_item_from_sdk` builds a `MatrixNotificationItem` from a `matrix_sdk_ui::notification_client::NotificationItem`.
2. `to_notification_markup(&summary.formatted_body)` normalizes the Matrix HTML to the freedesktop body-markup subset. Applied unconditionally; Win/Mac ignore the resulting field and use `plain_body`.
3. The item crosses the cxx-bridge as a `NotificationPayload`.
4. `NotificationsManager::postNotification` (platform-specific) reads the payload and calls the OS notification API.

Primary callsites:

- `src/rust/src/matrix_backend/runtime_notifications.rs` (payload construction, spoiler short-circuit detection)
- `src/rust/src/html_processor/notification/` (body markup normalizer + sibling tests)
- `src/notifications/Manager.cpp` (cross-platform helpers: spoiler replacement string, message-template formatting)
- `src/notifications/ManagerLinux.cpp` (dbus path)
- `src/notifications/ManagerWin.cpp` (WinToast)
- `src/notifications/ManagerMac.cpp` (UNUserNotification)

## Body Markup Normalization (Linux)

`src/rust/src/html_processor/notification/mod.rs::to_notification_markup` walks the Matrix `formatted_body` and emits the freedesktop spec's body-markup subset: `<b>`, `<i>`, `<u>`, `<a href="...">`. Everything else is converted or dropped:

- `<strong>` becomes `<b>`; `<em>` becomes `<i>`.
- `<p>...</p>` produces content followed by `\n\n` after close.
- `<br/>`, `<br>` produce `\n`.
- `<li>X</li>` produces `• X\n`. `<ul>`, `<ol>` wrappers are dropped (numbering is not preserved).
- `<h1>` through `<h6>`, `<blockquote>`, `<pre>` produce content followed by `\n\n` after close.
- `<code>` (inline) keeps content, drops tags.
- `<mx-reply>...</mx-reply>` removes the entire subtree.
- `<del>`, `<s>`, `<strike>`, `<span>`, `<font>`, `<div>`, `<sup>`, `<sub>`, `<details>`, `<summary>`, table machinery: tags dropped, content kept.
- `<img>` is dropped entirely. Inline images are handled separately, see below.
- Unknown tags: dropped, content kept.

`<a>` is the only attribute-bearing tag preserved. Hrefs are validated against an allow-list of schemes (`http`, `https`, `mailto`, `matrix`); anchors with a disallowed or missing href are dropped and the inner text is emitted bare. Output is capped at 1024 characters with a `…` suffix on truncation. Runs of 3+ newlines collapse to 2 to absorb intra-tag whitespace.

Spoiler messages short-circuit at the Rust layer: when `summary.formatted_body` contains `data-mx-spoiler`, the C++ side substitutes "Message contains spoiler." instead of either body. See `runtime_notifications.rs::contains_spoiler_markup` and `Manager.cpp::plainNotificationBody` / `formattedNotificationBody`.

## Image Handling (Linux)

Two slots, two purposes:

- **Avatar via `image-data` hint.** `ManagerLinux.cpp::systemPostNotification` sets `hints["image-data"] = icon` (a sender/room avatar `QImage`). Qt's `qDBusRegisterMetaType<QImage>()` serializes this to the spec's `(iiibiiay)` struct. All six major freedesktop daemons (KDE Plasma, GNOME Shell, mako, dunst, swaync, xfce4-notifyd) honor this slot.
- **Inline message image via body `<br><img src="file:///...">`.** Gated on the `body-images` capability bit, which today is advertised only by KDE Plasma. The thumbnail is requested via `MxcImageProvider::download(mediaMxcUrl, QSize(200, 80), ..., /*crop=*/false)` and written to disk. The `alt=` attribute uses `plainNotificationBody(...).toHtmlEscaped()` because the formatted body is Pango markup and would corrupt the HTML attribute if injected raw.

The two slots are independent. On Plasma both populate; on every other daemon only the avatar populates.

## Per-Platform Paths

| Platform | Body | Markup | Inline image | Sender avatar |
| --- | --- | --- | --- | --- |
| Linux (Plasma) | Normalized markup | Yes (spec subset) | Yes (body `<img>`) | Yes (`image-data`) |
| Linux (mako, GNOME Shell, dunst, swaync, xfce4-notifyd) | Normalized markup | Yes (spec subset; daemon may render even less) | No | Yes (`image-data`) |
| Windows | `plain_body` | No | No (toast XAML is unrelated) | Via WinToast image hint |
| macOS | `plain_body` | No | No | Via `UNNotificationAttachment` |

The Linux `hasMarkup_` branch is selected when the daemon's `GetCapabilities` reply contains `body-markup`. Win/Mac always pass plain body.

## Constraints and Design Decisions

Dated entries so future contributors don't re-investigate the same trade-offs.

**2026-05-19. Linux body switched to spec-subset markup.** Previously the formatted body was passed near-raw with only `<em>`/`<strong>` rewrites. mako and GNOME Shell render unknown tags as literal text, so `<p>`, `<ul>`, `<mx-reply>`, etc. leaked into popups (issue [#180](https://github.com/etkecc/komai/issues/180)). The normalizer collapses everything outside the spec subset. Plasma's tolerant body rendering loses a bit of fidelity (lists become bullets, blockquote/heading indentation flattens), but the loss is small and per-daemon coverage is much better. Commit `56267f376`.

**2026-05-20. Inline image switched from `crop=true` to `crop=false`.** The 200x80 box plus `crop=true` sliced a thin horizontal band from the vertical middle of portrait and square sources. With `crop=false` the thumbnail fits within 200x80 with aspect preserved (smaller for tall sources, but the whole image is visible). Larger boxes (256x256) were tested and rejected because Plasma's notification widget then grew tall enough to gain a scrollbar. Commit `7353039b7`.

**2026-05-20. HiDPI crispness via `<img width>`/`<img height>` is not pursued.** Verified by reading `libnotificationmanager/notification.cpp`: Plasma's notification sanitizer reads only `src` and `alt` attributes from body `<img>` tags and discards all others. Requesting a high-DPR thumbnail and constraining the rendered size via markup is not possible. Other daemons (mako, GNOME Shell, dunst, swaync, xfce4-notifyd) don't render body `<img>` at all (Pango markup lacks `<img>` in core), so the question doesn't arise for them. The current 200x80 logical thumbnail is the portable equilibrium. The alternative (NeoChat's pattern: drop body `<img>` entirely, only use the `image-data` slot for everything visual) would replace the sender avatar with the message image, a strict downgrade.

**Latent: mako `image-data` freeze.** [emersion/mako#629](https://github.com/emersion/mako/issues/629), open as of February 2026, reports that certain `image-data` payloads can freeze the daemon until `makoctl reload`. Komai sends an avatar via `image-data` today and is not specifically named in that issue, but the risk vector exists. No mitigation action unless reproducible reports against Komai surface.

## References

- [freedesktop notification spec, markup](https://specifications.freedesktop.org/notification/latest/markup.html)
- [freedesktop notification spec, icons and images](https://specifications.freedesktop.org/notification/latest/icons-and-images.html)
- [Plasma notification sanitizer](https://invent.kde.org/plasma/plasma-workspace/-/blob/master/libnotificationmanager/notification.cpp) (`Notification::Private::sanitize`)
- [NeoChat notifications manager](https://invent.kde.org/network/neochat/-/blob/master/src/app/notificationsmanager.cpp) (reference for the avatar-only-pixmap pattern)
- User-facing: [Notifications feature page](../user-guide/features/notifications.md)
