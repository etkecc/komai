# QML/UI Structure

This document defines where QML files live and how dependencies should flow between folders.

## Goals

- Keep ownership obvious by grouping files by domain/purpose.
- Keep file moves low risk by avoiding path-coupled imports and scattered loader strings.
- Keep dependency direction one-way (from higher-level shells/pages to lower-level reusable UI).

## Folder Layout

- `resources/qml/ui/`: low-level visual primitives and reusable UI utilities.
- `resources/qml/components/`: cross-screen reusable domain components.
- `resources/qml/dialogs/`: dialogs and overlays, grouped by purpose:
  - `account/`
  - `common/`
  - `media/`
  - `moderation/`
  - `navigation/`
  - `room/`
  - `timeline/`
  - `user/`
- `resources/qml/pages/`: full-screen/page entry points.
- `resources/qml/shell/`: app-shell composition (room list, app root, chat shell, shell coordinators).
- `resources/qml/shell/components/`: shell-local components and menus.
- `resources/qml/timeline/`: timeline-specific UI and styles.
- `resources/qml/timeline/components/`: timeline-local extracted subcomponents.
- `resources/qml/timeline/styles/`: style families and style-specific delegates.
- `resources/qml/composer/`: message composer/input cluster.
- `resources/qml/room/`: room-header and room-meta sections shared by timeline/shell.

Rule: avoid adding new root-level `resources/qml/*.qml` files. Place new files in one of the domain folders above.

## Layering Rules

- `ui/` is the lowest layer. It must not depend on `shell/`, `pages/`, `dialogs/`, or `timeline/`.
- `components/` can depend on `ui/` and model/context types from `cc.etke.komai`.
- `dialogs/` can depend on `ui/`, `components/`, and domain folders they directly serve.
- `timeline/`, `room/`, `composer/`, and `shell/` are feature layers that compose lower layers.
- `pages/` and `shell/Root.qml` are top-level composition entry points.

Keep dependencies directional. If a lower-level folder needs behavior from a higher-level folder, move that behavior upward instead of adding an upward import.

## Dynamic Component Loading Rule

When using `Qt.createComponent`, reference component URLs through a central catalog object instead of inlining raw `qrc:` strings at each callsite.

Benefits:

- path changes during file moves require edits in one place.
- fewer string duplication bugs.
- easier review of runtime-loaded component surface.

Catalog file:

- `resources/qml/ui/ComponentCatalog.qml`

Rule: every new runtime-loaded dialog/component URL must be added to `ComponentCatalog` and consumed through that catalog key.

## Import Rules

- Prefer `import cc.etke.komai` + direct type usage over deep relative imports like `../..`.
- Use relative imports only when there is no stable module/type option.
- Keep import changes mechanical and behavior-preserving; avoid mixing structural moves with functional changes.

## Refactoring Status

- Root-level QML files have been moved into domain folders (no `resources/qml/*.qml` files remain).
- Dialog-style root files were moved into `resources/qml/dialogs/navigation/` (`QuickSwitcher`, `ForwardCompleter`).
- Core UI primitives were moved into `resources/qml/ui/`.
- Composer-related files were moved into `resources/qml/composer/`.
