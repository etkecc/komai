# CLI Subcommand Architecture

Komai supports headless CLI subcommands that run without a display server.
CLI commands use `QCoreApplication` (not `QApplication`), so they work
over SSH, in containers, and in CI pipelines.

Commands fall into two categories:

- **D-Bus-backed** — talk to a running Komai instance over D-Bus
  (`app`, `rooms`, `user`, `settings`, `media`)
- **Offline** — work without a running instance (`theme`)


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
  │         ├─ runAppCommand()          ← D-Bus: version, api-version
  │         ├─ runRoomsCommand()        ← D-Bus: list, activate, join, new-direct-chat
  │         ├─ runUserCommand()         ← D-Bus: status, set-status
  │         ├─ runSettingsCommand()     ← D-Bus: ui theme, ui set-theme
  │         ├─ runMediaCommand()        ← D-Bus: fetch
  │         └─ runThemeCommand()        ← offline: tinted-import, tinted-search, list, create-sample
  │
  └─ (cliResult >= 0) → return cliResult
  │
  └─ QApplication app(...)              ← normal GUI startup
```


## Command group registry

`CliDispatch.cpp` maintains a `std::map<QString, HandlerFn>` mapping command
group names to handler functions. Currently registered groups:

| Group      | Handler               | File                          | Backend  |
|------------|-----------------------|-------------------------------|----------|
| `app`      | `runAppCommand`       | `src/cli/AppCommands.cpp`     | D-Bus    |
| `media`    | `runMediaCommand`     | `src/cli/MediaCommands.cpp`   | D-Bus    |
| `rooms`    | `runRoomsCommand`     | `src/cli/RoomCommands.cpp`    | D-Bus    |
| `settings` | `runSettingsCommand`  | `src/cli/SettingsCommands.cpp` | D-Bus   |
| `theme`    | `runThemeCommand`     | `src/cli/ThemeCommands.cpp`   | Offline  |
| `user`     | `runUserCommand`      | `src/cli/UserCommands.cpp`    | D-Bus    |


## D-Bus-backed commands

D-Bus commands use the client API in `src/dbus/Api.h` to call a running Komai
instance over the session bus.  Shared boilerplate (profile extraction, D-Bus
init, connectivity check) lives in `src/cli/CliDbusHelper.h`.

The `-p` / `--profile` flag selects which profile to target (maps to the D-Bus
service name `cc.etke.komai.profile.<id>`).  Default profile is `default`.

The `settings` group uses nested dispatch:
`settings` → subgroup (`ui`) → subcommand (`theme`, `set-theme`).


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

For D-Bus commands, use the helpers in `CliDbusHelper.h`:
```cpp
#include "CliDbusHelper.h"

auto args      = cli_dbus::positionalsAfter(argc, argv, QStringLiteral("foo"));
auto profileId = cli_dbus::profileFromArgs(argc, argv);
if (!cli_dbus::ensureConnected(profileId))
    return 1;
// Call dbus::* functions from Api.h
```


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
This lets `komai -p myprofile rooms list` work correctly.

`cli_dbus::positionalsAfter()` uses the same skipping rules to collect all
positional arguments after a given keyword (the group name), which gives
each handler its subcommand and arguments.


## HTTP client

Theme commands that need network access use `QNetworkAccessManager` with a
local `QEventLoop` for synchronous HTTP. A 15-second timeout is enforced via
`QTimer`. The `Network` component is added to `find_package(Qt6 ...)` in
`CMakeLists.txt`.
