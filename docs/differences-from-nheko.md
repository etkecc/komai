# Differences from nheko

Komai is a fork of [nheko](https://nheko.im/nheko-reborn/nheko) with UX improvements focused on desktop usability. This page lists the notable user-facing changes.


## 🎨 Theming

Komai ships 14 built-in color themes (light and dark variants): Komai, Nord, Catppuccin, Dracula, Solarized, and more. New themes are easy to add -- see [🎨 Themes](themes.md) for details.


## ✨ Visual Polish

- **Rounded corners throughout** -- bubbles, dialogs, media, avatars, reply popups, search, and more. No sharp edges
- **Rounded-rectangle avatars** by default instead of circles
- **Bubble chat by default** with distinct sender-colored bubbles, more padding, metadata outside the bubble, and a max-width cap for readability
- **Larger, more readable text** -- bigger default font, room list text, and timestamps
- **Separator lines** between room list entries and autocomplete items
- **Larger emoji** in the timeline by default
- **Per-room user colors** -- unique color assignment in small rooms, accent-complementary color in large rooms


## 🧭 Navigation and Layout

- **Room actions bar** (create, join, search) moved above the room list for a natural top-to-bottom flow
- **User avatar button** replaces the settings gear; context menu provides profile, status, settings, and logout in one place
- **Compact room list** with denser entry layout
- **Polished top bar** -- selectable topic text, hidden empty topic, dedicated settings button
- **Avatars on the bubble side** -- received messages show the avatar on the left, yours on the right


## 💬 Timeline Interaction

- **Click-to-toggle message actions** replace the hover-only action bar, eliminating mis-targeting on wide windows
- **Forward button** in the message action bar
- **Pinned and recent emoji reactions** directly in the action bar
- **Swipe-to-reply disabled** by default to prevent accidental replies on desktop


## ✏️ Composer and Replies

- Text emoticons auto-replaced with emoji
- Polished reply popup with proper background, headers, and spacing
- Polished forward dialog with a confirmation step
- Legacy VoIP call UI and sticker button hidden for a cleaner look


## 📋 Dialogs

- **Room search** -- rounded, with room-ID search support and keyboard hints
- **Room members** -- wider dialog with a full "Invite" button
- **Welcome, Login, and Registration pages** prettified with consistent layout and branding


## 🔧 Internal Improvements

- **Not so eager to destroy your session** during temporary secret storage failures ([nheko#1875](https://github.com/Nheko-Reborn/nheko/issues/1875)) -- you can now close, fix your keychain, and relaunch without losing data
- **Virtual timeline window** -- only the most recent messages are exposed to the UI at a time, enabling instant scroll-up from local cache
- **Faster room switching** by reducing off-screen pre-rendered messages
- **Predictable profile selection** -- launching without `-p` always uses the default profile instead of remembering the last-used one
- **Per-profile settings** -- all configuration (theme, notifications, sidebar widths, etc.) is stored per-profile, so each account can have its own look and feel


## 🌐 Translations

AI-assisted translation fills in gaps left by nheko's incomplete human translations, covering 30+ languages. See [Translations](translations.md) for details.
