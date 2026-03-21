# ⌨️ Keyboard Shortcuts

This page lists the keyboard shortcuts currently documented in Komai.

The examples below use Linux and Windows notation. On macOS, platform-standard shortcuts usually
use `Command` instead of `Ctrl`.


## 🌍 App-Wide Shortcuts

These shortcuts work across the main window unless a more specific control handles them first.

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

These shortcuts apply while you are viewing a room timeline.

| Shortcut | Action |
| --- | --- |
| `Ctrl+F` | Toggle search within the current room |
| `Ctrl+W` | Close the current room view |
| `Page Up` | Scroll the timeline up by about one page |
| `Page Down` | Scroll the timeline down by about one page |
| `Alt+Up` | Enter Selection mode at the bottom-most visible message, or move toward older messages if Selection mode is already active |
| `Alt+Down` | Enter Selection mode at the top-most visible message, or move toward newer messages if Selection mode is already active |
| `Escape` | Close the nearest timeline or composer state first. Repeated `Escape` always settles back to the composer |
| Any typed character | Focus the composer and start typing, except while Selection mode is active |

### Selection Mode

Selection mode lets you move through the timeline with the keyboard. One message is focused, and
you can also select more than one message. Focused and selected messages are shown differently.

Selection mode also supports several [Vim](https://en.wikipedia.org/wiki/Vim_(text_editor))-like
keys such as `j`/`k`, `Ctrl+U`, `Ctrl+D`, `gg`, and `Shift+G`.

Unlike Vim, `Escape` does not enter Selection mode. In Komai, `Escape` always moves back toward
typing: it closes local UI first and eventually leaves you in the composer.

`Alt+Up` and `Alt+Down` are also entry keys: they start from the bottom-most or top-most visible
message in the current viewport, not from the absolute ends of room history.

On platforms that provide stable native scan codes, Komai also tries to keep those Latin-letter
bindings on the same physical keys when you use a non-Latin keyboard layout.

| Shortcut | Action |
| --- | --- |
| `Up` / `k` | Move focus toward older messages |
| `Down` / `j` | Move focus toward newer messages |
| `Ctrl+U` | Move focus about half a screen toward older messages; from the composer, enter Selection mode and do the same jump |
| `Ctrl+D` | Move focus about half a screen toward newer messages |
| `gg` | Move focus to the oldest currently loaded message |
| `Shift+G` | Move focus to the latest message in the current view |
| `Space` | Toggle whether the focused message is in the explicit selection |
| `?` | Open Selection mode help |
| `Enter` | Open the inline message-actions bar for the selected or focused message and focus its first visible button |
| `r` | Reply to the selected or focused message |
| `t` | Reply in thread, or continue the selected or focused message's thread |
| `e` | Edit the selected or focused message |
| `f` | Forward the selected or focused message |
| `d` | Delete message |
| `u` | View the selected or focused message as raw JSON |
| `o` | Open the full **Message actions** dialog for the selected or focused message |
| `i` | Exit Selection mode and return to the composer |

Which message does an action use?

- If one message is selected, actions use that message.
- If nothing is selected, actions use the focused message.
- If more than one message is selected, direct message actions do nothing yet. Use `Escape` or the bottom bar's **Clear** button first.

### Inline Message Actions Bar

When you open the inline actions bar from the keyboard, Komai scrolls the selected or focused
message into view first if needed.

| Shortcut | Action |
| --- | --- |
| `Left` | Move focus to the previous visible inline action |
| `Right` | Move focus to the next visible inline action |
| `Enter` | Activate the focused inline action |
| `Escape` | Close the inline actions bar and return to Selection mode |


## ✍️ Composer

These shortcuts apply in the message composer.

| Shortcut | Action |
| --- | --- |
| `Ctrl+Shift+V` | Paste as plain text |
| `Ctrl+U` | Enter Selection mode and move about half a screen toward older messages |
| `Ctrl+P` | Load the previous composer draft/history entry |
| `Ctrl+N` | Load the next composer draft/history entry |
| `Tab` | Open the inline completer, or move within completer results |
| `Shift+Tab` | Move the other direction within completer results |
| `Up` | Move up inside the completer, or jump into editing the previous editable message when at the start of the composer |
| `Down` | Move down inside the completer, or move toward the next editable message when editing |
| `Escape` | Close the inline completer popup, or otherwise keep you in the composer |
| `Enter` / `Shift+Enter` / `Ctrl+Enter` | Send or insert a newline depending on your configured send-key setting |

Typing note: when the timeline has focus, typing usually moves focus into the composer. On some
platforms or keyboard layouts, some `Ctrl+letter` combinations may also do that even though they
are not dedicated composer shortcuts.


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


## 🧾 Message Actions Dialog

In the **Message actions** dialog, available shortcuts depend on the message type and your
permissions.

| Shortcut | Action |
| --- | --- |
| `Up` | Move focus to the previous visible action row |
| `Down` | Move focus to the next visible action row |
| `Tab` / `Shift+Tab` | Move focus forward or backward through the visible actions |
| `Enter` | Activate the focused action |
| `Escape` | Close the dialog |
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
| `Alt+D` | Delete message |
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

- Some shortcuts only work when the relevant popup, dialog, or media overlay is open.
- Some entries use Qt's platform-standard shortcuts, so the exact modifier may differ on macOS.
- This page documents the shortcuts currently implemented in Komai. If a shortcut is missing here,
  it may be unimplemented, platform-specific, or an incidental side effect rather than a deliberate
  user-facing binding.
