# 😀 Emoji Search and Picker

Komai has a localized emoji picker with strong search and completion support.

You can [✍️ trigger it](#%EF%B8%8F-how-to-trigger-it) from the composer, [🔎 search](#-search) by common words, localized keywords, and project-specific aliases from [🧾 multiple data sources](#-data-sources), and fine-tune results with [🎛️ emoji preferences](#%EF%B8%8F-emoji-preferences) for gender and skin tone.

Komai also supports [🖼️ custom emojis and stickers](#%EF%B8%8F-custom-emojis-and-stickers) via Matrix image packs.


## ✍️ How to Trigger It

In the message composer, start with a colon, then type your emoji keyword.

Komai accepts both the standard ASCII colon `:` and the full-width colon `：` used by some IMEs
(for example Japanese input).


## 🔎 Search

Komai searches emoji using multiple keyword sources:

- Unicode short names (for example `:tumbler_glass`)
- common CLDR keywords (for example `:whiskey`, `:tea`, `:酒`)
- Komai-specific aliases (for example tea aliases like `:houjicha`)

Search is case-insensitive where relevant, and supports localized terms.
Komai prioritizes exact/prefix matches first, then falls back to fuzzy matching when needed.
Very short queries are intentionally stricter to avoid unrelated results.

### Language and locale

Komai uses your current app locale for emoji keyword data.

- locale-specific CLDR keywords are loaded for your language
- English override aliases are treated as baseline and are available in all locales
- locale-specific overrides add extra local terms on top of that baseline

In practice:

| Query | `en` locale | `ja` locale |
| --- | --- | --- |
| `:whiskey` | ✅ works | ✅ works |
| `:houjicha` | ✅ works | ✅ works |
| `:玄米茶` | ✅ works (via Komai alias baseline) | ✅ works |
| `:酒` | ❌ usually does not match in English locale | ✅ works (from Japanese CLDR keywords) |

Why `:酒` differs:

- `:酒` is a Japanese keyword from Japanese CLDR data
- English locale does not normally include Japanese CLDR keywords for all emoji
- if you want a non-English term to work everywhere, add it as a Komai alias override

### Examples

| Query | Result | Works in |
| --- | --- | --- |
| `:whiskey` | 🥃 | 🌐 all locales |
| `:bourbon` | 🥃 | 🌐 all locales |
| `:houjicha` | 🍵 ([Hōjicha](https://en.wikipedia.org/wiki/H%C5%8Djicha)) | 🌐 all locales |
| `:ほうじ茶` | 🍵 ([Hōjicha](https://en.wikipedia.org/wiki/H%C5%8Djicha)) | 🌐 all locales |
| `:玄米茶` | 🍵 ([Genmaicha](https://en.wikipedia.org/wiki/Genmaicha)) | 🌐 all locales |
| `:nihonshu` | 🍶 ([Sake](https://en.wikipedia.org/wiki/Sake)) | 🌐 all locales |
| `:酒` | 🍶, 🍷, 🍺, 🥃 and other alcohol-related ones | 🇯🇵 `ja` locale |
| `:komai` | 🦁 ([🦁 Komai Identity](../identity.md)) | 🌐 all locales |


## 🎛️ Emoji Preferences

In **Application Settings → Composer → Emoji**, Komai provides two optional filters for the
inline emoji picker (the `:` / `：` completer in the message input):

- **Preferred gender**: `No preference`, `👨 Man`, `👩 Woman`
- **Preferred skin tone**: `No preference`, `👍🏻 Light`, `👍🏼 Medium-light`, `👍🏽 Medium`, `👍🏾 Medium-dark`, `👍🏿 Dark`

Gender note: this setting follows the Unicode emoji dataset and only applies to Unicode-defined
gender variants (`man` / `woman`).

Scope note: these preferences intentionally affect inline emoji completion only, and do not filter the full emoji
picker opened from the composer button.

Stored config values:

- `composer.input.emoji.preferred_gender`: `no_preference`, `man`, `woman`
- `composer.input.emoji.preferred_skin_tone`: `no_preference`, `light`, `medium_light`, `medium`, `medium_dark`, `dark`

Behavior:

- `No preference`: default search behavior (all matching variants can appear)
- gender/skin-tone preferences: matching generic entries still appear, while non-preferred variants are filtered out
- explicit searches for the other gender or another skin tone override the preference for that query

✨ Examples:

- `Preferred gender = 👨 Man`, `Preferred skin tone = No preference`, query `:beard`:
  - 🧔 `bearded_person`
  - 🧔🏻 `person_light_skin_tone_beard`
  - 🧔🏼 `person_medium_light_skin_tone_beard`
  - 🧔🏽 `person_medium_skin_tone_beard`
  - 🧔🏾 `person_medium_dark_skin_tone_beard`
  - 🧔🏿 `person_dark_skin_tone_beard`
  - 🧔‍♂️ `bearded_man`
  - 🧔🏻‍♂️ `man_light_skin_tone_beard`
  - 🧔🏼‍♂️ `man_medium_light_skin_tone_beard`
  - 🧔🏽‍♂️ `man_medium_skin_tone_beard`
  - 🧔🏾‍♂️ `man_medium_dark_skin_tone_beard`
  - 🧔🏿‍♂️ `man_dark_skin_tone_beard`

- `Preferred gender = 👨 Man`, `Preferred skin tone = 👍🏿 Dark`, query `:beard`:
  - 🧔 `bearded_person`
  - 🧔🏿 `person_dark_skin_tone_beard`
  - 🧔‍♂️ `bearded_man`
  - 🧔🏿‍♂️ `man_dark_skin_tone_beard`

- `Preferred gender = 👨 Man`, `Preferred skin tone = No preference`, query `:bearded woman`:
  - 🧔‍♀️ `bearded_woman`

- `Preferred gender = No preference`, `Preferred skin tone = 👍🏿 Dark`, query `:light skin tone beard`:
  - 🧔🏻 `person_light_skin_tone_beard`
  - 🧔🏼 `person_medium_light_skin_tone_beard`
  - 🧔🏻‍♂️ `man_light_skin_tone_beard`
  - 🧔🏼‍♂️ `man_medium_light_skin_tone_beard`
  - 🧔🏻‍♀️ `woman_light_skin_tone_beard`
  - 🧔🏼‍♀️ `woman_medium_light_skin_tone_beard`


## 🖼️ Custom Emojis and Stickers

Beyond standard Unicode emojis, Matrix supports **custom image emojis** and **stickers** via
[MSC2545 (Image Packs)](https://github.com/matrix-org/matrix-spec-proposals/pull/2545).

Custom emojis are small images (hosted on your homeserver) that can be used inline in messages,
just like regular emojis. Stickers are larger standalone images sent as their own message.

### Where packs come from

- **Your account**: a personal pack only you can use
- **Rooms**: packs defined in room state, available to everyone in that room
- **Spaces**: packs from parent spaces are inherited by child rooms

You can also globally enable packs from any room you're in, making them available everywhere.

### Using custom emojis

Custom emojis appear alongside Unicode emojis in the picker and inline completer (`:shortcode`).
When sent, they are embedded in the message HTML as `<img>` tags with `mxc://` URLs, so other
Matrix clients that support MSC2545 will render them inline.

### Managing packs

Open the emoji picker (by clicking the icon from the Composer) and click the settings icon to manage packs: create new ones, add or remove
images, toggle emoji/sticker usage, and enable packs globally.


## 🧾 Data Sources

Komai builds emoji search data from:

- Unicode emoji data: [emoji-test.txt](https://unicode.org/Public/emoji/latest/emoji-test.txt)
- Unicode CLDR annotations (localized keywords): [CLDR project](https://cldr.unicode.org/) and [cldr-json](https://github.com/unicode-org/cldr-json)

Komai then applies small project-specific alias overrides on top.


## 🔗 More Details

- User-facing summary in [Differences from nheko](../differences-from-nheko.md#-composer-and-replies)
- Technical architecture and pipeline details in [Emoji Architecture](../../architecture/emojis.md)
