# Turkish translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register

Turkish UI commonly uses **impersonal / passive constructions** ("Odaya katılınamadı", "Kullanıcı atıldı") — match existing translations. When direct address is unavoidable, use **formal "siz"**.

## Matrix / chat vocabulary

| English | Turkish |
|---|---|
| room | **oda** (pl. **odalar**) |
| space | **uzay** / **alan** |
| thread | **konu** / **iş parçacığı** |
| direct message, DM | **doğrudan mesaj** |
| invite (verb / noun) | **davet etmek** / **davet** |
| join (a room) | **katılmak** |
| leave (a room) | **ayrılmak** |
| redact (= delete a message) | **silmek** / **kaldırmak** |
| encryption | **şifreleme** |
| encrypted | **şifreli** / **şifrelenmiş** |
| verify / verification / verified (E2EE) | **doğrulamak** / **doğrulama** / **doğrulanmış** |
| user | **kullanıcı** |
| message | **mesaj** |
| device | **cihaz** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026) where possible — existing translations prefer it.
- Turkish uses apostrophes in genitive constructions with proper nouns: `%1'ye`, `Ahmet'e`. This is **correct and expected**; preserve it.
- Buttons/labels: Turkish UI typically uses infinitive-like forms ("Kaydet", "İptal", "Gönder").

## Plural forms

This language uses **a single plural form** for all counts. When a `numerus` source like `%n member(s)` is presented in the plural-form pass, return one translation that works grammatically regardless of count.

The form must preserve `%n`. Example: `%n member(s)` → `["%n üye"]`.
