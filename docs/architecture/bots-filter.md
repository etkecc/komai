# Bots Filter in Communities Sidebar

## Overview

The communities sidebar supports a "Bots" filter that shows direct chat rooms where the conversation partner is a likely bot or bridge service account. This filter sits between "Favourites" and "Low Priority" in the sidebar ordering and can be enabled/disabled via settings like the other filter sections.

## Design decisions

### Bot room = direct chat with a bot partner

A "bot room" is defined as a room that satisfies **both**:
1. It is a direct chat (as determined by `DirectChatResolver`)
2. The DM partner is a likely bot (as determined by `isLikelyBotUser()`)

This means "bot rooms" are a strict subset of "direct chats". Selecting the "Bots" filter shows only bot DMs. Selecting "Direct Chats" shows all DMs including bot ones.

### Bot detection heuristic

`isLikelyBotUser(userId, displayName)` in `src/utils/UtilsCore.cpp` uses case-insensitive pattern matching:
- userId starts with `@bot`
- userId contains `bot:`
- displayName contains `bridge bot`

This is intentionally conservative to minimize false positives.

### Integration with DirectChatResolver

`DirectChatResolver` already uses `isLikelyBotUser()` to eliminate bot members when computing the DM partner in 3-member rooms (e.g., [you, telegram-bot, real-user] resolves to real-user). A new `isBotRoom()` method checks whether the resolved partner itself is a bot.

### Tag ID and filtering

- Tag ID: `"bot"`
- `FilterBy::Bots` enum value in `FilteredRoomlistModel`
- `updateFilterTag()` maps `"bot"` to `FilterBy::Bots`
- `filterAcceptsRow()` accepts rooms that are direct AND have a bot partner
- `hideBots` flag allows hiding bot rooms from other views (independent of `hideDMs`)

### Settings

Setting key: `sidebars.communities.filters.bots`
Setting ID: `SidebarsCommunitiesFilterBots`
Default: enabled (`true`)
Only active when communities sidebar is visible (same guard as other filter settings).

### Sidebar ordering

The `Categories` enum in `CommunitiesModelData.cpp` controls sort order:
```
World → Direct → Bots → Favourites → Server → LowPrio → Space → UserTag
```

### Unread tracking

`botUnreads` is computed alongside `dmUnreads` during sync — for each room that is both a direct message and has a bot partner, its notification counts are accumulated.

## What we considered but did not implement

### Per-room "not a bot" override

Users may have rooms that match the bot heuristic but are actually conversations with real people (or vice versa). A per-room account data flag (e.g., a custom `cc.etke.not_bot` event) could let users override the heuristic. `isLikelyBotUser()` (or `isBotRoom()`) would check this flag first, and the override would cascade to all consumers.

This was deferred because:
- The current heuristic is conservative enough that false positives should be rare
- Adding account data events adds sync/storage complexity
- A UI for toggling the override (e.g., right-click menu on a room) would need design work
- It can be added later without breaking changes — `isBotRoom()` is the single chokepoint

### Separating bot rooms from Direct Chats entirely

We considered making "Bots" and "Direct Chats" mutually exclusive (Direct Chats would exclude bot rooms). This was rejected because:
- It would be a behavioral change for existing users
- Users filtering by "Direct Chats" likely still want to see their bot rooms
- The `hideBots` mechanism already lets users opt into hiding bot rooms from other views

### Bridge-specific categorization

Instead of a generic "Bots" filter, we considered separate filters per bridge type (Telegram, Signal, etc.) based on the bot user ID pattern. This was rejected as over-engineering — the generic filter covers the common case, and bridge-specific grouping could be done via Matrix spaces instead.
