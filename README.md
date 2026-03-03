<p align="center">
	<img src="resources/komai.svg" alt="Komai logo" width="128" />
</p>
<h1 align="center">Komai (<a target="_blank" href="https://en.wiktionary.org/wiki/%E3%81%93%E3%81%BE%E3%81%84">こまい</a>)</h1>
<h2 align="center">A fine <a target="_blank" href="https://matrix.org/">Matrix</a> chat app you can get to love</h2>

🧑‍💻 **Komai** is a [Matrix](https://matrix.org/) chat application built with [C++](https://en.wikipedia.org/wiki/C%2B%2B) and [QML](https://en.wikipedia.org/wiki/QML). It began as a [usability](https://en.wikipedia.org/wiki/Usability)-focused [fork](https://en.wikipedia.org/wiki/Fork_(software_development)) of [nheko](https://nheko.im/nheko-reborn/nheko) (see [differences from nheko](docs/user-guide/differences-from-nheko.md)).

Komai was started by the [etke.cc](https://etke.cc/) team, but contributions by anyone are welcome! It's fully [Free Software](https://www.gnu.org/philosophy/free-sw.html) ([GPL-3.0-or-later](LICENSES/GPL-3.0-or-later.txt)), with no [CLA](https://en.wikipedia.org/wiki/Contributor_License_Agreement) and no contributor gatekeeping.

If you're curious about the origin of this project and its name, see the [🦁 Identity](docs/user-guide/identity.md) documentation page.

## 🎯 Design Philosophy

- 🖥️ **Desktop-first UX** — optimized for large screens
- 👓 **Readable and easy to use** — comfortably readable text, larger hit targets, and interaction patterns that respect [Fitts's law](https://www.nngroup.com/articles/fitts-law/) so actions are easier to hit and harder to miss
- 🎨 **Subtle but effective** — [themeable](docs/user-guide/themes.md), [customizable](docs/user-guide/settings/README.md), user-first, insanely fast
- 🧱 **Built on [nheko](https://nheko.im/nheko-reborn/nheko)** — inherits nheko's solid Matrix protocol support while improving the interface


## 🌟 Features

Everything [nheko](https://nheko.im/nheko-reborn/nheko) offers, plus [UX improvements & additional features on top](docs/user-guide/differences-from-nheko.md).

Highlights:

- 💬 [Matrix](https://matrix.org/) messaging with end-to-end encryption support (powered by [mtxclient](https://github.com/Nheko-Reborn/mtxclient) and [olm](https://gitlab.matrix.org/matrix-org/olm))
- 📞 (Legacy) Voice & video calls (no [Element Call](https://github.com/element-hq/element-call) support yet)
- 📎 File, image, audio & emoji messages (including custom stickers)
- 💬 Replies, [Discord](https://discord.com/)-style threads, and message forwarding
- 👥 Multi-account support via dedicated [application profiles](docs/user-guide/application-profiles.md)
- 🎨 14 [built-in themes](docs/user-guide/themes.md#-built-in-themes), but also [🗂️ user-themable](docs/user-guide/themes.md#️-user-themes)
- 🌐 30+ languages with inherited nheko translations and AI-assisted gap filling (see [Translations](docs/maintainers/translations.md))
- 🧠 [User Interface](https://en.wikipedia.org/wiki/User_interface) that both grandma and you can use, making neither of you feel stupid or incapable
- 🔧 Lots of [configuration settings](docs/user-guide/settings/README.md) - you're in control
- 📋 Good support for hundreds of rooms and spaces
- ⚡ Quick & lightweight native application ([C++](https://en.wikipedia.org/wiki/C%2B%2B) and [QML](https://en.wikipedia.org/wiki/QML)). No [Electron](https://www.electronjs.org/) here
- 🕊️ Fully [Free Software](https://www.gnu.org/philosophy/free-sw.html) ([GPL-3.0-or-later](LICENSES/GPL-3.0-or-later.txt)), with no [CLA](https://en.wikipedia.org/wiki/Contributor_License_Agreement) and no contributor gatekeeping

If you're curious about the full list of changes from [nheko](https://nheko.im/nheko-reborn/nheko), see 📄 [Differences from nheko](docs/user-guide/differences-from-nheko.md).


## 📸 Screenshots

<!-- TODO: add screenshots -->


## 🚀 Getting Started

### Building from source

```sh
git clone https://github.com/etkecc/komai && cd komai
just build
just run
```

For dependencies, distro-specific package lists, CMake flags, and more, see 📄 [Native build](docs/maintainers/packaging/native.md).
For contributor workflow and local checks, see 📄 [Development](docs/maintainers/development.md).


## 📦 Packaging

See 📄 [Native build](docs/maintainers/packaging/native.md) for building from source (`just build && just run`), including dependencies, distro packages, and CMake flags.

Pre-built packaging formats (alphabetical):

- [AppImage](docs/maintainers/packaging/appimage.md) -- `just appimage-build-docker` (portable single-file bundle)
- [Arch Linux](docs/maintainers/packaging/archlinux.md) -- PKGBUILD for `makepkg` / AUR
- [Flatpak](docs/maintainers/packaging/flatpak.md) -- `just flatpak-build && just flatpak-install`

See 📄 [Packaging](docs/maintainers/packaging/README.md) for an overview of all methods.


## 📚 Documentation

See 📄 [Documentation](docs/README.md) for the full list of guides, including settings, theming, translations, and packaging.


## 🤝 Contributing

See [Development](docs/maintainers/development.md).


## 🆘 Support

- 💬 Matrix room: [#komai:etke.cc](https://matrix.to/#/#komai:etke.cc)
- 🐛 GitHub issues: [etkecc/komai/issues](https://github.com/etkecc/komai/issues)


## 🙏 Acknowledgements

Komai is built on top of [nheko](https://nheko.im/nheko-reborn/nheko) by the Nheko-Reborn team. We're grateful for their work on the Matrix protocol implementation and the Qt/QML client foundation.

- [Fluent Icons](https://github.com/microsoft/fluentui-system-icons)
- [Tinted Theming (Base16)](https://github.com/tinted-theming)
