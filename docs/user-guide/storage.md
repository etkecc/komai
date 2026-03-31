# 🗄️ Storage Locations

Komai stores data in several places depending on purpose.
Use this page as the physical "where", and [Settings](settings/README.md) as the behavioral "what/why".

Quick jumps:

- Profile semantics: [settings](settings/README.md#what-goes-where)
- Secret provider behavior: [settings](settings/README.md#secret-storage-modes)
- Theme files and loading rules: [themes.md](themes.md#-user-themes)

## Linux Paths

| Kind | Location |
| --- | --- |
| Profile settings files | `~/.config/komai/profiles/<profile-id>/` |
| Matrix SDK state store | `~/.local/share/komai/profiles/<profile-id>/matrix-sdk/state-store/` |
| Matrix SDK runtime cache | `~/.cache/komai/profiles/<profile-id>/matrix-sdk/cache/` |
| App media cache | `~/.cache/komai/profiles/<profile-id>/media/` |
| User themes | `~/.local/share/komai/themes/` (see [themes.md](themes.md#-user-themes)) |
| Log file (if file logging enabled) | `~/.cache/komai/profiles/<profile-id>/logs/komai.log` |
| HTTP alt-svc cache (HTTP/3 enabled) | `~/.cache/komai/profiles/<profile-id>/http/alt_svc_cache.txt` |

`<profile-id>` is the `-p` [Application Profile](application-profiles.md) name/identifier.

Komai currently stores persistent Matrix state through the Rust
`matrix-sdk` SQLite-backed store.

💡 If a profile is not explicitly specified, Komai opens the profile switcher unless only `default` exists.
See [Settings](settings/README.md#profile-location) for allowed profile-id characters.

## File Patterns We Write

Filesystem path patterns:

- Matrix SDK state store root: `~/.local/share/komai/profiles/<profile-id>/matrix-sdk/state-store/`
- Matrix SDK runtime cache root: `~/.cache/komai/profiles/<profile-id>/matrix-sdk/cache/`
- Shared media cache entry: `~/.cache/komai/profiles/<profile-id>/media/shared/full/<base64url(mxc-id)>.<ext>`
- Room media cache entry: `~/.cache/komai/profiles/<profile-id>/media/rooms/<base64url(room-id)>/full/<base64url(mxc-id)>.<ext>`
- Media thumbnails: `~/.cache/komai/profiles/<profile-id>/media/{shared|rooms/<base64url(room-id)>}/thumbnails/<base64url(mxc-id)>_<w>x<h>_<crop|scale>_radius<r>`
- Room-avatar cache: `~/.cache/komai/profiles/<profile-id>/notifications/room-avatar-<base64url(room-id)>.png`

Secret-store key prefixes (for `secret_service` and `file` fallback map keys).
`<env-tag>` isolates secrets across packaging formats depending on the filesystem
config paths they use, so that each unique filesystem path produces a unique keyring prefix
(`native`, `flatpak`, `snap`, or a 6-char hex hash for exotic environments):

- `komai.<env-tag>.<profile-id>.settings.`
- `komai.<env-tag>.<profile-id>.local_crypto.`
- `komai.<env-tag>.<profile-id>.matrix.`

## Profile File Split

Inside `~/.config/komai/profiles/<profile-id>/`:

- `config.yml` - durable preferences and advanced non-secret options
- `state.yml` - runtime/window/layout state
- `session.yml` - non-secret session/account metadata
- `secrets.yml` - only used when `secrets.provider=file`

See [Settings](settings/README.md#what-goes-where) for semantics and examples.

## Local Reset

Komai does not provide a database-compaction CLI on the matrix-sdk storage path.

If you need to reset local Matrix state for a profile:

- sign out of that profile and sign back in
- or, with Komai fully closed for that profile, inspect/remove the matrix-sdk state-store and cache directories manually

The media cache is separate and can be purged from the Settings UI while Komai is running.

## Secrets & Providers

- 🔐 `secrets.provider=secret_service` (default): secrets are in the OS secret backend via QtKeychain.
- 📄 `secrets.provider=file`: secrets are stored in `secrets.yml`.

Secure-backend secrets are not part of on-disk YAML backup.
For behavior and startup switching rules, see [Settings: Secret Storage Modes](settings/README.md#secret-storage-modes).
For backup guidance (including keychain backup), see [Settings: Backup and Restore](settings/backup-and-restore.md).

## See Also

- [Settings](settings/README.md)
- [Themes](themes.md)
- [Architecture: Storage](../architecture/storage.md)
- [Architecture: Settings](../architecture/settings/README.md)
