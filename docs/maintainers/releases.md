# 🚀 Releases

Komai uses [CalVer](https://calver.org/) versions of the form `YYYY.MM.DD.N`, where `N` is a counter that resets to `0` each day. Releases are cut from `main` and produced fully automatically by [`publish.yml`](../../.github/workflows/publish.yml) — once a `v*` tag is pushed and CI is green for it, AppImage / Flatpak / Snap artefacts are built and attached to a GitHub Release whose body is the matching CHANGELOG section.

For the CI pipeline shape and cache strategy, see [CI Pipeline](../architecture/ci.md).


## Version-bearing files

`VERSION` is the source of truth. Three other files have to agree with it; the `version-drift` pre-commit hook will refuse a commit where any of them disagree.

| File | What must match |
|---|---|
| `VERSION` | the version itself (e.g. `2026.05.05.0`) |
| `etc/packaging/archlinux/PKGBUILD` | `pkgver=<VERSION>` |
| `resources/komai.appdata.xml.in` | a `<release version="<VERSION>" .../>` entry |
| `CHANGELOG.md` | a top-level `## <VERSION>` section |

`just release-prepare` updates all four in one shot — never edit them by hand.


## Cutting a release

### 1. Pick the commit to release

Releases are cut from `main`. Either:

- finish the work you want included, push it to `main`, and release from there; or
- pick an existing `main` commit you already trust (typically the latest).

Either way: **wait for CI to be green for that commit** before tagging. The `publish.yml` workflow only fires on a successful `ci.yml` run for the tagged ref, so a failing CI = no artefacts. Check [the Actions tab](https://github.com/etkecc/komai/actions/workflows/ci.yml) for the run on that SHA.

### 2. Run `just release-prepare`

From a clean working tree on `main`:

```sh
just release-prepare           # auto-compute next version from today's UTC date
just release-prepare 2026.05.05.0   # or pass an explicit CalVer
```

The auto-computed version is `<today-UTC>.0`, or `<today-UTC>.<N+1>` if a release was already cut today.

This edits `VERSION`, `PKGBUILD`, `appdata.xml.in`, and inserts a new `## <VERSION>` section at the top of `CHANGELOG.md` containing a `<!-- TODO: fill in release notes -->` placeholder. It then prints the next steps.

### 3. Write the changelog entry

Open `CHANGELOG.md` and replace the TODO placeholder with the actual user-facing notes for this release. This text becomes the body of the GitHub Release — write it for end users, not commit-log readers.

### 4. Review, commit, tag, push

```sh
git diff
git commit -am 'Release v2026.05.05.0'
git tag v2026.05.05.0
git push && git push --tags
```

The drift hook runs on `git commit` and will block the commit if anything diverged.

### 5. Watch the publish run

Pushing the tag triggers `ci.yml` again on the tag ref. Once that goes green, [`publish.yml`](https://github.com/etkecc/komai/actions/workflows/publish.yml) fires automatically and runs four jobs serially on the self-hosted runner:

- `build-appimage` (~25–30 min cold)
- `build-flatpak` (~40 min cold, ~7–8 min on a warm state-dir)
- `build-snap` (~30 min cold)
- `release` — creates the GH Release, attaches all three artefacts, uses the matching CHANGELOG section as the release body.

Total wall time for a cold release is roughly 1h 40m. Most of that is the packaging jobs themselves; the release-creation step itself is ~3.5 min.

### 6. Verify

When `publish.yml` finishes, check the [Releases page](https://github.com/etkecc/komai/releases) — the new release should be there with three artefacts attached and the CHANGELOG section as its body. As a smoke test, download the AppImage and run it.


## Re-releasing after a bad release

**Don't reuse tags.** External caches (AUR, downloaded AppImages, mirror sites) will not pick up a moved tag. Instead, bump the counter and release again on the same day:

```sh
just release-prepare 2026.05.05.1
```

If the bad GH Release is publicly visible, delete it first so users don't pick up the broken artefacts.


## Pre-release smoke testing

[`packaging-check.yml`](../../.github/workflows/packaging-check.yml) is a `workflow_dispatch`-only workflow that builds any one of the three formats from a chosen branch — useful for testing packaging-recipe changes before tagging. It uses the same recipes as `publish.yml`, so a green check there is a strong signal that publish will succeed.

Locally, the equivalent recipes are `just flatpak-build`, `just appimage-build-docker`, and `just snap-build-docker` (each has a `*-native` variant too — see the [packaging guides](packaging/README.md)). Flatpak and Snap additionally expose `-install` / `-run` recipes for end-to-end smoke testing; for AppImage, just execute the produced `.AppImage` directly.


## Out of scope (manual)

These are not part of `publish.yml` and have to be done by hand:

- **AUR** — the `PKGBUILD` in this repo is the upstream copy; after a release, copy it (and the regenerated `.SRCINFO`) to `aur.archlinux.org:komai.git`.
- **Flathub** — Flathub hosts its own per-app manifest repo; submitting/updating Komai there is a separate PR against `flathub/flathub`.
- **Snap Store** — requires snapcraft login and a configured track.
