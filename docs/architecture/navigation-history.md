# Navigation History (Back/Forward)

Komai supports browser-like back/forward navigation using mouse back/forward buttons (or any future keybinding). This allows users to retrace their steps through filters and rooms.


## Concept

Navigation state is a pair: **filter + room**. The filter (communities sidebar) is the "container" and the room is the "item within it." Any change to either the active filter or the active room constitutes a navigation event.

A linear history stack with a cursor tracks these events:

```
[entry0] [entry1] [entry2] [entry3]
                     ^cursor
```

- **Back** moves the cursor left and restores that entry's filter + room (including "no room" states).
- **Forward** moves the cursor right (only available after going back).
- A new navigation event while the cursor is not at the end **truncates** all forward entries (same as browser behavior).


## Data Model

```cpp
struct NavigationEntry {
    QString filterId;    // CommunitiesModel filter (e.g. "", "people", "space:!abc:example.com")
    QString roomId;      // Currently selected room (empty = no room open)
    bool filterOnly;     // True when only the filter changed (room was carried over, not explicitly chosen)
};
```

The history is stored in a `std::vector<NavigationEntry>` with an `int cursor_` index. This is **in-memory only** -- not persisted to `state.yml` or any file. History resets on app restart.


## Integration Points

### 1. NavigationHistory class (`src/timeline/NavigationHistory.h/.cpp`)

Owns the stack and cursor. Key methods:

| Method | Description |
|--------|-------------|
| `push(filterId, roomId, filterOnly)` | Record a navigation event. Truncates forward history. Deduplicates against current entry. `filterOnly` marks entries where the room was carried over, not explicitly navigated to. |
| `back(currentFilterId, currentRoomId)` | Move cursor back, skipping filter-only entries (see below). No-op if at start. |
| `forward(currentFilterId, currentRoomId)` | Move cursor forward, skipping filter-only entries. No-op if at end. |
| `canGoBack()` | True if cursor > 0. |
| `canGoForward()` | True if cursor < stack size - 1. |

### 2. TimelineViewManager (`src/timeline/TimelineViewManager.h/.cpp`)

Owns the `NavigationHistory` instance.

**Initialization**: Seeds the history with an initial `{currentFilter, ""}` entry on construction, so there is always a "no room" state to go back to.

**Signal connections**:

- `CommunitiesModel::currentFilterIdChanged` -- pushes with `filterOnly=true` (captures the current room, but marks it as carried over)
- `RoomlistModel::currentRoomChanged` -- pushes with `filterOnly=false` (explicit room selection)

**Navigation methods** (`Q_INVOKABLE`):

`navigateBack()` and `navigateForward()`:
1. Set a `navigating_` guard flag to suppress push during restoration
2. Restore filter via `CommunitiesModel::setCurrentFilterId()`
3. Restore room via `RoomlistModel::setCurrentRoom()` -- empty string clears the room selection
4. Clear the guard flag

### 3. MainWindow (`src/ui/MainWindow.cpp`)

The existing `mousePressEvent` override handles `Qt::BackButton` and `Qt::ForwardButton` by calling `TimelineViewManager::navigateBack()` / `navigateForward()`.


## Filter-Only Entry Skipping

When the user switches a filter (e.g. clicks a space), the room that was already displayed doesn't change -- it's just carried over. These entries are recorded with `filterOnly=true` and capture the full state (filter + room) for accuracy, but `back()` and `forward()` **skip them entirely**.

This means pressing back jumps to the last state where the user explicitly chose a room (or had no room open), rather than stopping at intermediate filter switches.

**Example flow**:

1. App starts: `{filter="", room=""}` (seeded initial entry)
2. User opens room A: `{filter="", room="A", filterOnly=false}`
3. User clicks Space X: `{filter="space:X", room="A", filterOnly=true}` -- room A carried over
4. User clicks room B in Space X: `{filter="space:X", room="B", filterOnly=false}`

Pressing back from step 4: entry 3 is `filterOnly=true` → **skipped** → entry 2 is `filterOnly=false` → **restored** (filter="" + room A). Both the filter and room change in one back press.


## Other Edge Cases

- **No room open**: Entries with empty `roomId` are valid. Restoring such an entry clears the room selection (returns to the room list without any room open).
- **Stale rooms**: A room in history may have been left or removed. `setCurrentRoom()` handles missing rooms gracefully (defers or ignores).
- **Stale filters**: A filter (space/tag) may no longer exist. `setCurrentFilterId()` falls back to the default "All Rooms" filter (`""`).
- **Consecutive duplicates**: `push()` deduplicates -- if the new entry matches the current cursor entry (same filter + room), it is not added.
- **Guard flag**: The `navigating_` flag prevents `back()`/`forward()` restoration from pushing new entries onto the stack.
- **Stack size**: Capped at 100 entries by dropping the oldest entries when full.
