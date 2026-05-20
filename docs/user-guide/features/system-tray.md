# 📌 System Tray

Keep Komai running in the background so [🔔 notifications](notifications.md) still arrive after you close the window, and start it hidden from session-autostart entries.


## 🎛️ Two settings, two scopes

Both live under **Settings → Desktop → System tray**:

- **Close to tray**. Closing the window hides it to the tray instead of quitting. Click the tray icon to bring it back, right-click for a quit action. The foundation for everything else on this page.
- **Start in tray**. Komai starts hidden in the tray on *every* launch, including launcher-icon clicks. Persistent and global. Requires **Close to tray**.

If you want **only** your session-autostart launches to be silent while a launcher click still opens the window, leave **Start in tray** off and use the CLI flag below instead.


## 🚀 `--start-in-tray` CLI flag

The `--start-in-tray` flag hides the Komai window for *this* launch only, without touching the **Start in tray** setting. Designed for autostart entries:

```bash
komai --start-in-tray
```

A launcher-icon click (no flag) still opens the window the usual way.

### Requirements

Komai prints an error to stderr and exits instead of starting hidden when:

- **Close to tray** is disabled — no tray icon means the hidden window is unreachable.
- The desktop session has no working system tray.

### Multi-profile setups

Launching Komai without `-p` opens the [👥 application profile](application-profiles.md) switcher, which can't run hidden. Combining the two is rejected with an error. If you have multiple profiles, you can avoid this by passing `-p <profile>` explicitly, which skips the switcher:

```bash
komai -p work --start-in-tray
```

### Example autostart entry

Drop the following into `~/.config/autostart/komai.desktop` (add `-p <profile>` to `Exec=` for non-default profiles):

```ini
[Desktop Entry]
Type=Application
Name=Komai (autostart)
Exec=/absolute/path/to/komai --start-in-tray
Terminal=false
X-GNOME-Autostart-enabled=true
```

> 💡 Not the same as `komai profiles launcher create <profile>`. That one writes a per-profile **app menu launcher** to `~/.local/share/applications/` for reliable taskbar badges — see [👥 Application Profiles](application-profiles.md#reliable-app-badges-with-multiple-profiles). Autostart entries live in `~/.config/autostart/` and need to be written by hand.


## Related

- [🔔 Notifications](notifications.md) — what arrives while Komai sits in the tray
- [👥 Application Profiles](application-profiles.md) — `-p` and per-profile launchers
- [⚙️ Settings](../settings/README.md)
