# CLI Subcommand Architecture

Komai supports headless CLI subcommands that run without a display server.
CLI commands use `QCoreApplication` (not `QApplication`), so they work
over SSH, in containers, and in CI pipelines.


## Dispatch flow

```
main()
  │
  ├─ dispatchCliCommand(argc, argv)     ← src/cli/CliDispatch.cpp
  │    │
  │    ├─ findCommandGroup()            ← scans argv for first positional arg
  │    │
  │    ├─ (no match) → return -1        ← caller proceeds to GUI
  │    │
  │    └─ (match) → QCoreApplication + handler
  │         │
  │         └─ runThemeCommand()        ← src/cli/ThemeCommands.cpp
  │              │
  │              ├─ tinted-import
  │              ├─ tinted-search
  │              ├─ list
  │              └─ create-sample
  │
  └─ (cliResult >= 0) → return cliResult
  │
  └─ QApplication app(...)              ← normal GUI startup
```


## Command group registry

`CliDispatch.cpp` maintains a `std::map<QString, HandlerFn>` mapping command
group names to handler functions. Currently registered groups:

| Group   | Handler            | File                      |
|---------|--------------------|---------------------------|
| `theme` | `runThemeCommand`  | `src/cli/ThemeCommands.cpp` |


## Adding a new command group

1. Create `src/cli/FooCommands.h/.cpp` with a handler function:
   ```cpp
   int runFooCommand(int argc, char *argv[], QCoreApplication &app);
   ```
2. Register it in `CliDispatch.cpp`:
   ```cpp
   #include "FooCommands.h"
   // In commandGroups():
   {QStringLiteral("foo"), runFooCommand},
   ```
3. Add the source files to `CMakeLists.txt` in the `SRC_FILES` CLI section.
4. Done — `komai foo <subcommand>` is now available.


## Adding a new theme subcommand

1. Write a handler function in `ThemeCommands.cpp`:
   ```cpp
   static int handleNewThing(int argc, char *argv[], QCoreApplication &app) { ... }
   ```
2. Register it in the `subcommands()` table:
   ```cpp
   {QStringLiteral("new-thing"), handleNewThing},
   ```
3. Update the help text in `runThemeCommand()`.


## argv scanning

`findCommandGroup()` skips:
- Known option+value pairs (`-p profile`, `--log-level trace`, etc.)
- Flags starting with `-`
- `matrix:` URIs

The first remaining positional argument is treated as the command group name.
This lets `komai -p myprofile theme list` work correctly.


## HTTP client

Theme commands that need network access use `QNetworkAccessManager` with a
local `QEventLoop` for synchronous HTTP. A 15-second timeout is enforced via
`QTimer`. The `Network` component is added to `find_package(Qt6 ...)` in
`CMakeLists.txt`.
