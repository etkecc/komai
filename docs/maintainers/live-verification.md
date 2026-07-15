<!--
SPDX-FileCopyrightText: Komai Contributors

SPDX-License-Identifier: GPL-3.0-or-later
-->

# 🤖 Live Verification (Driving Komai Without Clicking)

How to exercise a real, running Komai end-to-end from a shell: launch it headless, drive flows through its CLI and IPC surfaces, and capture logs and screenshots as evidence. Written with AI agents and scripted verification in mind, but every technique works interactively too.

For the CLI/IPC architecture itself (command groups, dispatch flow, transport), see [CLI Subcommand Architecture](../architecture/cli.md).


## Launching headless

Komai runs fine on a virtual X server, which allows launching and screenshotting it without touching the desktop session:

```sh
Xvfb :77 -screen 0 1600x1000x24 &
DISPLAY=:77 QT_QPA_PLATFORM=xcb ./var/build/native/komai -p <profile> > /tmp/komai-run.log 2>&1 &
```

When the behavior under test is observable purely through logs, skip Xvfb entirely and render to nowhere; wrapping in `timeout` makes the run self-terminating, which is handy for scripted one-shot checks:

```sh
QT_QPA_PLATFORM=offscreen timeout 30 ./var/build/native/komai -p <profile> > /tmp/komai-run.log 2>&1
```

- The redirected output is the primary evidence stream: it carries both the C++ `komai::logging` output and the Rust/matrix-sdk `tracing` output.
- Screenshot the virtual display with ImageMagick: `DISPLAY=:77 import -window root shot.png`. For catching transient UI states (highlight flashes, spinners), capture in a tight loop and diff frames with `magick compare -metric AE`.
- Use a dedicated test profile (`-p <profile>`), never a profile holding a real session you care about. Profile state lives under `~/.config/komai/profiles/<profile-id>/`.

Two process-lifetime gotchas:

- **Closing the window quits the process** (unless the tray/background setting keeps it alive). A "did my window survive" check is worth adding to longer scripts.
- **Do not pipe the launch through `head`** (or anything that exits early). When the pipe reader exits, the next log write kills Komai with SIGPIPE, seconds or minutes later, which is confusing to debug. Redirect to a file and filter the file instead.


## Driving a running instance

Komai is single-instance per profile, and several surfaces route into the running app:

- **CLI subcommands** talk to the running instance over IPC:

  ```sh
  komai -p <profile> rooms list
  komai -p <profile> rooms timeline '#room:example.com'
  komai -p <profile> rooms join '!roomid:example.com'
  komai -p <profile> rooms send '!roomid:example.com' 'hello'
  ```

- **`matrix:` URIs on the command line** are forwarded to the running instance and handled exactly like a clicked link, making link-navigation flows scriptable:

  ```sh
  komai -p <profile> 'matrix:roomid/<id-without-!>/e/<eventid-without-$>?via=example.com'
  ```

- **The MCP server** (`komai mcp`, or the `komai-mcp` wrapper) exposes rooms/media/settings tools to MCP clients, backed by the same IPC.

Logs are the observable for most flows: run with the default `info` level and grep the output for the subsystem lines you care about. `--log-level` accepts `EnvFilter`-style directives (e.g. `warn,ui=debug`) when you need more or less. Note that oversized log events are truncated (`KOMAI_LOG_MAX_EVENT_BYTES` overrides, `0` disables).


## Seeding a test world

For flows that need specific history (pagination, threads, permalinks), build it on a throwaway homeserver rather than a real room:

- Any local homeserver with open registration works; a containerized Continuwuity or Synapse is convenient.
- Register a helper user and author everything through the plain client-server API, no SDK needed:

  ```sh
  HS=http://localhost:8008
  TOK=$(curl -s -X POST $HS/_matrix/client/v3/register \
    -d '{"username":"helper","password":"...","auth":{"type":"m.login.dummy"}}' | jq -r .access_token)
  ROOM=$(curl -s -X POST "$HS/_matrix/client/v3/createRoom?access_token=$TOK" \
    -d '{"name":"Test","preset":"public_chat"}' | jq -r .room_id)
  # PUT /rooms/$ROOM/send/m.room.message/<txnid> in a loop for history;
  # add "m.relates_to": {"rel_type": "m.thread", "event_id": ...} for thread replies.
  ```

- Point a test profile at that homeserver once (interactive sign-in), and it restores its session on every later headless launch. Have it join seeded rooms via `komai rooms join`.

This gives full control over event IDs, thread structure, and history depth, so a verification script can assert against exact targets.
