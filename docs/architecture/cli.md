# CLI Subcommand Architecture

Komai supports headless CLI subcommands that run without a display server.
CLI commands use `QCoreApplication` (not `QApplication`), so they work
over SSH, in containers, and in CI pipelines.

Commands fall into three categories:

- **IPC-backed** — talk to a running Komai instance over a local `QLocalSocket`
  transport
  (`app`, `rooms`, `user`, `settings`, `media`)
- **Wrapper-backed** — dispatch to another local helper process
  (`mcp`)
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
  │         ├─ runAppCommand()          ← IPC: version, api-version
  │         ├─ runMcpCommand()          ← wrapper: launches komai-mcp
  │         ├─ runRoomsCommand()        ← IPC: list, timeline, activate, join, membership
  │         ├─ runUserCommand()         ← IPC: status, set-status
  │         ├─ runSettingsCommand()     ← IPC: ui theme, ui set-theme
  │         ├─ runMediaCommand()        ← IPC: fetch
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
| `app`      | `runAppCommand`       | `src/cli/AppCommands.cpp`     | IPC      |
| `media`    | `runMediaCommand`     | `src/cli/MediaCommands.cpp`   | IPC      |
| `mcp`      | `runMcpCommand`       | `src/cli/McpCommands.cpp`     | Wrapper  |
| `rooms`    | `runRoomsCommand`     | `src/cli/RoomCommands.cpp`    | IPC      |
| `settings` | `runSettingsCommand`  | `src/cli/SettingsCommands.cpp` | IPC     |
| `theme`    | `runThemeCommand`     | `src/cli/ThemeCommands.cpp`   | Offline  |
| `user`     | `runUserCommand`      | `src/cli/UserCommands.cpp`    | IPC      |


## IPC transport

CLI commands communicate with the running Komai instance via a per-profile
`QLocalServer`/`QLocalSocket` pair:

- Unix: Unix domain socket
- Windows: named pipe

### Protocol

Simple JSON-lines over the transport — one request line, one response line:

```
→  {"method":"rooms.list","params":{"limit":2,"fields":["id","name"]}}
←  {"result":{"rooms":[...],"matchCount":37}}

→  {"method":"user.setStatusMessage","params":{"message":"brb"}}
←  {"result":true}

→  {"method":"media.fetch","params":{"mxcUri":"mxc://example.org/abc"}}
←  {"result":"<base64-encoded PNG>"}

→  {"method":"unknown.thing"}
←  {"error":"unknown method: unknown.thing"}
```

### Method table

| Method                       | Params                        | Result type     |
|------------------------------|-------------------------------|-----------------|
| `app.version`                | —                             | `string`        |
| `app.apiVersion`             | —                             | `string`        |
| `rooms.list`                 | all optional: `ids`, `query`, `isDm`, `encrypted`, `tag`, `parentSpace`, `minMemberCount`, `limit`, `offset`, `fields` | `object` |
| `rooms.timeline`             | `roomIdOrAlias`, `limit`?, `beforeEventId`?, `includeUnsignedFields`?, `fetchMode`? | `object` |
| `rooms.join`                 | `roomIdOrAlias`               | `true`          |
| `rooms.create`               | all optional: `name`, `topic`, `aliasLocalpart`, `preset`, `invite`, `isDirect`, `isEncrypted`, `isSpace`, `isPublic`, `roomVersion`, `powerLevelContentOverride`, `initialState`, `creationContent` | `object` |
| `rooms.newDirectChat`        | `userId`                      | `true`          |
| `rooms.invite`               | `roomIdOrAlias`, `userId`, `reason`? | `true`   |
| `rooms.kick`                 | `roomIdOrAlias`, `userId`, `reason`? | `true`   |
| `rooms.ban`                  | `roomIdOrAlias`, `userId`, `reason`? | `true`   |
| `rooms.unban`                | `roomIdOrAlias`, `userId`, `reason`? | `true`   |
| `rooms.leave`                | `roomIdOrAlias`, `reason`?    | `true`          |
| `rooms.getState`             | `roomIdOrAlias`, `eventType`, `stateKey`? | `object` |
| `rooms.setState`             | `roomIdOrAlias`, `eventType`, `content`, `stateKey`? | `object` |
| `rooms.setName`              | `roomIdOrAlias`, `name`       | `true`          |
| `rooms.setTopic`             | `roomIdOrAlias`, `topic`      | `true`          |
| `rooms.setPowerLevel`        | `roomIdOrAlias`, `userId`, `powerLevel` | `true` |
| `rooms.redact`               | `roomIdOrAlias`, `eventId`, `reason`? | `object` |
| `rooms.markRead`             | `roomIdOrAlias`, `eventId`?, `public`? | `true` |
| `rooms.markUnread`           | `roomIdOrAlias`, `unread`?    | `true`          |
| `rooms.readReceipts`         | `roomIdOrAlias`, `eventId`    | `object`        |
| `rooms.send`                 | `roomIdOrAlias`, `body`, `msgtype`?, `format`? | `object` |
| `rooms.sendImageFile`        | `roomIdOrAlias`, `path`, `body`? | `object`  |
| `rooms.sendImage`            | `roomIdOrAlias`, `mxcUri`, `body`?, `filename`?, `info`? | `object` |
| `user.userId`                | —                             | `string`        |
| `user.homeserverUrl`         | —                             | `string`        |
| `user.deviceId`              | —                             | `string`        |
| `user.statusMessage`         | —                             | `string`        |
| `user.setStatusMessage`      | `message`                     | `true`          |
| `settings.ui.theme`          | —                             | `string`        |
| `settings.ui.setTheme`       | `theme`                       | `true`          |
| `media.fetch`                | `mxcUri`                      | `string` (b64)  |
| `media.upload`               | `path`, `filename`?, `contentType`? | `object`  |

