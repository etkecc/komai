# Differences from nheko

Komai is a fork of [nheko](https://nheko.im/nheko-reborn/nheko) with UX improvements focused on desktop usability. This page lists all changes made on top of upstream nheko.


## 🎨 Branding and Themes

- **Rebranded** from nheko to Komai (strings, icons, spinner, binary name)
- **"Based on nheko" and "Fork by etke.cc"** attribution in the settings page
- **Data-driven theme system**: themes are defined as [Base16](https://github.com/tinted-theming/home) YAML files and compiled into the binary at build time. Adding a theme is just dropping a `.yaml` file.
- **14 built-in themes**: Komai light/dark, nheko light/dark, Breeze Dark, Catppuccin Latte/Mocha, Dracula, Gruvbox light/dark, Nord, Solarized light/dark, Tokyo Night
- **Two-ComboBox theme selector** in settings: pick a variant (light/dark), then choose from the filtered theme list
- **Komai logo marker** next to Komai-specific settings so they're easy to identify


## ⚙️ Defaults and Guards

- **Swipe-to-reply disabled** by default (accidental replies on desktop)
- **Font size adjustments**: 13pt default, larger room list text, state text, and timestamps
- **"Large emoji in timeline"** enabled by default
- **Circular avatars off** by default (rounded-rectangle style)
- **Bubble message style** enabled by default
- **Scrollbars in room list** enabled by default
- Fix: bubble style now properly respects the `disableSwipe` guard


## 🫧 Timeline and Bubbles

- **Distinct sender bubble color** using the highlight/accent hue
- **Polished inline reply** in bubbles (padding, rounded corners)
- **Larger hover action buttons** (16px → 32px)
- **Forward button** added to the message action bar
- **Pinned + recent emoji reactions** in the action bar
- **Subtler reaction pill styling** with larger emoji and pointer cursor
- **Timestamp/metadata moved outside the bubble**
- **Increased bubble padding** (4px → 8px)
- **Increased border radius** to 8px across timeline elements
- **Bubble max width capped** at ~85% of the timeline for readability
- **Distinct per-room user colors**: palette-based unique assignment in small rooms, accent-complementary uniform color in large rooms


## ⚡ Performance

- **Virtual timeline window**: cap exposed event range to 200, enabling instant scroll-up from cache
- **Faster room switching**: reduced ListView `displayMargin` to cut pre-created delegates


## ✏️ Composer

- **Polished message composer**: fully gate legacy VoIP calls (UI + events + ringtone + TURN), hide stickers, larger buttons, offset emoji picker, auto-replace text emoticons with emoji


## 💬 Reply and Forward

- **Polished reply popup**: background, headers, separator, Reply.qml fixes
- **Fixed phantom 22px gap** from broken username layout in replies


## 📋 Dialogs and Panels

- **Polished room search dialog**: rounded corners, header, placeholder text, keyboard hint
- **Room ID as a searchable field** in the room search
- **Separator lines between completer items** for better readability
- **Polished forward dialog**: rounded, better sizing, confirmation step
- **Komai logo on empty timeline screen** instead of a blank area
- **Separator lines between room list entries**
- **Polished room members dialog**: full "Invite" button with text+icon, wider/taller default size (600×750)


## 📐 Sidebar and Layout

- **Click-to-toggle message actions**: replaces the hover-only action bar with a toggle button anchored in the metadata row. The popup opens upward, centered on the button, and clamped to the delegate bounds. Eliminates mis-targeting on wide windows.
- **Avatars on bubble side**: received messages show avatar on the left, sent messages on the right. Configurable "Show own avatar" setting. Sender username display is configurable (Always / Only in large rooms / Never).
- **Room-actions bar moved above rooms**: natural top-to-bottom navigation, larger icons, distinct background
- **Polished communities sidebar**: row heights aligned with room list entries, redundant user info panel hidden when sidebar is active
- **User avatar button**: replaces the settings gear with the user's avatar + cog badge. Context menu provides profile, status, settings, and logout in one place.
- **Compact room list entries**: denser layout (avatarSize multiplier 2.3 → 2.0)
- **Polished room bar (TopBar)**: grey background, centered space icon, hidden empty topic, fixed row layout, reworked clickability (topic text is now selectable), simplified context menu, dedicated settings button, full-width topic/widgets
- **Styled room list scrollbars**: always-on thumb with neutral grey palette colors, matching groove background


## 🚪 Onboarding

- **Prettified Welcome dialog**: heading, subtitle, attribution footer
- **Reworked Registration page**: server choice guide and wider layout
- **Login page heading** for visual consistency with Register
- **Unified logo positioning** on Login and Register pages (top-aligned)


## 🔧 Build System

- **Upstream fixes backported**: Qt 6.9.2 reply rendering fix, Qt 6.10 private module build fix, room list scrollbar visibility fix
- **Theme generation at build time**: CMake runs `bin/generate-themes.py` to produce `ThemeDefinitions.h` from `resources/themes/*.yaml`
- **`justfile`** replaces the Makefile as the build system front-end
