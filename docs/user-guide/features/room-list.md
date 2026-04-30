# Room List

The room list is the sidebar showing every Matrix room you have joined, plus pending invites. It sits to the right of the [Communities sidebar](communities-sidebar.md) and is your primary way to switch between conversations.

## Switching Rooms

- **Click** a row to open that room. The exact behavior -- switch in place vs. open in a new [tab](tabs.md) -- is controlled by the [**Opening policy**](#settings) setting.
- **Middle-click** always opens the room in a new [tab](tabs.md), regardless of your opening policy.
- **Ctrl+Click** flips the configured behavior: if plain click switches in place, Ctrl+Click opens a new tab, and vice versa.
- **Right-click** (or long-press on touch) opens the room's [context menu](#context-menu).

For keyboard navigation -- focusing the list with `Ctrl+Shift+R`, moving with `Up`/`Down`/`j`/`k`, jumping to the next room with activity via `Alt+A`, and more -- see the **Sidebar Lists** and **App-Wide Shortcuts** sections of [Keyboard Shortcuts](keyboard-shortcuts.md#-sidebar-lists).

## Mark as Read

Komai offers two ways to clear the unread state of a room without having to open it:

### From the context menu

Right-click any unread room and choose **Mark as read**. The entry only appears for rooms that actually have something to mark -- not for spaces, invites, or rooms that are already read.

### Scrub gesture (mouse)

Hold the left mouse button on a room and drag back and forth horizontally, like wiping with an eraser. As you scrub, the row fades to indicate the gesture is being recognized. After roughly three direction reversals (within a 1.5 second window), the room is marked as read.

The gesture is intentionally a little demanding so casual drag-misfires don't trip it. If you let go before the threshold, the row fades back to normal and nothing happens.

The scrub is disabled on spaces, invites, and rooms that are already read.

Behind the scenes, both paths send a Matrix read receipt and advance your `m.fully_read` marker to the most recent known event. Other Matrix clients pick this up via account data.

## Context Menu

Right-click (or long-press on touch) a room to open its context menu:

- **Open in a new tab** -- open the room in a new [tab](tabs.md) without leaving your current room
- **Open in new window** -- detach the room into its own window
- **Copy room link** -- copy a `matrix.to` link to the clipboard
- **Mark as read** -- clear the unread state (only shown when there is something to clear; see [above](#mark-as-read))
- **Tag room as** -- toggle Matrix room tags (Favourite, Low priority, custom tags, etc.); used by the [Communities sidebar](communities-sidebar.md) filters
- **Room settings** -- open the room info / settings dialog
- **Leave room** -- leave the room (with confirmation)

Right-clicking on empty room-list space (rather than on a row) opens a small menu with a shortcut to the navigation settings page.

## Pause and Resume on Interaction

While you are hovering the room list, dragging the scrollbar, or have the room list focused, Komai pauses live re-ordering of the rooms. This prevents the row your cursor is over from sliding away the moment a new message arrives in some other room.

When updates are being held back, a small pause indicator appears near the bottom of the list. Hovering it shows: *"Live updates are paused while you interact with the room list."* The list catches up automatically as soon as you move away.

## Settings

Most of these live under **Settings > Look & Feel > Room List** (or via the gear icon on the room list itself).

| Setting | What it does |
|---|---|
| **Sort** | Order rooms by activity, alphabet, etc. |
| **Show unread indicators** | Whether unread rooms get bold names, count badges, row highlights, and the left-edge marker. Off disables the visual emphasis even when rooms have unread messages. |
| **Show last message time** | Whether each row shows the relative time of the latest message |
| **Last message preview** | Always show, only show for unencrypted, or never show the body of the last message |
| **Opening policy** | Whether plain click switches in place or opens a new [tab](tabs.md) (with `Ctrl+Click` doing the opposite) |
| **Width** | The sidebar's preferred width, draggable via its right edge |
| **Density** | Compact vs. spacious row heights and padding |

The communities sidebar has its own per-filter switches; see [Communities Sidebar > Settings](communities-sidebar.md#settings).

## Related

- [Communities Sidebar](communities-sidebar.md) -- filter the room list by tags, spaces, people, bots, and more
- [Room Tabs](tabs.md) -- multi-room workflow and recently-closed tab recovery
- [Keyboard Shortcuts > Sidebar Lists](keyboard-shortcuts.md#-sidebar-lists) -- full keyboard reference
