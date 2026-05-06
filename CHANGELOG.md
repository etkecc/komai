# Changelog

## 2026.05.06.3

- ✨ UI: own-bubble (right-aligned) reactions arriving in the same model update no longer stack one-per-line; pills lay out side-by-side again as long as the bubble has horizontal room ([9b4d2e4e3](https://github.com/etkecc/komai/commit/9b4d2e4e3)).
- 📦 AUR: the PKGBUILD now builds against the distro Rust toolchain instead of upstream's rustup channel pin. Fixes `Could not find toolchain '1.95.0-x86_64-unknown-linux-gnu'` for AUR users whose default rustup toolchain is something else, and avoids a ~250 MB rustup auto-install side-effect during the build ([adbbc560f](https://github.com/etkecc/komai/commit/adbbc560f)).

## 2026.05.06.2

- ✨ UI: attribution footer stacks vertically on narrow widths so the Sponsor / Report-an-issue buttons don't crowd the attribution text ([226ceea61](https://github.com/etkecc/komai/commit/226ceea61)).
- ✨ UI: when an attachment is added in the message composer, the caption field for the newest attachment auto-focuses ([fc33ed914](https://github.com/etkecc/komai/commit/fc33ed914)).
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
