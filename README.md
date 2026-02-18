<p align="center">
	<img src="resources/komai.svg" alt="Komai logo" width="128" />
	<h1 align="center">Komai (細い)</h1>
</p>

🧑‍💻 **Komai** is a usability-focused fork of [nheko](https://nheko.im/nheko-reborn/nheko), a desktop [Matrix](https://matrix.org/) client built with Qt/C++/QML, by [etke.cc](https://etke.cc/).

The name *Komai* ([細い](https://en.wiktionary.org/wiki/%E3%81%93%E3%81%BE%E3%81%84), "fine/slender" in Japanese) carries several layers: "ko" evokes small/fine (小), "m" nods to **M**atrix, and "ai" (愛) means love — but also a nod to **AI**, since this client is largely vibe-coded. A small Matrix client you can possibly get to love.

The name also evokes [Komainu](https://en.wikipedia.org/wiki/Komainu) (狛犬), the mythical lion-dog guardians of Shinto shrines. If *nheko* nods to *neko* (猫, "cat"), then *Komai* answers with the *inu* (犬, "dog") of komainu — yet the komainu is also part lion, which is itself a cat. A fork that is both a playful contrast and a quiet kinship with its upstream.


## 🎯 Design Philosophy

- 🖥️ **Desktop-first UX** — optimized for large screens where the room list is always visible
- 👓 **Readable by everyone** — all visible text must be comfortably readable at default settings, including by elderly users
- 🎨 **Subtle but effective** — visual changes are noticeable without being jarring
- 🧱 **Built on nheko** — inherits nheko's solid Matrix protocol support while improving the interface


## 🌟 Features

Everything nheko offers, plus [UX improvements on top](docs/differences-from-nheko.md):

- 🔐 End-to-end encryption
- 📞 VoIP calls (voice & video)
- 💬 Typing notifications, read receipts, presence
- 📎 File, image, audio & emoji messages (including custom stickers)
- 💬 Replies, forwards, and message reactions
- 🔍 Room switcher (`Ctrl-K`)
- 🎨 14 built-in themes (Komai, Nord, Catppuccin, Dracula, Solarized, and more)
- 🫧 Polished bubble-style messages with per-sender colors
- 🖱️ Click-to-toggle message actions (replaces finicky hover-only bar)
- 👤 Avatars on bubble side with configurable sender name display
- 📐 Compact room list, polished sidebar, and reworked room bar
- ⚡ Performance optimizations (virtual timeline window, faster room switching)

For the full list of changes, see 📄 [Differences from nheko](docs/differences-from-nheko.md).


## 📸 Screenshots

<!-- TODO: add screenshots -->


## 🚀 Getting Started

### Building from source

```sh
git clone https://github.com/etkecc/komai && cd komai
just build
just run
```

For dependencies, distro-specific package lists, CMake flags, and more, see 📄 [Native build](docs/packaging/native.md).


## 🎨 Themes

Komai ships with many built-in themes and makes it easy to add more. See 📄 [Themes](docs/themes.md) for details.


## 🌐 Translations

Komai inherits human-made translations from nheko and fills in the gaps with AI-assisted translation (Claude CLI) to ensure complete coverage across 30+ languages. See 📄 [Translations](docs/translations.md) for details.


## 📦 Packaging

See 📄 [Native build](docs/packaging/native.md) for building from source (`just build && just run`), including dependencies, distro packages, and CMake flags.

Pre-built packaging formats (alphabetical):

- [AppImage](docs/packaging/appimage.md) -- `just appimage-build-docker` (portable single-file bundle)
- [Arch Linux](docs/packaging/archlinux.md) -- PKGBUILD for `makepkg` / AUR
- [Flatpak](docs/packaging/flatpak.md) -- `just flatpak-build && just flatpak-install`

See 📄 [Packaging](docs/packaging/README.md) for an overview of all methods.


## 📚 Documentation

See 📄 [Documentation](docs/README.md) for the full list of guides, including configuration, theming, translations, and packaging.


## 🤝 Contributing

See [CONTRIBUTING](.github/CONTRIBUTING.md).


## 🆘 Support

- 💬 Matrix room: [#komai:etke.cc](https://matrix.to/#/#komai:etke.cc)
- 🐛 GitHub issues: [etkecc/komai/issues](https://github.com/etkecc/komai/issues)


## 🙏 Acknowledgements

Komai is built on top of [nheko](https://nheko.im/nheko-reborn/nheko) by the Nheko-Reborn team. We're grateful for their work on the Matrix protocol implementation and the Qt/QML client foundation.

- [Fluent Icons](https://github.com/microsoft/fluentui-system-icons)
- [Base16 themes](https://github.com/tinted-theming/home)
