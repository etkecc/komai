# 🌐 Translations

Komai inherits many human-made translations from nheko — but some languages had no translations at all, and others had very low coverage. Both result in a poor experience for non-English speakers.

To address this, Komai uses AI-assisted translation to fill in the gaps. We believe that properly-guided AI can produce good-enough translations, which can later be improved by human contributors if and when necessary. Having complete (even if imperfect) translations is far better than having none.

The pipeline is provider-neutral in principle; the current backend is the [Claude CLI](https://docs.anthropic.com/en/docs/claude-cli), wired up via `call_claude()` in [`bin/translations/translate.py`](../../bin/translations/translate.py). Swap that one function to use a different LLM.


## 📂 Directory structure

Translation files live in per-language directories under [`resources/langs/`](../../resources/langs/):

```
resources/langs/
  GUIDE.md                      # Instructions given to the LLM for translation
  de/
    komai_de.ts                  # German translations
    GUIDE.md                     # Optional: German-specific translation instructions
  fr/
    komai_fr.ts
  ...
```

The English file (`en/komai_en.ts`) serves as the reference — it contains all source strings.


## 🔄 Updating source strings

After modifying UI strings in source code (C++, QML, or Rust-originated strings in C++ translation modules), run:

```sh
just translations-update
```

This calls Qt's `lupdate` to scan `src/` and `resources/qml/` and update all `.ts` files with new, changed, or removed strings, then normalizes the XML format. Newly added strings appear as `type="unfinished"` in each language file.

### XML normalization

The `.ts` files are kept in a canonical XML format produced by ElementTree. This ensures that the translation script and `lupdate` don't fight over formatting. Normalization is run automatically as part of `just translations-update`, but can also be run standalone:

```sh
just translations-normalize            # all languages
just translations-normalize --lang de  # single language
```


## 🤖 AI-powered translation

The translation pipeline lives in [`bin/translations/translate.py`](../../bin/translations/translate.py).

### ⚙️ How it works

1. The script parses a `.ts` file and extracts all `<translation type="unfinished">` entries, along with their context, source-file location, and any translator comments (`comment` / `extracomment`).
2. It sorts unfinished entries by `(context, source)` so related strings batch together, giving the model consistency pressure within a single call.
3. It sends batches of source strings as JSON to the configured LLM.
4. The LLM returns translations following the rules in [`resources/langs/GUIDE.md`](../../resources/langs/GUIDE.md).
5. A post-translation validator checks that placeholders (`%1`/`%n`/`%L1`), HTML-tag counts, `&&` literal ampersands, and keyboard shortcuts are preserved. Failing translations are **rejected** and remain `unfinished` — the next run will retry them.
6. The script injects the validated translations back into the `.ts` file, removing the `type="unfinished"` attribute.
7. 💾 Each batch is saved immediately — safe to abort mid-run (Ctrl+C) without losing progress.
8. 🔁 Re-running the script only processes remaining unfinished strings.

### 📝 Translation instructions

The file [`resources/langs/GUIDE.md`](../../resources/langs/GUIDE.md) is the prompt given to the LLM. It contains rules about preserving placeholders (`%1`, `%2`, `%L1`), HTML tags, XML entities, keyboard shortcuts, Qt mnemonic accelerators (`&File` / `&&`), Matrix vocabulary consistency, quote style, and untranslatable terms (Matrix, Komai, etc.).

Optional per-language guides can be placed at `resources/langs/{LANGUAGE}/GUIDE.md` (e.g., `resources/langs/de/GUIDE.md` for German-specific instructions like formal/informal address). If present, they are appended to the general guide.

### 🔤 Translating a single language

```sh
just translations-claude-translate-lang de
```

Options (passed after the language code):

| Flag | Default | Description |
|------|---------|-------------|
| `--batch-size N` | 75 | Strings per LLM call |
| `--model MODEL` | CLI default | Model to use |
| `--dry-run` | off | Show unfinished strings without translating |
| `--print-prompt` | off | Print the rendered prompt for the first batch and exit (debug aid) |

Example with options:

```sh
just translations-claude-translate-lang ja --batch-size 50 --dry-run
```

### 🌍 Translating all languages

```sh
just translations-claude-translate-all
```

This iterates over every language directory (skipping `en`), calling `translations-claude-translate-lang` for each. Failures for one language don't stop the others.

The same options can be passed:

```sh
just translations-claude-translate-all --batch-size 50
```

### 🚀 Typical workflow

```sh
# 1. Update source strings after code changes
just translations-update

# 2. Translate all languages
just translations-claude-translate-all

# 3. Review the diff, commit
git diff resources/langs/
git add resources/langs/ && git commit -m "Update translations"
```

### ⚠️ Caveats

- **Rust-originated strings** (timeline state events, event type labels, errors, etc.) are translated on the C++ side via dedicated translation modules. When adding or changing these strings in Rust, update the corresponding `tr()` call in the appropriate C++ module and run `just translations-update`. See [architecture/translations.md](../architecture/translations.md) for details.
- **Plural forms** (numerus messages like `%n file(s)`) are not yet supported by the AI translation pipeline. They require language-specific plural rules and multiple translation variants per string. The script skips them and reports the count.
- Short or ambiguous strings (single words like "Call", "State") may occasionally be skipped by the model. Re-running picks them up since only unfinished strings are processed. The common `GUIDE.md` explicitly tells the model not to skip, but honours this imperfectly — which is why the script also logs specific skipped sources so you can iterate on the prompt.
- The validator is intentionally lenient: only placeholder, HTML-tag, `&&`, and keyboard-shortcut preservation are enforced. Cosmetic differences (quote style, whitespace) are not. Tune `validate_translation()` in `bin/translations/translate.py` if false positives or false negatives surface.
- The script requires the configured LLM CLI to be installed and authenticated (currently `claude`).
- Very large batches may hit context limits. The default batch size of 75 works well in practice.
- AI translations should be reviewed, especially for languages with complex grammar or honorific systems. Per-language `GUIDE.md` files can help steer the model toward the right register.

For technical details on why the system works the way it does (XML normalization, ElementTree vs regex, plural form limitations), see [architecture/translations.md](../architecture/translations.md).
