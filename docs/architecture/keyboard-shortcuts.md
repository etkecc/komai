# ⌨️ Keyboard Shortcuts Architecture

This document describes how keyboard shortcuts are implemented in Komai and where different kinds
of shortcuts should live.

For the user-facing shortcut list, see [../user-guide/keyboard-shortcuts.md](../user-guide/keyboard-shortcuts.md).

## Layers

Komai uses three main shortcut layers:

1. `Shortcut` objects for commands that should work without custom key-event parsing.
2. `Keys.onPressed` handlers for focus-sensitive behavior, typed text, and multi-step precedence.
3. Small helper objects when the same shortcut behavior needs to be shared or made layout-agnostic.

The codebase intentionally mixes these approaches. There is no single global shortcut registry.

## Common Patterns

### App-wide shortcuts

App-wide bindings usually live in dedicated QML containers and use `Shortcut`:

- [resources/qml/shell/components/AppShortcuts.qml](../../resources/qml/shell/components/AppShortcuts.qml)
- [resources/qml/shell/components/RoomListActionsBar.qml](../../resources/qml/shell/components/RoomListActionsBar.qml)
- [resources/qml/room/components/RoomHeader.qml](../../resources/qml/room/components/RoomHeader.qml)

Use this pattern when:

- the action is global or window-wide
- it does not depend on typed text
- Qt's shortcut routing is good enough

Prefer `StandardKey` for platform-standard actions such as quit, find, cancel, zoom, or paging.
Use explicit sequences only for app-specific commands such as room switching or dialog actions.

### View-local shortcuts

Timeline, overlay, and dialog shortcuts often use local `Shortcut` objects inside the component that
owns the behavior:

- [resources/qml/shell/CommunitiesList.qml](../../resources/qml/shell/CommunitiesList.qml)
- [resources/qml/shell/RoomList.qml](../../resources/qml/shell/RoomList.qml)
- [resources/qml/timeline/components/TimelineKeyboardShortcuts.qml](../../resources/qml/timeline/components/TimelineKeyboardShortcuts.qml)
- [resources/qml/dialogs/timeline/MessageActionsDialog.qml](../../resources/qml/dialogs/timeline/MessageActionsDialog.qml)
- [resources/qml/dialogs/media/MediaOverlay.qml](../../resources/qml/dialogs/media/MediaOverlay.qml)
- [resources/qml/shell/components/RoomJoinCreateDialog.qml](../../resources/qml/shell/components/RoomJoinCreateDialog.qml)

This keeps enable/disable logic close to the UI state that owns the shortcut.

### Raw key handling

Use `Keys.onPressed` when shortcut behavior depends on current focus, typed characters, or ordered
precedence that would be awkward with plain `Shortcut` objects.

Important examples:

- [resources/qml/shell/CommunitiesList.qml](../../resources/qml/shell/CommunitiesList.qml)
  The communities sidebar keeps a keyboard cursor separate from the active filter and handles
  `Tab`, arrows, Vim-like movement keys, space collapse/expand, `Enter`, and `Escape` in one place.
- [resources/qml/shell/RoomList.qml](../../resources/qml/shell/RoomList.qml)
  The room list uses the same focused-list pattern, including local `Tab` routing to the communities
  sidebar, Vim-like movement keys, page-sized jumps, and `Enter` activation.
- [resources/qml/timeline/TimelineView.qml](../../resources/qml/timeline/TimelineView.qml)
  Timeline typing moves focus into the composer, except while Selection mode is active.
- [resources/qml/composer/MessageInput.qml](../../resources/qml/composer/MessageInput.qml)
  The composer distinguishes paste, send/newline logic, completer navigation, voice recording
  toggle (`Ctrl+R`), and Selection mode entry from the start-of-top-line `Up` boundary.
- [resources/qml/timeline/components/MatrixRoomWalkModeSupport.qml](../../resources/qml/timeline/components/MatrixRoomWalkModeSupport.qml)
  Selection mode movement, action keys, inline-actions navigation, `gg`, and `Escape` precedence
  live in the matrix room walk-mode controller.

Use raw key handling when:

- the key may also produce text
- focus and mode state matter
- the action is a sequence such as `gg`
- the action is not a simple one-shot command

## Sidebar List Focus Mode

The communities sidebar and room list now use the same focused-list model:

