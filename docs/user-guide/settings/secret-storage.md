# 🔐 Secret Storage

Komai supports two secret storage providers configured via `secrets.provider` in profile `config.yml`.

Provider modes:

- `secret_service` (default): stores secrets in the OS secure backend via QtKeychain
- `file`: stores secrets in `secrets.yml` inside the profile directory

Quick read:

- ✅ `secret_service` is preferred (more secure)
- ⚠️ `file` is a compatibility fallback for environments where secure backend support is missing

Startup behavior:

- On first profile creation, Komai picks the best available provider.
- In pre-auth startup (no persisted session identity yet), Komai can re-evaluate provider availability and switch provider.
- Once session identity exists, Komai keeps the configured provider to avoid secret loss from accidental provider switching.

Security notes:

- Secrets are never written to `config.yml`.
- Secrets are never written to `state.yml`.
- `secrets.yml` (file provider) is written with owner read/write permissions.

Testing override:

- `KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY=unavailable`
- `KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY=available`

See also:

- [Settings Overview](README.md)
- [Backup and Restore](backup-and-restore.md)
- [Storage Locations](../operations/storage.md#secrets--providers)
