# 📎 Attachments

Send files, images, and audio in any room via the **paperclip button** in the composer, by **drag-and-drop**, or by **pasting** an image from the clipboard. The recipient sees a preview (with a blurhash placeholder while it loads) and can click through to the [media viewer](media-playback.md).


## 🛡️ Client-side image metadata stripping

Photos taken on phones routinely embed metadata that you don't see — GPS coordinates of where the picture was taken, camera make/model, capture timestamp, software version, and (for edited images) IPTC/XMP fields like caption, copyright, or keywords.

**Matrix homeservers store and serve uploaded files byte-for-byte.** Synapse, Dendrite, and others do not strip this metadata for you — once you upload a vacation photo with GPS coordinates, every recipient and the homeserver itself can see exactly where it was taken. The only place this metadata can be removed is on the sender's side, before the upload leaves the client.

Komai does this by default. The toggle lives in **Settings → Composer → Attachments → "Strip image metadata before upload"**.


### What gets stripped

For supported formats (JPEG, PNG, WebP):

- **EXIF** — GPS coordinates, camera make/model, lens, exposure, capture timestamp, software/firmware version, and Apple/Samsung/etc. `MakerNote` blobs that often contain extra device fingerprinting.
- **IPTC** — caption, copyright, keywords, byline, and the rest of the IPTC IIM dictionary, in any image that's been through Photoshop or a DAM workflow.
- **XMP** — Adobe-style XML metadata, including author, edit history, and rating.
- **PNG text chunks** (`tEXt`, `iTXt`, `zTXt`) — frequently carry XMP, software banners, and arbitrary user-supplied notes.
- **PNG `tIME`** — last-modified timestamp.
- **JPEG comments** (`COM` segments) — free-form text comments.


### What is kept

- **Image data** — never re-encoded; pixels go through unchanged.
- **ICC color profile** — kept for color accuracy (it's not personal data).
- **JFIF marker** in JPEGs — required for compatibility.
- **PNG `iCCP`, `pHYs`, `tRNS`, `gAMA`, `sRGB`, …** — color and rendering metadata, kept.
- **Animation chunks** — `acTL`/`fcTL`/`fdAT` for APNG, `ANIM`/`ANMF` for animated WebP, all kept; animation continues to work.


### Supported formats

| Format | Stripping | Notes |
| --- | --- | --- |
| **JPEG** (`.jpg`, `.jpeg`) | ✅ | EXIF, XMP, IPTC, COM segments removed |
| **PNG** (including APNG) | ✅ | EXIF and text chunks removed |
| **WebP** (still and animated) | ✅ | EXIF and XMP chunks removed |
| **HEIC / HEIF** | ❌ pass-through | Common iPhone format, ISO BMFF container — not yet supported |
| **AVIF** | ❌ pass-through | Same container family as HEIC |
| **GIF** | ❌ pass-through | Rarely carries EXIF; not supported |
| **SVG, TIFF, raw camera formats** | ❌ pass-through | Not supported |
| Other formats (PDF, video, audio, …) | n/a | Not images; never inspected |

For pass-through formats Komai uploads the original bytes unchanged, exactly as before this feature existed. The upload itself still succeeds; only the privacy-protection step is skipped.


### Orientation handling

Phone photos commonly carry an EXIF `Orientation` tag (e.g. "rotate 90° clockwise") rather than baking the rotation into pixels — image viewers rotate at display time. If we just stripped that tag, your portrait photo would arrive sideways or upside-down.

Komai handles this transparently: when an image's orientation tag is non-default, the rotation is **baked into the pixels** before stripping. For PNG and WebP this is lossless. For JPEG it requires re-encoding at quality 92 (the only place this feature is lossy — and only for the small subset of images that actually need rotation; un-rotated JPEGs go through the lossless path).

If keeping the original JPEG byte-for-byte matters more to you than auto-rotation, turn the toggle off and rotate the source file before sending.


### Avatars

Profile and room avatar uploads always have their metadata stripped, regardless of the toggle above. Avatars are public to every member of every room you join, so leaking GPS or camera metadata via your profile picture is never desirable.


### Best-effort by design

Stripping is best-effort: if a JPEG, PNG, or WebP file happens to be malformed or uses an unusual variant Komai's parser doesn't recognise, the upload silently falls back to the original bytes (logged at warning level). The reasoning: a one-time bug should not break image sending. The trade-off — that a malformed file might still leak metadata — is preferable to "image sends are broken until you ship a hotfix."

If you need a hard guarantee for a sensitive image, scrub it manually first (e.g. `exiftool -all= file.jpg`) and inspect the result before sending.


### What's *not* covered yet

- **HEIC, AVIF, and other ISO-BMFF-based formats** as listed above.

Tracked as follow-up work; if it matters to you, file a GitHub issue.


### How to disable

If you'd rather upload originals untouched (for example, you publish photographs and your IPTC copyright/byline matters):

**Settings → Composer → Attachments → "Strip image metadata before upload"** → off.

The change applies to subsequent uploads. Already-sent images are unaffected (the homeserver stored them as you sent them).


## 📤 Sending an attachment

- **Composer paperclip button** — opens a file picker.
- **Drag-and-drop** — drop one or more files onto the chat window.
- **Paste** — paste an image directly from the clipboard (e.g. a screenshot).
- A caption typed in the composer is sent alongside the file as the message body.
