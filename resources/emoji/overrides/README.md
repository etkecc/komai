# Emoji Overrides

Tracked, human-maintained emoji search overrides.

- `global.yml` applies across locales.
- `locale/<code>.yml` applies to one locale.

Entry shape (minimal):

```yaml
version: 1
emoji:
  - id: "1F943"
    preview: "🥃"
    locale: "bg"
    display_name: "Уиски"
    primary_token: "уиски"
    tokens_add: ["бърбън", "скоч"]
    tokens_remove: ["алкохол"]
```

Helper:

```bash
just emoji-add-token "🥃" bg "уиски"
```
