# Stickers and GIF Search

> **Status:** Research / design exploration. Not yet implemented, no committed timeline.
> **Related issue:** [#133 -- GIF keyboard / easy GIF sending](https://github.com/etkecc/komai/issues/133)

This document captures the current thinking on how Komai could grow from its existing
MSC2545 sticker-pack support into a unified picker that also covers searched GIFs and,
optionally, AI-generated images. It is meant to make the trade-offs visible, surface
useful prior art, and gather feedback before any code lands.

## TL;DR

- The GIF-search market shifted in 2026: **Tenor is shutting down on 30 June 2026**, **GIPHY's free tier is unusable** for any real user base, and **[Klipy](https://klipy.com)** (founded by ex-Tenor staff, adopted by WhatsApp / Discord / Bluesky) has become the practical default.
- Matrix forbids external HTTPS URLs in media events. The mature alternative -- a federation media proxy like tulir's `klipy.mau.dev` -- requires operating a service that etke.cc does not want to own and would compromise real E2EE for sticker bytes. So the chosen path is **per-user provider keys + client-side upload to the user's own homeserver**, with a discoverability pattern modeled on Komai's existing voice-transcription UX (composer button always visible; activating without keys configured jumps you to settings).
- Komai already has [MSC2545](https://github.com/matrix-org/matrix-spec-proposals/pull/2545) image-pack infrastructure (the existing sticker picker), so the work is best framed as **growing one picker with multiple sources** rather than building a separate "GIF feature".
- The proposed direction is a **pluggable, multi-provider picker**: built-in MSC2545 packs + optional GIF search (Klipy or GIPHY, user-supplied API key) + optional AI image generation, all behind one tabbed UI and one `/s` slash command.
- A **Klipy partner key shipped in the build** (Signal-style) would obviate the per-user-signup friction entirely. One credential to obtain, no service to operate; worth one outreach to Klipy as a follow-up.

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
| **Pixabay** | Free API (key required, permissive licensing). Animated-GIF catalog is small. | Niche |
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

### Why no HTTPS URLs in media events

There is no MSC allowing external HTTPS URLs in media events, and this is **deliberate**. If a recipient's homeserver could be told to fetch an arbitrary URL on the sender's behalf, an attacker could log the recipient's homeserver IP. The `mxc://` mandate is the mitigation.

The practical consequence: the bytes a sticker references must be reachable as `mxc://<server>/<id>`, not as a direct CDN URL.

### Considered alternative: a federation media proxy

`<server>` in an `mxc://` URI does not have to be the sender's own homeserver. Any host implementing the Matrix federation media API can answer, and tulir's [`mautrix-go/mediaproxy`](https://github.com/mautrix/go/tree/master/mediaproxy) library is the de-facto implementation of this pattern: a thin Go shim that claims a server-name, holds a federation signing key, and responds to media downloads with an [MSC3916](https://github.com/matrix-org/matrix-spec-proposals/pull/3916) multipart `Location` redirect to the upstream CDN. The mautrix bridges (Telegram, Signal, WhatsApp, Meta, ...) use the same library for their "direct media" feature, which serves bridged media via federation redirects rather than reuploading it to Matrix.

For sticker / GIF use specifically, two implementations exist on top of it: tulir's [`giphyproxy`](https://github.com/maunium/stickerpicker/tree/master/giphyproxy) (Giphy-only; backs his public `klipy.mau.dev` instance) and the third-party [`giphytenormatrixproxy`](https://github.com/pix/giphytenormatrixproxy) (a fork that adds Tenor and a local-file mode). Element-with-widget-stickers users hit `klipy.mau.dev` today -- the event embeds `mxc://klipy.mau.dev/g-<id>` directly and no download-and-reupload happens on the sender side. There is no "ecosystem" of GIF proxies beyond these two; both are built on the same library, and one is a fork of the other.

We considered adopting this pattern and decided against it:

1. **It would demand operating a service, and search is not part of the proxy.** The proxy only handles media bytes (federation MXC -> CDN redirect). The *search* side -- Klipy/Tenor query -> result list -- is handled in existing implementations by a browser widget that the proxy serves and that talks to Klipy/Tenor directly from JS, with the operator's API key injected into the HTML template. That pattern does not carry over to a native QML client; Komai would have to add a search API to the proxy that does not exist anywhere upstream. The result would not be a "federation media proxy" we could pick up off the shelf, it would be a "Komai sticker service" we wrote from scratch and operated. etke.cc does not want to take that on right now.
2. **Self-hosting it as a fallback is a steep ask.** Each homeserver operator would need to deploy a federating Go service with DNS and `.well-known` delegation just to enable GIFs for their users, fragmenting the UX along admin lines. No existing Komai-adjacent deployment tooling -- including the [matrix-docker-ansible-deploy](https://github.com/spantaleev/matrix-docker-ansible-deploy/) playbook -- supports it today.
3. **E2EE attachments lose their teeth.** In encrypted rooms the proxy can only serve **plaintext** bytes at the CDN URL, so the `m.sticker` event has to embed `content.url` rather than `content.file` (EncryptedFile). The event metadata is still megolm-encrypted, but the bytes themselves are not. Element / widget-based stickerpickers have shipped this for years; most users do not realize their sticker bytes travel unencrypted. We do not want to inherit that.

The chosen direction is below in **Proposed architecture**: per-user provider keys with download-and-reupload (encrypted in E2EE rooms). It pays a storage and bandwidth cost and asks each user to paste a Klipy / GIPHY / AI-provider key, but keeps Komai a pure client with no operational dependencies and preserves real E2EE for sticker bytes. A possible later optimization -- shipping a Klipy partner key in the build, Signal-style -- is sketched in the **Optional upgrade: Klipy partner key** subsection further down.

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

The default proposed direction is **upload as-is** and rely on `info.is_animated: true` to signal the autoplay-loop intent. Transcoding is a future optimization worth revisiting if storage growth becomes a real complaint.

## Proposed architecture

### Pluggable providers

A common interface every provider implements:

1. **Provider shape** -- declare whether the provider is a *fast, multi-result search* (e.g. GIF providers: query -> N thumbnails in sub-second time, results auto-update as the user types) or a *slow, single-result generation* (e.g. AI image: explicit "Generate" click -> one result in multi-second time, with a "Regenerate" affordance to try again). The picker uses this to choose between grid-on-type and prompt-and-generate UI patterns.
2. **Search / generate** -- given a query or prompt, return result metadata + thumbnail URLs. Search providers return N results; generation providers return 1.
3. **Preview** -- show a thumbnail or animated preview without committing to the full download.
4. **Send** -- obtain the full media, upload to the user's homeserver, send an `m.sticker` event.
5. **Configuration** -- declare what settings it needs (API key, endpoint URL, model name, etc.).
6. **Attribution** -- declare what branding must be shown (e.g. "Powered by KLIPY").
7. **Cost / rate hints** -- declare any per-request cost or rate caveat the picker should surface to the user before firing a request. An AI provider may want to display "~$0.04 / generation" next to the Generate button; a GIF provider running on a shipped partner key may want to surface "100 req/min shared with all Komai users" when the rate window is tight.

The shape distinction matters because AI image generation has a fundamentally different UX from GIF search:

- **Latency.** GIF search returns in sub-second time; AI generation is multi-second, sometimes 10-20s for a single sticker.
- **Result count.** GIF search returns a grid (typically 20+); AI generation returns 1. Generating multiple candidates in parallel is possible but costly.
- **Cost.** GIF search is free at the user's tier; AI generation costs real money per request, so "fire another one because the first wasn't great" is not free.
- **Affordances.** GIF search wants a query field that updates results as the user types. AI generation wants a prompt field, an explicit "Generate" trigger, and a "Regenerate" / "Try again" button rather than auto-firing.

These differences are real and aren't going to be papered over by a clever common UI. AI is still worth including because it covers the "I want a specific sticker that doesn't exist on Klipy / GIPHY" need; we just need the picker layer to render the AI tab differently rather than forcing it into a GIF-grid shape it does not fit.

### Provider matrix

| Provider | Type | Needs key? | Source of media | Send mechanism |
|----------|------|-----------|----------------|----------------|
| **MSC2545 image packs** | Built-in | No | User's own packs, room packs, space packs | Already on a homeserver, reference existing MXC URLs |
| **Klipy GIF search** | External | Yes (user-supplied, or shipped partner key -- see below) | Animated GIF library | Download from CDN, upload to user's homeserver, send as `m.sticker` |
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

One picker popup, with a tab per enabled provider. Search-shape providers (GIF) render a query-and-grid layout:

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

Generation-shape providers (AI image) render a prompt-and-result layout instead. No auto-fire on typing; explicit Generate / Regenerate, with the cost hint visible:

```
+--------------------------------------------------+
| [Stickers] [Klipy] [GIPHY] [AI]                  |
+--------------------------------------------------+
| Prompt: [a cat wearing a top hat________]  [Gen] |
+--------------------------------------------------+
|                                                  |
|                  +-------------+                 |
|                  |             |                 |
|                  |   result    |                 |
|                  |             |                 |
|                  +-------------+                 |
|                                                  |
|              [Regenerate]    [Send]              |
+--------------------------------------------------+
| ~$0.04 / generation              Powered by OpenAI |
+--------------------------------------------------+
```

- The "Stickers" tab is the existing MSC2545 picker.
- Each tab renders the layout that matches its provider's shape (grid for search, prompt-and-result for generation).
- The attribution footer changes per active tab; generation-shape tabs also surface a cost / rate hint when the provider exposes one.

### Discovery without operator infrastructure

Per-user keys are the right answer for ownership and E2EE reasons (see above), but they fail badly if the feature is hidden behind a settings page nobody opens. The fix is to make the unconfigured state itself the discovery surface, the way Komai's existing **voice transcription** feature already does:

- The sticker / GIF button is **always** visible in the composer, whether or not any provider is configured.
- Activating it always opens the picker. When the user lands on a tab whose provider is not configured (Klipy without a key, GIPHY without a key, AI without an OpenAI key), the tab body is replaced with a short "Not configured. [Open Settings]" placeholder rather than an empty grid.
- Clicking "Open Settings" jumps straight to **Settings -> Integrations -> Stickers & GIFs**, with the relevant provider section focused.
- The MSC2545 tab still works as today, so a brand-new user is not staring at a totally empty picker -- the existing sticker-pack experience covers the zero-config case, and GIF / AI tabs surface themselves through this empty-state prompt.

This pattern is deliberately stolen from voice transcription, which renders a composer button that works the same way: the button is there, activating it without a configured OpenAI-compatible endpoint jumps you to the relevant settings. Users discover the feature by reaching for it, not by trawling settings.

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

### Optional upgrade: Klipy partner key

The per-user-key path above asks every user to sign up at klipy.com to enable GIF search. That is real friction even with a 2-minute signup and a well-designed empty state. The one way to make it disappear without operating any service is to **negotiate a single Klipy partner key for Komai and ship it in the build**, Signal-style. For the user the experience becomes "GIFs just work".

- Klipy is actively courting clients post-Tenor. Their partner-panel form is not approval-gated the way GIPHY's is, and they have strong commercial incentive to onboard Matrix clients.
- Cost to Komai: one credential to obtain and rotate. No DNS, no signing key, no federation reachability, no Ansible role, no ongoing service obligation.
- Risks worth weighing: a single rate limit shared across every Komai instance globally (Klipy's tiers would have to accommodate our scale); Klipy can revoke the key at will; their ToS may require "Powered by Klipy" attribution surfaces and takedown obligations we would have to honor; an open-source binary cannot meaningfully hide the key from extraction.

This is worth one outreach to Klipy and a careful read of their terms. If it works out, the per-user API-key field becomes optional rather than mandatory (overridable by power users on locked-down homeservers or those who want their own rate limit), and the "Not configured" empty state for Klipy disappears for everyone. If it does not, the per-user path is unchanged and we have lost nothing but an email.

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

1. **Klipy partner key.** Worth approaching Klipy for a partner relationship to ship a build-time key (Signal-style)? Are the risks acceptable -- one shared rate limit across all Komai users globally, key extractability from an open-source binary, ToS attribution and takedown obligations?
2. **AI generation.** AI fits into the picker as a distinct provider shape (slow, single-result, costs real money per request) and the proposed interface accommodates both this shape and the GIF-search shape. Open question is whether to pursue AI in parallel with GIF search or as a separate effort -- or, conversely, whether it should lead, since AI providers are unambiguously API-key-gated and do not carry the dying-service (Tenor) / deprecated-API / "depends on someone's proxy" risks that GIF providers currently do.
3. **Slash command name.** `/s` (covers all sources), `/gif` (matches the original issue's wording), or both?
4. **GIF size handling.** Accept that a 5 MB raw GIF gets uploaded as-is (current proposal), or transcode to MP4 client-side first to save 5-10x in storage at the cost of a video-player render?
5. **Branding.** Provider attribution in the picker is required by the providers' ToS. Are there other places (e.g. the message itself, a tooltip on the sticker) where attribution should also surface?

The previously-listed "default provider strategy" question is resolved: composer button always visible; unconfigured tabs show "Not configured. [Open Settings]", matching how voice transcription handles its unconfigured state. The "bundled community sticker packs" question is also dropped from the v1 scope -- the direction is to invest in GIF and AI sources rather than deepen manual pack curation.

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
- [maunium/stickerpicker](https://github.com/maunium/stickerpicker) (and its [giphyproxy](https://github.com/maunium/stickerpicker/tree/master/giphyproxy) federation media proxy)
- [giphytenormatrixproxy](https://github.com/pix/giphytenormatrixproxy)
- [mautrix-go/mediaproxy](https://github.com/mautrix/go/tree/master/mediaproxy) (the library both proxies above are built on)

See also: [QML/UI Structure](../qml-ui.md), [Storage Architecture](../storage.md).
