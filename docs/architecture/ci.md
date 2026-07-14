# 🚦 CI Pipeline

This page documents how Komai's GitHub Actions pipeline is structured,
why the caches are shaped the way they are, and what to do if you need
to extend or debug it. The actual workflow files live in
[`.github/workflows/`](../../.github/workflows/) and contain inline
comments matching the rationale here.

## Short version

- Three workflows: **`ci.yml`** runs on every push/PR (lint + build +
  tests), **`publish.yml`** runs on release tags and produces AppImage
  / Flatpak / Snap artefacts, **`ci-image.yml`** rebuilds the
  preinstalled-deps container image used by both.
- Komai is a Rust + C++ + Qt project, so an unwarmed CI run does
  ~1000-crate Rust dep compilation + a large C++ build + linking
  several binaries. Without caching this takes 20+ minutes per push;
  with the current caching it lands in single-digit minutes for typical
  source changes.
- Cache layers, in order of impact: **cargo target dir** (Rust dep
  compile artefacts), **ccache** (C/C++ object cache), then several
  smaller caches (rustup toolchains, cargo registry, mise/prek tools).
- The cargo target dir caching has a **non-obvious gotcha** with
  Corrosion (the CMake ↔ cargo bridge) — see
  [The Corrosion target-dir gotcha](#the-corrosion-target-dir-gotcha)
  below before touching it.
- Workflows are configured for self-hosted Linux runners but are
  written portably — they fall back to GitHub-hosted runner behaviour
  (longer total runtime, same correctness) where the optional
  host-mounted cache disk isn't present.

## Workflows

### `ci.yml` — every push, every PR

One `build-and-lint` job that:

1. Checks out the repo.
2. Restores caches (rustup, cargo registry, ccache, cargo target,
   mise+prek).
3. Configures CMake.
4. Builds (the main `komai` binary + Corrosion-driven Rust crates).
5. Runs prek pre-commit hooks (lint).
6. Builds and runs C++ tests (CTest).
7. Runs Rust unit tests (`cargo test --lib`).

Steps are intentionally split rather than batched into a single
`prek-run-on-all` invocation, so the GitHub Actions UI shows
per-phase timing (Build vs lint vs Rust tests vs C++ tests) — this
is what made the cache-strategy investigations possible.

### `publish.yml` — release tags only (`v*`)

Three packaging jobs (AppImage, Flatpak, Snap) plus a release job that
attaches the artefacts to a GitHub Release. Triggered via
`workflow_run` on a successful `ci.yml` run for a `v*` tag.

These are independent of `ci.yml`'s caches because each packaging
toolchain (flatpak-builder, linuxdeployqt, snapcraft) has its own
build sandbox and dependency-fetch logic. Caching strategies for
these are evolving — see [Outstanding work](#outstanding-work).

### `ci-image.yml` — preinstalled-deps container

Builds and pushes `ghcr.io/etkecc/komai/ci:latest`, an Arch-based
container with every pacman build dep installed (Qt, GStreamer,
mold, ccache, etc.) plus a non-root `ci` user whose UID matches the
runner's host user. Triggered on changes to `etc/ci/Dockerfile`.

Both `ci.yml` and `publish.yml` use this image, so a fresh build
doesn't need to reinstall hundreds of packages on every CI run.

## Cache strategy

### Why we cache

Komai builds:

- ~1000 Rust crate dependencies (transitive matrix-sdk + ruma + tokio
  + rustls + …) — cold compile is several minutes even on fast
  hardware.
- The Komai C++ codebase plus generated Qt artifacts (moc, uic, rcc)
  — cold compile is several more minutes.
- Test binaries (Rust `cargo test --lib` + C++ ctest binaries).
- Linking of `komai`, `komai_tests`, and per-test executables (mold
  helps here, see [Why mold](#why-mold)).

A cold CI run lands in the 20-25 minute range. With the current
caching it's typically single-digit minutes for changes that don't
touch dependencies.

### What we cache

| Cache | Source path | Why |
|---|---|---|
| **rustup toolchains** | `var/cache/rustup` | Avoids re-downloading the pinned Rust toolchain on every run. |
| **cargo registry + git db** | `var/cache/cargo-home/registry/{index,cache}` + `git/db` | Avoids re-fetching crate index and git-pinned dep snapshots. |
| **ccache** | `var/cache/ccache` (or host-mounted) | Per-source-file C/C++ object cache, keyed by source + flags hash. |
| **cargo target dir** | `var/build/native/cargo` (or host-mounted) | Rust crate compile artefacts (`.rlib`, `.rmeta`, fingerprints). |
| **mise + prek** | `var/mise`, `var/prek` | Tool managers (mise) and the prek-hook environment. |

### The Corrosion target-dir gotcha

Komai uses [Corrosion](https://github.com/corrosion-rs/corrosion) to
integrate cargo into the CMake build. Corrosion **explicitly passes
`--target-dir <path>` on its cargo CLI invocation** (see
`Corrosion.cmake _add_cargo_build`, around line 898 in v0.6.1 and
also in upstream master as of 2026-04). The `<path>` is computed at
CMake-configure time as:

    ${CMAKE_BINARY_DIR}/cargo/${folder}_${sha1_abs_manifest_path:0:5}

…where `folder` is the manifest's parent dir name (`rust` for
`src/rust/Cargo.toml`) and the SHA1 input is the *absolute* manifest
path. **This CLI flag overrides the `CARGO_TARGET_DIR` environment
variable.** As of upstream master there's no public knob to redirect
Corrosion's chosen path.

The practical consequence: setting `CARGO_TARGET_DIR=/wherever` in a
shell env, then running `just build`, makes Corrosion ignore the env
var and write to its own computed path. Direct `cargo test --lib`
invocations (which honor the env) end up writing to a *different*
target dir, and any cache that targets either path sees only half
the picture.

The CI workflow handles this by **replicating Corrosion's path
computation** in a workflow step ("Compute unified cargo target
dir") and exporting `CARGO_TARGET_DIR` to match exactly. Both
writers — Corrosion's cargo and direct `cargo test --lib` — then
converge on the same physical tree, and one cache captures both.

The replication uses `printf '%s' "${MANIFEST}" | sha1sum | cut -c1-5`,
which produces the same result as CMake's `string(SHA1 …)` because
both hash the bytes of the input string identically.

If Corrosion ever exposes a public knob for the target dir, this
replication can be removed in favour of just setting that knob. The
[upstream issue / PR around `_add_cargo_build`](https://github.com/corrosion-rs/corrosion/blob/master/cmake/Corrosion.cmake)
has a "todo: use --target-dir to potentially reuse artifacts"
comment as a future direction.

### The cargo-target lock-hash key

When the workflow falls back to `actions/cache` for the cargo target
dir (i.e., when no host-mounted cache disk is available), the cache
key is:

    cargo-target-v2-${{ hashFiles('src/rust/Cargo.lock', 'rust-toolchain.toml') }}

Notably — it does **not** include the commit SHA. Rationale:

- Cargo fingerprints for the ~1000 dep crates are determined by
  resolved dep versions (Cargo.lock) and the rustc version
  (rust-toolchain.toml). They do **not** depend on Komai's own source
  code. Only Komai's own crate(s) rebuild when our source changes,
  and that's cheap.
- A re-saved tarball on an unchanged-lock commit would be nearly
  identical to the existing cache entry — wasting bandwidth and
  upload time on every CI run.
- With the lock-hash key, `actions/cache` detects that the primary
  key already exists and **skips the save step entirely** on the
  common case. The save cost is paid only on lock-change commits
  (Renovate dep bumps, manual upgrades).

GitHub evicts cache entries after 7 days of non-access, but **a
successful restore counts as access** and resets the clock — so the
entry stays alive as long as CI is running at all. Only a >7-day
silence forces a cold rebuild on the next run.

The `v2-` prefix is a manual version gate. **Bump it to `v3-` if
anything in the workflow changes the rustc/cargo command line in a
fingerprint-affecting way** (RUSTFLAGS, linker choice, profile
tweaks, etc.) — otherwise we'd restore stale entries built with
different flags.

The cascade fallback is a single `cargo-target-v2-` entry: any
previous v2 cache → warm starting tree, partial rebuild for changed
crates only, dramatically cheaper than starting cold.

### Host-mounted cache (optional fast path)

`actions/cache` works by tar+zstd-ing the cached path, uploading it to
GitHub's cache backend, and on next run downloading + extracting it.
On a self-hosted runner with limited bandwidth to GitHub's backend,
this round-trip becomes the dominant CI cost — easily tens of
minutes for a multi-GB cargo target tree.

If the runner provides a persistent host-mounted directory at
`/var/lib/komai-ci`, the CI workflow:

1. Detects the mount by checking for a `cargo-target/` subdir
   (which the runner host pre-creates with permissive write perms,
   not Docker auto-creating an empty dir as a side effect of the
   bind-mount).
2. Symlinks `var/build/native/cargo` to
   `/var/lib/komai-ci/cargo-target` so Corrosion's computed path
   resolves through the symlink onto the persistent disk.
3. Likewise for ccache: tries `mkdir -p /var/lib/komai-ci/ccache`
   and points `CCACHE_DIR` at it.
4. Skips the corresponding `actions/cache` steps entirely
   (`if: env.HOST_CACHE != 'true'` for cargo target,
   `if: env.CCACHE_HOST_OK != 'true'` for ccache — the separate
   flag is intentional, so ccache can fall back to actions/cache
   independently if its mkdir fails).

This is a structural coupling to runners that provide the mount. The
workflow stays portable: if the mount is absent (GitHub-hosted
runners or any self-hosted runner without the host disk), all cache
steps run as `actions/cache` and the workflow behaves correctly,
just slower.

The host-mount path is intentionally world-writable so future cache
types (e.g., flatpak/appimage/snap state on `publish.yml`) can be
added entirely from the workflow side without coordinating runner-
host changes.

Because the host mount never expires, it can outlive the weekly CI
image refresh while holding artifacts the new image's toolchain can
no longer safely link. Cargo does not track the system C toolchain
as a rebuild input: `cc`-crate build scripts cache `.o` files
compiled by the previous image's gcc, and cargo reuses them verbatim
under the new one. The `Invalidate host-mounted cargo target on
toolchain change` step in `ci.yml` guards this by fingerprinting the
toolchain packages (`pacman -Q gcc glibc binutils mold`) into
`.toolchain-stamp` inside the cargo-target dir and wiping the tree
on mismatch, at the cost of one cold Rust build after each toolchain
bump. ccache needs no such guard: it keys on the compiler binary
itself.

## Why mold

The Rust toolchain, Corrosion, and the C++ build all use
[mold](https://github.com/rui314/mold) as the linker:

- **Rust side**: `CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_RUSTFLAGS:
  "-C link-arg=-fuse-ld=mold"`. Local A/B benchmarks: incremental
  rebuild 3.37s → 2.93s (-13%), peak link RSS 3.3 GB → 1.0 GB.
  RSS reduction is meaningful on smaller runners.
- **C++ side**: CMake configured with `-DCMAKE_LINKER_TYPE=MOLD`.
  Local A/B for the `komai` binary link: 4.90s → 1.73s (-65%), peak
  RSS 1.2 GB → 108 MB.

The rustflags are scoped to the target triple (not the host triple)
deliberately: host-build deps (proc-macros, build.rs scripts) are
many small/fast links where mold's setup cost is wasted.

## Why ccache PCH sloppiness

ccache refuses to cache any compile that uses precompiled headers
unless `CCACHE_SLOPPINESS` is set. Komai's main `komai` target uses
`target_precompile_headers`, which is roughly 85% of the build by
volume. The configured sloppiness is:

    pch_defines,time_macros,include_file_mtime,include_file_ctime

These tell ccache that PCH define hashes, `__TIME__`/`__DATE__`
macros, and header mtimes/ctimes can vary without changing the
actual compiler output. This is the standard ccache-with-PCH
configuration; without it, ccache hit rate on the main build drops
to near zero.

## Performance characteristics

Approximate timings on a healthy warm cache (numbers will drift over
time as deps grow and hardware varies — these are directional, not
contractual):

- **Pure-translation push** (no source/lock changes): single-digit
  minutes total. Build collapses to a near-no-op (`Finished` reported
  in seconds), Rust tests are a fingerprint hit.
- **Single-file source change**: a few minutes longer because the
  affected crate (and its rdeps) recompile. Linking is fast (mold).
- **Cargo.lock change** (Renovate bump, manual upgrade): partial
  rebuild of just the changed dep crates — much cheaper than a cold
  build. The cache save step does run on these (because the
  primary cache key changed), so total wall time is somewhat higher.
- **Cold cache** (rare — eviction after 7-day silence, or a v-prefix
  bump): 20+ minutes. Bootstraps the cache for subsequent runs.

The single-largest contributor to total wall time after Rust+ccache
caches are warm is typically link time and the cache transfer phase
itself (when `actions/cache` is in use).

## Extending the workflow

### Adding a new cache

If you want to cache a new directory (e.g., a new tool's state):

1. **Decide the key strategy**:
   - **Content-hash key** (`hashFiles('foo.lock')`) if the cache is
     fully determined by some pinned input. Saves are skipped on
     unchanged-input commits.
   - **Per-commit key** (`-${{ github.sha }}` suffix) if the cache
     evolves on every run and stale fallbacks are still useful via
     `restore-keys`. More save bandwidth but always-current cache.
2. **Write the cache step** with both `key` and `restore-keys`.
3. **If the cache is large and the runner supports host-mounting**:
   add a wiring step similar to `Wire host-mounted ccache` and gate
   the `actions/cache` step with an `if:` that checks a per-cache
   flag (not the global `HOST_CACHE` — keep flags per-cache so each
   can independently fall back to actions/cache when the host-mount
   isn't writable for some reason).

### Bumping the cargo-target cache version

If you change something that affects rustc fingerprints (RUSTFLAGS,
linker choice, build profile tweaks, target-cpu, etc.), bump
`cargo-target-v2-` to `cargo-target-v3-` in both the `key` and
`restore-keys` of the relevant cache step. Old-version entries will
age out automatically via the 7-day TTL.

### Changing the Corrosion target-dir replication

If the upstream Corrosion path-computation algorithm ever changes
(currently `${CMAKE_BINARY_DIR}/cargo/${folder}_${sha1[:5]}`), update
the "Compute unified cargo target dir" step to match. The local
formula is documented inline in that step's comment.

If Corrosion ever exposes a public knob to redirect its target dir,
prefer using that knob over our replication.

## Troubleshooting

### "Permission denied" creating a cache subdir

If the host-mount is provisioned but a `mkdir` inside
`/var/lib/komai-ci/<subdir>` fails with `Permission denied`, the
parent dir's perms are too tight. The mount root needs to be
world-writable so the non-root container user can create new
subdirs without runner-host changes for every new cache type.

The CI workflow degrades gracefully on this: the affected cache
falls back to `actions/cache` rather than failing the build.

### `Cache cargo target dir: 0s (skipped)` even though I expected it to run

The `if:` gate (`env.HOST_CACHE != 'true'`) skips the actions/cache
step when the host-mount is detected. Look for the
`Compute unified cargo target dir` step output — it logs
`Host-mount active: …` or `No host-mount detected; falling back …`.

### Build is slow even though "everything's cached"

A few things can cause this:

- **Lock or toolchain change** — the cargo-target cache primary key
  is content-hashed on `Cargo.lock` + `rust-toolchain.toml`. Either
  changing forces a partial rebuild of dep crates.
- **A v-prefix bump in the cache key** — old caches won't match,
  next run is cold, the run after is warm.
- **>7 days of CI silence** — cache entries evicted, next run cold.
- **Corrosion path replication is out of sync** — if you see Build
  reading "fresh" for *zero* dep crates and recompiling everything,
  the replicated path may have drifted from what Corrosion actually
  computes; the cache is hitting a different directory.

### How to confirm both host-mount caches are active

Look for these two lines in the cache step list:

- `Cache cargo target dir: 0s (skipped)` ← Phase A active
- `Cache ccache: 0s (skipped)` ← Phase B active

And these two lines should be **absent** (skipped main step → no
post step generated):

- `Post Cache cargo target dir`
- `Post Cache ccache`

## Outstanding work

The Flatpak job in `publish.yml` uses the same host-mount-with-
fallback pattern as `ci.yml`: the wire step checks for a writable
`/var/lib/komai-ci`, symlinks the flatpak-builder state dir into
it when present, and gates the `actions/cache` step on a
`FLATPAK_HOST_OK` flag. Since the job runs directly on the host
(no `container:` block), the symlink works without any volume
bind-mount.

The AppImage and Snap jobs still build from cold on every release.
Both use Docker (`just appimage-build-docker`,
`just snap-build-docker`), so threading host-mounted caches
through to the docker invocation is more invasive than the Flatpak
case — the `just` recipes themselves need to know about the
mount path.

### Picking up the AppImage + Snap caching work

The pattern to follow:

1. **Investigate what each recipe actually downloads/builds**:
   - `bin/release/appimage-build-docker.sh` (or wherever the AppImage
     recipe lives) — look for `linuxdeployqt` downloads, Qt plugin
     staging, and any `cargo` / `ccache` invocations inside the
     container.
   - The Snap recipe (snapcraft) — look for the parts cache (typically
     under `~/.cache/snapcraft` or `parts/` in the project) and any
     vendored crate / system-package fetches.
2. **Decide what's worth caching**: focus on big downloads (Qt
     bundles, compiled deps) and on caches that snapcraft/AppImage
     already maintain internally — those just need to be mounted to a
     host path instead of an ephemeral container volume.
3. **Modify the `just` recipe** to bind-mount a host path into the
     container. The recipe should accept a "cache root" environment
     variable (default to a workspace path) and pass `-v $CACHE_ROOT/
     appimage:/some/in-container/path` to `docker run`. The CI
     workflow side then sets that variable to `/var/lib/komai-ci/
     appimage-cache` (or similar) when the host-mount is detected.
4. **Wire the workflow** with the same flag-and-fallback pattern as
     Phase C: `WIRE_HOST_X` step that mkdirs and sets `X_HOST_OK`,
     plus an `if:`-gated `actions/cache` fallback (if/when one is
     even worth adding for these jobs).
5. **Measure cold-vs-warm** by running the same release twice — once
     with the host-cache wiped (or a fresh subdir) for the cold
     baseline, once with the warm cache. The publish workflow only
     fires on `v*` tags, so do this in proximity to a planned release
     where you can observe both runs back-to-back.

The high-level sketch above intentionally leaves the recipe-side
work concrete-but-unspecified — the right mount points and env
variable names depend on what the docker recipes look like at the
time you do this.
