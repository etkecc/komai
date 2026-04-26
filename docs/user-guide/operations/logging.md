# Logging

Komai writes all log output to **stderr**. It does not create or manage log files itself -- that responsibility belongs to the system running the application.

## Where logs go

| Installation method | Where logs end up | How to view |
| --- | --- | --- |
| Native (`.desktop` entry on systemd) | systemd journal | `journalctl --user -t komai` |
| Snap | systemd journal via snap | `snap logs komai` or `journalctl --user` |
| Flatpak | systemd journal via flatpak | `flatpak logs cc.etke.komai` or `journalctl --user` |
| Terminal launch | Printed directly to the terminal | Visible in real time |

## Collecting logs for a bug report

The easiest way to get clean, relevant logs is to launch Komai from a terminal:

```sh
# Native or local build
komai -p default 2>&1 | tee /tmp/komai-log.txt

# Snap
snap run komai -p default 2>&1 | tee /tmp/komai-log.txt

# Flatpak
flatpak run cc.etke.komai -p default 2>&1 | tee /tmp/komai-log.txt
```

Then reproduce the issue, close Komai, and include the relevant portion of `/tmp/komai-log.txt` in your bug report. This gives focused logs tied to the exact session where the problem occurred.

## Adjusting log verbosity

Use the `--log-level` (`-l`) flag to control which messages are shown:

```sh
# Only warnings and errors
komai -l warn

# Default level (info) with extra detail for a specific target
komai -l info,net=debug

# Maximum verbosity
komai --debug
```

The level string is a comma-separated list of `target=level` pairs compatible with the Rust [EnvFilter](https://docs.rs/tracing-subscriber/latest/tracing_subscriber/filter/struct.EnvFilter.html) syntax. Available targets include `ui`, `db`, `net`, `crypto`, `qml`, and `rust`. The `RUST_LOG` environment variable is used as a fallback when `--log-level` is not set.

## Disabling log output

```sh
komai --log-type none
```