- an application-scoped `Shortcut` enters the list directly
- `focusKeyboardNavigation()` seeds a keyboard cursor from the current filter or room
- the target `ListView` receives focus with `Qt.callLater(...)`
- movement changes the keyboard cursor only; activation is deferred to `Enter`
- `ensureKeyboardCursorVisible()` keeps the focused row scrolled into view

Entry shortcuts:

- [resources/qml/shell/CommunitiesList.qml](../../resources/qml/shell/CommunitiesList.qml): `Ctrl+Shift+C`
- [resources/qml/shell/RoomList.qml](../../resources/qml/shell/RoomList.qml): `Ctrl+Shift+R`

Both list views use `Keys.onPressed` for the actual navigation layer because behavior depends on
focus, key sequences, and local routing rules:

- `Up` / `Down` and `j` / `k` move the keyboard cursor
- `Home` / `End`, `gg`, and `Shift+G` jump within the list
- `Ctrl+U` / `Ctrl+D` move about half a screen
- `Enter` activates the focused room or filter
- `Escape` returns focus to the composer
- `Tab` / `Shift+Tab` cycle locally between the two sidebar lists

The communities sidebar adds `Left` / `Right` and `h` / `l` for collapsing or expanding the
currently focused space.

This keeps sidebar browsing predictable: arrows and Vim-like keys inspect items first, while
`Enter` is the commit point that actually switches the room or filter.

## Selection Mode Implementation

Selection mode is implemented by
[resources/qml/timeline/components/MatrixRoomView.qml](../../resources/qml/timeline/components/MatrixRoomView.qml)
and
[resources/qml/timeline/components/MatrixRoomWalkModeSupport.qml](../../resources/qml/timeline/components/MatrixRoomWalkModeSupport.qml).

Its view-local state is:

- `walkModeActive`
- `focusedEventId`
- `selectedEventIds`
- `selectionAnchorEventId`

Despite the internal `walkMode*` names, user-facing docs and UI call this feature `Selection mode`.

### Entry points

The main entry paths are:

- plain `Up` from [resources/qml/composer/MessageInput.qml](../../resources/qml/composer/MessageInput.qml) when the composer caret is already at the start of the top line; it enters at the bottom-most visible message
- `Ctrl+U` from the composer, routed from [resources/qml/composer/MessageInput.qml](../../resources/qml/composer/MessageInput.qml) into the timeline
- `Ctrl+Click` on a timeline message, routed from [resources/qml/timeline/TimelineMessageStyleBase.qml](../../resources/qml/timeline/TimelineMessageStyleBase.qml) into [resources/qml/timeline/components/MatrixRoomView.qml](../../resources/qml/timeline/components/MatrixRoomView.qml)

`TimelineView.qml` also blocks its usual "type to focus composer" behavior while Selection mode is
active.

Komai intentionally does not use Vim's `Escape`-to-enter-navigation model. Here, `Escape` is a
return-to-compose key: it closes the nearest active timeline/composer sub-state first and otherwise
focuses the composer instead of entering Selection mode.

### Key dispatch

`MessageView.handleWalkModeKey()` is the central dispatcher for Selection mode keys. It handles:

- movement keys such as `Up`, `Down`, `j`, `k`, `Ctrl+U`, `Ctrl+D`, `gg`, and `Shift+G`
- focus-routing keys such as `Tab`, `Shift+Tab`, `Left` / `Right`, and `h` / `l`
- action keys such as `r`, `t`, `e`, `f`, `d`, `u`, `o`, and `Enter`
- Selection mode lifecycle keys such as `Space`, `Escape`, `i`, and `?`
- inline action-bar navigation when keyboard actions are open

This is intentionally one ordered handler because `Escape`, `Enter`, and focus movement depend on
current sub-state.

`Ctrl+C` and `Ctrl+Shift+C` are handled outside `handleWalkModeKey()`. That keeps standard copy
sequences out of the layout-agnostic Latin-key path and lets Selection mode defer to
delegate-local text selection when focus is inside a message text item.

### Selection mode focus routing

When inline message actions are not open, Selection mode has three nearby focus zones:

- the timeline selection itself
- the bottom Selection mode bar
- the room header action buttons

`MessageView.qml` owns the routing rules, but it calls helper methods on:

