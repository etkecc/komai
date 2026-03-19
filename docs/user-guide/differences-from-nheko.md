# 🔀 Differences from nheko

Komai is a fork of [nheko](https://nheko.im/nheko-reborn/nheko) with UX improvements focused on desktop usability. This page lists the notable user-facing changes.

> 💡 This document was written in February-March 2026, around Komai's initial release period. As both Komai and nheko evolve, some details here may become outdated over time.

For project background and naming context, see [🦁 Identity](identity.md).


## 🎨 Theming

Komai ships multiple built-in color themes (light and dark variants): Komai, Nord, Catppuccin, Dracula, Rosé Pine, Tokyo Night, and more, and the built-in set is maintained to meet [WCAG AA contrast](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html) for Komai's common UI text pairings.

New themes are easy to add (see [🎨 Themes](themes.md)) so they become built-in ones, but you don't have to -- Komai also supports [🗂️ User themes](themes.md#-user-themes).
Theme authors can also define exact user color palettes in the theme itself, instead of relying on automatic color derivation, for more predictable and intentional results.


## ✨ Visual Polish

- **Rounded corners throughout** -- bubbles, dialogs, media, avatars, reply popups, search, and more. No sharp edges
- **Most dialogs were redesigned as in-app overlays** -- they now match the app's look, open with more sensible default sizes, and are often usable immediately without manual resizing
- **Rounded-rectangle avatars** by default instead of circles
- **Bubble chat by default** with distinct sender-colored bubbles, more padding, metadata outside the bubble, and a max-width cap for readability
- **Larger, more readable text** -- bigger default font, room list text, and timestamps
- **Separator lines** between room list entries and autocomplete items
- **Larger emoji** in the timeline by default
- **Per-room user colors** -- unique color assignment in small rooms, accent-complementary color in large rooms
- **Fluent Icons workflow** -- we now keep [Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons) easy to update and extend, with details in [Icons Architecture](../architecture/icons.md) and the full [Icon Catalog](../architecture/icons-list.md).


## 🧭 Navigation and Layout

- **Room actions bar** (create, join, search) moved above the room list for a natural top-to-bottom flow
- **User avatar button** with settings gear -- click opens Settings directly; right-click provides profile, status, and logout
- **Application Profiles UI** -- dedicated profile switcher/manager for creating, launching, and deleting app profiles (instead of relying only on CLI `-p` usage). See [👥 Application Profiles](application-profiles.md)
- **Compact room list** with denser entry layout
- **Polished top bar** -- selectable topic text, hidden empty topic, dedicated settings button
- **Smarter direct chat detection** -- Komai combines [`m.direct`](https://spec.matrix.org/v1.17/client-server-api/#mdirect) account data with member-count heuristics to (hopefully in most cases) correctly identify direct chats, including 3-member rooms where one member is a bridge bot -- giving them the right name, avatar, and room-list placement
- **Revamped communities sidebar** -- configurable [filter sections](communities-sidebar.md#filter-sections) (People, Bots, Groups, Favourites, Server Notices, Low Priority) with local attention counting (unread messages and unsent drafts) that works even when notifications are disabled at the homeserver level. Every filter can be [hidden, badged, or excluded from "All rooms"](communities-sidebar.md#per-filter-options) via Settings > Sidebars or right-click. See [🏘️ Communities Sidebar](communities-sidebar.md)
- **Avatars on the bubble side** -- received messages show the avatar on the left, yours on the right
- **Mouse back/forward navigation** -- use mouse back/forward buttons to navigate through your room and filter history, like a browser. Filter-only intermediate steps are skipped automatically so back always jumps to a meaningfully different state


## 💬 Timeline Interaction

- **Click-to-toggle message actions** replace the hover-only action bar, eliminating mis-targeting on wide windows
- **Revamped in-app media viewer** -- replaces nheko's basic image popup with a proper media overlay:
  - **Unified gallery for images and videos** -- prev/next navigation browses all media in the room chronologically, with automatic prefetching so navigation stays responsive
  - **Video playback** -- videos play directly in the media viewer with full playback controls (play/pause, seek, volume, mute), so you no longer need to squint at tiny timeline thumbnails or download files to watch them. In unencrypted rooms, videos stream directly without requiring a full download first
  - **Maximized, not fullscreen** -- the overlay fills the screen but keeps the taskbar accessible, so you can adjust system volume, switch apps, or check notifications without dismissing the viewer
  - **Rounded media corners** and a clean backdrop for a polished look
  - **Edge-friendly controls** -- the action bar (Forward, Open, Copy, Save, Close) sits in the top-right corner and the prev/next bars span the full left/right screen edges, all reachable with a quick flick (see [Fitts's law](https://www.nngroup.com/articles/fitts-law/))
  - **Extra actions** -- **Forward Message** and **Open** (in an external program) alongside the standard Copy, Save, and Close
  - **Keyboard-friendly** -- Left/Right arrows navigate, Space toggles video playback, Escape closes
  - **Timeline video thumbnails** -- videos in the timeline show a thumbnail with a play button overlay and hover effects; clicking opens the media viewer directly
- **Revamped inline audio player** -- audio clips stay in the timeline, play without a separate Download step, and support adjustable playback speed
- **Forward button** in the message action bar
- **Pinned and recent emoji reactions** directly in the action bar
- **Swipe-to-reply disabled** by default to prevent accidental replies on desktop
- **Formatted-message code block highlighting** (powered by [KSyntaxHighlighting](https://api.kde.org/frameworks/syntax-highlighting/html/index.html)) with MIME-assisted and heuristic language auto-detection for unlabeled fenced blocks, and a user toggle under [Settings](settings/README.md).
- **Replaced the HTML message renderer with [litehtml](https://github.com/litehtml/litehtml)** -- produces prettier blockquotes, code blocks, and overall HTML rendering, and enables collapsible long messages with "Show more" / "Show less" buttons so they don't dominate the timeline.
- **Styled user and room mention pills** -- `@user` and `#room` mentions in timeline messages are rendered as styled pills with inline avatars instead of plain links


## ✏️ Composer and Replies

- **Persistent per-room drafts** -- unsent composer text is restored after app restarts/crashes, and rooms with drafts are highlighted in the room list, so you won't forget to finish them
- **Text emoticons auto-replaced with emoji** (enabled by default, configurable in Settings)
- **Emoji picker doesn't block sending** -- typing `:)`, `:D`, `:P`, etc. and pressing Enter sends the message immediately instead of requiring a second Enter to dismiss the picker first
- **Revamped inline emoji picker** -- larger emoji previews, full-width rows, a header with a close button, rounded corners, and a scrollbar for long result lists
- **Improved emoji discovery** -- emoji search/completion now uses localized [Unicode CLDR](https://cldr.unicode.org/) keywords, so common names are easier to find (for example, `:whiskey` finds 🥃 instead of requiring `:tumbler_glass`)
- **Inline pickers individually toggleable** -- the inline emoji (`:`), room (`#`), and user (`@`) pickers can each be enabled or disabled in Settings (all enabled by default)
- **Revamped file/image attachment staging area** -- file uploads now show in a compact vertical list with an "Attachments" header bar, per-file remove buttons, file-type-specific icons, HiDPI-aware rounded image previews, and caption support for image uploads (each image can be captioned individually via its filename field, or a shared caption can be typed in the composer)
- Polished reply popup with proper background, headers, and spacing
- Polished forward dialog with a confirmation step and a more relevant default set of rooms (excluding "Low Priority" or rooms you haven't interacted in recently)
- Legacy VoIP call UI and sticker button hidden for a cleaner look


## 📋 Dialogs

- 🧭 **Revamped Explore Public Rooms dialog** -- a modern room directory with three browsing modes: your homeserver, a configurable [Matrix Room Search](https://github.com/etkecc/mrs) (MRS) server (optional, enabled by default; the default server is [matrixrooms.info](https://matrixrooms.info/?utm_source=komai&utm_medium=docs&utm_campaign=differences-from-nheko) hosted by [etke.cc](https://etke.cc/?utm_source=komai&utm_medium=docs&utm_campaign=differences-from-nheko)), or any custom Matrix server. Features include:
  - **Room size filtering** -- filter results by member count (up to 2,000 / up to 10,000 / any) on every tab, with color-coded member badges and confirmation dialogs before joining large rooms
  - **Language filtering** -- narrow MRS results by language via a searchable dropdown
  - **Room alias display** with a copy button, clickable links in room topics, space badges, and automatic pagination
- **Room search** -- rounded, with room-ID search support and keyboard hints
- **Room members** -- wider dialog with a full "Invite" button
- **Welcome, Login, and Registration pages** prettified with consistent layout and branding
- **Polished SSO completion page** -- the browser page shown after Single Sign-On uses the Komai logo, the current theme's color palette, and translated status messages instead of a plain-text response


## 🔐 Encryption Setup and Recovery

- **Clear encryption backup prompt** -- when backup is missing, Komai shows a focused banner that explains why setup matters and lets you start immediately
- **One-click backup setup flow** -- creates a new security key and then shows a dedicated "save this key" dialog with copy support and keyboard-friendly actions
- **Improved encryption prompts** -- unlock and verification flows now use clearer, more human wording so next steps are easier to understand


## ⚙️ Reorganized Settings

- **Tabbed Settings panel** -- settings grouped into logical tabs instead of one long scrollable list
- **All user-facing settings are surfaced in the UI** -- unlike upstream nheko, Komai exposes every supported user-facing setting through Application Settings (internal tuning keys such as low-level DB limits remain config-only)
- **Audio media handling controls** -- Timeline settings now distinguish image, video, and audio external-open behavior, and let you choose the default inline playback speed
- **Improved text accessibility controls** -- UI text follows the configured font size more consistently, and you can adjust font size quickly with keyboard shortcuts (`Ctrl` + `+` and `Ctrl` + `-`)
- **Profile and Logout buttons** easily accessible in the Session tab
- **Account notifications toggle** -- control your homeserver's master push rule directly from Settings > Notifications, letting you mute notifications across all clients/devices without leaving Komai
- **Configurable browser command for links** -- allows opening links through a custom command (for a specific browser/profile or script workflow), while still supporting system defaults when not set.
- **Revamped settings storage** -- Komai uses per-profile, human-readable YAML files (`config.yml`, `state.yml`, `session.yml`, `secrets.yml`) instead of one monolithic Qt settings store.


## 🔧 Internal Improvements

- We completed a major codebase reorganization and refactoring to make Komai easier to maintain and evolve.
- **Not so eager to destroy your session** during temporary secret storage failures ([nheko#1875](https://github.com/Nheko-Reborn/nheko/issues/1875)) -- you can now close, fix your keychain, and relaunch without losing data
- **Virtual timeline window** -- only the most recent messages are exposed to the UI at a time, enabling instant scroll-up from local cache
- ⚡ **Faster room switching** by reducing up-front timeline work during room changes. In local testing, one of our slowest rooms became about:
  - 4x faster (about 400ms to 100ms to rendered timeline)
  - 40-50x faster in perceived performance (about 8ms from room click to room-list update + timeline loading state)
  - no exhaustive benchmark campaign was run, so treat these numbers with the usual benchmark grain of salt
  - implementation details and tracing knobs: [⚡ Performance Tracing](../architecture/performance.md)
- **Remembers your last open room** -- restarting the app brings you right back where you left off
- **Predictable profile selection** -- launching without `-p` opens the profile switcher unless only `default` exists, instead of remembering the last-used profile. See [Application Profiles](application-profiles.md)
- **Per-profile settings** -- all settings (theme, notifications, sidebar widths, etc.) are stored per-profile, so each account can have its own look and feel. See [Settings](settings/README.md#profiles)
- **Human-readable YAML settings** -- settings are stored in YAML files instead of Qt's INI format, making manual editing and backup straightforward. See [Settings](settings/README.md)
- **Split settings by concern** -- each profile stores `config.yml` (preferences), `state.yml` (runtime/layout), `session.yml` (session metadata), and `secrets.yml` (file-mode fallback secrets), instead of one monolithic file
- **Hierarchical key organization** -- settings are grouped in nested sections (`ui.*`, `timeline.*`, `composer.*`, etc.) rather than flat keys
- **Hardened formatted-message HTML pipeline** -- timeline HTML is now sanitized with stricter Matrix-spec-oriented tag/attribute rules and safer linkification behavior to reduce HTML-injection risk.
- **Profile-scoped data and cache layout** -- runtime data and caches are explicitly grouped under `~/.local/share/komai/profiles/<profile-id>/...` and `~/.cache/komai/profiles/<profile-id>/...`
- **More resilient local cache behavior** -- Komai cleans room-local cache data more aggressively when you leave rooms (both database records and downloaded media files for that room are removed), treats incompatible cache formats as a rebuild of local cache instead of a full profile wipe, and reorganizes its LMDB cache layout around shared room stores instead of per-room named-store fan-out. In practice this removes the old LMDB named-store choke point around very large accounts; practical limits now come from overall cache size and work, not one LMDB store budget per room.
- **Unified avatar thumbnail sizing** -- all avatar displays request a single standardized thumbnail size, so the same avatar is fetched and cached once instead of being re-downloaded at several slightly different sizes
- **Centralized storage path construction** -- path logic is unified in one helper module instead of ad-hoc joins across callsites
- **Documented sample profile files** -- canonical examples are available in [settings/examples/profile](settings/examples/profile/)

### Architecture Details

- Settings architecture differences: [architecture/differences-from-nheko/settings.md](../architecture/differences-from-nheko/settings.md)
- Secret service behavior differences: [architecture/differences-from-nheko/secret-services.md](../architecture/differences-from-nheko/secret-services.md)
- Settings name mapping for patch porting: [architecture/differences-from-nheko/settings-mapping.md](../architecture/differences-from-nheko/settings-mapping.md)


## 🌐 Translations

AI-assisted translation fills in gaps left by nheko's incomplete human translations, covering 30+ languages. See [Translations](../maintainers/translations.md) for details.
