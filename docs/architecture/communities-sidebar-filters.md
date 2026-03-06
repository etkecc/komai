# Communities Sidebar Filters

The communities sidebar displays configurable filter sections that let users narrow the room list by category. Each filter is a fixed row in the sidebar model with its own setting, tag ID, sort position, and room-level filtering logic.

## Architecture overview

Filters flow through three layers:

1. **CommunitiesModel** (sidebar model) — fixed rows rendered in the sidebar, unread tracking
2. **FilteredCommunitiesModel** — visibility of sidebar rows based on settings
3. **FilteredRoomlistModel** — room-level filtering when a sidebar filter is selected

### Fixed-row descriptor (`FixedFilterRow`)

Each fixed filter row is defined in `CommunitiesModel::fixedFilters_`, a `std::array<FixedFilterRow, kFixedRowCount>` initialized in the header:

```cpp
struct FixedFilterRow {
    QString id;    // tag ID used by filtering ("", "dm", "bot")
    QString icon;  // sidebar icon path
    mtx::responses::UnreadNotifications unreads{};
};
```

The `data()` method in `CommunitiesModelData.cpp` serves all fixed rows from a single code path. Display names and tooltips come from `fixedFilterDisplayName()` / `fixedFilterTooltip()` (separate methods to support `tr()`).

### Sidebar sort order

The `Categories` enum in `CommunitiesModelData.cpp` defines sidebar ordering:

```
World → Direct → Bots → Favourites → Server → LowPrio → Space → UserTag
```

### Unread tracking

Each fixed filter row tracks its own `unreads` counter. During `initializeSidebar()` and `sync()`, notification diffs are applied to `fixedFilters_[row].unreads`. Tag-based filters (Favourites, Server Notices, Low Priority) use `tagNotificationCache` instead.

## Filter reference

### All Rooms

<img src="../../resources/icons/fluent/assets/Globe/SVG/ic_fluent_globe_32_regular.svg" width="24" height="24" alt="All Rooms icon">

| Property | Value |
|---|---|
| Tag ID | `""` (empty) |
| Row constant | `kRowAllRooms` (0) |
| Icon | `world.svg` |
| Setting | always shown |
| Room filter | `FilterBy::Nothing` — shows everything except previews, spaces, and hidden items |

### Direct Chats

<img src="../../resources/icons/fluent/assets/Person/SVG/ic_fluent_person_24_regular.svg" width="24" height="24" alt="Direct Chats icon">

| Property | Value |
|---|---|
| Tag ID | `"dm"` |
| Row constant | `kRowDirectChats` (1) |
| Icon | `person.svg` |
| Setting key | `sidebars.communities.filters.direct_chats` |
| Setting ID | `SidebarsCommunitiesFilterDirectChats` |
| Default | enabled |
| Room filter | `FilterBy::DirectChats` — accepts rooms where `RoomlistModel::IsDirect` is true |

A room is a direct chat if it appears in the user's `m.direct` account data. `DirectChatResolver` resolves the DM partner.

### Bots

<img src="../../resources/icons/fluent/assets/Bot Sparkle/SVG/ic_fluent_bot_sparkle_24_regular.svg" width="24" height="24" alt="Bots icon">

| Property | Value |
|---|---|
| Tag ID | `"bot"` |
| Row constant | `kRowBots` (2) |
| Icon | `robot-sparkle.svg` |
| Setting key | `sidebars.communities.filters.bots` |
| Setting ID | `SidebarsCommunitiesFilterBots` |
| Default | enabled |
| Room filter | `FilterBy::Bots` — accepts rooms where `RoomlistModel::IsBotRoom` is true |
| Visibility | only shown when `hasBotRooms_` is true (at least one bot room exists) |

Bot rooms are a strict subset of direct chats. A room is a bot room when its DM partner matches the bot heuristic in `isLikelyBotUser()` (`src/utils/UtilsCore.cpp`):

- User ID starts with `@bot` (case-insensitive)
- User ID contains `bot:`
- Display name contains `bridge bot`

`DirectChatResolver::isBotRoom()` is the single entry point — it resolves the DM partner, then checks the heuristic.

### Favourites

