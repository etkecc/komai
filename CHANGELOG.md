# Changelog

## 2026.07.15.0

- ✨ Feature: a new **Auto** [theme mode](https://github.com/etkecc/komai/blob/77d271ec9b5e85f36728d043affb40989058237b/docs/user-guide/features/themes.md) follows your desktop's light/dark preference, flipping live within your chosen theme family; new profiles default to it ([58c7299ef](https://github.com/etkecc/komai/commit/58c7299ef), [84b639560](https://github.com/etkecc/komai/commit/84b639560)).
- 🐛 Fix: clicking a `matrix:` message permalink now jumps to the linked message, loading older history as needed; previously it silently did nothing ([93f5a322d](https://github.com/etkecc/komai/commit/93f5a322d)).
- 🐛 Fix: a `matrix:` permalink to a reply in a collapsed thread opens the thread view on that reply ([93f5a322d](https://github.com/etkecc/komai/commit/93f5a322d)).
- 🐛 Fix: the message a permalink jumps to is highlighted and stays in view while the timeline settles, instead of drifting off screen ([2b6cfaac6](https://github.com/etkecc/komai/commit/2b6cfaac6), [5873bd4f4](https://github.com/etkecc/komai/commit/5873bd4f4)).
- 🐛 Fix: a Matrix library error that dumped the entire timeline into one log line no longer floods the terminal; a single log event is capped at 16 KiB (`KOMAI_LOG_MAX_EVENT_BYTES` overrides, `0` disables) ([77d271ec9](https://github.com/etkecc/komai/commit/77d271ec9)).
- 🎨 Polish: Komai's homepage is now [komai.chat](https://komai.chat); the About tab shows it in its header, and homepage links across the app, docs, and packaging metadata point there ([ef4e2dc33](https://github.com/etkecc/komai/commit/ef4e2dc33), [e7bbbf069](https://github.com/etkecc/komai/commit/e7bbbf069)).
- 📦 Flatpak: the bundled GStreamer is updated to v1.28.5 ([34f77eb25](https://github.com/etkecc/komai/commit/34f77eb25)).
- 📦 AUR: the PKGBUILD drops its `/shrug` man-page workaround, obsolete since the upstream fix shipped in v2026.07.07.1 ([844119ce5](https://github.com/etkecc/komai/commit/844119ce5)).

## 2026.07.07.1

- 🐛 Fix: filtering the timeline with a search query no longer leaves messages overlapping each other ([d377973c8](https://github.com/etkecc/komai/commit/d377973c8)).
- 📦 Calls: the bundled Element Call is updated to v0.21.0 ([09b76071b](https://github.com/etkecc/komai/commit/09b76071b)).
- 🔧 Build: the bundled qtkeychain is updated to v0.17.0, keeping Komai's existing Windows credential naming so stored sign-ins keep working ([317f1a3d3](https://github.com/etkecc/komai/commit/317f1a3d3), [0f72f7daf](https://github.com/etkecc/komai/commit/0f72f7daf), [80636f66f](https://github.com/etkecc/komai/commit/80636f66f)).

_(Re-cut of v2026.07.07.0, whose publish failed on GitHub rate limiting while downloading emoji data during the packaging builds; the pipeline now fetches those sources robustly ([e46281b3c](https://github.com/etkecc/komai/commit/e46281b3c)). No other application changes from that tag.)_

## 2026.07.02.2

- ✨ Feature: double-clicking a word and dragging now extends the message text selection word by word ([2946a34d8](https://github.com/etkecc/komai/commit/2946a34d8)).
- ✨ Feature: large media downloads in the media viewer show a progress ring with a live percentage instead of an indeterminate spinner ([077a924f1](https://github.com/etkecc/komai/commit/077a924f1), [fa966131e](https://github.com/etkecc/komai/commit/fa966131e)).
- 🐛 Fix: videos in the media viewer play reliably; on homeservers without HTTP range support (e.g. Synapse) they could previously sit on their thumbnail forever, and audio now starts playing while it downloads ([a249e305d](https://github.com/etkecc/komai/commit/a249e305d), [bb1308457](https://github.com/etkecc/komai/commit/bb1308457)).
- 🐛 Fix: wide-gamut photos (e.g. iPhone Display P3) no longer look desaturated; embedded color profiles are now applied when decoding ([b8b4c438d](https://github.com/etkecc/komai/commit/b8b4c438d)).
- 🐛 Fix: saving an attachment via "Save as" no longer crashes the app ([a1a45e504](https://github.com/etkecc/komai/commit/a1a45e504)).
- 🐛 Fix: the Linux AppImage no longer crashes when opening the microphone for speech-to-text voice input ([4d359399f](https://github.com/etkecc/komai/commit/4d359399f)).
- 🎨 Polish: media viewer images are crisp now, with high-quality downscaling at rest and sharp zooming ([9373831fd](https://github.com/etkecc/komai/commit/9373831fd), [3846a7d02](https://github.com/etkecc/komai/commit/3846a7d02), [9dc56c91a](https://github.com/etkecc/komai/commit/9dc56c91a)).
- 🎨 Polish: the media viewer opens images faster, and stepping through a gallery with prev/next is instant thanks to caching and prefetching ([7ecaad4ca](https://github.com/etkecc/komai/commit/7ecaad4ca), [1273ad1d5](https://github.com/etkecc/komai/commit/1273ad1d5), [f7dd0b998](https://github.com/etkecc/komai/commit/f7dd0b998), [78b5d7904](https://github.com/etkecc/komai/commit/78b5d7904)).
- 🎨 Polish: the transcription status banner reveals its full text on hover when it is clipped ([37b0b677a](https://github.com/etkecc/komai/commit/37b0b677a)).
- 📦 Calls: the bundled Element Call is updated to v0.20.3 ([7f2b0b5d9](https://github.com/etkecc/komai/commit/7f2b0b5d9)).

_(Re-cut of v2026.07.02.0 and v2026.07.02.1, whose publishes failed on packaging issues; fixed by moving the Flatpak's bundled GStreamer to the stable 1.28 series ([8b92301c4](https://github.com/etkecc/komai/commit/8b92301c4), [92dc6f5d2](https://github.com/etkecc/komai/commit/92dc6f5d2)) and correcting a Rust-bridge declaration mismatch that broke the Windows, macOS, and Snap builds ([40f6bbc98](https://github.com/etkecc/komai/commit/40f6bbc98)). No other application changes from those tags.)_

## 2026.06.28.0

- 🐛 Fix: messages no longer occasionally wrap a trailing emoji (or clip the last character) onto a new line ([baa5cfebd](https://github.com/etkecc/komai/commit/baa5cfebd)).
- 🐛 Fix: scrolling no longer occasionally hijacks the message selection onto a different message as timeline rows are recycled ([c714cd94c](https://github.com/etkecc/komai/commit/c714cd94c)).
- 🐛 Fix: content-less membership and profile-change events no longer raise desktop notifications ([174ab5775](https://github.com/etkecc/komai/commit/174ab5775)).
- 📦 Calls: the bundled Element Call is updated to v0.20.2 ([a585007f8](https://github.com/etkecc/komai/commit/a585007f8)).
- 🎨 Polish: refreshed the bundled icon sets (Font Awesome 7.3.0, Fluent UI System Icons v1.1.331) ([a02239c85](https://github.com/etkecc/komai/commit/a02239c85), [711e233d1](https://github.com/etkecc/komai/commit/711e233d1)).

## 2026.06.22.0

- ✨ Feature: the default profile can now have its own desktop launcher, like named profiles, instead of only the profile-switcher entry ([76ee22179](https://github.com/etkecc/komai/commit/76ee22179)).
- 🎨 Polish: `komai --help` and `--version` show `komai` as the command name instead of the full path it was launched from ([cb5185544](https://github.com/etkecc/komai/commit/cb5185544)).
- 📦 Packaging: Komai honors a `KOMAI_EXECUTABLE_PATH` override when relaunching itself, so prebuilt AppImage installs route profile switches and generated desktop launchers through the correct entry point ([be86c9537](https://github.com/etkecc/komai/commit/be86c9537)).

## 2026.06.21.0

🎉 **[Element Call](https://github.com/element-hq/element-call) comes to Komai!** Modern, encrypted 1:1 and group voice and video calls now work on Linux, Windows, and macOS. See the [guide](https://github.com/etkecc/komai/blob/47cd44713/docs/user-guide/features/element-call.md) to get started ([699263321](https://github.com/etkecc/komai/commit/699263321), [05b207a75](https://github.com/etkecc/komai/commit/05b207a75), [f0866c102](https://github.com/etkecc/komai/commit/f0866c102)).

- ✨ Feature: support for forwarding a message to several rooms at once ([8f9144324](https://github.com/etkecc/komai/commit/8f9144324), [47cd44713](https://github.com/etkecc/komai/commit/47cd44713)).
- ✨ Feature: resize the message input area by dragging its top edge ([8687b2c18](https://github.com/etkecc/komai/commit/8687b2c18)).
- 🐛 Fix: failed media thumbnails back off and retry automatically, and retry right away when connectivity returns, instead of staying broken ([3c9e637f5](https://github.com/etkecc/komai/commit/3c9e637f5), [1a29daa35](https://github.com/etkecc/komai/commit/1a29daa35)).
- 🐛 Fix: timeline tooltips render as plain text, and the avatar sender tooltip is a compact single line ([859a9d0f6](https://github.com/etkecc/komai/commit/859a9d0f6), [e8ecf7da3](https://github.com/etkecc/komai/commit/e8ecf7da3)).
- 🐛 Fix: the reply preview no longer gets stuck in a layout loop near the height-limit cap ([87c765223](https://github.com/etkecc/komai/commit/87c765223)).
- 🐛 Fix: Komai probes whether the secure store is writable and explains an unreadable store cipher, instead of failing silently ([4cddd800a](https://github.com/etkecc/komai/commit/4cddd800a)).
- 🎨 Polish: the Media Viewer action buttons match the app's other control bars ([e39ab0438](https://github.com/etkecc/komai/commit/e39ab0438)).
- 🎨 Polish: the thread view drops its dashed outline ([b3e2b5311](https://github.com/etkecc/komai/commit/b3e2b5311)).

## 2026.06.02.0

- 📦 Linux: the AppImage, Flatpak, and Snap bundles are now published for arm64 (`aarch64`) alongside x86-64, so Komai runs on arm64 Linux machines ([4988b678b](https://github.com/etkecc/komai/commit/4988b678b)).

_(No application changes from v2026.05.31.1; this release only adds the new arm64 Linux builds.)_

## 2026.05.31.1

- ✨ Feature: the timeline width at which "Adaptive" horizontal positioning collapses messages to one side is now configurable (Settings → Timeline → Presentation), instead of being fixed at 1600px ([89e859c16](https://github.com/etkecc/komai/commit/89e859c16)).
- ✨ Feature: pasting a message that contains a user link now mentions that user ([8678dbaba](https://github.com/etkecc/komai/commit/8678dbaba)).
- 🐛 Fix: typing `@room` or mentioning a user reliably notifies the target again; intentional mentions stopped being sent after the matrix-sdk migration, and the mention indication bar is back ([013810df3](https://github.com/etkecc/komai/commit/013810df3)).
- 🐛 Fix: `/me` and the other body-sending slash commands (`/notice`, `/plain`, `/html`) now carry mentions, so a pinged user is actually notified ([0766a1c90](https://github.com/etkecc/komai/commit/0766a1c90)).
- 📝 Docs: the sample [config.yml](https://github.com/etkecc/komai/blob/5563b1143/docs/user-guide/settings/examples/profile/config.yml) now matches the current settings schema ([5563b1143](https://github.com/etkecc/komai/commit/5563b1143)).

_(Re-cut of v2026.05.31.0, whose publish failed at the Windows link step after the Rust 1.96 toolchain upgrade; fixed by linking bcrypt explicitly ([df9852f6b](https://github.com/etkecc/komai/commit/df9852f6b)). No other changes from that tag.)_

## 2026.05.27.0

- ✨ Feature: support for upgrading rooms to a newer room version, via Room Info → "Upgrade..." or the new `/upgraderoom` slash command. ([42e5c8f71](https://github.com/etkecc/komai/commit/42e5c8f71), [c770f5b5f](https://github.com/etkecc/komai/commit/c770f5b5f)).
- ✨ Feature: a new "Auto-hide with a single tab" setting can allow you to hide the [tab bar](https://github.com/etkecc/komai/blob/a66fa422a/docs/user-guide/features/tabs.md) while only one tab is open ([ca92d074b](https://github.com/etkecc/komai/commit/ca92d074b), [1d9be75a0](https://github.com/etkecc/komai/commit/1d9be75a0)).
- ✨ Feature: a new "Show button labels" setting can suppress the room header's action button labels (Search, Pin, Threads, Members, encryption, Leave) at all times, as opposed to only when the bar is tight on space ([9217671d2](https://github.com/etkecc/komai/commit/9217671d2)).
- ✨ Feature: a new "Monochrome" style for the [tray icon](https://github.com/etkecc/komai/blob/a66fa422a/docs/user-guide/features/system-tray.md), with light and dark variants ([b588a9c04](https://github.com/etkecc/komai/commit/b588a9c04)).
- 🐛 Fix: Reply, Reply in thread, and Edit no longer leave the composer hidden behind the [selection-mode](https://github.com/etkecc/komai/blob/a66fa422a/docs/user-guide/features/selection-mode.md) bar when invoked while multiple messages are selected ([d3619665a](https://github.com/etkecc/komai/commit/d3619665a)).
- 🐛 Fix: room list timestamps and last-message previews keep their visual hierarchy on themes whose button-label color equals the body text color ([999f422b4](https://github.com/etkecc/komai/commit/999f422b4)).
- 🐛 Fix: short room-header tooltips no longer wrap onto two lines in rare cases (e.g. "Read" rendering as "Rea" + "d") ([d011ede87](https://github.com/etkecc/komai/commit/d011ede87), [5858b4683](https://github.com/etkecc/komai/commit/5858b4683)).
- 🐛 Fix: "Copy room link" from the room context menu now prefers the canonical room alias when one is set ([178f7b20c](https://github.com/etkecc/komai/commit/178f7b20c)).
- 🐛 Fix: mouse-wheel scrolling is now 5x faster on the Account tab and the settings sidebar ([7ce871523](https://github.com/etkecc/komai/commit/7ce871523)).
- 🐛 Fix: pressing Up in an empty composer to edit your last message works again when thread replies are collapsed or in-room search is active ([a5594f836](https://github.com/etkecc/komai/commit/a5594f836)).

_(Silly thing we do: this is a re-cut of an abandoned v2026.05.26.0, whose publish workflow got wedged during a GitHub Actions outage and could not be revived. No code changes from that tag.)_

## 2026.05.21.0

- ✨ Feature: a new "Adaptive" timeline horizontal positioning, which opposes by sender on narrow timelines and collapses to the reading-direction leading edge on ultrawide windows (above 1600px). New installs use Adaptive by default; existing users keep their choice ([1ef044281](https://github.com/etkecc/komai/commit/1ef044281), [61bb37195](https://github.com/etkecc/komai/commit/61bb37195)).
- 🐛 Fix: pin/unpin works again in rooms whose `m.room.pinned_events` state event has been redacted ([5982b4912](https://github.com/etkecc/komai/commit/5982b4912)).
- 🐛 Fix: the Read receipts dialog honors the global scrollbar policy and keeps a small gap between the scrollbar and the user cards ([6435dc31b](https://github.com/etkecc/komai/commit/6435dc31b)).
- 🐛 Fix: the "Read" double-check on the timeline delivery indicator is tinted with the brand highlight, so Sent and Read are easier to distinguish ([0433f956a](https://github.com/etkecc/komai/commit/0433f956a)).
- 📝 Copy: the timeline delivery indicator previously labeled "Received" is now "Sent", which is more honest about what the indicator actually confirms ([16f387fb4](https://github.com/etkecc/komai/commit/16f387fb4)).

## 2026.05.20.1

- 🐛 Fix: in rare cases when switching between rooms with active [threads](https://github.com/etkecc/komai/blob/c8a45c18c93fa48b5f6977505e6d1699256c9d6e/docs/user-guide/features/threads.md), thread messages from one room could briefly render in another room's timeline (regression from v2026.05.20.0) ([709b34bd5](https://github.com/etkecc/komai/commit/709b34bd5), [f9a31e0e4](https://github.com/etkecc/komai/commit/f9a31e0e4)).
- 🐛 Fix: rare crash when switching between rooms that each have an active thread ([709b34bd5](https://github.com/etkecc/komai/commit/709b34bd5)).
- 🐛 Fix: tab switches to/from a room with an active thread feel instant again ([f9a31e0e4](https://github.com/etkecc/komai/commit/f9a31e0e4)).
- 📦 AUR: dropped the misleading `hunspell-en_us` optdepends entry; en_US is bundled, and any installed `hunspell-<lang>` is auto-discovered ([5d904064e](https://github.com/etkecc/komai/commit/5d904064e)).

## 2026.05.20.0

- ✨ Feature: in-room search highlights matches inline in rendered messages ([079aeedaf](https://github.com/etkecc/komai/commit/079aeedaf)).
- ✨ Feature: [copying](https://github.com/etkecc/komai/blob/33648ab2542daba8c5689eb454cc70d55c2a7fad/docs/user-guide/features/selection-mode.md#-acting-on-the-selection) of multiple messages prefixes each line with sender and timestamp; single-message copying stays bare ([7d1eb34ad](https://github.com/etkecc/komai/commit/7d1eb34ad)).
- ✨ Feature: a [`--start-in-tray`](https://github.com/etkecc/komai/blob/33648ab2542daba8c5689eb454cc70d55c2a7fad/docs/user-guide/features/system-tray.md#---start-in-tray-cli-flag) CLI flag starts Komai hidden for one launch, for silent autostart entries without flipping the persistent "Start in tray" setting ([8185d258b](https://github.com/etkecc/komai/commit/8185d258b)).
- ✨ Feature: timeline [drag-to-select](https://github.com/etkecc/komai/blob/33648ab2542daba8c5689eb454cc70d55c2a7fad/docs/user-guide/features/selection-mode.md#click-and-drag) can now start from the empty gutter beside the message bubble for easier selection ([7806b6c5d](https://github.com/etkecc/komai/commit/7806b6c5d)).
- ✨ Feature: previously-opened [threads](https://github.com/etkecc/komai/blob/33648ab2542daba8c5689eb454cc70d55c2a7fad/docs/user-guide/features/threads.md#-viewing-a-single-thread) re-open instantly from an in-session cache ([5397e89c7](https://github.com/etkecc/komai/commit/5397e89c7)).
- 🐛 Fix: switching threads no longer flashes the previous thread's content for a split-second ([3608e8b42](https://github.com/etkecc/komai/commit/3608e8b42)).
- 🐛 Fix: video messages are no longer invisible (zero height) ([9dfa9259d](https://github.com/etkecc/komai/commit/9dfa9259d)).
- 🐛 Fix: Room Info no longer crashes when switching away from its Settings tab ([1acf3a4c7](https://github.com/etkecc/komai/commit/1acf3a4c7)).
- 🐛 Fix: read receipts gated on window focus, not just visibility, so side-by-side windows and taskbar previews no longer mark as read ([cd0a38339](https://github.com/etkecc/komai/commit/cd0a38339)).
- 🐛 Fix: copied messages no longer paste a stray placeholder glyph in front of @mentions ([e4d2a35dd](https://github.com/etkecc/komai/commit/e4d2a35dd)).
- 🐛 Fix: mouse-wheel scrolling is now 5x faster in Settings, Application Profiles, Room List, Communities, Room Directory, Threads, Pinned Messages, the Sticker Picker grid, Invite, and Create Direct ([355f837ef](https://github.com/etkecc/komai/commit/355f837ef)).
- 🐛 Fix: in [thread view](https://github.com/etkecc/komai/blob/33648ab2542daba8c5689eb454cc70d55c2a7fad/docs/user-guide/features/threads.md#-viewing-a-single-thread), the composer's reply / attachments / transcription bars no longer expose a white notch at their rounded top corners on light themes ([688218a2e](https://github.com/etkecc/komai/commit/688218a2e)).
- 🐛 Fix: the right-click message context menu opens at the click site on Wayland; right-clicks on the bubble fire reliably ([a03e74965](https://github.com/etkecc/komai/commit/a03e74965), [58a8580e2](https://github.com/etkecc/komai/commit/58a8580e2)).
- 🐛 Fix: the active-call bar's top corners are rounded, matching the composer bars ([6a1c17eba](https://github.com/etkecc/komai/commit/6a1c17eba)).
- 🐛 Fix: Linux desktop [notifications](https://github.com/etkecc/komai/blob/33648ab2542daba8c5689eb454cc70d55c2a7fad/docs/user-guide/features/notifications.md) strip non-spec HTML so daemons like mako and gnome-shell stop rendering tags as literal text ([56267f376](https://github.com/etkecc/komai/commit/56267f376)).
- 🐛 Fix: the inline image in Linux desktop [notifications](https://github.com/etkecc/komai/blob/33648ab2542daba8c5689eb454cc70d55c2a7fad/docs/user-guide/features/notifications.md) no longer center-crops portrait and square sources ([7353039b7](https://github.com/etkecc/komai/commit/7353039b7)).
- 🐛 Fix: rare duplicate or missing message bubbles in the timeline ([485153357](https://github.com/etkecc/komai/commit/485153357), [910f932800](https://github.com/etkecc/komai/commit/910f932800)).
- 📝 Docs: new [Notifications](https://github.com/etkecc/komai/blob/33648ab2542daba8c5689eb454cc70d55c2a7fad/docs/user-guide/features/notifications.md) user-guide page ([b1e419ec2](https://github.com/etkecc/komai/commit/b1e419ec2)).
- 📝 Docs: new [System Tray](https://github.com/etkecc/komai/blob/33648ab2542daba8c5689eb454cc70d55c2a7fad/docs/user-guide/features/system-tray.md) user-guide page ([8185d258b](https://github.com/etkecc/komai/commit/8185d258b)).

## 2026.05.18.0

- ✨ Feature: a Markdown [formatting toolbar](https://github.com/etkecc/komai/blob/9a47c4f2a9a5eb851e25244d02cc763b718e2344/docs/user-guide/features/keyboard-shortcuts.md#formatting-toolbar) over the composer selection (Bold, Italic, Code, Quote, Link), with matching `Ctrl+B/I/E/Shift+>/L` shortcuts. Toggleable in Settings -> Composer -> Input ([1e7cf3353](https://github.com/etkecc/komai/commit/1e7cf3353), [9a47c4f2a](https://github.com/etkecc/komai/commit/9a47c4f2a)).
- ✨ Feature: [drag-to-select](https://github.com/etkecc/komai/blob/9a47c4f2a9a5eb851e25244d02cc763b718e2344/docs/user-guide/features/selection-mode.md#click-and-drag) messages in the timeline; `Ctrl` / `Cmd` / `Shift` adds to the existing selection. Toggleable in Settings -> Timeline ([112685279](https://github.com/etkecc/komai/commit/112685279), [483223570](https://github.com/etkecc/komai/commit/483223570), [c35e38488](https://github.com/etkecc/komai/commit/c35e38488), [7669bb29f](https://github.com/etkecc/komai/commit/7669bb29f), [18757a191](https://github.com/etkecc/komai/commit/18757a191), [deaed9ecb](https://github.com/etkecc/komai/commit/deaed9ecb)).
- ✨ Feature: new installs default to streaming [voice transcription](https://github.com/etkecc/komai/blob/9a47c4f2a9a5eb851e25244d02cc763b718e2344/docs/user-guide/features/voice-transcription.md) (OpenAI `gpt-realtime-whisper`, word-by-word). Existing users keep their current settings. The realtime path also moved to OpenAI's GA wire shape ([74cfab97f](https://github.com/etkecc/komai/commit/74cfab97f), [36349dae7](https://github.com/etkecc/komai/commit/36349dae7), [a128f70cc](https://github.com/etkecc/komai/commit/a128f70cc)).
- ✨ Feature: long replies scroll inside the composer's "Replying to ..." preview, and the top edge is now a drag handle to grow it ([26e8f3493](https://github.com/etkecc/komai/commit/26e8f3493), [e8d39f481](https://github.com/etkecc/komai/commit/e8d39f481), [911c51048](https://github.com/etkecc/komai/commit/911c51048)).
- ✨ Feature: a "Show date dividers" toggle in Settings -> Timeline -> Presentation hides the centered date and "later" labels between messages ([23b68387a](https://github.com/etkecc/komai/commit/23b68387a), [35c22510f](https://github.com/etkecc/komai/commit/35c22510f)).
- ✨ Feature: the inline emoji picker dismisses itself for emoticon shortcuts (like `:)`, `:D`, etc.) instead of getting in the way and suggesting low-quality results ([e25d4eb65](https://github.com/etkecc/komai/commit/e25d4eb65)).
- 🐛 Fix: `Ctrl+Z` in the composer works again on every press; [spellcheck](https://github.com/etkecc/komai/blob/9a47c4f2a9a5eb851e25244d02cc763b718e2344/docs/user-guide/features/spellcheck.md) no longer pollutes the undo stack ([da357cb0a](https://github.com/etkecc/komai/commit/da357cb0a)).
- 🐛 Fix: extreme-wide (banner-aspect) reply preview images fit without upscaling and center-cropping ([4ed8cf2d9](https://github.com/etkecc/komai/commit/4ed8cf2d9)).
- 🐛 Fix: thread reply badges no longer double-count in rare circumstances (back-pagination, collapse-replies on) ([9508c0632](https://github.com/etkecc/komai/commit/9508c0632)).
- 🐛 Fix: clicking a `matrix.to` link to a room you're already in navigates to it instead of popping the join modal ([8e1363f8a](https://github.com/etkecc/komai/commit/8e1363f8a)).
- 🐛 Fix: a `matrix:` URI passed at startup survives the [application profile](https://github.com/etkecc/komai/blob/9a47c4f2a9a5eb851e25244d02cc763b718e2344/docs/user-guide/features/application-profiles.md) selector when you have multiple profiles (previously dropped) ([7581ebed0](https://github.com/etkecc/komai/commit/7581ebed0)).
- 🐛 Fix: the inline emoji picker opens after emoji and punctuation, not only after whitespace ([e9fca4cd6](https://github.com/etkecc/komai/commit/e9fca4cd6)).
- 🐛 Fix: spellcheck no longer flags every word as misspelled in Japanese IMEs' "wide Latin" (Zenkaku) mode ([0e8f5e111](https://github.com/etkecc/komai/commit/0e8f5e111)).
- 🐛 Fix: room names in the Forward Message dialog, Quick Switcher, mention pills, slash commands, and the emoji/sticker picker render at full brightness again (regression from last release) ([401d3c734](https://github.com/etkecc/komai/commit/401d3c734)).

## 2026.05.15.0

🎉 **The first pre-built macOS binary of Komai!** Every tagged release now ships an Apple Silicon (arm64) `.dmg` (`komai-<version>-macos-arm64.dmg`) alongside the AppImage, Flatpak, Snap, and Windows ZIP. The build is unsigned and not notarized: on macOS 13/14, right-click `komai.app` and choose **Open**; on macOS 15+, open the app once, then go to System Settings -> Privacy & Security and click **Open Anyway**. Subsequent launches don't re-prompt ([e4ed671a4](https://github.com/etkecc/komai/commit/e4ed671a4)).

Huge thanks to [@hareeen](https://github.com/hareeen) (Suyoung Hwang) whose [PR #158](https://github.com/etkecc/komai/pull/158) did the heavy lifting of making the native macOS build work in the first place. Komai building and running on macOS was a "tentative, untested by maintainers" footnote before that PR.

- ✨ Feature: [offline spell checking](https://github.com/etkecc/komai/blob/v2026.05.15.0/docs/user-guide/features/spellcheck.md) in the message composer and the room-topic editor, backed by [Hunspell](https://hunspell.github.io/) dictionaries. English (US) is built in ([be56c4452](https://github.com/etkecc/komai/commit/be56c4452)).
- ✨ Feature: the "Leave room" dialog now takes an optional reason, sends it to the homeserver, and Komai renders the reason inline in the timeline of other rooms when it sees it on incoming leave events ([f7644bf51](https://github.com/etkecc/komai/commit/f7644bf51), [3390c2041](https://github.com/etkecc/komai/commit/3390c2041), [5952ac4c7](https://github.com/etkecc/komai/commit/5952ac4c7)).
- ✨ Feature: private read receipts. When "Show others when I've read their messages" is off, Komai sends `m.read.private` receipts instead of suppressing receipts entirely, so your own read state still syncs across your sessions and you don't need to use "Mark as read" manually ([e972f7b67](https://github.com/etkecc/komai/commit/e972f7b67)).
- ✨ Feature: image, file, video, and audio messages now respect the "Auto-convert Markdown to HTML" composer setting on send, so captions go out with HTML formatting when the toggle is on ([310473558](https://github.com/etkecc/komai/commit/310473558)).
- ✨ Feature: formatted captions on image, file, video, and audio messages now render in the timeline instead of showing as raw Markdown ([1f77580ba](https://github.com/etkecc/komai/commit/1f77580ba), [183f344c8](https://github.com/etkecc/komai/commit/183f344c8)).
- ✨ Feature: a clear button (and `Ctrl+F` focus shortcut) on the settings search box ([adef26bd8](https://github.com/etkecc/komai/commit/adef26bd8)).
- ✨ Feature: a snackbar confirms when a message report has been delivered, instead of the action vanishing silently ([0ae57d73e](https://github.com/etkecc/komai/commit/0ae57d73e)).
- 🐛 Fix: read/received delivery indicators now stay live in the thread timeline, and thread read receipts are honored in the Read Receipts dialog and the merged-timeline snapshot ([3b8ef3c6e](https://github.com/etkecc/komai/commit/3b8ef3c6e), [2ce7137e4](https://github.com/etkecc/komai/commit/2ce7137e4), [375c1644b](https://github.com/etkecc/komai/commit/375c1644b), [33b728ca5](https://github.com/etkecc/komai/commit/33b728ca5)).
- 🐛 Fix: timeline selection and walk-mode are now scoped to the active timeline model, so multi-select works correctly in thread view; the ListView also positions by its own model's row index, avoiding off-by-one slips in subset timelines ([e68aacdce](https://github.com/etkecc/komai/commit/e68aacdce), [39f65b433](https://github.com/etkecc/komai/commit/39f65b433)).
- 🐛 Fix: the "Replying to" banner has been relocated above the staged file attachments row ([2213a048b](https://github.com/etkecc/komai/commit/2213a048b)).
- 🐛 Fix: highlighted code blocks no longer render with a doubled rectangle around the highlight ([ccd733e2a](https://github.com/etkecc/komai/commit/ccd733e2a)).
- 🐛 Fix: long context-menu labels are no longer truncated with an ellipsis; the menu grows to fit ([fc8473e31](https://github.com/etkecc/komai/commit/fc8473e31)).
- 🐛 Fix: the upload-progress logo on the composer attach button now respects the "Enable UI animations" setting (Settings -> General -> Interface), just like the spinner and search-status icons already did ([33d8c39a3](https://github.com/etkecc/komai/commit/33d8c39a3)).
- 🐛 Fix: disabled controls are now visually distinct from enabled ones in every built-in theme via a derived "Disabled" palette group ([33080a97a](https://github.com/etkecc/komai/commit/33080a97a)).
- 🐛 Fix: textarea context menus use Fluent icons (as opposed to falling back to system icons) to guarantee consistency ([403d39af1](https://github.com/etkecc/komai/commit/403d39af1)).
- 🐛 Fix: the audio attachment loading spinner hides once playback actually starts ([77c3b4e0d](https://github.com/etkecc/komai/commit/77c3b4e0d)).
- 🐛 Fix: leaving rooms now auto-closes their respective tabs ([487e8f1bc](https://github.com/etkecc/komai/commit/487e8f1bc)).
- 🐛 Fix: invite rooms can no longer be opened as a tab (they were never really functional as one) ([392bff165](https://github.com/etkecc/komai/commit/392bff165)).
- 📝 Copy: the encryption flow now consistently says "recovery key" and "recovery passphrase" everywhere ([a62a0a2bb](https://github.com/etkecc/komai/commit/a62a0a2bb)).
- 📝 Copy: the score field has been dropped from the message-report dialog (the Matrix spec deprecated it and servers ignore it) ([b9a72cc53](https://github.com/etkecc/komai/commit/b9a72cc53)).
- 🔧 Build: two macOS-specific compiler warnings cleaned up ([782de9a28](https://github.com/etkecc/komai/commit/782de9a28), [05f01033c](https://github.com/etkecc/komai/commit/05f01033c)).

## 2026.05.10.0

🎉 **The first pre-built Windows binary of Komai!** Every tagged release now ships a Windows no-installer ZIP (`komai-<version>-windows-x64-no-installer.zip`) alongside the AppImage, Flatpak, and Snap. The build is unsigned, so SmartScreen warns on first launch ("More info" → "Run anyway") ([62ad50480](https://github.com/etkecc/komai/commit/62ad50480), [d644ad8fc](https://github.com/etkecc/komai/commit/d644ad8fc)).

- ✨ Feature: spaces are now first-class. A "Space" badge appears in the room header, and the composer is hidden in space rooms (since space messages don't surface on other clients) ([d80eedad6](https://github.com/etkecc/komai/commit/d80eedad6), [e28820c9b](https://github.com/etkecc/komai/commit/e28820c9b)).
- ✨ Feature: room-level "Mark as unread" from the [room list](https://github.com/etkecc/komai/blob/b3cec3d0a88ee10e317592863834f0011f9cf38d/docs/user-guide/features/room-list.md#mark-as-unread) and the [tab context menu](https://github.com/etkecc/komai/blob/b3cec3d0a88ee10e317592863834f0011f9cf38d/docs/user-guide/features/tabs.md#-mark-as-read-or-unread); the unread state syncs across devices and to other Matrix clients ([920b2f29f](https://github.com/etkecc/komai/commit/920b2f29f), [9036b1fd5](https://github.com/etkecc/komai/commit/9036b1fd5), [cc67e76b5](https://github.com/etkecc/komai/commit/cc67e76b5)).
- ✨ Feature: per-room override for "Show others when I'm typing" and "Show others when I've read their messages" in Room Info → Preferences ([a385946fa](https://github.com/etkecc/komai/commit/a385946fa), [708a65566](https://github.com/etkecc/komai/commit/708a65566)).
- 🐛 Fix: the "Show others when I've read their messages" toggle now actually has effect; it had been ignored before, so receipts went out regardless ([e7a815081](https://github.com/etkecc/komai/commit/e7a815081)).
- 🐛 Fix: child-space references and other previously-unrouted state events render properly in the timeline, instead of falling through to a generic "Unsupported" placeholder ([c084371fb](https://github.com/etkecc/komai/commit/c084371fb), [cba45c300](https://github.com/etkecc/komai/commit/cba45c300)).
- 🐛 Fix: Sign In gives up quickly when the homeserver is unreachable, instead of silently retrying for up to 15 minutes ([871086e6f](https://github.com/etkecc/komai/commit/871086e6f)).
- 🐛 Fix: Escape returns focus to the composer when focus is on highlighted message text or on the room-list / communities sidebar; Tab and Shift+Tab from a highlighted message do the same ([4173428fa](https://github.com/etkecc/komai/commit/4173428fa), [a5ead0faf](https://github.com/etkecc/komai/commit/a5ead0faf)).
- 🔧 Build: Komai no longer depends on OpenSSL, thanks to matrix-sdk 0.17.0. This simplifies packaging by removing a dependency ([5bbcbcf1c](https://github.com/etkecc/komai/commit/5bbcbcf1c), [7caacac4e](https://github.com/etkecc/komai/commit/7caacac4e), [be76bf2c1](https://github.com/etkecc/komai/commit/be76bf2c1), [e035c7366](https://github.com/etkecc/komai/commit/e035c7366)).
- 🔧 Build: `pkg-config` is no longer required when VOIP and X11 are both disabled (typical Windows and macOS builds) ([e5778ed20](https://github.com/etkecc/komai/commit/e5778ed20)).
- 📦 Flatpak: the bundle filename now carries version + arch (`komai-<version>-<arch>.flatpak`), matching the AppImage / Snap / Windows ZIP naming ([c7fea0559](https://github.com/etkecc/komai/commit/c7fea0559)).

## 2026.05.09.0

- ✨ Feature: Shift+Click range selection in the timeline, extending the existing Ctrl+Click multi-select ([4d9e9ced4](https://github.com/etkecc/komai/commit/4d9e9ced4)).
- 🐛 Fix: redacted messages no longer offer React, Reply, Pin, or Delete actions; inspection actions (Copy permalink, View raw, Read receipts, Report) remain available ([4a56ec5d9](https://github.com/etkecc/komai/commit/4a56ec5d9)).
- 🐛 Fix: Forwarding from the media viewer overlay works again; previously, confirming a target room silently failed ([00f73aa81](https://github.com/etkecc/komai/commit/00f73aa81)).
- 🐛 Fix: the Forward dialog no longer jumps vertically when switching to confirm mode ([690feb75b](https://github.com/etkecc/komai/commit/690feb75b)).
- 🐛 Fix: mention pills now show the default avatar (Bauhaus, LetterInitial, etc.) when there's no avatar or while the avatar is still loading, matching the timeline ([8800bd9cc](https://github.com/etkecc/komai/commit/8800bd9cc), [e3d645ef5](https://github.com/etkecc/komai/commit/e3d645ef5)).
- 🐛 Fix: mention pills refresh in-place when avatar settings change, instead of waiting for the timeline to reload ([154ec98f0](https://github.com/etkecc/komai/commit/154ec98f0)).
- 🐛 Fix: the `@room` row in the mention picker now uses the actual room avatar instead of the generic fallback ([c20adde65](https://github.com/etkecc/komai/commit/c20adde65)).
- 🐛 Fix: media fetches are now capped by a timeout, so a single broken or hung URL no longer starves the media pipeline ([64a632e41](https://github.com/etkecc/komai/commit/64a632e41), [e3d645ef5](https://github.com/etkecc/komai/commit/e3d645ef5)).

## 2026.05.08.0

- 🔧 Build: Komai no longer depends on the `Qt5Compat.GraphicalEffects` QML module (or the `qt6-5compat` shared library it pulls in); the rounded-corner masking sites have been ported to `QtQuick.Effects.MultiEffect`. Distro packagers can drop `qml6-module-qt5compat-graphicaleffects` (and the equivalent on other distros) from their dependency lists ([992f2e651](https://github.com/etkecc/komai/commit/992f2e651)).
- 🔧 Build: `matrix-sdk` is now sourced from the crates.io 0.17.0 release instead of a git-pinned `main`-branch snapshot taken on top of 0.16.0 ([c27bdef12](https://github.com/etkecc/komai/commit/c27bdef12)).

## 2026.05.07.0

- 🐛 Fix: drag-and-drop into the message composer now works under Flatpak ([594dc71e6](https://github.com/etkecc/komai/commit/594dc71e6)).
- 🐛 Fix: "Open" and "Show in folder" on saved attachments work correctly under Flatpak; "Show in folder" now opens the user's actual save location instead of the doc-portal mount ([024bcfa88](https://github.com/etkecc/komai/commit/024bcfa88), [82eb3c6e4](https://github.com/etkecc/komai/commit/82eb3c6e4)).
- 🐛 Fix: the Copy action on the media overlay works again; for images and stickers, the paste now lands as a decoded image so apps like GIMP accept it ([56fd28a2e](https://github.com/etkecc/komai/commit/56fd28a2e)).
- 🐛 Fix: typing indicators no longer hang forever when a server's "stopped typing" event is dropped; stale entries are now pruned after 15s ([665935516](https://github.com/etkecc/komai/commit/665935516), [de7771c42](https://github.com/etkecc/komai/commit/de7771c42)).
- ✨ Feature: the custom emoji picker (`~`) now shows a friendly empty-state message instead of a blank popup when no matches are found or no custom emojis are defined; its header is retitled "Pick a custom emoji or sticker" so it is visibly distinct from the regular `:` emoji picker ([16e524967](https://github.com/etkecc/komai/commit/16e524967), [ed01bf662](https://github.com/etkecc/komai/commit/ed01bf662)).
- 🐛 Fix: on the dark-nheko theme, setting cards were invisible against the Settings page backdrop until hovered; the layered-surface look is restored ([f8964c496](https://github.com/etkecc/komai/commit/f8964c496)). Automation is now in place to ensure future built-in themes won't suffer from the same issue ([5c7977512](https://github.com/etkecc/komai/commit/5c7977512)).
- 🔒 Security: hickory DNS bumped to 0.26.1, picking up fixes for GHSA-mxqq-qxc6-39c2 (high) and GHSA-c4g7-w9pp-7r2x (medium) ([e65f4efe9](https://github.com/etkecc/komai/commit/e65f4efe9)).
- 📦 AUR: the PKGBUILD now builds inside sandboxed `makepkg` runners (notably `rua`) when `rust` is satisfied by `rustup` ([10338c585](https://github.com/etkecc/komai/commit/10338c585)).
- 🔧 Build: distro builds on Qt 6.5+ are no longer blocked by an accidental Qt 6.10 dependency in the settings search proxy ([a76f99e7c](https://github.com/etkecc/komai/commit/a76f99e7c)).
- 🔧 Build: early progress on a Windows port: the project now compiles to a `komai.exe`. The binary is not yet runnable (runtime DLL/plugin staging still pending) and more work is needed before Windows becomes a supported target.

## 2026.05.06.3

- 🐛 Fix: own-bubble (right-aligned) reactions arriving together no longer stack one-per-line; pills lay out side-by-side again as long as the bubble has horizontal room ([9b4d2e4e3](https://github.com/etkecc/komai/commit/9b4d2e4e3)).
- 📦 AUR: the PKGBUILD now builds against the distro Rust toolchain instead of upstream's rustup channel pin. Fixes `Could not find toolchain '1.95.0-x86_64-unknown-linux-gnu'` for AUR users whose default rustup toolchain is something else, and avoids a ~250 MB rustup auto-install side-effect during the build ([adbbc560f](https://github.com/etkecc/komai/commit/adbbc560f)).

## 2026.05.06.2

- 🐛 Fix: attribution footer stacks vertically on narrow widths so the Sponsor / Report-an-issue buttons don't crowd the attribution text ([226ceea61](https://github.com/etkecc/komai/commit/226ceea61)).
- ✨ Feature: when an attachment is added in the message composer, the caption field for the newest attachment auto-focuses ([fc33ed914](https://github.com/etkecc/komai/commit/fc33ed914)).
- 🔧 Build: distro packagers can build against the system `qtkeychain`, `KDSingleApplication`, and `litehtml` via `-DCPM_USE_LOCAL_PACKAGES=ON` ([2ac8559db](https://github.com/etkecc/komai/commit/2ac8559db)).
- 🔧 Build: distro-packaged binaries (AUR, Flatpak, AppImage, Snap) shrink by ~26 MiB; `-rdynamic` is now gated to `CMAKE_BUILD_TYPE=Debug` only, so optimized release builds no longer export every static-archive symbol into `.dynsym` ([08d733e92](https://github.com/etkecc/komai/commit/08d733e92)).
- 📦 AUR: `package()` is one `cmake --install` line; no post-install scrub needed ([0e5be7318](https://github.com/etkecc/komai/commit/0e5be7318)).

## 2026.05.06.1

🎉 **The first public release of Komai!**

Komai is a desktop-first [Matrix](https://matrix.org/) chat application built with Rust, C++, and QML. It's a usability-focused descendant of [nheko](https://nheko.im/nheko-reborn/nheko), now running on the Rust [matrix-sdk](https://github.com/matrix-org/matrix-rust-sdk) and a growing Rust core.

- 📖 [README](https://github.com/etkecc/komai#readme): what Komai is and what it does
- 📝 [Introducing Komai](https://etke.cc/blog/introducing-komai/): the [etke.cc](https://etke.cc/) team's announcement post
- 📚 [User Guide](https://github.com/etkecc/komai/blob/main/docs/user-guide/README.md) · 🔀 [Differences from nheko](https://github.com/etkecc/komai/blob/main/docs/user-guide/differences-from-nheko.md)
- 📥 [Installation](https://github.com/etkecc/komai/blob/main/docs/user-guide/installation.md): AppImage, Flatpak, Snap, AUR (Linux x86_64)
- 💬 [#komai:etke.cc](https://matrix.to/#/#komai:etke.cc) · 🐛 [Issues](https://github.com/etkecc/komai/issues)
