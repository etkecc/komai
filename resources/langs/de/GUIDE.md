# German translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register: informal ("du")

Address the user with **du/dich/dir/dein** — not "Sie". This matches the existing translations and is the convention for modern FOSS/chat UIs.

- "Do you want to join this room?" → "Möchtest du den Raum betreten?"
- "your messages" → "deine Nachrichten"
- "You can verify them" → "Du kannst sie verifizieren"

## Matrix / chat vocabulary

Use these consistently — do not switch between synonyms from one string to the next:

| English | German |
|---|---|
| room | **Raum** (pl. **Räume**) |
| space | **Space** |
| thread | **Thread** |
| direct message, DM | **Direktnachricht** |
| invite (verb / noun) | **einladen** / **Einladung** |
| join (a room) | **betreten** |
| leave (a room) | **verlassen** |
| knock | **anklopfen** |
| reaction | **Reaktion** |
| redact (= delete a message) | **löschen** |
| encryption | **Verschlüsselung** |
| encrypted | **verschlüsselt** |
| verify / verification / verified (E2EE) | **verifizieren** / **Verifizierung** / **verifiziert** |
| user | **Nutzer** (prefer this over "Benutzer" for consistency) |
| message | **Nachricht** |
| server | **Server** |
| device | **Gerät** |
| avatar | **Avatar** |

## Style

- **Buttons / short labels**: prefer infinitive — "Speichern", "Löschen", "Hinzufügen", "Einladen" (not "Speichere!" or "Ich speichere").
- **Noun phrases** work well for titles: "Erlaubte Raumeinstellungen", "Primärer Alias".
- Use the **horizontal ellipsis `…`** (U+2026), not three dots `...`. Example: `"Calling..."` → `"Wählen …"`.
- Capitalisation: only the first word of a sentence / label is capitalised (German sentence case), unless the word is a noun (which is always capitalised in German).
- **Quotes**: typographic „…" is acceptable but not required; ASCII `"…"` is fine too.
