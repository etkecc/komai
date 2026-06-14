# Element Call bundle 📞

Komai embeds the [Element Call](https://github.com/element-hq/element-call)
"embedded" web bundle and serves it to a QtWebEngine view over the secure
`komai-ec://` scheme (see `src/voip/ElementCallWebProfile.{h,cpp}`).

The bundle is pinned in [`sources.lock.yml`](./sources.lock.yml) and fetched by
[`fetch.py`](./fetch.py) into `var/element-call/<version>/dist/`, which CMake
embeds via `qt_add_resources` (gated on the `ELEMENT_CALL` option). The bundle
ends up compiled into the binary, so only the build needs it present.

## Commands

- `just element-call-fetch` — download/unpack the pinned bundle into
  `var/element-call/<version>/` (CMake also runs this at configure time; run it
  explicitly to pre-populate for offline packaging builds).

## Updating the pin

Renovate proposes `version` bumps (npm `@element-hq/element-call-embedded`).
When bumping, also update `sha256` to the new tarball's hash — `fetch.py`
verifies it and prints the correct hash on mismatch.

## Upstream

- https://github.com/element-hq/element-call
- npm: https://www.npmjs.com/package/@element-hq/element-call-embedded
