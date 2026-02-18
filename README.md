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

You'll need the same dependencies as nheko (see [Build Requirements](#-build-requirements) below), plus:
- [just](https://github.com/casey/just) command runner
- Python 3 (for theme generation)

```sh
git clone https://github.com/etkecc/komai && cd komai

# See all available commands
just

# Build (configures CMake automatically on first run)
just build

# Run
just run
```

### Quick reference

| Command | What it does |
|---------|-------------|
| `just build` | Configure (if needed) + build |
| `just rebuild` | Clean configure + build |
| `just run` | Build if needed, then run |
| `just clean` | Remove build directory |
| `just configure-debug` | Configure a debug build |
| `just import-theme <slug>` | Import a [Base16 theme](https://github.com/tinted-theming/schemes) |
| `just generate-themes` | Regenerate theme header from YAML files |
| `just lint` | Run the code formatter |


## 🎨 Themes

Komai ships with many built-in themes and makes it easy to add more. See 📄 [Themes](docs/themes.md) for details.


## 🌐 Translations

Komai inherits human-made translations from nheko and fills in the gaps with AI-assisted translation (Claude CLI) to ensure complete coverage across 30+ languages. See 📄 [Translations](docs/translations.md) for details.


## 🔧 Build Requirements

- **Qt6** (6.5 or greater)
- **CMake** 3.15 or greater
- **Python 3** (for theme generation at build time)
- **C++20 compiler**: GCC 11.3+, Clang 16+, or MSVC 19.13+
- [mtxclient](https://github.com/Nheko-Reborn/mtxclient)
- [coeurl](https://nheko.im/Nheko-Reborn/coeurl)
- [LMDB](https://www.symas.com/lmdb) + [lmdb++](https://github.com/hoytech/lmdbxx)
- [cmark](https://github.com/commonmark/cmark) 0.29+
- [libolm](https://gitlab.matrix.org/matrix-org/olm)
- [spdlog](https://github.com/gabime/spdlog) + [fmt](https://github.com/fmtlib/fmt)
- [qtkeychain](https://github.com/frankosterfeld/qtkeychain) 0.12+
- [KDSingleApplication](https://github.com/KDAB/KDSingleApplication) 1.0+
- [GStreamer](https://gitlab.freedesktop.org/gstreamer) 1.20+ (optional, for VoIP — pass `-DVOIP=OFF` to disable)
- XCB, XCB-EWMH (optional, for X11 screensharing — pass `-DSCREENSHARE_X11=OFF` to disable)

Most dependencies can be bundled automatically by passing `-DHUNTER_ENABLED=ON -DBUILD_SHARED_LIBS=OFF` or individual `-DUSE_BUNDLED_*` flags to CMake.

<details>
<summary>📦 Arch Linux packages</summary>

```bash
sudo pacman -S qt6-base qt6-tools qt6-multimedia qt6-svg cmake gcc \
    fontconfig lmdb lmdbxx cmark qtkeychain-qt6
```
</details>

<details>
<summary>📦 Debian 13+ / Ubuntu 24.04+</summary>

```bash
sudo apt install -y cmake libevent-dev libspdlog-dev libre2-dev \
    liblmdb++-dev libcurl4-openssl-dev libssl-dev libolm-dev libcmark-dev \
    nlohmann-json3-dev libkdsingleapplication-qt6-dev \
    qt6-base-dev qt6-tools-dev qt6-svg-dev qt6-multimedia-dev \
    qt6-declarative-dev qtkeychain-qt6-dev qt6-base-private-dev \
    qt6-declarative-private-dev

git clone https://github.com/etkecc/komai && cd komai
just build  # or: cmake -S. -Bvar/build/native -DCMAKE_BUILD_TYPE=Release -DUSE_BUNDLED_COEURL=1 -DUSE_BUNDLED_MTXCLIENT=1 -DUSE_BUNDLED_LMDBXX=1 && cmake --build var/build/native
```
</details>


## 🤝 Contributing

See [CONTRIBUTING](.github/CONTRIBUTING.md).


## 🆘 Support

- 💬 Matrix room: [#komai:etke.cc](https://matrix.to/#/#komai:etke.cc)
- 🐛 GitHub issues: [etkecc/komai/issues](https://github.com/etkecc/komai/issues)


## 🙏 Acknowledgements

Komai is built on top of [nheko](https://nheko.im/nheko-reborn/nheko) by the Nheko-Reborn team. We're grateful for their work on the Matrix protocol implementation and the Qt/QML client foundation.

- [Fluent Icons](https://github.com/microsoft/fluentui-system-icons)
- [Base16 themes](https://github.com/tinted-theming/home)
