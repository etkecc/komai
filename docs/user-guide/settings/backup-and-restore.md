# 💾 Backup and Restore

Check [Secret Storage](secret-storage.md) first so you know whether your secrets are on disk or in an OS keychain backend.

Profile-file backup:

```bash
cp -r ~/.config/komai/profiles ~/komai-profiles-backup
```

Restore:

```bash
cp -r ~/komai-profiles-backup/* ~/.config/komai/profiles/
```

Important:

- 📄 In `file` mode, this includes `secrets.yml` (fallback secrets).
- 🔐 In `secret_service` mode, secure-backend secrets are not in YAML files; profile-file backup alone is incomplete.
- 💾 For `secret_service`, also back up your OS keychain/credential backend.

Common secure-backend backup targets:

- GNOME Keyring/libsecret: keyring files (commonly `~/.local/share/keyrings/`) or export via keyring UI
- KWallet: wallet files (commonly `~/.local/share/kwalletd/`) or export with KWallet Manager
- KeePassXC-backed setups: KeePassXC database file
- macOS/Windows: include system keychain/credential-store data in your normal machine/user backup strategy

See also:

- [Settings Overview](README.md)
- [Storage Locations](../operations/storage.md#secrets--providers)