### Socket naming

The socket name follows the pattern `komai-cli-<profile-id>`, e.g.
`komai-cli-default` or `komai-cli-work`. The socket is placed wherever
`QLocalServer` defaults to (typically `$XDG_RUNTIME_DIR` or `/tmp`).

### Architecture layers

```
CLI process                              GUI process
───────────                              ───────────
IpcClient.h                              IpcServer.h/.cpp
  cli_ipc::call()                          ↓ QLocalServer
  ↓ QLocalSocket                         handleRequest()
  → JSON request line ──────────────→      ↓
  ← JSON response line ←────────────    SharedLogic.h/.cpp
                                          (business logic)

                                        D-Bus adaptors (Backend.cpp)
                                          ↓ access checks
                                        SharedLogic.h/.cpp
                                          (same business logic)
```

The shared business logic in `src/ipc/SharedLogic.h/.cpp` is called by both
the IPC server (no access gating — socket filesystem permissions suffice) and
the D-Bus adaptors (which wrap calls with `dbusReadAccessEnabled()` /
`dbusWriteAccessEnabled()` checks).


## IPC-backed commands

IPC commands use the client helpers in `src/cli/IpcClient.h` to send
JSON-line requests to a running Komai instance over a local socket.

The `-p` / `--profile` flag selects which profile to target (maps to the
socket name `komai-cli-<id>`). Default profile is `default`.

The `settings` group uses nested dispatch:
`settings` → subgroup (`ui`) → subcommand (`theme`, `set-theme`).

The `rooms` group currently exposes read subcommands `list` and `timeline`, plus write
subcommands such as `activate`, `join`, `new-direct-chat`, `send`, image sending, and the
membership operations `invite`, `kick`, `ban`, `unban` and `leave`, `create`, and the state
operations `get-state`, `set-state`, `set-name`, `set-topic` and `set-power-level`, plus
`redact`, `mark-read`, `mark-unread` and `read-receipts`.

## MCP wrapper command

`komai mcp serve` is a thin C++ wrapper around the Rust `komai-mcp` binary.

The wrapper is responsible for:

- making `mcp` show up in `komai --help`
- reusing the normal `-p` / `--profile` parsing semantics
- forwarding the resolved profile to `komai-mcp`
- forwarding `--access read_only|read_write`

The Rust helper then speaks MCP over stdio and uses the same IPC transport
protocol described above as its backend boundary.


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

For IPC commands, use the helpers in `IpcClient.h`:
```cpp
#include "IpcClient.h"

auto args      = cli_ipc::positionalsAfter(argc, argv, QStringLiteral("foo"));
auto profileId = cli_ipc::profileFromArgs(argc, argv);
if (!cli_ipc::ensureConnected(profileId))
    return 1;
auto response = cli_ipc::call(profileId, QStringLiteral("foo.bar"));
```

To add a new server-side method, add the business logic to `SharedLogic.h/.cpp`
and register the method name in `IpcServer::handleRequest()`.


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

`cli_ipc::positionalsAfter()` uses the same skipping rules to collect all
positional arguments after a given keyword (the group name), which gives
each handler its subcommand and arguments.


## HTTP client

Theme commands that need network access use `QNetworkAccessManager` with a
local `QEventLoop` for synchronous HTTP. A 15-second timeout is enforced via
`QTimer`. The `Network` component is added to `find_package(Qt6 ...)` in
`CMakeLists.txt`.
