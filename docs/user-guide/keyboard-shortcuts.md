# ⌨️ Keyboard Shortcuts

Komai has a mix of app-wide shortcuts and context-specific shortcuts that only work when a
particular view, dialog, or popup is open.

The examples below use Linux and Windows notation. On macOS, Qt platform-standard shortcuts
usually use `Command` instead of `Ctrl`.


## 🌍 App-Wide Shortcuts

These shortcuts work across the main application window unless a more specific control handles
them first.

| Shortcut | Action |
| --- | --- |
| `Ctrl+N` | Open the **New** dialog |
| `Ctrl+Shift+N` | Fallback for opening the **New** dialog when `Ctrl+N` is captured by a focused text field |
| `Ctrl+K` | Open **Find & switch room or space** |
| `Ctrl+P` | Alternative shortcut for **Find & switch room or space** |
| `Ctrl++` / `Ctrl+=` | Increase UI font size |
| `Ctrl+-` | Decrease UI font size |
| `Alt+A` | Jump to the next room with activity |
| `Ctrl+Shift+A` | Fallback for **next room with activity** |
| `Ctrl+Down` / `Ctrl+PgDown` | Switch to the next room |
| `Ctrl+Up` / `Ctrl+PgUp` | Switch to the previous room |
| `Ctrl+Q` | Quit Komai |


## 💬 Timeline and Room View

These shortcuts apply while you are looking at a room timeline.

| Shortcut | Action |
| --- | --- |
| `Ctrl+F` | Toggle search within the current room |
| `Ctrl+W` | Close the current room view |
| `Page Up` | Scroll the timeline up by about one page |
| `Page Down` | Scroll the timeline down by about one page |
| `Escape` | Clear uploads, cancel reply/edit/thread state, then focus the composer |
| Any typed character | Focus the composer and start typing immediately |

### Reply Navigation Shortcuts

These are implemented in the timeline itself and work on the currently selected reply target.

| Shortcut | Action |
| --- | --- |
| `Alt+Up` | Select or move the reply target upward through the timeline |
| `Alt+Down` | Move the reply target downward through the timeline |
| `Alt+F` | Forward the currently selected reply target |
| `Ctrl+E` | Edit the currently selected reply target |

Practical note: these shortcuts are fairly power-user oriented. They act on the message currently
stored as the timeline's reply target, not on a generic visual selection cursor.


## ✍️ Composer

These shortcuts apply in the message composer.

| Shortcut | Action |
| --- | --- |
| `Ctrl+Shift+V` | Paste as plain text |
| `Ctrl+U` | Clear the composer |
| `Ctrl+P` | Load the previous composer draft/history entry |
| `Ctrl+N` | Load the next composer draft/history entry |
| `Tab` | Open the inline completer, or move within completer results |
| `Shift+Tab` | Move the other direction within completer results |
| `Up` | Move up inside the completer, or jump into editing the previous editable message when at the start of the composer |
| `Down` | Move down inside the completer, or move toward the next editable message when editing |
| `Escape` | Close the inline completer popup |
| `Enter` / `Shift+Enter` / `Ctrl+Enter` | Send or insert a newline depending on your configured send-key setting |

Typing note: when the room timeline has focus, typing a character usually moves focus into the
composer automatically. On some platforms or keyboard layouts, certain `Ctrl+letter` combinations
may also trigger that focus behavior even though they are not dedicated composer shortcuts.


## 🔎 Find and Switch Room

In the **Find & switch room or space** dialog:

| Shortcut | Action |
| --- | --- |
| `Up` | Move selection up |
| `Down` | Move selection down |
| `Tab` | Move selection down |
| `Shift+Tab` | Move selection up |
| `Enter` | Open the selected room or space |
| `Escape` | Close the dialog |


## ➕ New Dialog

In the **New** dialog opened by `Ctrl+N`:

| Shortcut | Action |
| --- | --- |
| `Alt+J` | Join room |
| `Alt+E` | Explore public rooms |
| `Alt+D` | New direct chat |
| `Alt+R` | New room |
| `Alt+S` | New space |


## ↪️ Forward Message

In the **Forward Message** dialog:

| Shortcut | Action |
| --- | --- |
| `Up` | Move selection up |
| `Down` | Move selection down |
| `Tab` | Move selection down |
| `Shift+Tab` | Move selection up |
| `Enter` | Pick the selected destination room |
| `Escape` | Cancel the confirmation step |
| `Enter` (during confirmation) | Confirm forwarding |


## 🧾 Message Actions

In the **Message actions** dialog, the available shortcuts depend on the selected event type and
your permissions.

| Shortcut | Action |
| --- | --- |
| `Alt+C` | Copy text, or copy media for media events |
| `Alt+H` | Copy formatted text |
| `Alt+L` | Copy link location |
| `Alt+K` | Copy permalink |
| `Alt+P` | Pin or unpin the message |
| `Alt+M` | Mark the message as read |
| `Alt+S` | Save media as |
| `Alt+O` | Open media in an external program |
| `Alt+I` | Show read receipts |
| `Alt+U` | View raw message |
| `Alt+E` | View decrypted raw message |
| `Alt+D` | Remove message |
| `Alt+R` | Report message |


## 🖼️ Media Viewer

In the media overlay / image viewer:

| Shortcut | Action |
| --- | --- |
| `Escape` | Close the overlay |
| `Ctrl+C` | Copy the current media |
| `Left` | Show the previous media item in gallery mode |
| `Right` | Show the next media item in gallery mode |
| `Space` | Toggle video playback |
| `Tab` | Move focus into the action buttons |
| `Shift+Tab` | Move focus into the action buttons from the opposite end |


## 👥 Invite Dialog

In **Invite users to room**:

| Shortcut | Action |
| --- | --- |
| `Ctrl+Enter` | Confirm the current selection and send the invites |


## 📝 Notes

- Some shortcuts are only active when the relevant popup, dialog, or media overlay is visible.
- Some entries use Qt's platform-standard shortcuts, so the exact modifier may differ on macOS.
- This page documents the shortcuts currently implemented in Komai. If a shortcut is missing here,
  it may be unimplemented, platform-specific, or an incidental side effect rather than a deliberate
  user-facing binding.