- [resources/qml/timeline/components/TimelineWalkModeBar.qml](../../resources/qml/timeline/components/TimelineWalkModeBar.qml) for first/last/next/previous enabled Selection mode bar buttons
- [resources/qml/room/components/RoomHeader.qml](../../resources/qml/room/components/RoomHeader.qml) for the last visible room-header action button

Current behavior:

- `Tab` from the timeline focuses the first enabled Selection mode bar button
- `Shift+Tab` from the timeline focuses the last visible room-header action button
- `Shift+Tab` from the first Selection mode bar button returns to the timeline
- `Left` / `Right` and `h` / `l` move through the Selection mode bar, with timeline-to-bar entry from the matching edge

### Action targeting

Selection mode distinguishes the focused message from the explicit selection.

Current targeting rule:

- `forward` and `remove` operate on all selected messages that support that action, ordered from
  oldest to newest
- `Ctrl+C` and `Ctrl+Shift+C` also iterate the current selection in oldest-to-newest order,
  skipping messages that have no copyable text
- otherwise one selected message wins
- otherwise the focused message is used
- with more than one selected message, the other direct actions remain unavailable

That targeting logic is kept near `MessageView` so keyboard shortcuts, the bottom Selection mode
bar, and inline actions use the same event target.

### Help dialog

The Selection mode help dialog lives in [resources/qml/dialogs/timeline/SelectionModeHelpDialog.qml](../../resources/qml/dialogs/timeline/SelectionModeHelpDialog.qml).

It is intentionally reference-only:

- rows look like message-action rows, but are not clickable
- dialog-local `Shortcut` objects only show a warning on the matching row
- the shortcuts do not execute Selection mode actions while the help dialog is open

This avoids teaching the user that the help dialog itself is an active shortcut surface.

## Layout-Agnostic Latin Keys

Selection mode, sidebar list focus mode, and composer `Ctrl+U` use
[src/ui/LayoutAgnosticKeys.cpp](../../src/ui/LayoutAgnosticKeys.cpp) and
[src/ui/LayoutAgnosticKeys.h](../../src/ui/LayoutAgnosticKeys.h) for Vim-like Latin-letter keys.

`LayoutAgnosticKeys.matchesLatinKey()` checks:

1. the logical Qt key first
2. native scan codes as a fallback on Linux and Windows
3. no native fallback on macOS yet

This makes keys such as `j`, `k`, `h`, `l`, `d`, `u`, `g`, and `o` follow the same physical key on
supported platforms even when the active keyboard layout is non-Latin.

Current call sites:

- [resources/qml/shell/CommunitiesList.qml](../../resources/qml/shell/CommunitiesList.qml)
- [resources/qml/shell/RoomList.qml](../../resources/qml/shell/RoomList.qml)
- [resources/qml/timeline/components/MatrixRoomWalkModeSupport.qml](../../resources/qml/timeline/components/MatrixRoomWalkModeSupport.qml)
- [resources/qml/composer/MessageInput.qml](../../resources/qml/composer/MessageInput.qml) for composer `Ctrl+U`

Qt's own `Shortcut` handling is still used elsewhere for many custom shortcuts such as `Alt+J` or
`Ctrl+N`. Those may already work across layouts on some platforms, but they are not using
`LayoutAgnosticKeys`.

### Adding another layout-agnostic key

To add another Latin-letter key to this system:

1. Add its logical key and native scan-code mapping to `LayoutAgnosticKeys.cpp`.
2. Use `LayoutAgnosticKeys.matchesLatinKey(...)` from the relevant QML handler.
3. Update [../user-guide/keyboard-shortcuts.md](../user-guide/keyboard-shortcuts.md) if the binding is user-facing.

Keep this helper small and deliberate. It is intended for bindings where physical-key behavior is
important, not for every shortcut in the app.

## Practical Rules

When adding or changing shortcuts:

- prefer `StandardKey` for standard commands
- prefer local `Shortcut` objects for simple commands owned by one view or dialog
- use `Keys.onPressed` when focus, text input, precedence, or key sequences matter
- keep user-facing wording in [../user-guide/keyboard-shortcuts.md](../user-guide/keyboard-shortcuts.md)
- keep implementation rationale in architecture docs like this one

If a shortcut must survive non-Latin keyboard layouts in a physical-key sense, do not assume plain
Qt shortcut parsing is enough. Consider `LayoutAgnosticKeys` explicitly.
