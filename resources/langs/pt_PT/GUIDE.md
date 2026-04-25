# European Portuguese translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

**Important**: this is **European Portuguese (pt_PT)** — distinct from Brazilian Portuguese (pt_BR). Vocabulary and spelling differ in several places; when in doubt, pick the European form.

## Register

Prefer **impersonal constructions** ("Falha ao convidar...", "Utilizador expulso: %1") — this matches the existing translations. When direct address is needed, European Portuguese UI conventionally uses "tu" (informal) in casual apps; match existing strings.

## Matrix / chat vocabulary (European forms)

| English | pt_PT |
|---|---|
| room | **sala** (pl. **salas**) |
| space | **espaço** |
| thread | **tópico** or **conversa** |
| direct message, DM | **mensagem direta** |
| invite (verb / noun) | **convidar** / **convite** |
| join (a room) | **entrar em** |
| leave (a room) | **sair de** |
| redact (= delete a message) | **redigir** / **eliminar** |
| encryption | **encriptação** (pt_PT) — **not** "criptografia" (pt_BR) |
| encrypted | **encriptada/o** — **not** "criptografada" (pt_BR) |
| verify / verification / verified (E2EE) | **verificar** / **verificação** / **verificada/o** |
| user | **utilizador** (pt_PT) — **not** "usuário" (pt_BR) |
| message | **mensagem** |
| device | **dispositivo** |

## Typography

- Existing translations mostly use ASCII `...` for ellipsis. Continue that, or use `…` (U+2026) consistently — pick one and stick to it within a batch.
- Buttons/labels: use infinitive ("Guardar", "Cancelar", "Enviar").

## Plural forms

This language uses **2 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **other** — every other integer count, including 0

Each form must preserve `%n`. Example: `%n member(s)` → `["%n membro", "%n membros"]`.
