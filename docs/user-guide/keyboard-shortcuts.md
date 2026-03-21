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
| `Escape` | Close the nearest timeline or composer state first. Repeated `Escape` always settles back to the composer |
| Any typed character | Focus the composer and start typing, except while Selection mode is active |

### Selection Mode

Selection mode lets you move through the timeline with the keyboard and optionally select more
than one message. Focused and selected messages are shown differently.

`Ctrl+Click` on a message also enters Selection mode if needed and toggles that message in the
explicit selection.

Selection mode also supports several [Vim](https://en.wikipedia.org/wiki/Vim_(text_editor))-like
keys such as `j`/`k`, `Ctrl+U`, `Ctrl+D`, `gg`, and `Shift+G`.

Unlike Vim, `Escape` does not enter Selection mode. In Komai, `Escape` always moves back toward
typing: it closes local UI first and eventually leaves you in the composer.

You can enter Selection mode from the composer with `Up` when the caret is already at the start of
the top composer line. Komai also differs from older chat-style `Up` behavior: it enters
Selection mode instead of jumping straight into last-message editing.

On some platforms, Komai also keeps those Latin-letter bindings on the same physical keys when you
use a non-Latin keyboard layout.

| Shortcut | Action |
| --- | --- |
| `Up` / `k` | Move focus toward older messages |
| `Down` / `j` | Move focus toward newer messages |
| `Left` / `h` | Move focus to the previous Selection mode bar button, or focus the last one from the timeline |
| `Right` / `l` | Move focus to the next Selection mode bar button, or focus the first one from the timeline |
| `Tab` | Focus the first enabled Selection mode bar button, then move to the next one |
| `Shift+Tab` | Move to the previous Selection mode bar button; from the first one, return to the timeline; from the timeline, move to the last room-header action button |
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
| `f` | Forward selected messages, or the selected or focused message |
| `d` | Delete selected messages, or the selected or focused message |
| `u` | View the selected or focused message as raw JSON |
| `o` | Open the full **Message actions** dialog for the selected or focused message |
| `i` | Exit Selection mode and return to the composer |

Which message does an action use?

- `f` and `d` use the current selection.
- Other actions use one selected message, or the focused message if nothing is selected.
- With more than one message selected, the other actions are unavailable.

Focus note:

- `Up` / `Down` keep moving through messages.
- `Tab` / `Shift+Tab` move between the timeline, the Selection mode bar, and nearby header controls.

### Inline Message Actions Bar

When you open the inline actions bar from the keyboard, Komai scrolls the selected or focused
message into view first if needed.

| Shortcut | Action |
| --- | --- |
| `Left` / `h` | Move focus to the previous visible inline action |
| `Right` / `l` | Move focus to the next visible inline action |
| `Up` / `k` | In the two-row layout, move focus to the previous visible inline action |
| `Down` / `j` | In the two-row layout, move focus to the next visible inline action |
| `gg` | Focus the first visible inline action |
| `Shift+G` | Focus the last visible inline action |
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
| `Up` | Move up inside the completer, or enter Selection mode when the caret is already at the start of the top composer line |
| `Down` | Move down inside the completer |
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


## ↪️ Forward Dialog

In the forward dialog:

- The title changes to match what you are forwarding.
- When you forward several selected messages, the dialog tells you how many will be sent.
- If some selected messages cannot be forwarded, the dialog says they will be skipped.

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
