# Communities Sidebar

The communities sidebar is a panel on the left side of Komai that lets you quickly filter your room list by category. Each filter section can be toggled on or off in **Settings > Sidebars**.

## Filter Sections

### All Rooms

<img src="../../resources/icons/fluent/assets/Globe/SVG/ic_fluent_globe_32_regular.svg" width="24" height="24" alt="All Rooms icon">

Shows every room you've joined, without any filtering. Always visible.

### Favourites

<img src="../../resources/icons/fluent/assets/Star/SVG/ic_fluent_star_28_regular.svg" width="24" height="24" alt="Favourites icon">

Shows rooms you've marked as favourites. You can favourite a room from its context menu (right-click) or room settings.

Uses the standard Matrix [`m.favourite`](https://spec.matrix.org/v1.17/client-server-api/#events-14) room tag -- favourites sync across all your Matrix clients. This filter only appears when you have at least one favourite room.

### People

<img src="../../resources/icons/fluent/assets/Person/SVG/ic_fluent_person_24_regular.svg" width="24" height="24" alt="People icon">

Shows direct chats with real people, excluding users detected as bots. This filter only appears when you have at least one non-bot direct chat.

Bot detection uses heuristics: user ID starts with `@bot`, contains `bot:`, or display name contains "bridge bot".

### Bots

<img src="../../resources/icons/fluent/assets/Bot Sparkle/SVG/ic_fluent_bot_sparkle_24_regular.svg" width="24" height="24" alt="Bots icon">

Shows direct chats where your conversation partner is a bot or bridge service account (e.g., a Telegram bridge bot). This filter only appears when you have at least one bot room.

### Groups

<img src="../../resources/icons/fluent/assets/People/SVG/ic_fluent_people_24_regular.svg" width="24" height="24" alt="Groups icon">

Shows multi-participant rooms that are not direct chats. This filter only appears when you have at least one group room.

### Server Notices

<img src="../../resources/icons/fluent/assets/Tag/SVG/ic_fluent_tag_32_regular.svg" width="24" height="24" alt="Server Notices icon">

Shows rooms created by your homeserver administrator for important announcements and service messages. This filter only appears if your server uses the [Server Notices](https://spec.matrix.org/v1.17/client-server-api/#server-notices) feature. Server notice rooms are tagged with [`m.server_notice`](https://spec.matrix.org/v1.17/client-server-api/#events-14).

### Low Priority

<img src="../../resources/icons/fluent/assets/Arrow Circle Down/SVG/ic_fluent_arrow_circle_down_32_regular.svg" width="24" height="24" alt="Low Priority icon">

Shows rooms you've marked as low priority. Like favourites, this uses a standard Matrix [`m.lowpriority`](https://spec.matrix.org/v1.17/client-server-api/#events-14) room tag and syncs across clients. This filter only appears when you have at least one low-priority room.

## Settings

Each filter has a toggle in **Settings > Sidebars**:

| Setting | What it controls | Default |
|---|---|---|
| Show Favourites filter | Favourites section | On |
| Show People filter | People section | On |
| Show Bots filter | Bots section | On |
| Show Groups filter | Groups section | On |
| Show Server Notices filter | Server Notices section | On |
| Show Low Priority filter | Low Priority section | On |

These toggles are only active when the communities sidebar itself is visible.

## Hiding Sections

You can also temporarily hide individual sections by right-clicking them in the sidebar and choosing **Hide**. This is different from disabling the filter in settings -- hidden sections can be restored from the sidebar context menu without going into settings.

When a section is hidden, its rooms are also hidden from the room list.

## Spaces

In addition to the filter sections above, the communities sidebar shows your [Matrix Spaces](https://spec.matrix.org/v1.17/client-server-api/#spaces) -- hierarchical groups of rooms. Spaces appear between the fixed filters and tags, and can be collapsed/expanded if they contain sub-spaces.

---

For technical details, see [Communities Sidebar Filters (architecture)](../architecture/communities-sidebar-filters.md).
