# Stickers and GIF Search

> **Status:** Research / design exploration. Not yet implemented, no committed timeline.
> **Related issue:** [#133 -- GIF keyboard / easy GIF sending](https://github.com/etkecc/komai/issues/133)

This document captures the current thinking on how Komai could grow from its existing
MSC2545 sticker-pack support into a unified picker that also covers searched GIFs and,
optionally, AI-generated images. It is meant to make the trade-offs visible, surface
useful prior art, and gather feedback before any code lands.

## TL;DR

- The GIF-search market shifted in 2026: **Tenor is shutting down on 30 June 2026**, **GIPHY's free tier is unusable** for any real user base, and **[Klipy](https://klipy.com)** (founded by ex-Tenor staff, adopted by WhatsApp / Discord / Bluesky) has become the practical default.
- Matrix forbids external HTTPS URLs in media events, so **every GIF that gets sent has to be uploaded to the user's own homeserver** first. There is no shortcut.
- Komai already has [MSC2545](https://github.com/matrix-org/matrix-spec-proposals/pull/2545) image-pack infrastructure (the existing sticker picker), so the work is best framed as **growing one picker with multiple sources** rather than building a separate "GIF feature".
- The proposed direction is a **pluggable, multi-provider picker**: built-in MSC2545 packs + optional GIF search (Klipy or GIPHY, user-supplied API key) + optional AI image generation, all behind one tabbed UI and one `/s` slash command.

## Why this is bigger than just GIFs

The original issue describes a GIF picker. In practice, "I want to send a reaction GIF" sits next to two adjacent needs Komai users already have or might want:

- **Sticker packs** (MSC2545) -- already implemented, but discoverability is poor and there is no obvious place to add new sources.
- **AI-generated images** -- novel for Matrix clients. "Make me a sticker of a cat wearing a hat" is the same UX shape as a GIF search: query in, image out, send as `m.sticker`.

Treating these as one feature with pluggable backends gives users a single mental model ("open the picker, switch tab if you want a different source") and avoids three separate buttons cluttering the composer.

## Provider landscape

### GIPHY

- API key is mandatory.
- The "beta" key is capped at **100 calls per hour per key**, shared across every user of the app. Effectively useless beyond personal experimentation.
- A production key requires manual application, approval, and (per public reports from 2024-2025) a non-trivial commercial agreement.
- Mandatory "Powered by GIPHY" branding and per-item creator attribution (ToS section 5A).
- **Verdict:** poor fit for an open-source client.

### Tenor (Google)

- On **13 January 2026**, Google announced Tenor's API will close to new key registrations. Existing keys keep working until **30 June 2026**, then are permanently discontinued.
- **Verdict:** do not build on this.

### Klipy

- Founded by former Tenor employees specifically to fill the void. WhatsApp, Discord, and Bluesky have all migrated or are migrating to it.
- API surface is near-identical to Tenor's. Migration is reportedly often a base-URL swap.
- Free "test" key allows **100 calls per minute** with no expiry. Generous enough for individual users and small instances.
- Production key needs a partner-panel form but approval is not manually gated the way GIPHY's is.
- "Powered by KLIPY" branding and a "Search KLIPY" placeholder are required.
- **Verdict:** currently the best commercial option. Fits both individual hobbyist use and a future negotiated-key arrangement.

### Keyless alternatives

| Option | Notes | Realistic? |
|--------|-------|-----------|
| **DuckDuckGo image search** | Used as a fallback by `matrix-gif-keyboard` via an undocumented endpoint with `type_image=gif`. Fragile and unsupported. | Backup only |
| **SearXNG** (self-hosted) | Real JSON API at `/search?format=json`. GIF filtering is hacky and result quality depends on configured engines. | Useful for self-hosters who refuse external dependencies |
| **Pixabay** | Free API (key required, permissive licensing). Animated-GIF catalogue is small. | Niche |
| **Purpose-built FOSS GIF index** | Does not exist. | N/A |

### What other clients do

Every major chat app (Signal, Telegram, Discord, WhatsApp) ships **a single dedicated production API key baked into the build**. None require end users to bring their own. Some (Signal) proxy search through their own servers for privacy. For an open-source project without that infrastructure, **user-supplied keys are the honest starting point**, with a future option to negotiate a shared production key once usage justifies it.

In the Matrix ecosystem, no major client has native GIF search today. Element's [issue #321](https://github.com/element-hq/element-meta/issues/321) has been open since 2015. The closest existing solutions are widgets and bots ([matrix-gif-keyboard](https://github.com/torinreilly/matrix-gif-keyboard), [giphytenormatrixproxy](https://github.com/pix/giphytenormatrixproxy), [GiphyMaubot](https://github.com/maubot/giphy)). FluffyChat and Nheko support MSC2545 sticker packs but no GIF search.

## Matrix spec constraints

### `m.sticker` event type

Stable since [MSC1158](https://github.com/matrix-org/matrix-spec-proposals/pull/1158) (~2018). A few facts that shape the design:

- It is a top-level room event (not an `m.room.message` msgtype).
- The `url` field **must be an `mxc://` URI**. HTTP/S URLs are not allowed.
- `info.is_animated` boolean was added in [Matrix v1.18](https://matrix.org/blog/2026/03/26/matrix-v1.18-release/) (March 2026, [MSC4230](https://github.com/matrix-org/matrix-spec-proposals/pull/4230)). Clients that read it can pick smarter autoplay behavior.
- Clients render stickers differently from images: floating, larger, no download chrome.

### Why no external URLs

There is no MSC allowing external HTTPS URLs in media events, and this is **deliberate**. If a recipient's homeserver could be told to fetch an arbitrary URL on the sender's behalf, an attacker could log the recipient's homeserver IP. MXC mandate is the mitigation.

The practical consequence: **every GIF the user picks has to be downloaded from the provider, then uploaded to the user's own homeserver**. There is no "embed by URL" path.

### MSC2545 image packs (Komai already supports this)

[MSC2545](https://github.com/matrix-org/matrix-spec-proposals/pull/2545) defines the de-facto sticker-pack format the wider Matrix ecosystem uses. Komai has full support: `src/imagepacks/` (C++ models), `resources/qml/emoji/StickerPicker.qml` (picker), `resources/qml/dialogs/media/ImagePack*.qml` (management dialogs), and `src/rust/src/matrix_backend/runtime_image_packs.rs` on the Rust side. Pack discovery walks account data, enabled room packs, current room state, and parent spaces.

Any new picker work should layer **on top of this** rather than around it.

### E2EE rooms

In encrypted rooms, each upload is encrypted client-side with a fresh random AES key and IV. Sending the same GIF to two encrypted rooms therefore produces two unrelated ciphertexts, two uploads, two MXC URIs. This is a fundamental property of probabilistic encryption -- no client-side trick can deduplicate it. The cache strategy below reflects this.

### GIF file size and the transcoding question

A representative 5-second 480p reaction GIF is in the 3-8 MB range. The same content as `video/mp4` (H.264) is typically 200-600 KB, an order of magnitude smaller. Currently no Matrix client transcodes GIFs to video before upload, and Synapse will only generate a JPEG thumbnail of the first frame, not transcode.

There is a real trade-off if Komai were to transcode:

- **Pro:** dramatically smaller uploads, kinder to homeservers and to bandwidth-limited users.
- **Con:** clients render `image/gif` and `video/mp4` differently. Reaction-GIF UX (silent autoplay, infinite loop, no chrome) is what `image/gif` produces by default. `video/mp4` typically shows with play / pause / seek controls, which is the wrong shape for a reaction.

The default proposed direction is **upload as-is** and rely on `info.is_animated: true` to signal the autoplay-loop intent. Transcoding is a future optimisation worth revisiting if storage growth becomes a real complaint.

## Proposed architecture

### Pluggable providers

A common interface every provider implements:

1. **Search / browse** -- given a query (or no query for a default feed), return result metadata + thumbnail URLs.
2. **Preview** -- show a thumbnail or animated preview without committing to the full download.
3. **Send** -- obtain the full media, upload to the user's homeserver, send an `m.sticker` event.
4. **Configuration** -- declare what settings it needs (API key, endpoint URL, model name, etc.).
5. **Attribution** -- declare what branding must be shown (e.g. "Powered by KLIPY").

### Provider matrix

| Provider | Type | Needs key? | Source of media | Send mechanism |
|----------|------|-----------|----------------|----------------|
| **MSC2545 image packs** | Built-in | No | User's own packs, room packs, space packs | Already on a homeserver, reference existing MXC URLs |
| **Bundled / community sticker packs** ([maunium-style](https://github.com/maunium/stickerpicker)) | Built-in or registry | No | Curated packs, MXC URLs on a third-party homeserver | Reference existing MXC URLs (re-upload needed for E2EE rooms). Maunium packs use their own JSON schema, not MSC2545, so this provider needs a compatibility layer. |
| **Klipy GIF search** | External | Yes (user-supplied) | Animated GIF library | Download from CDN, upload to user's homeserver, send as `m.sticker` |
| **GIPHY GIF search** | External | Yes (user-supplied) | Animated GIF library | Same as Klipy |
| **AI image generation** (e.g. OpenAI Images) | External | Yes (user-supplied) | Generated on demand | Generate, download, upload, send as `m.sticker` |
| **SearXNG** | Self-hosted | No (instance URL) | Whatever the user's SearXNG indexes | Download, upload, send |

### Settings sketch

Each provider is independently enabled or disabled. There is no single "GIF provider" dropdown; multiple can be on at once.

```
Settings -> Integrations -> Stickers & GIFs

[x] MSC2545 Image Packs                 (built-in, always on)
    Manage packs...

[ ] Bundled / Community Sticker Packs
    Registry URL: [________________________]

[x] Klipy GIF Search
    API Key:  [________________________]
    "Powered by KLIPY" attribution will be shown.

[ ] GIPHY GIF Search
    API Key:  [________________________]
    "Powered by GIPHY" attribution will be shown.

[ ] AI Image Generation (OpenAI)
    API Key:  [________________________]
    Model:    [dall-e-3 v]
```

### Picker UI

One picker popup, with a tab per enabled provider:

```
+--------------------------------------------------+
| [Stickers] [Klipy] [GIPHY] [AI]                  |
+--------------------------------------------------+
| Search: [shocked dog______________________]  [x] |
+--------------------------------------------------+
|  +-----+ +-----+ +-----+ +-----+                 |
|  | GIF | | GIF | | GIF | | GIF |                 |
|  +-----+ +-----+ +-----+ +-----+                 |
|  +-----+ +-----+ +-----+ +-----+                 |
|  | GIF | | GIF | | GIF | | GIF |                 |
|  +-----+ +-----+ +-----+ +-----+                 |
+--------------------------------------------------+
|                                Powered by KLIPY  |
+--------------------------------------------------+
```

- The "Stickers" tab is the existing MSC2545 picker.
- Each tab shows that provider's content, with the right search affordance (a query field for GIF search, a prompt field for AI).
- The attribution footer changes per active tab.

### `/s` slash command

The slash command opens the same picker UI with pre-filled state. It does not duplicate any rendering logic.

```
/s                          -> open picker, default tab
/s shocked dog              -> open picker, default search tab, pre-filled query
/s klipy shocked dog        -> open picker, Klipy tab, pre-filled query
/s giphy dancing cat        -> open picker, GIPHY tab, pre-filled query
/s ai cat wearing a hat     -> open picker, AI tab, pre-filled prompt
```

If the first word after `/s` matches an enabled provider id, route to that tab. Otherwise treat the entire string as a search query for the default / last-used tab.

`/s` is preferred over `/gif` because the picker covers stickers and (potentially) AI too. A `/gif` alias could be added if user feedback warrants it.

### Local cache

A simple SQLite table keyed by `(provider, content_id)` storing the resulting `mxc_url`, alt text, and dimensions, lets us **skip the download + upload round-trip when the same GIF is sent again to an unencrypted room**. For encrypted rooms the upload step is unavoidable (see above), but we can still skip the provider download by reusing locally cached bytes. Eviction is LRU or time-based; the exact policy is uninteresting and can be tuned later.

This is intentionally **not** persisted as a personal MSC2545 image pack. Putting transient GIFs into account data would inflate it without bound and pollute the user's curated sticker pack as seen by other clients.

## Sending flow

For each selected item:

1. Look up `(provider, content_id)` in the local cache.
2. If cached and the target room is unencrypted, reuse the cached `mxc://` URL directly.
3. Otherwise download the media from the provider's CDN.
4. Upload to the user's homeserver:
   - Unencrypted room: plain upload, cache the resulting `mxc://`.
   - Encrypted room: encrypt with a fresh AES key + IV, upload the ciphertext, build the `EncryptedFile` payload.
5. Send an `m.sticker` event referencing the upload, with `info.is_animated: true` for GIFs.

A representative event:

```json
{
  "type": "m.sticker",
  "content": {
    "body": "Cat typing furiously",
    "url": "mxc://homeserver.example/gif123",
    "info": {
      "mimetype": "image/gif",
      "w": 480,
      "h": 270,
      "size": 3145728,
      "is_animated": true,
      "thumbnail_url": "mxc://homeserver.example/thumb123",
      "thumbnail_info": { "mimetype": "image/jpeg", "w": 480, "h": 270, "size": 45000 }
    }
  }
}
```

Encrypted rooms use the `file` (EncryptedFile) field in place of `url`.

## Open questions

These are the points where outside input would be most useful:

1. **Default provider strategy.** Ship with no provider enabled by default and let users opt in (current proposal), or pre-enable Klipy with a placeholder key field that nags until set?
2. **Bundled sticker packs.** Worth shipping a small curated set of community packs in the app, or strictly opt-in via a registry URL?
3. **AI generation.** Useful enough to include as a first-class provider, or out of scope for v1 of this work?
4. **Slash command name.** `/s` (covers all sources), `/gif` (matches the original issue's wording), or both?
5. **GIF size handling.** Accept that a 5 MB raw GIF gets uploaded as-is (current proposal), or transcode to MP4 client-side first to save 5-10x in storage at the cost of a video-player render?
6. **Branding.** Provider attribution in the picker is required by the providers' ToS. Are there other places (e.g. the message itself, a tooltip on the sticker) where attribution should also surface?

## Sources

### Provider research

- [GIPHY Developer FAQ](https://developers.giphy.com/faq/)
- [GIPHY API Terms of Service (section 5A: branding)](https://support.giphy.com/hc/en-us/articles/360028134111)
- [Tenor API shutdown announcement](https://digitalbiztalk.com/article/google-shuts-down-tenor-api-what-developers-need-to-know-in-2026)
- [Klipy API documentation](https://docs.klipy.com/)
- [Migrate from Tenor to Klipy](https://klipyblog.medium.com/migrate-from-tenor-to-klipy-gif-api-in-10-seconds-eecdb241c936)
- [WhatsApp replacing Tenor with Klipy](https://gadgets.beebom.com/news/whatsapp-to-replace-tenor-with-klipy-for-gifs-and-stickers)
- [Signal's GIPHY proxy approach](https://signal.org/blog/giphy-experiment/)

### Matrix specification

- [MSC1158: Sticker messages](https://github.com/matrix-org/matrix-spec-proposals/pull/1158)
- [MSC2545: Image Packs](https://github.com/matrix-org/matrix-spec-proposals/pull/2545)
- [MSC4230: Animated image flag](https://github.com/matrix-org/matrix-spec-proposals/pull/4230)
- [Matrix v1.18 release notes](https://matrix.org/blog/2026/03/26/matrix-v1.18-release/)

### Matrix ecosystem

- [Element issue #321 (GIF keyboard, open since 2015)](https://github.com/vector-im/element-meta/issues/321)
- [matrix-gif-keyboard](https://github.com/torinreilly/matrix-gif-keyboard)
- [maunium/stickerpicker](https://github.com/maunium/stickerpicker)
- [giphytenormatrixproxy](https://github.com/pix/giphytenormatrixproxy)

See also: [QML/UI Structure](../qml-ui.md), [Storage Architecture](../storage.md).
