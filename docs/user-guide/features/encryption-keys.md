# 🔐 Encryption Key Export and Import

Komai can save the end-to-end encryption keys this session holds into a passphrase-protected file, and load such a file back in. The file format is the standard Matrix key export format, so it is interchangeable with [Element](https://element.io/) and [Nheko](https://nheko-reborn.github.io/): keys exported from one client can be imported into any of the others.

This is a manual, offline complement to [key backup](https://matrix.org/docs/matrix-concepts/end-to-end-encryption/): a file you control, useful before signing out of a device or when migrating between clients.

## Exporting keys

1. Open `Settings → Account` and find the **Encryption keys** row.
2. Press **Export…**, choose a passphrase, and confirm it.
3. Pick where to save the file. A timestamped filename that includes your account id is suggested, for example `2026-08-10-1518-matrix-account-alice-example.com-e2ee-keys.txt`.

The file contains every room key this session knows about, encrypted with your passphrase. The dialog reports how many keys were written.

## Importing keys

1. In the same **Encryption keys** row, press **Import…**.
2. Select a previously exported key file (from Komai, Element, or Nheko) and enter the passphrase it was exported with.

Importing is always safe: keys the session already holds are skipped, and the dialog reports how many keys were new out of how many the file contains. Messages that previously showed as undecryptable become readable once their keys are imported.

## Security notes

- The exported file grants the ability to **decrypt your encrypted message history** to anyone who has both the file and its passphrase. Choose a strong passphrase and store the file like a credential, not like a document.
- The export contains decryption keys only; it does not contain your login, password, or the ability to send messages as you.
- Deleting the file after a successful import on the target device is a reasonable habit.
