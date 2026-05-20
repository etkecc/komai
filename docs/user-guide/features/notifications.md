# 🔔 Notifications

How Komai pings you about new messages, and how to control it.


## 🎛️ Two switches, two scopes

Komai layers two notification toggles that need to be thought about together:

- **Enable notifications for this account**. Account-wide, server-side. Lives under *Settings → Desktop → Account notifications*. Synced to your Matrix homeserver, so flipping it off mutes **every Matrix client and device** signed into this account, not just Komai.
- **Enable system notifications**. Per-profile, client-side. Lives under *Settings → Desktop → System notifications*. Only affects this Komai [👥 application profile](application-profiles.md); other clients keep notifying. Grayed out when the account toggle above is off.

Both toggles only control **system notifications** (the popups your OS shows). Komai always tracks unread state client-side regardless, with badges visible in the [📋 room list](room-list.md), [🏘️ communities sidebar](communities-sidebar.md), and [📑 room tabs](tabs.md). If you want that unread state surfaced *outside* Komai (window title count, app/taskbar badge), see **Attention indicators** below.


## 🚪 Per-room overrides

Each room has its own notification setting in **Room Info → Notifications**:

- **All messages**. Every new message pings you. Default for direct chats and small rooms.
- **Mentions only**. Only `@you` mentions and room-wide mentions ping. Good for busy rooms you still want to follow.
- **Muted**. No notifications and no "unread" indicator for this room.

Like the account toggle above, this is **synced to your Matrix account**: it lives on the homeserver as a per-room push rule, so the choice applies to every Matrix client and device signed in, not just Komai.


## 🤫 Low priority rooms

A separate per-room toggle, set from **right-click a room → Tags → Low priority** in the [📋 room list](room-list.md). This is a Matrix room tag (synced across clients), and it's **independent of the Notifications dropdown above**.

A low priority room still receives system notifications and counts toward unread state per its Notifications setting. What changes is *visual emphasis*: in the [📋 room list](room-list.md) and [📑 room tabs](tabs.md), a low priority room with ordinary unread messages stays muted in appearance (no bold, no grown badge). A mention or other "loud" notification breaks through and shows normally.

The [🏘️ communities sidebar](communities-sidebar.md) has a built-in **Low Priority** filter if you want a focused view of those rooms.


## 🔒 Privacy and content

- **Message content in notifications**. Choose how much message text appears in the popup, or hide it entirely. Useful when your screen is visible to others.
- **Spoiler messages** show "Message contains spoiler." instead of the content. Always on.
- **Encrypted messages** show "sent a message" rather than decrypted content.


## ✨ Beyond the popup

- **Flash app window/taskbar on incoming messages**. A quieter cue than a popup.
- **Attention indicators** (*Settings → Desktop → Attention indicators*). Unread count in the window title or on the app/taskbar badge. On Linux with multiple profiles, see the badge note in [👥 Application Profiles](application-profiles.md#reliable-app-badges-with-multiple-profiles).
- **System tray** (*Settings → Desktop → System tray*). "Close to tray" keeps Komai running after you close the window so notifications still arrive.


## 🖥️ What it looks like on each OS

- 🐧 **Linux**. System notifications follow the [freedesktop notification spec](https://specifications.freedesktop.org/notification/latest/markup.html). Bold, italic, links, and the sender avatar render everywhere. Lists become bullets. Inline message images appear on KDE Plasma; other daemons (GNOME Shell, mako, dunst, swaync) don't advertise body-image support, so they show no inline image.
- 🪟 **Windows**. Toast notifications. Plain text body (no markup). If the message has an image, it appears as the toast's hero image above the text.
- 🍎 **macOS**. Banner notifications. Plain text body (no markup). If the message has an image, it appears alongside the text as an attachment.

## Related

- [👥 Application Profiles](application-profiles.md)
- [⚙️ Settings](../settings/README.md).
