# QML/UI Structure

This document defines how QML files should be organized and how dependencies should flow as we refactor the current layout.

## Current Issues

- Too many root-level files under `resources/qml/` make ownership unclear.
- Many dynamic loaders hardcode `qrc:/resources/qml/...` paths across multiple files.
- Deep relative imports (`..`, `../..`) tightly couple files to directory layout.

## Target Layering

- `resources/qml/ui/`: low-level visual primitives and reusable UI utilities.
- `resources/qml/components/`: cross-screen reusable domain components.
- `resources/qml/dialogs/`: dialogs and overlays.
- `resources/qml/pages/`: full-screen/page entry points.
- `resources/qml/timeline/`: timeline-specific UI and styles.
- `resources/qml/shell/` (planned): app-shell composition (main chat shell, room list shell, top bar shell).
- `resources/qml/composer/` (planned): message composer/input cluster.

Rule: avoid creating new root-level files in `resources/qml/` unless they are true top-level application entry points.

Current move status:

- `QuickSwitcher.qml` moved from root to `resources/qml/dialogs/QuickSwitcher.qml`.
- `ForwardCompleter.qml` moved from root to `resources/qml/dialogs/ForwardCompleter.qml`.
- Core UI primitives moved from root to `resources/qml/ui/`:
  - `ElidedLabel.qml`
  - `MatrixText.qml`
  - `MatrixTextField.qml`
  - `StatusIndicator.qml`
  - `ToggleButton.qml`

## Dynamic Component Loading Rule

When using `Qt.createComponent`, reference component URLs through a central catalog object rather than inlining raw `qrc:` strings at each callsite.

Benefits:

- path changes during file moves require edits in one place
- fewer string duplication bugs
- easier review of runtime-loaded component surface

Current catalog file:

- `resources/qml/ui/ComponentCatalog.qml`

## Import Rule (Migration Direction)

- Prefer stable module/type access over deep relative import chains when practical.
- During migration, keep behavior unchanged first; restructure imports in mechanical, reviewable batches.
