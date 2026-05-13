# 🔤 Spell checking

Komai underlines misspelled words as you type (in the message composer and the room topic editor) and offers corrections when you right-click a flagged word.

It is offline and uses the standard [Hunspell](https://hunspell.github.io/) dictionaries (the same `.dic`/`.aff` files LibreOffice, Firefox, and most Linux desktops use).

A copy of the **English (US)** dictionary is built in, so spell checking works out of the box on every platform without additional setup.

See a screenshot of [🖼️Spellcheck in action in the Composer textarea](../screenshots/composer-spellcheck.webp).

## Contents

- [How it works](#how-it-works)
- [Choosing languages](#choosing-languages)
- [Installing more dictionaries](#installing-more-dictionaries)
  - [Linux (native package)](#linux-native-package)
  - [Flatpak](#flatpak)
  - [Snap](#snap)
  - [macOS](#macos)
  - [Windows](#windows)
  - [Manual (any platform)](#manual-any-platform)
- [Corrections and your personal dictionary](#corrections-and-your-personal-dictionary)
- [What is skipped](#what-is-skipped)
- [Where things are stored](#where-things-are-stored)
- [CJK languages](#cjk-languages)

## How it works

A word is flagged only when *none* of the enabled dictionaries recognizes it. That means you can enable several languages at once (say English and Bulgarian) and mixing them in a single message produces no false positives. Right-clicking a flagged word offers spelling suggestions, plus the option to add the word to your personal dictionary.

Komai discovers dictionaries from three places, and uses whichever it finds:

1. the **built-in** English (US) dictionary (always present);
2. a per-user `hunspell/` folder in Komai's data directory (see [Manual](#manual-any-platform));
3. the **system** Hunspell locations — `/usr/share/hunspell`, `/usr/share/myspell/dicts`, `$XDG_DATA_DIRS/hunspell` on Linux; `~/Library/Spelling` on macOS — which also picks up dictionaries already installed for LibreOffice or other apps.

## Choosing languages

Open `Settings → Composer → Spellcheck`:

- **Enable spell checking** turns the whole feature on or off.
- Below it, tick the languages you write in. Only the dictionaries Komai could find appear here — to add more, see the next section.

On first run Komai enables the dictionary that matches your system language, falling back to English (US).

## Installing more dictionaries

A "dictionary" is a pair of files, `<locale>.dic` and `<locale>.aff` (for example `de_DE.dic` + `de_DE.aff`). Once Komai can see such a pair, the language shows up in the settings list — reopen the settings page (or restart Komai) after installing one.

### Linux (native package)

Install your distribution's `hunspell-<language>` package:

| Distribution | Command (example: German) |
| --- | --- |
| Arch | `sudo pacman -S hunspell-de` |
| Debian / Ubuntu | `sudo apt install hunspell-de-de` |
| Fedora | `sudo dnf install hunspell-de` |
| openSUSE | `sudo zypper install myspell-de_DE` |

Package names vary by distro — search for `hunspell` (or `myspell`) plus your language. Komai also reuses dictionaries already installed for LibreOffice, so you may already have some.

### Flatpak

Install the matching `org.freedesktop.Platform.Hunspell.<locale>` runtime extension from Flathub:

```sh
flatpak install flathub org.freedesktop.Platform.Hunspell.de_DE
```

The extensions for the languages you've configured in Flatpak (`flatpak config --set languages …`) are pulled in automatically when you install Komai, so for most people no extra step is needed.

### Snap

The snap ships a small set of dictionaries baked in by the packager; because of strict confinement it can't read dictionaries installed elsewhere on the system. To add one yourself, use the [Manual](#manual-any-platform) method — the data directory inside the snap is `~/snap/komai/current/.local/share/komai/`.

### macOS

There is no system Hunspell directory on macOS (the OS spell checker uses a different mechanism Komai can't read), so the built-in English dictionary is what you get out of the box. For other languages, use the [Manual](#manual-any-platform) method, or install [LibreOffice](https://www.libreoffice.org/) language packs and copy their `.dic`/`.aff` pairs over.

### Windows

Same as macOS — there's no shared Hunspell directory, so use the [Manual](#manual-any-platform) method for languages beyond the built-in English.

### Manual (any platform)

Download a `<locale>.dic` and `<locale>.aff` pair — the [LibreOffice dictionaries repository](https://github.com/LibreOffice/dictionaries) has them for dozens of languages — and drop both files into a `hunspell/` folder inside Komai's data directory:

| Install type | Path |
| --- | --- |
| Linux (native) | `~/.local/share/komai/hunspell/` |
| Flatpak | `~/.var/app/cc.etke.komai/data/komai/hunspell/` |
| Snap | `~/snap/komai/current/.local/share/komai/hunspell/` |
| macOS | `~/Library/Application Support/komai/hunspell/` |
| Windows | `%LOCALAPPDATA%\komai\hunspell\` |

Create the `hunspell/` folder if it doesn't exist. The file names must keep the `.dic`/`.aff` extensions and match each other (e.g. `fr_FR.dic` + `fr_FR.aff`).

## Corrections and your personal dictionary

Right-click a flagged word:

- pick a suggested correction to replace the word;
- **Add to dictionary** to accept the word permanently — it goes into a `personal.dic` file (one word per line) in Komai's data directory, and is shared across all your [👥 application profiles](application-profiles.md).

## What is skipped

These never get flagged: URLs and email addresses, backtick-delimited code spans and fenced code blocks, `@mentions`, `#tags`, `:emoji:` shortcodes, all-caps acronyms, and `camelCase` / identifier-looking tokens.

## Where things are stored

- The on/off state and the list of enabled languages live in your per [👥 application profile](application-profiles.md) `config.yml` (under `composer.input.spellcheck.*`), alongside the rest of Komai's settings.
- `personal.dic` lives directly in Komai's data directory (e.g. `~/.local/share/komai/personal.dic`), and any dictionaries you place manually live in a `hunspell/` folder under it (see the [Manual](#manual-any-platform) table for the path on your platform). Both are shared across profiles.

## CJK languages

Chinese, Japanese, and Korean are not covered — there is no Hunspell dictionary for word-by-word checking of those, and the scripts are simply left alone (typing in them never produces a flag).

## Related

- [⌨️ Keyboard Shortcuts](keyboard-shortcuts.md)
- [⚙️ Settings](../settings/README.md) — where `config.yml` lives and how per-profile config works.
