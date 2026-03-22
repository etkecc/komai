# Communities Sidebar

The communities sidebar is a panel on the left side of Komai that lets you quickly filter your room list by category. Each filter can be configured in **Settings > Sidebars**.

## Keyboard Navigation

You can focus the communities sidebar directly with `Ctrl+Shift+C`, then move through filters with
`Up` / `Down` or `j` / `k`, expand or collapse spaces with `Right` / `Left` or `l` / `h`, activate
the focused filter with `Enter`, and use `Tab` / `Shift+Tab` to cycle between the communities
sidebar and the room list.

For the full list, including `gg`, `Shift+G`, and `Ctrl+U` / `Ctrl+D`, see
[⌨️ Keyboard Shortcuts](keyboard-shortcuts.md#-sidebar-lists).

## Filter Sections

### All Rooms

<img src="../../resources/icons/fluent/assets/Globe/SVG/ic_fluent_globe_32_regular.svg" width="24" height="24" alt="All Rooms icon">

Shows every room you've joined, except rooms belonging to filters that have been [excluded](#per-filter-options). Always visible in the sidebar.

### Favourites

<img src="../../resources/icons/fluent/assets/Star/SVG/ic_fluent_star_28_regular.svg" width="24" height="24" alt="Favourites icon">

Shows rooms you've marked as favourites. You can favourite a room from its context menu (right-click) or room settings.

Uses the standard Matrix [`m.favourite`](https://spec.matrix.org/v1.17/client-server-api/#events-14) room tag -- favourites sync across all your Matrix clients. This filter only appears when you have at least one favourite room.

### People

<img src="../../resources/icons/fluent/assets/Person/SVG/ic_fluent_person_24_regular.svg" width="24" height="24" alt="People icon">

Shows [direct chats](#direct-chat-detection) with real people, excluding users [detected as bots](#bot-detection-heuristics). This filter only appears when you have at least one non-bot direct chat.

### Bots

<img src="../../resources/icons/fluent/assets/Bot Sparkle/SVG/ic_fluent_bot_sparkle_24_regular.svg" width="24" height="24" alt="Bots icon">

Shows [direct chats](#direct-chat-detection) where the conversation partner is [detected as a bot](#bot-detection-heuristics) or bridge service account. This filter only appears when you have at least one bot room.

### Groups

<img src="../../resources/icons/fluent/assets/People/SVG/ic_fluent_people_24_regular.svg" width="24" height="24" alt="Groups icon">

Shows multi-participant rooms that are not [direct chats](#direct-chat-detection). This filter only appears when you have at least one group room.

### Server Notices

<img src="../../resources/icons/fluent/assets/Tag/SVG/ic_fluent_tag_32_regular.svg" width="24" height="24" alt="Server Notices icon">

Shows rooms created by your homeserver administrator for important announcements and service messages. This filter only appears if your server uses the [Server Notices](https://spec.matrix.org/v1.17/client-server-api/#server-notices) feature. Server notice rooms are tagged with [`m.server_notice`](https://spec.matrix.org/v1.17/client-server-api/#events-14).

### Low Priority

<img src="../../resources/icons/fluent/assets/Arrow Circle Down/SVG/ic_fluent_arrow_circle_down_32_regular.svg" width="24" height="24" alt="Low Priority icon">

Shows rooms you've marked as low priority. Like favourites, this uses a standard Matrix [`m.lowpriority`](https://spec.matrix.org/v1.17/client-server-api/#events-14) room tag and syncs across clients. This filter only appears when you have at least one low-priority room.

## Settings

Each filter has up to three options, configurable in **Settings > Sidebars > Communities Sidebar** or via right-click context menu on the filter button:

### Per-filter options

| Option | What it does | Default |
|---|---|---|
| <img src="../../resources/icons/fluent/assets/Eye/SVG/ic_fluent_eye_24_regular.svg" width="16" height="16"> **Show** | Whether the filter button appears in the sidebar | On |
| <img src="../../resources/icons/fluent/assets/Counter/SVG/ic_fluent_counter_24_regular.svg" width="16" height="16"> **Attention badges** | Show attention badges (unread messages and unsent drafts) for this filter | On for most filters\* |
| <img src="../../resources/icons/fluent/assets/Globe/SVG/ic_fluent_globe_24_regular.svg" width="16" height="16"> **Include in 'All rooms'** | Include this filter's rooms in the "All rooms" view | On |

\*Attention badges are **off** by default for **All Rooms** and **Low Priority**.

The "All rooms" filter only has the **Attention badges** option (it is always shown and cannot exclude from itself).

These options are only active when the communities sidebar itself is visible.

### Hiding from the context menu

You can also hide a filter by right-clicking it in the sidebar and choosing **Hide this filter**. This is equivalent to turning off the **Show** toggle in Settings.

## Direct Chat Detection

Komai determines whether a room is a direct chat using two methods:

1. **m.direct account data** — if a room appears in your [m.direct](https://spec.matrix.org/v1.17/client-server-api/#direct-messaging) account data, it is definitively a direct chat. This is the authoritative source and syncs across Matrix clients.

2. **Member-count heuristic** — rooms not listed in m.direct are auto-detected as direct chats if they have 2-3 members. In 3-member rooms (common with bridges), Komai checks whether one member is a bot to identify the real conversation partner.

## Bot Detection Heuristics

Komai uses [heuristics](../../src/utils/BotDetection.cpp) to classify users as bots or bridge service accounts. This classification determines whether a direct chat appears in the **People** or **Bots** filter. All checks are case-insensitive.

A user is considered a bot if any of the following match. Checks are applied in the order listed -- this matters for the puppet escape hatch below.

| # | Heuristic | Example | Result |
|---|---|---|---|
| 1 | User ID starts with `@bot` | `@botserv:example.com` | Bot |
| 2 | User ID contains `bot:` | `@telegrambot:example.com` | Bot |
| 3 | Localpart contains `puppet` (**escape**) | `@_discordpuppet__123:example.com` | **Human** |
| 4 | User ID starts with `@_` | `@_irc_user:example.com` | Bot |
| 5 | Localpart ends with `bridge` | `@heisenbridge:example.com` | Bot |
| 6 | Display name contains "bridge bot" | "Telegram Bridge Bot" | Bot |
| 7 | Display name ends with "bot" (as a word) | "Hookshot Bot" | Bot |
| 8 | Display name starts with "bot" (as a word) | "Bot Service" | Bot |

The puppet escape (rule 3) exists because bridge puppets represent real people on other platforms. Since it is checked *after* rules 1-2, a puppet whose user ID also starts with `@bot` or contains `bot:` is still classified as a bot (e.g. `@botpuppet:example.com`).

Words like "robot" or "Robert" do not trigger the "bot" display name rules, because they require "bot" to appear as a standalone word (not part of a larger word).

## Spaces

In addition to the filter sections above, the communities sidebar shows your [Matrix Spaces](https://spec.matrix.org/v1.17/client-server-api/#spaces) -- hierarchical groups of rooms. Spaces appear between the fixed filters and tags, and can be collapsed/expanded if they contain sub-spaces.

---

For technical details, see [Communities Sidebar Filters (architecture)](../architecture/communities-sidebar-filters.md).
