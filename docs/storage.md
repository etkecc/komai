# Storage Locations

Komai stores data in several places depending on purpose.

## Linux Paths

| Kind | Location |
| --- | --- |
| Profile configuration files | `~/.config/komai/profiles/<profile-id>/` |
| Chat database (default backend: LMDB in standard builds; memory if LMDB backend is disabled at build time) | `~/.local/share/komai/profiles/<profile-id>/db/<hash>/` |
| User themes | `~/.local/share/komai/themes/` |
| Media cache | `~/.cache/komai/profiles/<profile-id>/media_cache/` |
| Log file (if file logging enabled) | `~/.cache/komai/profiles/<profile-id>/komai.log` |
| HTTP alt-svc cache (HTTP/3 enabled) | `~/.cache/komai/profiles/<profile-id>/curl_alt_svc_cache.txt` |

`<profile-id>` is the `-p` profile name/identifier.

`<hash>` is `hex(sha256(user_id))`.

## File Patterns We Write

Filesystem path patterns:

- Database (default backend: LMDB in standard builds; memory if LMDB backend is disabled at build time): `~/.local/share/komai/profiles/<profile-id>/db/<hash>/`
- Media cache entry: `~/.cache/komai/profiles/<profile-id>/media_cache/<base64url(mxc-id)>.<ext>`
- Media cache (media subdir): `~/.cache/komai/profiles/<profile-id>/media_cache/media/<base64url(mxc-id)>.<ext>`
- Media thumbnails: `~/.cache/komai/profiles/<profile-id>/media_cache/<base64url(mxc-id)>_<w>x<h>_<crop|scale>_radius<r>`
- Windows room-avatar cache: `~/.cache/komai/profiles/<profile-id>/notifications/room-avatar-<base64url(room-id)>.png`

Secret-store key prefixes (for `secret_service` and `file` fallback map keys):

- `komai.<profile_hash>.settings.`
- `komai.<profile_hash>.local_crypto.`
- `komai.<profile_hash>.matrix.`

## Profile File Split

Inside `~/.config/komai/profiles/<profile-id>/`:

- `config.yml` - durable preferences and advanced non-secret options
- `state.yml` - runtime/window/layout state
- `session.yml` - non-secret session/account metadata
- `secrets.yml` - only used when `secrets.provider=file`

See [Configuration](configuration.md) for semantics and examples.

## Secrets

- `secrets.provider=secret_service` (default): secrets are in the OS secret backend via QtKeychain.
- `secrets.provider=file`: secrets are stored in `secrets.yml`.

Secure-backend secrets are not part of on-disk YAML backup.

## See Also

- [Configuration](configuration.md)
- [Architecture: Storage](architecture/storage.md)
- [Architecture: Configuration](architecture/configuration.md)
