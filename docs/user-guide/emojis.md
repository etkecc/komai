# 😀 Emoji Search and Picker

Komai has a localized emoji picker with strong search and completion support.

Emojis are sourced from [🧾 multiple Data Sources](#-data-sources) and Komai can find them by common words, localized keywords, and project-specific aliases. We can add more of our own aliases over time.


## ✍️ How to Trigger It

In the message composer, start with a colon, then type your emoji keyword.

Komai accepts both the standard ASCII colon `:` and the full-width colon `：` used by some IMEs
(for example Japanese input).

For concrete query examples and locale scope, see [✨ Examples](#-examples).


## 🔎 What You Can Search By

Komai searches emoji using multiple keyword [sources](#-data-sources):

- Unicode short names (for example `:tumbler_glass`)
- common CLDR keywords (for example `:whiskey`, `:tea`, `:酒`)
- Komai-specific aliases (for example tea aliases like `:houjicha`)

Search is case-insensitive where relevant, and supports localized terms.


## 🌍 Language Behavior

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


## ✨ Examples

| Query | Result | Works in |
| --- | --- | --- |
| `:whiskey` | 🥃 | 🌐 all locales |
| `:bourbon` | 🥃 | 🌐 all locales |
| `:houjicha` | 🍵 ([Hōjicha](https://en.wikipedia.org/wiki/H%C5%8Djicha)) | 🌐 all locales |
| `:ほうじ茶` | 🍵 ([Hōjicha](https://en.wikipedia.org/wiki/H%C5%8Djicha)) | 🌐 all locales |
| `:玄米茶` | 🍵 ([Genmaicha](https://en.wikipedia.org/wiki/Genmaicha)) | 🌐 all locales |
| `:nihonshu` | 🍶 ([Sake](https://en.wikipedia.org/wiki/Sake)) | 🌐 all locales |
| `:酒` | 🍶, 🍷, 🍺, 🥃 and other alcohol-related ones | 🇯🇵 `ja` locale |
| `:komai` | 🦁 ([🦁 Komai Identity](identity.md)) | 🌐 all locales |


## 🧠 Matching Notes

Komai prioritizes exact/prefix matches first, then falls back to fuzzy matching when needed.
Very short queries are intentionally stricter to avoid unrelated results.


## 🧾 Data Sources

Komai builds emoji search data from:

- Unicode emoji data: [emoji-test.txt](https://unicode.org/Public/emoji/latest/emoji-test.txt)
- Unicode CLDR annotations (localized keywords): [CLDR project](https://cldr.unicode.org/) and [cldr-json](https://github.com/unicode-org/cldr-json)

Komai then applies small project-specific alias overrides on top.


## 🔗 More Details

- User-facing summary in [Differences from nheko](differences-from-nheko.md#-composer-and-replies)
- Technical architecture and pipeline details in [Emoji Architecture](../architecture/emojis.md)
