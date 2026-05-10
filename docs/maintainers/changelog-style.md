# 📝 Changelog style

This is the format every `CHANGELOG.md` section follows. The same text becomes the body of the GitHub Release, so it has to read for end users, not for someone scrolling `git log`.

For the mechanics (how to bump versions, tag, and publish), see [releases.md](releases.md). This document only covers the **shape and voice** of the entries you write into the new section that `just release-prepare` scaffolded.


## Categories

Each bullet starts with a category icon and one-word kind label:

| Prefix | Meaning |
|---|---|
| `✨ Feature:` | A capability the user didn't have before. |
| `🐛 Fix:` | Behaviour that was broken or wrong, and is now correct. |
| `🔧 Build:` | Build system, dependencies, CI, dev tooling. Visible to packagers and contributors, not to end users. |
| `📦 Flatpak:` / `📦 AUR:` / `📦 Snap:` / `📦 Windows:` | Per-format packaging changes (filenames, manifests, install paths, runtime extensions). Use the format name, not a generic "Packaging". |
| `🔒 Security:` | Security advisories, CVE-shaped fixes, dependency bumps that close a vulnerability. |

If a bullet doesn't fit any of these, it's probably too minor to include (see [What to leave out](#what-to-leave-out)).

### Feature vs Fix — the deciding question

Did the user **gain a capability they didn't have before** (Feature), or did **something they already had stop being broken** (Fix)?

When in doubt, look at the verb:

- "Adds X" / "now supports X" / "X is now available" → Feature
- "X works again" / "X no longer Y" / "X is no longer Z" / "X now actually has effect" → Fix


## Voice

- Write for end users, not commit-log readers. The reader is someone who runs Komai, not someone who reviewed the patch.
- Lead with the user-visible behaviour, not the implementation. "Spaces show a 'Space' badge in the header" — not "the SpaceBadge QML component is wired into RoomHeader.qml".
- Present tense, active voice. "Komai now does X." not "X has been added."
- Avoid em dashes (`—`) in user-facing text. Use a hyphen, colon, comma, or split into two sentences.
- Avoid wall-clock time estimates (e.g. "expect ~30 min", "on a decent connection"). They age poorly across hardware.
- Avoid mentioning internal abstractions (model class names, QML component names, Rust trait names) unless they are themselves the user-visible thing.
- Avoid bare Matrix event names (`m.room.message`, `m.marked_unread`) in feature/fix bullets. They belong in commit messages, not release notes. Exception: when the feature *is* about server-side persistence and the event name is the most precise name for what the user is enabling — then a short backtick-quoted reference is fine.


## Commit links

Every bullet ends with a parenthesised list of commit links:

```markdown
([abc12345](https://github.com/etkecc/komai/commit/abc12345))
```

For bullets that bundle multiple commits, list them comma-separated:

```markdown
([abc12345](https://github.com/etkecc/komai/commit/abc12345), [def67890](https://github.com/etkecc/komai/commit/def67890))
```

Use 9-character short hashes (the default `git log --oneline` format) — long enough to be unambiguous against the repo's history, short enough not to wrap.


## Linking to docs

When a bullet references a documented concept (a guide section, a feature page), link to it with a **commit-SHA permalink**, not `/blob/main/`:

```markdown
[room list](https://github.com/etkecc/komai/blob/<HEAD-SHA>/docs/user-guide/features/room-list.md#mark-as-unread)
```

Rationale: docs move (renames, splits, graduations from `research/`), and a permalink anchored to the SHA at release time keeps the link working for readers who land on an old release page months later.

Use the SHA of `HEAD` at the time the release commit is being prepared. (The release commit itself isn't created yet when you draft the changelog, so HEAD is the latest content commit.)


## Bullet granularity

- One commit can be one bullet, or a group of related commits can be one bullet — the unit is the **user-facing story**, not the patch boundary.
- If a feature shipped across 4 commits but tells one story ("Mark as unread, surfaced in two places, with sync"), it's one bullet listing all four hashes at the end.
- If two commits are independently interesting (a new feature **and** a fix to an old toggle that started gating it), they're two bullets — even if the commits touched the same area.
- Long bullets (more than ~3 sentences) are a smell. Split, or trim the technical justification.


## Headline paragraph for marquee releases

When a release has a single landmark story (first public release, first Windows binary, a major UI overhaul), open the section with a 🎉 paragraph **above** the bullet list:

```markdown
## 2026.05.10.0

🎉 **The first pre-built Windows binary of Komai!** Every tagged release now ships … ([abc12345](https://github.com/etkecc/komai/commit/abc12345)).

- ✨ Feature: …
- 🐛 Fix: …
```

The headline paragraph is plain prose (no leading dash), bold-leads the marquee point, and still ends with the relevant commit links. Subsequent bullets keep the normal format.

Don't force a headline — most releases don't have one. If the biggest item is just another `✨ Feature:` bullet, leave it as a bullet.


## What to leave out

Not every commit deserves a bullet. Skip:

- Cosmetic tweaks invisible to most users (centring a label, a one-pixel padding fix on a hover state).
- Internal cleanups with no user-visible impact (silencing a QML console warning, suppressing a CI build warning, refactoring an internal module).
- Documentation-only changes (README updates, research docs, internal architecture notes).
- Test-only changes.
- Repo-meta changes (issue templates, GitHub config, contributor-only files).

When in doubt: would a user notice if this commit hadn't shipped? If no, it's probably out.


## Worked example

Here's a real release section that follows this style:

```markdown
## 2026.05.10.0

🎉 **The first pre-built Windows binary of Komai!** Every tagged release now ships a Windows no-installer ZIP (`komai-<version>-windows-x64-no-installer.zip`) alongside the AppImage, Flatpak, and Snap. The build is unsigned, so SmartScreen warns on first launch ("More info" → "Run anyway") ([62ad50480](https://github.com/etkecc/komai/commit/62ad50480), [d644ad8fc](https://github.com/etkecc/komai/commit/d644ad8fc)).

- ✨ Feature: spaces are now first-class. A "Space" badge appears in the room header, and the composer is hidden in space rooms (since space messages don't surface on other clients) ([d80eedad6](https://github.com/etkecc/komai/commit/d80eedad6), [e28820c9b](https://github.com/etkecc/komai/commit/e28820c9b)).
- ✨ Feature: per-room override for "Show others when I'm typing" and "Show others when I've read their messages" in Room Info → Preferences ([a385946fa](https://github.com/etkecc/komai/commit/a385946fa), [708a65566](https://github.com/etkecc/komai/commit/708a65566)).
- 🐛 Fix: the "Show others when I've read their messages" toggle now actually has effect; it had been ignored before, so receipts went out regardless ([e7a815081](https://github.com/etkecc/komai/commit/e7a815081)).
- 🐛 Fix: Sign In gives up quickly when the homeserver is unreachable, instead of silently retrying for up to 15 minutes ([871086e6f](https://github.com/etkecc/komai/commit/871086e6f)).
- 🔧 Build: Komai no longer depends on OpenSSL, thanks to matrix-sdk 0.17.0. This simplifies packaging by removing a dependency ([5bbcbcf1c](https://github.com/etkecc/komai/commit/5bbcbcf1c), [7caacac4e](https://github.com/etkecc/komai/commit/7caacac4e)).
- 📦 Flatpak: the bundle filename now carries version + arch (`komai-<version>-<arch>.flatpak`), matching the AppImage / Snap / Windows ZIP naming ([c7fea0559](https://github.com/etkecc/komai/commit/c7fea0559)).
```


## Drafting workflow

1. Run `git log v<PREVIOUS_VERSION>..HEAD --no-merges` to enumerate the commits.
2. For each commit, decide: skip ([What to leave out](#what-to-leave-out)) or include?
3. For included commits, group related ones into single bullets and pick a category.
4. Draft each bullet with user-facing voice, ending in the commit-link parens.
5. If there's a marquee story, lift it into a headline paragraph.
6. Read the section once through as a user. Cut anything that reads like commit-log paste. Confirm the version-and-shape with the operator before committing.
