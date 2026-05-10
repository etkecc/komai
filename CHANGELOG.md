# Changelog

## 2026.05.10.0

🎉 **The first pre-built Windows binary of Komai!** Every tagged release now ships a Windows no-installer ZIP (`komai-<version>-windows-x64-no-installer.zip`) alongside the AppImage, Flatpak, and Snap. The build is unsigned, so SmartScreen warns on first launch ("More info" → "Run anyway") ([62ad50480](https://github.com/etkecc/komai/commit/62ad50480), [d644ad8fc](https://github.com/etkecc/komai/commit/d644ad8fc)).

- ✨ Feature: spaces are now first-class. A "Space" badge appears in the room header, and the composer is hidden in space rooms (since space messages don't surface on other clients) ([d80eedad6](https://github.com/etkecc/komai/commit/d80eedad6), [e28820c9b](https://github.com/etkecc/komai/commit/e28820c9b)).
- ✨ Feature: room-level "Mark as unread" from the [room list](https://github.com/etkecc/komai/blob/b3cec3d0a88ee10e317592863834f0011f9cf38d/docs/user-guide/features/room-list.md#mark-as-unread) and the [tab context menu](https://github.com/etkecc/komai/blob/b3cec3d0a88ee10e317592863834f0011f9cf38d/docs/user-guide/features/tabs.md#-mark-as-read-or-unread); the unread state syncs across devices and to other Matrix clients ([920b2f29f](https://github.com/etkecc/komai/commit/920b2f29f), [9036b1fd5](https://github.com/etkecc/komai/commit/9036b1fd5), [cc67e76b5](https://github.com/etkecc/komai/commit/cc67e76b5)).
- ✨ Feature: per-room override for "Show others when I'm typing" and "Show others when I've read their messages" in Room Info → Preferences ([a385946fa](https://github.com/etkecc/komai/commit/a385946fa), [708a65566](https://github.com/etkecc/komai/commit/708a65566)).
- 🐛 Fix: the "Show others when I've read their messages" toggle now actually has effect; it had been ignored before, so receipts went out regardless ([e7a815081](https://github.com/etkecc/komai/commit/e7a815081)).
- 🐛 Fix: child-space references and other previously-unrouted state events render properly in the timeline, instead of falling through to a generic "Unsupported" placeholder ([c084371fb](https://github.com/etkecc/komai/commit/c084371fb), [cba45c300](https://github.com/etkecc/komai/commit/cba45c300)).
- 🐛 Fix: Sign In gives up quickly when the homeserver is unreachable, instead of silently retrying for up to 15 minutes ([871086e6f](https://github.com/etkecc/komai/commit/871086e6f)).
- 🐛 Fix: Escape returns focus to the composer when focus is on highlighted message text or on the room-list / communities sidebar; Tab and Shift+Tab from a highlighted message do the same ([4173428fa](https://github.com/etkecc/komai/commit/4173428fa), [a5ead0faf](https://github.com/etkecc/komai/commit/a5ead0faf)).
- 🔧 Build: Komai no longer depends on OpenSSL, thanks to matrix-sdk 0.17.0. This simplifies packaging by removing a dependency ([5bbcbcf1c](https://github.com/etkecc/komai/commit/5bbcbcf1c), [7caacac4e](https://github.com/etkecc/komai/commit/7caacac4e), [be76bf2c1](https://github.com/etkecc/komai/commit/be76bf2c1), [e035c7366](https://github.com/etkecc/komai/commit/e035c7366)).
- 🔧 Build: `pkg-config` is no longer required when VOIP and X11 are both disabled (typical Windows and macOS builds) ([e5778ed20](https://github.com/etkecc/komai/commit/e5778ed20)).
- 📦 Flatpak: the bundle filename now carries version + arch (`komai-<version>-<arch>.flatpak`), matching the AppImage / Snap / Windows ZIP naming ([c7fea0559](https://github.com/etkecc/komai/commit/c7fea0559)).

## 2026.05.09.0

- ✨ Feature: Shift+Click range selection in the timeline, extending the existing Ctrl+Click multi-select ([4d9e9ced4](https://github.com/etkecc/komai/commit/4d9e9ced4)).
- 🐛 Fix: redacted messages no longer offer React, Reply, Pin, or Delete actions; inspection actions (Copy permalink, View raw, Read receipts, Report) remain available ([4a56ec5d9](https://github.com/etkecc/komai/commit/4a56ec5d9)).
- 🐛 Fix: Forwarding from the media viewer overlay works again; previously, confirming a target room silently failed ([00f73aa81](https://github.com/etkecc/komai/commit/00f73aa81)).
- 🐛 Fix: the Forward dialog no longer jumps vertically when switching to confirm mode ([690feb75b](https://github.com/etkecc/komai/commit/690feb75b)).
- 🐛 Fix: mention pills now show the default avatar (Bauhaus, LetterInitial, etc.) when there's no avatar or while the avatar is still loading, matching the timeline ([8800bd9cc](https://github.com/etkecc/komai/commit/8800bd9cc), [e3d645ef5](https://github.com/etkecc/komai/commit/e3d645ef5)).
- 🐛 Fix: mention pills refresh in-place when avatar settings change, instead of waiting for the timeline to reload ([154ec98f0](https://github.com/etkecc/komai/commit/154ec98f0)).
- 🐛 Fix: the `@room` row in the mention picker now uses the actual room avatar instead of the generic fallback ([c20adde65](https://github.com/etkecc/komai/commit/c20adde65)).
- 🐛 Fix: media fetches are now capped by a timeout, so a single broken or hung URL no longer starves the media pipeline ([64a632e41](https://github.com/etkecc/komai/commit/64a632e41), [e3d645ef5](https://github.com/etkecc/komai/commit/e3d645ef5)).

## 2026.05.08.0

- 🔧 Build: Komai no longer depends on the `Qt5Compat.GraphicalEffects` QML module (or the `qt6-5compat` shared library it pulls in); the rounded-corner masking sites have been ported to `QtQuick.Effects.MultiEffect`. Distro packagers can drop `qml6-module-qt5compat-graphicaleffects` (and the equivalent on other distros) from their dependency lists ([992f2e651](https://github.com/etkecc/komai/commit/992f2e651)).
- 🔧 Build: `matrix-sdk` is now sourced from the crates.io 0.17.0 release instead of a git-pinned `main`-branch snapshot taken on top of 0.16.0 ([c27bdef12](https://github.com/etkecc/komai/commit/c27bdef12)).

## 2026.05.07.0

- 🐛 Fix: drag-and-drop into the message composer now works under Flatpak ([594dc71e6](https://github.com/etkecc/komai/commit/594dc71e6)).
- 🐛 Fix: "Open" and "Show in folder" on saved attachments work correctly under Flatpak; "Show in folder" now opens the user's actual save location instead of the doc-portal mount ([024bcfa88](https://github.com/etkecc/komai/commit/024bcfa88), [82eb3c6e4](https://github.com/etkecc/komai/commit/82eb3c6e4)).
- 🐛 Fix: the Copy action on the media overlay works again; for images and stickers, the paste now lands as a decoded image so apps like GIMP accept it ([56fd28a2e](https://github.com/etkecc/komai/commit/56fd28a2e)).
- 🐛 Fix: typing indicators no longer hang forever when a server's "stopped typing" event is dropped; stale entries are now pruned after 15s ([665935516](https://github.com/etkecc/komai/commit/665935516), [de7771c42](https://github.com/etkecc/komai/commit/de7771c42)).
- ✨ Feature: the custom emoji picker (`~`) now shows a friendly empty-state message instead of a blank popup when no matches are found or no custom emojis are defined; its header is retitled "Pick a custom emoji or sticker" so it is visibly distinct from the regular `:` emoji picker ([16e524967](https://github.com/etkecc/komai/commit/16e524967), [ed01bf662](https://github.com/etkecc/komai/commit/ed01bf662)).
- 🐛 Fix: on the dark-nheko theme, setting cards were invisible against the Settings page backdrop until hovered; the layered-surface look is restored ([f8964c496](https://github.com/etkecc/komai/commit/f8964c496)). Automation is now in place to ensure future built-in themes won't suffer from the same issue ([5c7977512](https://github.com/etkecc/komai/commit/5c7977512)).
- 🔒 Security: hickory DNS bumped to 0.26.1, picking up fixes for GHSA-mxqq-qxc6-39c2 (high) and GHSA-c4g7-w9pp-7r2x (medium) ([e65f4efe9](https://github.com/etkecc/komai/commit/e65f4efe9)).
- 📦 AUR: the PKGBUILD now builds inside sandboxed `makepkg` runners (notably `rua`) when `rust` is satisfied by `rustup` ([10338c585](https://github.com/etkecc/komai/commit/10338c585)).
- 🔧 Build: distro builds on Qt 6.5+ are no longer blocked by an accidental Qt 6.10 dependency in the settings search proxy ([a76f99e7c](https://github.com/etkecc/komai/commit/a76f99e7c)).
- 🔧 Build: early progress on a Windows port: the project now compiles to a `komai.exe`. The binary is not yet runnable (runtime DLL/plugin staging still pending) and more work is needed before Windows becomes a supported target.

## 2026.05.06.3

- 🐛 Fix: own-bubble (right-aligned) reactions arriving together no longer stack one-per-line; pills lay out side-by-side again as long as the bubble has horizontal room ([9b4d2e4e3](https://github.com/etkecc/komai/commit/9b4d2e4e3)).
- 📦 AUR: the PKGBUILD now builds against the distro Rust toolchain instead of upstream's rustup channel pin. Fixes `Could not find toolchain '1.95.0-x86_64-unknown-linux-gnu'` for AUR users whose default rustup toolchain is something else, and avoids a ~250 MB rustup auto-install side-effect during the build ([adbbc560f](https://github.com/etkecc/komai/commit/adbbc560f)).

## 2026.05.06.2

- 🐛 Fix: attribution footer stacks vertically on narrow widths so the Sponsor / Report-an-issue buttons don't crowd the attribution text ([226ceea61](https://github.com/etkecc/komai/commit/226ceea61)).
- ✨ Feature: when an attachment is added in the message composer, the caption field for the newest attachment auto-focuses ([fc33ed914](https://github.com/etkecc/komai/commit/fc33ed914)).
- 🔧 Build: distro packagers can build against the system `qtkeychain`, `KDSingleApplication`, and `litehtml` via `-DCPM_USE_LOCAL_PACKAGES=ON` ([2ac8559db](https://github.com/etkecc/komai/commit/2ac8559db)).
- 🔧 Build: distro-packaged binaries (AUR, Flatpak, AppImage, Snap) shrink by ~26 MiB; `-rdynamic` is now gated to `CMAKE_BUILD_TYPE=Debug` only, so optimized release builds no longer export every static-archive symbol into `.dynsym` ([08d733e92](https://github.com/etkecc/komai/commit/08d733e92)).
- 📦 AUR: `package()` is one `cmake --install` line; no post-install scrub needed ([0e5be7318](https://github.com/etkecc/komai/commit/0e5be7318)).

## 2026.05.06.1

🎉 **The first public release of Komai!**

Komai is a desktop-first [Matrix](https://matrix.org/) chat application built with Rust, C++, and QML. It's a usability-focused descendant of [nheko](https://nheko.im/nheko-reborn/nheko), now running on the Rust [matrix-sdk](https://github.com/matrix-org/matrix-rust-sdk) and a growing Rust core.

- 📖 [README](https://github.com/etkecc/komai#readme): what Komai is and what it does
- 📝 [Introducing Komai](https://etke.cc/blog/introducing-komai/): the [etke.cc](https://etke.cc/) team's announcement post
- 📚 [User Guide](https://github.com/etkecc/komai/blob/main/docs/user-guide/README.md) · 🔀 [Differences from nheko](https://github.com/etkecc/komai/blob/main/docs/user-guide/differences-from-nheko.md)
- 📥 [Installation](https://github.com/etkecc/komai/blob/main/docs/user-guide/installation.md): AppImage, Flatpak, Snap, AUR (Linux x86_64)
- 💬 [#komai:etke.cc](https://matrix.to/#/#komai:etke.cc) · 🐛 [Issues](https://github.com/etkecc/komai/issues)
