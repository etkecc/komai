# Differences from nheko

Komai is a fork of [nheko](https://nheko.im/nheko-reborn/nheko) with UX improvements focused on desktop usability. This page lists the notable user-facing changes.


## 🎨 Theming

Komai ships 14 built-in color themes (light and dark variants): Komai, Nord, Catppuccin, Dracula, Solarized, and more.

New themes are easy to add (see [🎨 Themes](themes.md)) so they become built-in ones, but you don't have to -- Komai also supports [🗂️ User themes](themes.md#-user-themes).


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
- **User avatar button** with settings gear -- click opens Settings directly; right-click provides profile, status, and logout
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


## ⚙️ Reorganized Settings

- **Tabbed Settings panel** -- settings grouped into logical tabs instead of one long scrollable list
- **Profile and Logout buttons** easily accessible in the Session tab
- **Configurable browser command for links** -- allows opening links through a custom command (for a specific browser/profile or script workflow), while still supporting system defaults when not set.


## 🔧 Internal Improvements

- **Not so eager to destroy your session** during temporary secret storage failures ([nheko#1875](https://github.com/Nheko-Reborn/nheko/issues/1875)) -- you can now close, fix your keychain, and relaunch without losing data
- **Virtual timeline window** -- only the most recent messages are exposed to the UI at a time, enabling instant scroll-up from local cache
- **Faster room switching** by reducing off-screen pre-rendered messages
- **Predictable profile selection** -- launching without `-p` always uses the default [profile](configuration.md#profiles) instead of remembering the last-used one
- **Per-profile settings** -- all configuration (theme, notifications, sidebar widths, etc.) is stored per-profile, so each account can have its own look and feel. See [Configuration](configuration.md#profiles)
- **Human-readable YAML configuration** -- settings are stored in YAML files instead of Qt's INI format, making manual editing and backup straightforward. See [Configuration](configuration.md)
- **Split configuration by concern** -- each profile stores `config.yml` (preferences), `state.yml` (runtime/layout), `session.yml` (session metadata), and `secrets.yml` (file-mode fallback secrets), instead of one monolithic file
- **Hierarchical key organization** -- settings are grouped in nested sections (`ui.*`, `timeline.*`, `composer.*`, etc.) rather than flat keys
- **Profile-scoped data and cache layout** -- runtime data and caches are explicitly grouped under `~/.local/share/komai/profiles/<profile-id>/...` and `~/.cache/komai/profiles/<profile-id>/...`
- **Centralized storage path construction** -- path logic is unified in one helper module instead of ad-hoc joins across callsites
- **Documented sample profile files** -- canonical examples are available in [architecture/configuration-examples/profile](architecture/configuration-examples/profile/)

### Architecture Details

- Configuration architecture differences: [architecture/differences-from-nheko/configuration.md](architecture/differences-from-nheko/configuration.md)
- Secret service behavior differences: [architecture/differences-from-nheko/secret-services.md](architecture/differences-from-nheko/secret-services.md)
- Settings name mapping for patch porting: [architecture/differences-from-nheko/settings-mapping.md](architecture/differences-from-nheko/settings-mapping.md)


## 🌐 Translations

AI-assisted translation fills in gaps left by nheko's incomplete human translations, covering 30+ languages. See [Translations](translations.md) for details.