<img src="../../resources/icons/fluent/assets/Star/SVG/ic_fluent_star_28_regular.svg" width="24" height="24" alt="Favourites icon">

| Property | Value |
|---|---|
| Tag ID | `"tag:m.favourite"` |
| Icon | `star.svg` |
| Setting key | `sidebars.communities.filters.favourites` |
| Setting ID | `SidebarsCommunitiesFilterFavourites` |
| Default | enabled |
| Room filter | `FilterBy::Tag` with `filterStr = "m.favourite"` |

Rooms tagged with `m.favourite` via the Matrix [room tagging API](https://spec.matrix.org/latest/client-server-api/#room-tagging).

### Server Notices

<img src="../../resources/icons/fluent/assets/Tag/SVG/ic_fluent_tag_32_regular.svg" width="24" height="24" alt="Server Notices icon">

| Property | Value |
|---|---|
| Tag ID | `"tag:m.server_notice"` |
| Icon | `tag.svg` |
| Setting key | `sidebars.communities.filters.server_notices` |
| Setting ID | `SidebarsCommunitiesFilterServerNotices` |
| Default | enabled |
| Room filter | `FilterBy::Tag` with `filterStr = "m.server_notice"` |

Rooms tagged with `m.server_notice` by the homeserver. See the Matrix spec [Server Notices module](https://spec.matrix.org/latest/client-server-api/#server-notices).

### Low Priority

<img src="../../resources/icons/fluent/assets/Arrow Circle Down/SVG/ic_fluent_arrow_circle_down_32_regular.svg" width="24" height="24" alt="Low Priority icon">

| Property | Value |
|---|---|
| Tag ID | `"tag:m.lowpriority"` |
| Icon | `lowprio.svg` |
| Setting key | `sidebars.communities.filters.low_priority` |
| Setting ID | `SidebarsCommunitiesFilterLowPriority` |
| Default | enabled |
| Room filter | `FilterBy::Tag` with `filterStr = "m.lowpriority"` |

Rooms tagged with `m.lowpriority` via the Matrix [room tagging API](https://spec.matrix.org/latest/client-server-api/#room-tagging).

## Adding a new filter

Touch points for a new fixed-row filter:

1. **`CommunitiesModel.h`** — add `kRow` constant, bump `kFixedRowCount`, add entry to `fixedFilters_` initializer
2. **`CommunitiesModelData.cpp`** — add cases in `fixedFilterDisplayName()` and `fixedFilterTooltip()`; add `Categories` enum value and `tagIdToCat()` entry; add `filterAcceptsRow()` check; add signal connection
3. **`CommunitiesModelSync.cpp`** — accumulate unreads during init and sync
4. **`RoomlistModel.h`** — add `FilterBy` enum value and `updateFilterTag()` mapping
5. **`FilteredRoomlistModel.cpp`** — add `filterAcceptsRow()` branch for room-level filtering
6. **Settings layer** — add setting key, definition, schema descriptor, facade property, getter, setter, UI row (see `SettingKeys.h`, `.inc` files, `UserSettingsPage.h`)

For tag-based filters (backed by Matrix room tags), unread tracking uses `tagNotificationCache` instead of `fixedFilters_[].unreads`, so step 3 is not needed.

## Hidden tags mechanism

Users can hide individual sidebar sections via context menu. Hidden tag IDs are stored in `state.yml` at `sidebars.communities.hidden_tags`. The `FilteredRoomlistModel` parses these to populate `hiddenTags`, `hiddenSpaces`, `hideDMs`, and `hideBots` flags, which suppress matching rooms from the room list when the section is hidden.

## Design decisions

### Bots as subset of Direct Chats

"Direct Chats" shows all DMs including bot ones. "Bots" shows only bot DMs. This avoids a behavioral change for users who filter by Direct Chats expecting to see all their conversations. The `hideBots` mechanism lets users opt into hiding bot rooms from other views independently.

### Conservative bot heuristic

The pattern-matching approach prioritizes avoiding false positives (real people classified as bots). A per-room override via account data (e.g., `cc.etke.not_bot`) was deferred — `isBotRoom()` is the single chokepoint for adding this later.

---

For user-facing documentation, see [Communities Sidebar (user guide)](../user-guide/communities-sidebar.md).
