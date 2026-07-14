# 🎨 Themes

Komai uses a data-driven theme system. Built-in themes are defined as YAML files in [`resources/themes/`](../../../resources/themes/) and compiled into the binary at build time. Each YAML file contains resolved palette colors directly — what you see in the YAML is what the app uses.


## 🧰 Built-in themes

Komai ships with several built-in themes (see [`resources/themes/`](../../../resources/themes/) for the current list), including Komai light/dark, nheko light/dark, and popular community themes like Catppuccin, Dracula, Matrix, Nord, Rosé Pine, and Tokyo Night. The built-in set is maintained to meet [WCAG AA contrast](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html) for Komai's common UI text pairs.

See a [🖼️ Screenshot of the dark-matrix built-in theme](../screenshots/themes-dark-matrix.webp).


## 🌗 Follow the system theme

Set the theme toggle to **Auto** and Komai follows your desktop's light/dark preference: pick a theme family and Komai paints its light member by day and its dark member by night, flipping live the moment the OS switches. No restart, no second dropdown. Auto is the default for a fresh profile, so a first launch already matches your desktop instead of flashing light at someone who runs dark.

The toggle has three positions:

- **Light** / **Dark**: pin the variant yourself; the OS preference is ignored.
- **Auto**: follow the OS, staying inside the current theme family (`light-nord` ⇄ `dark-nord`). Mixing families across variants (light-komai by day, dark-dracula by night) is not supported; the single dropdown tracks one family at a time.

**Linux needs a desktop that speaks up.** Auto reads the freedesktop light/dark preference through your desktop portal. On a full desktop (GNOME, KDE, most others) it just works. On a bare window manager with no portal, the OS says nothing, so Auto has nothing to follow: rather than guess, Komai leaves your theme exactly as you set it. If Auto never flips, that's the missing portal, not a bug.

In Auto, an OS flip repaints straight away and triggers **no save of its own**. Your saved theme stays the family you picked; the light-or-dark half is re-derived from the OS at every launch.


## ⚙️ Where Your Current Theme Choice Is Stored

Komai stores the theme per profile in `~/.config/komai/profiles/<profile-id>/config.yml`:

- `ui.theme.slug`: the selected theme (under Auto, the family Komai follows)
- `ui.theme.mode`: `light`, `dark`, or `auto`

Example:

```yaml
ui:
  theme:
    slug: dark-komai
    mode: auto
```

Under `auto`, `slug` records the family you chose; the light-or-dark member is resolved from the OS at launch and on each flip, and is not written back on a flip. A profile from before this key existed keeps its look: Komai derives the mode from the slug's variant on first load, so upgrading never changes what you already see.

