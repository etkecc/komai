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

- [resources/qml/timeline/components/TimelineKeyboardShortcuts.qml](../../resources/qml/timeline/components/TimelineKeyboardShortcuts.qml)
- [resources/qml/dialogs/timeline/MessageActionsDialog.qml](../../resources/qml/dialogs/timeline/MessageActionsDialog.qml)
- [resources/qml/dialogs/media/MediaOverlay.qml](../../resources/qml/dialogs/media/MediaOverlay.qml)
- [resources/qml/shell/components/RoomJoinCreateDialog.qml](../../resources/qml/shell/components/RoomJoinCreateDialog.qml)

This keeps enable/disable logic close to the UI state that owns the shortcut.

### Raw key handling

Use `Keys.onPressed` when shortcut behavior depends on current focus, typed characters, or ordered
precedence that would be awkward with plain `Shortcut` objects.

Important examples:

- [resources/qml/timeline/TimelineView.qml](../../resources/qml/timeline/TimelineView.qml)
  Timeline typing moves focus into the composer, except while Selection mode is active.
- [resources/qml/composer/MessageInput.qml](../../resources/qml/composer/MessageInput.qml)
  The composer distinguishes paste, send/newline logic, completer navigation, edit-history
  navigation, and Selection mode entry.
- [resources/qml/timeline/MessageView.qml](../../resources/qml/timeline/MessageView.qml)
  Selection mode movement, action keys, inline-actions navigation, `gg`, and `Escape` precedence
  all live here.

Use raw key handling when:

- the key may also produce text
- focus and mode state matter
- the action is a sequence such as `gg`
- the action is not a simple one-shot command

## Selection Mode Implementation

Selection mode is implemented in [resources/qml/timeline/MessageView.qml](../../resources/qml/timeline/MessageView.qml).

Its view-local state is:

- `walkModeActive`
- `focusedEventId`
- `selectedEventIds`

Despite the internal `walkMode*` names, user-facing docs and UI call this feature `Selection mode`.

### Entry points

The main entry paths are:

- `Escape` in the timeline via `handleEscape()`
- `Alt+Up` in [resources/qml/timeline/components/TimelineKeyboardShortcuts.qml](../../resources/qml/timeline/components/TimelineKeyboardShortcuts.qml)
- `Ctrl+U` from the composer, routed from [resources/qml/composer/MessageInput.qml](../../resources/qml/composer/MessageInput.qml) into the timeline

`TimelineView.qml` also blocks its usual "type to focus composer" behavior while Selection mode is
active.

### Key dispatch

`MessageView.handleWalkModeKey()` is the central dispatcher for Selection mode keys. It handles:

- movement keys such as `Up`, `Down`, `j`, `k`, `Ctrl+U`, `Ctrl+D`, `gg`, and `Shift+G`
- action keys such as `r`, `t`, `e`, `f`, `d`, `u`, `o`, and `Enter`
- Selection mode lifecycle keys such as `Space`, `Escape`, `i`, and `?`
- inline action-bar navigation when keyboard actions are open

This is intentionally one ordered handler because `Escape`, `Enter`, and focus movement depend on
current sub-state.

### Action targeting

Selection mode distinguishes the focused message from the explicit selection.

Current targeting rule:

- one selected message wins
- otherwise the focused message is used
- multiple selected messages are allowed, but direct message actions no-op in the current pass

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

Selection mode uses [src/ui/LayoutAgnosticKeys.cpp](../../src/ui/LayoutAgnosticKeys.cpp) and
[src/ui/LayoutAgnosticKeys.h](../../src/ui/LayoutAgnosticKeys.h) for Vim-like Latin-letter keys.

`LayoutAgnosticKeys.matchesLatinKey()` checks:

1. the logical Qt key first
2. native scan codes as a fallback on Linux and Windows
3. no native fallback on macOS yet

This makes keys such as `j`, `k`, `d`, `u`, `g`, and `o` follow the same physical key on supported
platforms even when the active keyboard layout is non-Latin.

Current call sites:

- [resources/qml/timeline/MessageView.qml](../../resources/qml/timeline/MessageView.qml)
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
