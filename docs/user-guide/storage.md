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
| Chat database | `~/.local/share/komai/profiles/<profile-id>/db/<encoded-user-id>/` |
| User themes | `~/.local/share/komai/themes/` (see [themes.md](themes.md#-user-themes)) |
| Media cache | `~/.cache/komai/profiles/<profile-id>/media_cache/` |
| Log file (if file logging enabled) | `~/.cache/komai/profiles/<profile-id>/komai.log` |
| HTTP alt-svc cache (HTTP/3 enabled) | `~/.cache/komai/profiles/<profile-id>/curl_alt_svc_cache.txt` |

`<profile-id>` is the `-p` profile name/identifier.

💡 If a profile is not explicitly specified, the application uses the `default` profile.
See [Settings](settings/README.md#profile-location) for allowed profile-id characters.

`<encoded-user-id>` is the Matrix user ID escaped to a cross-platform-safe ASCII component:
bytes outside `[A-Za-z0-9._@+-]` are encoded as `%HH` (uppercase hex).

## File Patterns We Write

Filesystem path patterns:

- Database: `~/.local/share/komai/profiles/<profile-id>/db/<encoded-user-id>/`
- Media cache entry: `~/.cache/komai/profiles/<profile-id>/media_cache/<base64url(mxc-id)>.<ext>`
- Media cache (media subdir): `~/.cache/komai/profiles/<profile-id>/media_cache/media/<base64url(mxc-id)>.<ext>`
- Media thumbnails: `~/.cache/komai/profiles/<profile-id>/media_cache/<base64url(mxc-id)>_<w>x<h>_<crop|scale>_radius<r>`
- Windows room-avatar cache: `~/.cache/komai/profiles/<profile-id>/notifications/room-avatar-<base64url(room-id)>.png`

Secret-store key prefixes (for `secret_service` and `file` fallback map keys):

- `komai.<profile-id>.settings.`
- `komai.<profile-id>.local_crypto.`
- `komai.<profile-id>.matrix.`

## Profile File Split

Inside `~/.config/komai/profiles/<profile-id>/`:

- `config.yml` - durable preferences and advanced non-secret options
- `state.yml` - runtime/window/layout state
- `session.yml` - non-secret session/account metadata
- `secrets.yml` - only used when `secrets.provider=file`

See [Settings](settings/README.md#what-goes-where) for semantics and examples.

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
