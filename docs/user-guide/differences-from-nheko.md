# 🔀 Differences from nheko

Komai is a fork of [nheko](https://nheko.im/nheko-reborn/nheko) with UX improvements focused on desktop usability. This page lists the notable user-facing changes.

> 💡 This document was written in February-March 2026, around Komai's initial release period. As both Komai and nheko evolve, some details here may become outdated over time.

For project background and naming context, see [🦁 Identity](identity.md).


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
- **Revamped in-app image viewer** -- cleaner fullscreen media view with rounded media corners, corner-friendly controls, and extra actions like **Forward Message** and **Open** (in an external program); keyboard navigation is improved, and Close is pinned to the top-right for fast edge targeting ([Fitts's law](https://www.nngroup.com/articles/fitts-law/))
- **Forward button** in the message action bar
- **Pinned and recent emoji reactions** directly in the action bar
- **Swipe-to-reply disabled** by default to prevent accidental replies on desktop
- **Formatted-message code block highlighting** (powered by [KSyntaxHighlighting](https://api.kde.org/frameworks/syntax-highlighting/html/index.html)) with MIME-assisted and heuristic language auto-detection for unlabeled fenced blocks, and a user toggle under [Settings](settings/README.md).


## ✏️ Composer and Replies

- Text emoticons auto-replaced with emoji
- Polished reply popup with proper background, headers, and spacing
- Polished forward dialog with a confirmation step
- Legacy VoIP call UI and sticker button hidden for a cleaner look


## 📋 Dialogs

- **Room search** -- rounded, with room-ID search support and keyboard hints
- **Room members** -- wider dialog with a full "Invite" button
- **Welcome, Login, and Registration pages** prettified with consistent layout and branding


## 🔐 Encryption Setup and Recovery

- **Clear encryption backup prompt** -- when backup is missing, Komai shows a focused banner that explains why setup matters and lets you start immediately
- **One-click backup setup flow** -- creates a new security key and then shows a dedicated "save this key" dialog with copy support and keyboard-friendly actions
- **Improved encryption prompts** -- unlock and verification flows now use clearer, more human wording so next steps are easier to understand


## ⚙️ Reorganized Settings

- **Tabbed Settings panel** -- settings grouped into logical tabs instead of one long scrollable list
- **All user-facing settings are surfaced in the UI** -- unlike upstream nheko, Komai exposes every supported user-facing setting through Application Settings (internal tuning keys such as low-level DB limits remain config-only)
- **Improved text accessibility controls** -- UI text follows the configured font size more consistently, and you can adjust font size quickly with keyboard shortcuts (`Ctrl` + `+` and `Ctrl` + `-`)
- **Profile and Logout buttons** easily accessible in the Session tab
- **Configurable browser command for links** -- allows opening links through a custom command (for a specific browser/profile or script workflow), while still supporting system defaults when not set.
- **Revamped settings storage** -- Komai uses per-profile, human-readable YAML files (`config.yml`, `state.yml`, `session.yml`, `secrets.yml`) instead of one monolithic Qt settings store.


## 🔧 Internal Improvements

- **Not so eager to destroy your session** during temporary secret storage failures ([nheko#1875](https://github.com/Nheko-Reborn/nheko/issues/1875)) -- you can now close, fix your keychain, and relaunch without losing data
- **Virtual timeline window** -- only the most recent messages are exposed to the UI at a time, enabling instant scroll-up from local cache
- ⚡ **Faster room switching** by reducing up-front timeline work during room changes. In local testing, one of our slowest rooms became about:
  - 4x faster (about 400ms to 100ms to rendered timeline)
  - 40-50x faster in perceived performance (about 8ms from room click to room-list update + timeline loading state)
  - no exhaustive benchmark campaign was run, so treat these numbers with the usual benchmark grain of salt
  - implementation details and tracing knobs: [⚡ Performance Tracing](../architecture/performance.md)
- **Predictable profile selection** -- launching without `-p` always uses the default [profile](settings/README.md#profiles) instead of remembering the last-used one
- **Per-profile settings** -- all settings (theme, notifications, sidebar widths, etc.) are stored per-profile, so each account can have its own look and feel. See [Settings](settings/README.md#profiles)
- **Human-readable YAML settings** -- settings are stored in YAML files instead of Qt's INI format, making manual editing and backup straightforward. See [Settings](settings/README.md)
- **Split settings by concern** -- each profile stores `config.yml` (preferences), `state.yml` (runtime/layout), `session.yml` (session metadata), and `secrets.yml` (file-mode fallback secrets), instead of one monolithic file
- **Hierarchical key organization** -- settings are grouped in nested sections (`ui.*`, `timeline.*`, `composer.*`, etc.) rather than flat keys
- **Profile-scoped data and cache layout** -- runtime data and caches are explicitly grouped under `~/.local/share/komai/profiles/<profile-id>/...` and `~/.cache/komai/profiles/<profile-id>/...`
- **Centralized storage path construction** -- path logic is unified in one helper module instead of ad-hoc joins across callsites
- **Documented sample profile files** -- canonical examples are available in [settings/examples/profile](settings/examples/profile/)

### Architecture Details

- Settings architecture differences: [architecture/differences-from-nheko/settings.md](../architecture/differences-from-nheko/settings.md)
- Secret service behavior differences: [architecture/differences-from-nheko/secret-services.md](../architecture/differences-from-nheko/secret-services.md)
- Settings name mapping for patch porting: [architecture/differences-from-nheko/settings-mapping.md](../architecture/differences-from-nheko/settings-mapping.md)


## 🌐 Translations

AI-assisted translation fills in gaps left by nheko's incomplete human translations, covering 30+ languages. See [Translations](../maintainers/translations.md) for details.
