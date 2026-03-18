# Binary Size Notes

This note explains why a local build-tree binary such as `var/build/native/komai` is noticeably larger than a distro-packaged binary such as `/usr/bin/nheko`.

Komai is a real fork of Nheko, so comparing binary size against Nheko is a reasonable maintenance check. The point of the comparison is not to match Nheko at all costs. It is to detect accidental regressions, understand where Komai adds real payload, and separate expected fork-specific costs from build or packaging mistakes.

Komai also intentionally diverges from Nheko in behavior, UI, and bundled functionality. See the user-facing overview in [`docs/user-guide/differences-from-nheko.md`](../user-guide/differences-from-nheko.md). That broader scope is one valid reason the final executable can end up larger even when the build is healthy.

## Short version

- `var/build/native/komai` is a local development artifact, not a package-shaped install image.
- The default native build is configured as `Release`, but the resulting executable is not stripped.
- Packaged binaries are typically stripped and may move debug info into separate debug packages.
- Komai also embeds a substantial amount of QML cache and Qt resources into the executable, so even a stripped build is still materially larger than some packaged upstream binaries.
- Comparing against Nheko is still useful because it helps identify unexpected growth in the fork relative to its upstream baseline.

## What the native build does

The default native build entry point is [`justfile`](../../justfile):

- `just configure` uses `-DCMAKE_BUILD_TYPE=Release`
- `just build` builds in `var/build/native/`

That means a large local binary does not automatically imply a `Debug` build.

## Why the local binary looks large

Three effects matter most.

### 1. The build-tree binary is not stripped

Local `var/build/native/komai` is expected to keep symbol tables unless a separate strip/install step removes them. This alone can add tens of megabytes compared to a packaged executable.

In one local measurement:

- unstripped `Release` build: about `52 MiB`
- stripped copy of the same binary: about `28 MiB`

So the first comparison to make is always:

- local build-tree binary vs stripped local copy

not:

- local build-tree binary vs packaged distro executable

### 2. Komai embeds QML cache and Qt resources

Komai intentionally bundles QML and generated runtime data into the executable through CMake resource integration.

Relevant build paths:

- QML module embedding via [`CMakeLists.txt`](../../CMakeLists.txt)
- emoji runtime resource embedding via [`CMakeLists.txt`](../../CMakeLists.txt)

In one local `MinSizeRel + LTO` measurement, the final executable still contained roughly:

- `~7.3 MiB` of compiled QML cache payload
- `~4.9 MiB` of Qt resource payload

The most significant resource contributor was generated emoji runtime data. The source-side generated JSON set was much larger again, but Qt resource compression reduced its footprint in the final executable.

### 3. Packaged builds often use better size-oriented toolchain flags

Distro builds commonly inherit packaging flags that local developer builds do not. On Arch, for example, package builds typically include linker/hardening settings such as:

- `-Wl,--as-needed`
- `-Wl,-z,now`
- `-Wl,-z,pack-relative-relocs`
- LTO via `-flto=auto`

Those flags can reduce relocation overhead and generally produce a more package-optimized binary than a plain local CMake `Release` build.

## What is normal

The following is normal and not by itself a bug:

- `var/build/native/komai` is much larger than `/usr/bin/nheko`
- local `Release` builds are larger than packaged installs
- stripped Komai is still larger than stripped nheko because Komai carries more embedded runtime payload

## What to compare instead

If you want an apples-to-apples comparison, compare one of these pairs:

- stripped local Komai vs packaged nheko
- packaged Komai vs packaged nheko
- local Komai built with distro packaging flags vs packaged nheko

Avoid comparing:

- unstripped `var/build/native/komai`

against:

- stripped `/usr/bin/nheko`

## Why compare against Nheko at all

Nheko is the upstream application Komai forked from, so it is the most relevant baseline when investigating whether Komai has introduced avoidable binary growth.

This comparison is useful for:

- spotting regressions after adding new features or generated assets
- checking whether build or packaging settings drifted in a way that bloats the executable
- distinguishing intentional Komai-specific payload from accidental overhead

This comparison is less useful for:

- demanding exact parity despite different features, resources, and packaging choices
- judging a local build-tree binary against a distro-installed package without normalizing for stripping and toolchain flags

## When to investigate further

Binary size is worth a deeper investigation if:

- package size becomes a distribution problem
- startup or memory behavior suggests resource over-embedding
- emoji locale data or other generated assets grow significantly
- QML/resource additions introduce unexpected large jumps in executable size
- growth cannot be explained by intentional fork differences documented in [`docs/user-guide/differences-from-nheko.md`](../user-guide/differences-from-nheko.md)

The highest-leverage areas are usually:

- generated emoji runtime data
- compiled QML cache payload
- package/linker flags used outside the native developer build