See [Settings: What Goes Where](../settings/README.md#what-goes-where) for config semantics and
[Settings: Profile Location](../settings/README.md#profile-location) for profile paths.


## 🗂️ User themes

In addition to built-in themes, Komai loads custom theme YAML files at startup from XDG data directories. Drop a `.yml` file into one of these paths and restart Komai:

1. `~/.local/share/komai/themes/` — user themes (highest priority)
2. `/usr/local/share/komai/themes/` — locally-installed system themes
3. `/usr/share/komai/themes/` — distro-packaged themes

The filename (without the extension) becomes the theme slug. User themes appear in the Settings dropdown alongside built-in themes.
For storage context, see [Storage Locations](../operations/storage.md#linux-paths).

**Priority rules:**
- Built-in themes always win over external themes with the same slug
- If the same slug exists in multiple directories, the first occurrence wins (user dir beats system dir)
- External themes are sorted after all built-in themes in the dropdown

**Format:** Use the same YAML format as built-in themes (see [Theme YAML format](#theme-yaml-format) below). The `author` and `source_base16` fields are optional and ignored at runtime. The `userColors` section is required — themes without it are skipped.

**No hot-reload:** Themes are loaded once at startup. Restart Komai to pick up new or changed theme files.

**Graceful handling:** If a theme file has missing keys, invalid hex values, missing `userColors`, or other errors, it is skipped with a log warning. Other themes still load normally. If a previously-selected custom theme file is removed, Komai falls back to the default built-in theme.


## ✨ Adding a new theme

### 🌈 Importing from tinted-theming (CLI)

The `komai theme` commands let you import [Base16 themes](https://github.com/tinted-theming/schemes/tree/spec-0.11/base16) directly into your user themes directory — no rebuild needed. If Komai is already running, restart to load imported or changed theme files. Browse available themes visually at the [Tinted Gallery](https://tinted-theming.github.io/tinted-gallery/), and use the schemes directory for canonical theme slugs.

```sh
# Search available themes
komai theme tinted-search
komai theme tinted-search dracula

# Import a theme (writes to ~/.local/share/komai/themes/)
komai theme tinted-import rose-pine

# Import with a custom name
komai theme tinted-import rose-pine my-rose

# Override variant detection
komai theme tinted-import rose-pine --variant light

# Overwrite an existing imported theme
komai theme tinted-import rose-pine --force

# List all available themes (built-in + user)
komai theme list

# Create a starter theme for manual customisation
komai theme create-sample light my-custom
komai theme create-sample dark cool-dark
```

After importing or creating a theme, restart Komai — the new theme appears in the Settings dropdown.

These commands work without a display server (SSH, containers) and do not require the source tree or Python.

### ✍️ Hand-crafted themes

Drop a `.yml` file into [`resources/themes/`](../../../resources/themes/) with all palette keys and a `userColors` section, then rebuild for a built-in theme. Or drop it into `~/.local/share/komai/themes/` for a user theme (no rebuild needed; Komai restart required).

Built-in themes are held to a stricter bar than ad-hoc user themes: they should pass Komai's WCAG AA audit for the common text pairings used throughout the app. For the baseline accessibility requirement, see [WCAG 2.2 Contrast (Minimum)](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html).


## 🧩 Theme YAML format

Each theme defines 16 Qt palette colors and 5 app-level semantic colors under `palette:`, plus a `userColors` section for message-bubble/user colors. Unknown keys are rejected by the validator to catch typos.

```yaml
name: "My Theme"
author: "Your Name"
variant: "light"   # or "dark"
palette:
  window: "#ffffff"          # main window background
  windowText: "#334258"      # main window text
  base: "#ffffff"            # input field / list background
  alternateBase: "#e8e8e8"   # alternating row background
  text: "#334258"            # input field / list text
  brightText: "#f2f5f8"      # bright text (hover states)
  button: "#ffffff"          # button background
  buttonText: "#555459"      # button text
  light: "#efefef"           # lighter than button (3D border)
  mid: "#dcdcdc"             # between button and dark (3D border)
  dark: "#334258"            # darker than button (hover bg, 3D border)
  highlight: "#f49300"       # selection / focus highlight
  highlightedText: "#243040" # text on highlight
  link: "#8f5200"            # hyperlinks
  toolTipBase: "#ffffff"     # tooltip background
  toolTipText: "#334258"     # tooltip text
  attention: "#a82353"       # attention / destructive accents (e.g. unread-mention badge background)
  attentionText: "#f2f5f8"   # text shown on attention backgrounds
  success: "#008000"         # success accents
  warning: "#f49300"         # warning accents
  error: "#dd3d3d"           # error messages
userColors:
  self:
    background: "#fbdaa7"      # required bubble background for your own messages
    text: "#334258"            # optional bubble body text override
    secondaryText: "#555459"   # optional bubble secondary/inactive text override
    link: "#8f5200"            # optional bubble link override
  others:                      # other-user bubble slots (minimum 1)
    - background: "#d3e1e5"
      text: "#334258"
      secondaryText: "#555459"
      link: "#8f5200"
    - background: "#bfe8cb"
    - background: "#e7d9f1"
    - background: "#c5e4ea"
```

The `userColors` section is **required**. It controls how sender names and message bubbles are colored in the timeline:

- **`self`** — a bubble slot for your own messages
- **`others`** — a list of bubble slots assigned to other users. In small rooms (fewer members than slots), each user gets a distinct slot. In large rooms, all other users share the first slot.

Each bubble slot has:

- **`background`** — required literal bubble fill color
- **`text`** — optional bubble body text color
- **`secondaryText`** — optional bubble secondary/inactive text color (notice text, collapsed-message controls, reply helper text)
- **`link`** — optional bubble hyperlink color

Fallbacks are straightforward:

- missing `text` falls back to `palette.text`
- missing `secondaryText` falls back to `palette.buttonText`
- missing `link` falls back to `palette.link`

The bubble fill itself uses the authored `background` directly. Komai does not add extra runtime tinting on top of it.

When importing a theme via `komai theme tinted-import` or creating one via `komai theme create-sample`, the `userColors` section is auto-generated from the theme's highlight color, base surface, and variant. The generated values are already softened for direct bubble use, and the commands may also emit explicit per-bubble `text`, `secondaryText`, and `link` overrides when the global palette would not be good enough on those bubble backgrounds.

Outer timeline metadata such as the timestamp beside a bubble uses the normal timeline palette, not the bubble slot overrides.

Imported themes also include an optional `source_base16:` section with the original Base16 palette for reference. This section is ignored by the build.

See [docs/architecture/themes.md](../../architecture/themes.md) for the technical pipeline details.


## ⚙️ How it works

Built-in themes live in [`resources/themes/*.yml`](../../../resources/themes/) and are compiled into the binary. Editing a theme YAML and rebuilding is all that's needed. See [Architecture: Themes](../../architecture/themes.md) for internals.
