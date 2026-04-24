# French translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register: formal ("vous")

Address the user with **vous / votre / vos** — not "tu". This is the standard French UI register and matches the existing translations.

- "Do you want to join this room?" → "Voulez-vous rejoindre ce salon ?"
- "your messages" → "vos messages"
- "You can verify them" → "Vous pouvez les vérifier"

## Matrix / chat vocabulary

Use these consistently — do not switch between synonyms from one string to the next:

| English | French |
|---|---|
| room | **salon** (pl. **salons**) — never "pièce" or "chambre" |
| space | **espace** |
| thread | **fil** (or **thread** in casual contexts) |
| direct message, DM | **message direct** |
| invite (verb / noun) | **inviter** / **invitation** |
| join (a room) | **rejoindre** |
| leave (a room) | **quitter** |
| knock | **frapper** (sometimes "toquer") |
| reaction | **réaction** |
| redact (= delete a message) | **effacer** |
| encryption | **chiffrement** |
| encrypted | **chiffré(e)** |
| verify / verification / verified (E2EE) | **vérifier** / **vérification** / **vérifié(e)** |
| user | **utilisateur** (or **contact** when referring to someone you interact with) |
| message | **message** |
| server | **serveur** |
| device | **appareil** |
| avatar | **avatar** |

## Typography (important)

French typography requires a **non-breaking space before** the punctuation marks `:`, `;`, `!`, `?`, and `»`, and **after** `«`. Use the narrow NBSP `U+202F` (` `) for `!`, `?`, `;`, `:` in tight UI contexts; a regular NBSP `U+00A0` (`\xa0`) is also acceptable and appears widely in the existing translations.

Examples from the existing file:
- "Ce message n'est pas chiffré !" → `Ce message n'est pas chiffré !`
- "Échec de l'invitation de %1 dans %2 : %3" → `Échec de l'invitation de %1 dans %2\xa0: %3`
- "Voulez-vous rejoindre ce salon ?" → `Voulez-vous rejoindre ce salon\xa0?`

## Style

- **Buttons / short labels**: prefer infinitive — "Ajouter", "Enregistrer", "Supprimer", "Inviter" (not "Ajoutez !" or similar).
- **Noun phrases** work well for titles: "Appel vidéo", "Autre salon", "Configuration des salons autorisés".
- Use the **horizontal ellipsis `…`** (U+2026), not three dots `...`. Example: `"Calling..."` → `"Appel en cours…"`.
- **Quotes**: prefer ASCII `"…"` in translations — the pipeline emits JSON, and mixing typographic `«` with ASCII `"` inside the same string breaks the JSON envelope. If you do use guillemets « … », both the opening `«` and closing `»` must be typographic — never mix with ASCII `"`.
- Inclusive language (e.g. "utilisateurs(rices)") is used sparingly in the existing translation — follow the source text's neutrality and don't add inclusive suffixes unless the source clearly addresses a specific audience.
