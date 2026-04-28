<p align="center">
	<img src="resources/komai.svg" alt="Komai logo" width="128" />
</p>
<h1 align="center">Komai (<a target="_blank" href="https://en.wiktionary.org/wiki/%E3%81%93%E3%81%BE%E3%81%84">こまい</a>)</h1>
<h2 align="center">A fine <a target="_blank" href="https://matrix.org/">Matrix</a> chat app you can get to love</h2>

🦁 **Komai** is a desktop-first [Matrix](https://matrix.org/) chat application built with [Rust](https://en.wikipedia.org/wiki/Rust_(programming_language)), [C++](https://en.wikipedia.org/wiki/C%2B%2B) and [QML](https://en.wikipedia.org/wiki/QML). It traces its origins to a [usability](https://en.wikipedia.org/wiki/Usability)-focused [fork](https://en.wikipedia.org/wiki/Fork_(software_development)) of [nheko](https://nheko.im/nheko-reborn/nheko), rebuilt around the Rust [matrix-sdk](https://github.com/matrix-org/matrix-rust-sdk) runtime with a growing Rust core.

**🤖 Komai is [built with AI](docs/user-guide/ai.md).** Professional [engineers](https://etke.cc/about/) + AI coding agents ([Claude Code](https://docs.anthropic.com/en/docs/claude-code/overview), [Codex](https://openai.com/index/introducing-codex/)) working together to build a complex native application in a language stack that isn't the team's primary expertise. We think AI in capable hands can deliver above-average results.

Komai was created by the [etke.cc](https://etke.cc/) team, but contributions by anyone are welcome! It's fully [Free Software](https://www.gnu.org/philosophy/free-sw.html) ([GPL-3.0-or-later](LICENSES/GPL-3.0-or-later.txt)), with no [CLA](https://en.wikipedia.org/wiki/Contributor_License_Agreement) and no contributor gatekeeping.

If you're curious about the origin of this project and its name, see the [🦁 Identity](docs/user-guide/identity.md) documentation page.

## 🎯 Design Philosophy

- 🖥️ **Desktop-first UX** — optimized for large screens
- 👓 **Readable and easy to use** — comfortably readable text, larger hit targets, and interaction patterns that respect [Fitts's law](https://www.nngroup.com/articles/fitts-law/)
- 🎨 **Yours to shape** — [themeable](docs/user-guide/features/themes.md), [customizable](docs/user-guide/settings/README.md), and [config-management friendly](docs/user-guide/settings/README.md#configuration-management) via plain-YAML files
- ⚡ **Responsive by design** — native performance is a design constraint
- 🎓 **Educate, don't over-abstract away** — like [Arch Linux](https://wiki.archlinux.org/title/Arch_Linux), we prefer exposing Matrix's real concepts over hiding them
- 🧠 **For both grandma and power users** — neither dumbed down nor buried in complexity


## 🌟 Features

- 💬 [Matrix](https://matrix.org/) messaging with end-to-end encryption support (powered by the Rust [matrix-sdk](https://github.com/matrix-org/matrix-rust-sdk) runtime)
- 📞 Legacy Voice & video calls (no [Element Call](https://github.com/element-hq/element-call) support yet)
- 📎 File, image, audio & emoji messages (with custom emojis), with a [built-in media viewer](docs/user-guide/features/media-playback.md) featuring gallery navigation and in-app video playback
- 😀 Richer emoji discovery via localized [Unicode CLDR](https://cldr.unicode.org/) keywords (for example, `:whiskey` finds 🥃) -- see [Emoji Search and Picker](docs/user-guide/features/emojis.md)
- 💬 Replies, [Discord](https://discord.com/)-style [threads](docs/user-guide/features/threads.md), and message forwarding
- 👥 Multi-account support via dedicated [application profiles](docs/user-guide/features/application-profiles.md)
- 🎨 10+ [built-in themes](docs/user-guide/features/themes.md#-built-in-themes), maintained to meet [WCAG AA contrast](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html) for common UI text pairings. Also [🗂️ user-themable](docs/user-guide/features/themes.md#️-user-themes)
- 🌐 30+ languages with inherited nheko translations and AI-assisted gap filling (see [Translations](docs/maintainers/translations.md))
- 🔧 Lots of [configuration settings](docs/user-guide/settings/README.md)
- 🧭 Polished [Room Directory](docs/user-guide/features/room-directory.md) with first-class [Matrix Rooms Search](https://github.com/etkecc/mrs) support (enabled by default via [matrixrooms.info](https://matrixrooms.info/?utm_source=komai&utm_medium=docs&utm_campaign=readme) by [etke.cc](https://etke.cc/?utm_source=komai&utm_medium=docs&utm_campaign=readme)), language filtering, and room size filtering
- 📋 Good support for hundreds of rooms and spaces
- 📑 Browser-style [room tabs](docs/user-guide/features/tabs.md) for juggling multiple conversations at once, with pinning and more (a power-user feature still rare among Matrix clients)
- ⌨️ [Keyboard-driven main chat workflow](docs/user-guide/features/keyboard-shortcuts.md), with human and [Vim](https://en.wikipedia.org/wiki/Vim_(text_editor))-style shortcuts
- 🔀 [Selection mode](docs/user-guide/features/keyboard-shortcuts.md#selection-mode) for bulk actions (forwarding, moderation)
- ⚡ Quick & relatively lightweight native application ([Rust](https://en.wikipedia.org/wiki/Rust_(programming_language)), [C++](https://en.wikipedia.org/wiki/C%2B%2B) and [QML](https://en.wikipedia.org/wiki/QML)). No [Electron](https://www.electronjs.org/) here
- 🤖 Human- and agent-ready local automation via [Model Context Protocol (MCP)](docs/user-guide/features/automations/mcp.md), [CLI commands](docs/user-guide/features/automations/cli.md), and the [D-Bus API](docs/user-guide/features/automations/dbus.md)
- 🕊️ Fully [Free Software](https://www.gnu.org/philosophy/free-sw.html) ([GPL-3.0-or-later](LICENSES/GPL-3.0-or-later.txt)), with no [CLA](https://en.wikipedia.org/wiki/Contributor_License_Agreement) and no contributor gatekeeping

Curious where Komai came from and what changed along the way? See 📄 [Differences from nheko](docs/user-guide/differences-from-nheko.md).


## 📸 Screenshots

![Main view](docs/user-guide/screenshots/main-view.webp)

A few more screenshots:

- [🖼️ Welcome page](docs/user-guide/screenshots/welcome.webp)
- [🖼️ Sign in](docs/user-guide/screenshots/sign-in.webp)
- [🖼️ Register](docs/user-guide/screenshots/registration.webp)
- [🖼️ Settings](docs/user-guide/screenshots/settings.webp)
- [🖼️ Dark Matrix theme](docs/user-guide/screenshots/themes-dark-matrix.webp)

More screenshots are inlined on individual feature pages — see the [👤 User Guide](docs/user-guide/README.md).


## 📥 Installation

Komai ships as **AppImage**, **Flatpak**, and **Snap** packages on the [GitHub Releases](https://github.com/etkecc/komai/releases) page, plus a [`komai`](https://aur.archlinux.org/packages/komai) package on the Arch Linux AUR.

Komai is **Linux-only** for now (x86_64). There are no official Windows or macOS builds, and building from source on those platforms has never been tested by the maintainers. If you'd like to try, see 📄 [Native build — Windows and macOS notes](docs/maintainers/packaging/native.md#-windows-and-macos-untested) for tentative pointers.

See 📄 [Installation](docs/user-guide/installation.md) for download links and install commands. To build Komai yourself, see 📄 [Native build](docs/maintainers/packaging/native.md).


## 📚 Documentation

See 📄 [Documentation](docs/README.md) for the full list of guides, including settings, theming, translations, and packaging.


## 🤝 Contributing

See [Development](docs/maintainers/development.md).


## 🆘 Support

- 💬 Matrix room: [#komai:etke.cc](https://matrix.to/#/#komai:etke.cc)
- 🐛 GitHub issues: [etkecc/komai/issues](https://github.com/etkecc/komai/issues)


## 🙏 Acknowledgements

Komai started as a fork of [nheko](https://nheko.im/nheko-reborn/nheko) by the Nheko-Reborn team. We're grateful for the original application and the Qt/QML groundwork that made Komai possible.

- [Boring Avatars](https://github.com/boringdesigners/boring-avatars) — default avatar generation algorithms (Beam, Marble, Bauhaus styles), ported from TypeScript to C++
- [Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons) — primary icon set (MIT)
- [Font Awesome Free](https://github.com/FortAwesome/Font-Awesome) — supplementary icons and brand logos (CC BY 4.0)
- [Tinted Theming (Base16)](https://github.com/tinted-theming)
