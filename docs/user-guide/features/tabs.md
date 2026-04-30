# 📑 Room Tabs

Tabs let you keep multiple rooms open at once and switch between them, just like a web browser. Unusual among Matrix clients, most of which force you to pick a single active room from the [room list](room-list.md) at a time.

![Room tabs](../screenshots/tabs.webp)


## 🎯 Quick start

- `Ctrl+T` — open a new tab (gives you a search field for jumping to any room)
- `Ctrl+W` — close the current tab
- `Ctrl+Tab` / `Ctrl+Shift+Tab` — next / previous tab
- `Alt+1` … `Alt+9` — jump to tab by position
- `Ctrl+Shift+T` — reopen the most recently closed tab

See [⌨️ Keyboard Shortcuts → Tabs](keyboard-shortcuts.md#-tabs) for the full keyboard reference.


## ➕ Opening tabs

You can open a new tab in several ways:

- `Ctrl+T`
- Click the **New** button on the right side of the tab bar
- Double-click the empty space in the tab bar
- `Ctrl+Click` a room in the room list (opens it in a new tab instead of the current one)

A new tab starts empty, showing the **New Tab** page — search your rooms, join one, or explore the public directory.

> 💡 Komai keeps at most one empty "New Tab" at a time. Trying to open a second one focuses (and briefly shakes) the existing one.


## 🔁 Switching tabs

- **Click** a tab to switch to it
- `Ctrl+Tab` for next, `Ctrl+Shift+Tab` for previous
- `Alt+1` … `Alt+9` to jump to the tab at that position
- **Scroll wheel** on the tab bar scrolls horizontally when tabs overflow


## ✖️ Closing tabs

- Click the **×** button on the tab
- **Middle-click** anywhere on the tab
- `Ctrl+W` closes the active tab
- Right-click a tab for more close options:
  - **Close Tab**
  - **Close Other Tabs**
  - **Close Tabs to the Right**
  - **Close Unpinned Tabs**

When you close the active tab, Komai switches to the tab you were on most recently — not just the one physically next to it. That means closing stays predictable even if you've been jumping around.


## ↩️ Reopening closed tabs

Pressed `Ctrl+W` by mistake? `Ctrl+Shift+T` restores the most recently closed tab. Keep pressing it to keep walking back through your close history; tabs that are still open are skipped automatically, and pinned tabs come back pinned.

Closed-tab history is kept for the current session.


## 📌 Pinning tabs

Pin a tab to keep it in a fixed position at the left of the tab bar — great for rooms you always want within reach (family chat, team room, favourite community).

**How to pin or unpin:**

- Click the pin icon on the tab (the icon's visibility is configurable — see [Settings](#%EF%B8%8F-settings))
- Right-click a tab → **Pin Tab** / **Unpin Tab**
- Drag a tab across the pinned/unpinned boundary to pin or unpin it in one motion

**What pinning does:**

- Pinned tabs always sit at the left; unpinned tabs stay to their right
- Pinned tabs can't be accidentally replaced — when you click a room in the sidebar, Komai skips pinned tabs and either opens a new tab or reuses an unpinned one
- Pinned tabs persist across restarts (so do unpinned tabs, by the way)


## 🔀 Reordering tabs

Drag tabs left or right to rearrange them. Drag a tab across the pinned/unpinned boundary to pin or unpin it in the same motion. Drop it back in place (or press `Esc` mid-drag) to cancel.


## ⚙️ Settings

Tab behavior is configurable under **Settings → Navigation**:

- **Pin button visibility** — Always / On hover / Never
- **Tab labels** — full labels or avatar-only, configurable separately for pinned and unpinned tabs
- **Tab widths** — preferred and minimum width in pixels
- **Room list opening policy** — whether clicking a room in the sidebar opens a new tab or reuses the current one (`Ctrl+Click` always opens a new tab regardless)
- **Recently-closed timelines pool** — how many recently-viewed room timelines are kept in memory for faster re-opening


## 💡 Tips

- **`Ctrl+Click` a room** in the [room list](room-list.md) sidebar to open it in a new tab without leaving the current one
- **Tabs persist across restarts** — open and pinned tabs are restored on the next launch
- **Shrink tabs to icons** by setting tab labels to avatar-only in Settings, fitting many more tabs in the bar
- **Cascading close** — `Ctrl+W` + `Ctrl+Shift+T` make a quick "peek, close, undo" workflow when you just want to glance at another room
