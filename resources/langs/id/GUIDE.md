# Indonesian translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register

Use **"Anda"** (polite, UI-conventional) for direct address, or drop pronouns entirely where natural — the existing translations frequently omit pronouns (e.g., "Pesan ini tidak terenkripsi!"). Do not use informal "kamu".

## Matrix / chat vocabulary

| English | Indonesian |
|---|---|
| room | **ruangan** (pl. same; Indonesian has no plural marking) |
| space | **ruang** |
| thread | **utas** |
| direct message, DM | **pesan langsung** |
| invite (verb / noun) | **mengundang** / **undangan** |
| join (a room) | **bergabung dengan** / **masuk ke** |
| leave (a room) | **keluar dari** |
| redact (= delete a message) | **meredaksi** (the existing translations use this; **menghapus** is an acceptable plainer alternative) |
| encryption | **enkripsi** |
| encrypted | **terenkripsi** |
| verify / verification / verified (E2EE) | **memverifikasi** / **verifikasi** / **terverifikasi** |
| user | **pengguna** |
| message | **pesan** |
| device | **perangkat** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026) where possible.
- Buttons/labels: use the verb stem without prefix ("Simpan", "Batal", "Kirim") — Indonesian UI convention.
- Capitalisation: sentence case — first word only.

## Plural forms

This language uses **a single plural form** for all counts. When a `numerus` source like `%n member(s)` is presented in the plural-form pass, return one translation that works grammatically regardless of count.

The form must preserve `%n`. Example: `%n member(s)` → `["%n anggota"]`.
