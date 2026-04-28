# 🩹 Stickers

Stickers in Matrix are larger standalone images sent as their own
[`m.sticker`](https://spec.matrix.org/v1.17/client-server-api/#msticker) event, distinct from custom
emojis (which are inline `<img>` tags inside a regular text message).

Both stickers and custom emojis are stored in [MSC2545 image
packs](https://github.com/matrix-org/matrix-spec-proposals/pull/2545) -- the same pack can mark
images as `emoticon`, `sticker`, or both. Komai's pack management UI lives in [Custom
Emojis](emojis.md#-managing-packs) and is shared between the two.


## ✅ What works today

- **Receiving and displaying stickers**: `m.sticker` events sent by other Matrix clients render in
  the timeline as expected.
- **Gallery navigation**: stickers participate in the in-app media viewer alongside images and
  videos.
- **Pack management**: you can browse, create, edit, and delete image packs, mark images as
  `sticker` (so other clients can use them as stickers), and enable packs globally.


## 🚫 What doesn't work today

**Sending stickers from the composer is not supported.** Komai forks from
[Nheko](https://github.com/Nheko-Reborn/nheko), which does support sending stickers, but the
existing UI around sticker management and sending isn't in a state we want to ship.

We may re-imagine and re-implement sticker sending in a future release. Until then, custom emojis
(see [😀 Emoji Search and Picker](emojis.md)) cover the common case of sending pack images inline,
including the dedicated `~shortcode` completer that filters to image-pack images only.
