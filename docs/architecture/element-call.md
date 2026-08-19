# 📞 Element Call

Komai embeds [Element Call](https://github.com/element-hq/element-call) to provide
MatrixRTC voice and video. Element Call is a web app (a React single-page
app) that runs inside a QtWebEngine view and talks to its host over the Matrix
Widget API. Komai plays the host role natively: a `matrix_sdk::widget` driver in
Rust answers the widget's Matrix requests against the already-logged-in client,
while the C++/QML layer owns the webview, the call UI, and the ring/notification
behavior.

This is the same shape Element X (iOS/Android) uses. It is independent of Komai's
legacy 1:1 GStreamer call stack (`src/voip/CallManager`, `WebRTCSession`); the two
coexist, each behind its own build option (`ELEMENT_CALL` and `VOIP`). For the
legacy stack see the [legacy calls user guide](../user-guide/features/legacy-calls.md).

## Short version

- Element Call ships as a pinned, embedded web bundle compiled into the binary as
  a Qt resource and served to the webview over a custom **secure** URL scheme,
  `komai-ec://`. A secure origin is mandatory: Chromium refuses `getUserMedia` on
  `qrc://`/`file://`.
- The webview speaks the Matrix Widget API as plain `window.postMessage` traffic.
  An injected page script pipes that traffic to C++; C++ relays it to a native
  Rust widget driver and back. The driver does all the Matrix work (events,
  to-device, OpenID, delayed events, capability negotiation).
- Media never touches Komai's networking. Element Call connects directly to a
  LiveKit SFU, which Komai discovers on its behalf and hands over the Widget API.
  Komai only proxies Matrix signaling.
- The call surface is an in-room collapsible panel that reflows above the timeline,
  with a global "back to call" bar, room-list/tab indicators, an incoming-call ring
  bar, and a true-fullscreen mode.
- QtWebEngine (Chromium) is the heavy dependency. Everything web-facing is gated
  behind the `ELEMENT_CALL` CMake option so packagers can build it out.

## Hard requirements

Element Call only works against a homeserver with a MatrixRTC backend: a LiveKit
SFU plus an `lk-jwt-service`, advertised either through the MSC4519 endpoint
(`GET /_matrix/client/unstable/org.matrix.msc4143/rtc/transports`) or the
`.well-known` `org.matrix.msc4143.rtc_foci` list, with the relevant MSCs (delayed
events / MSC4140, MSC4222) enabled. There is no SFU-less mesh fallback in current
Element Call. A homeserver without this backend will load the lobby but cannot
connect a call.

Element Call 0.24.0 dropped its own `.well-known` lookup and asks the host for
the transports over the Widget API instead (MSC4515), so the discovery above is
Komai's job now: see the MSC4515 intercept in `runtime_element_call.rs`.

## Architecture

```
┌─────────────────────────── Komai (native) ────────────────────────────┐
│  QML  ElementCallPanel ── hosts ──▶  WebEngineView                      │
│         (+ active bar, ring bar,        │ loads komai-ec://app/room#?…   │
│          room/tab indicators)           │ URL built by matrix-sdk        │
│                                         ▼                                │
│                       injected page script intercepts window.postMessage │
│                                         │  (QWebChannel = dumb JS→C++ pipe)│
│        C++ ElementCallWidgetSession  ◀──┘                                │
│            │  raw Widget API JSON, both ways (+ host-action intercept)    │
│            ▼  (cxx FFI)                                                   │
│   Rust  matrix_sdk::widget::WidgetDriver::run(room, caps)                 │
│            │  uses the SAME logged-in Client                              │
└────────────┼─────────────────────────────────────────────────────────────┘
             ▼
   Homeserver (sync, to-device, room state, OpenID)  +  LiveKit SFU + lk-jwt
```

Three layers, each with a clear job.

### Rust layer (the widget driver)

`src/rust/src/matrix_backend/runtime_element_call.rs` drives the Matrix Widget API
with `matrix_sdk::widget` (the `experimental-widgets` SDK feature). This is the
piece that every JS Matrix client reimplements on top of matrix-js-sdk; the Rust
SDK ships it natively, so Komai gets capability negotiation, event relay,
encrypted to-device, OpenID, and MSC4157 delayed events for free, all running
against the existing `Client`.

Per call, `start_element_call_session`:

1. Validates the room synchronously (so the caller learns about a dead handle or
   unknown room before any async work), allocates a session id, and spawns the
   session task.
2. Builds the widget URL with
   `WidgetSettings::new_virtual_element_call_widget(props, config)` then
   `settings.generate_webview_url(room, client_props)`. The URL is **never**
   hand-rolled; the SDK fills in `widgetId`, `userId`, `deviceId`, `baseUrl`,
   `roomId`, `perParticipantE2EE`, theme, and language. `element_call_url` points
   at the embedded bundle's secure origin (`komai-ec://app/room`).
3. Appends a few chrome and behavior parameters to the URL fragment (see
   [URL tuning](#url-tuning-why-the-fragment-is-edited)).
4. Spawns three cooperating tasks tied together by one `CancellationToken`:
   - the **driver task** (`WidgetDriver::run`), single-shot and long-lived, which
     suspends for the whole call and returns when the widget disconnects;
   - the **receive task**, which loops `handle.recv()` and forwards each
     driver→widget message to C++ via `matrix_notify_element_call_widget_message`;
   - the **forward loop** (the session task itself), which drains an in-order
     `mpsc` channel of widget→driver messages pushed in from C++ and feeds them to
     `handle.send()`. Forwarding stays on one task so message order is preserved;
     do not spawn-per-message.

   `recv() == None`, driver exit, or an explicit `stop` all cancel the token,
   which winds the other two down.

The widget URL is delivered to C++ asynchronously
(`matrix_notify_element_call_widget_url_ready`) once the homeserver profile lookup
inside `generate_webview_url` completes; teardown is reported via
`matrix_notify_element_call_widget_stopped`.

**Capabilities are auto-approved, no prompt.** `ElementCallCapabilitiesProvider`
ignores the requested set and returns `element_call_required_permissions(...)`
verbatim, because Element Call is a trusted first-party widget. That permission
set is ported from matrix-sdk-ffi's `get_element_call_required_permissions` but
spelled with **string-literal event types** rather than ruma enum variants,
because the RTC / call.member types (`org.matrix.msc4075.*`, `org.matrix.msc4310.*`,
`org.matrix.msc3401.call.member`) live behind `unstable-msc*` ruma features Komai
does not enable. The strings are the spec-correct on-the-wire identifiers. The set
covers reading the MatrixRTC session state, sending the per-device call-member
state event (across all of MSC3401/MSC3779/MSC4143's state-key shapes), to-device
encryption keys, raise-hand reactions and redactions, and the MSC4075 notification
plus MSC4310 decline.

The receive side of ringing lives separately in
`src/rust/src/matrix_backend/runtime_rtc.rs`. It mirrors the legacy
`runtime_calls` pattern: one raw `AnySyncMessageLikeEvent` handler watches every
synced room for `m.rtc.notification` (an incoming call) and `m.rtc.decline` (a
call rejected on another device), parses the JSON by hand, and forwards typed
structs to C++. It is server-independent (it does not need a homeserver RTC push
rule) and compiles unconditionally (no QtWebEngine dependency), so on
`-DELEMENT_CALL=OFF` builds the C++ side simply ignores the callbacks. Declining
sends an `m.rtc.decline` (`org.matrix.msc4310.rtc.decline`, an `m.reference` to the
notification) via `room.send_raw`.

### C++ layer (webview host + bridge + controller)

`src/voip/`:

- **`ElementCallWebProfile`** owns the QtWebEngine plumbing:
  - `registerUrlScheme()` registers `komai-ec` as `SecureScheme | LocalAccessAllowed
    | CorsEnabled | ServiceWorkersAllowed` (`+ FetchApiAllowed` on Qt 6.6+) with
    `Syntax::Host` so URLs look like `komai-ec://app/...` and `location.origin` is a
    normal comparable origin (the Widget API checks it). This must run in
    `MainApplication.cpp` **before** `QtWebEngineQuick::initialize()` because
    Chromium reads the scheme registry once at startup.
  - `ElementCallSchemeHandler` serves the embedded bundle from the `:/element-call`
    Qt resource, with an explicit extension→MIME map (ES modules need a JavaScript
    MIME, `.wasm` needs `application/wasm` for streaming compilation). Element Call
    is a single-page app, so any extensionless path (a client-side route like
    `/room`) falls back to `index.html`; only genuine missing assets 404.
  - It installs the handler on a **persistent, named** `QQuickWebEngineProfile`
    (not the default off-the-record one) so Element Call's `localStorage` (which
    holds every setting, including the chosen microphone and camera) and Chromium's
    per-origin device-id salt survive across restarts. Storage and cache live under
    the active Komai profile's own data/cache directories, so call state never
    leaks between Komai profiles.
  - `bridgeUserScripts()` returns the two `QWebEngineScript`s that form the
    page→host half of the bridge (built in C++ because Qt 6's `WebEngineScript` is
    a value type and cannot be created in QML).

- **`ElementCallWidgetSession`** (a `QML_ELEMENT`, QtWebEngine-free) is the
  per-call session object. It calls the three Rust FFI entry points
  (`matrix_element_call_{start,send_message,stop}_session`), receives the three
  notify callbacks (marshalled onto the GUI thread by the Matrix backend bridge),
  and exposes the call URL and a `messageToWidget` signal to QML. It also runs the
  **host-action intercept**: a handful of Widget API actions the driver does not
  implement are answered locally instead of forwarded:
  - `io.element.close` → reply `{}` and emit `closeRequested()` so the surface
    tears down (this is how Element Call's own hangup dismisses the panel);
  - `set_always_on_screen` → acknowledged;
  - `io.element.device_mute` → Element Call reports its current mic/camera state
    here (on join and after each toggle); the session mirrors it onto Q_PROPERTYs so
    the native header toggles reflect reality, then acks.

  A second intercept lives on the Rust side, in `runtime_element_call.rs`, because
  it needs the logged-in `Client`:
  - `org.matrix.msc4515.get_rtc_transports` → Element Call asks where the
    MatrixRTC backend is. `resolve_rtc_transports()` tries the MSC4519
    `rtc/transports` endpoint and falls back to the `.well-known` `rtc_foci` list,
    which is still all most homeservers publish, then replies directly into the
    webview.
  - `supported_api_versions` → the driver's own answer is amended on its way out
    to add `org.matrix.msc4515`, since Element Call will not send the action above
    unless the host advertises it. Splicing rather than replacing keeps the rest
    of the list tracking whatever matrix-sdk supports.

  Native mic/camera toggles send `io.element.device_mute` *to* the widget with only
  the changed field; the resulting fromWidget report is what actually flips the
  properties (no optimistic local state). Komai-originated requests carry a
  non-UUID `requestId` and are tracked in `pendingHostRequests_` so Element Call's
  replies to them are swallowed rather than handed to the driver (which would log
  an "invalid UUID" error).

- **`ElementCallController`** is an always-compiled QML singleton (`ElementCall`)
  that mirrors `CallManager`'s shape: `supported`/`active`/`activeRoomId`,
  `startCall`/`hangup`/`notifyStopped`, plus all the incoming-ring state. It holds
  **no** QtWebEngine state (the panel owns the session). It receives the
  `m.rtc.notification`/`m.rtc.decline` events from Rust and decides whether to ring
  (in-app ring bar + ringtone, honoring the shared `callsAudioRingtone` setting),
  raise a desktop notification (honoring the room's notify mode), or stay silent.

### QML layer (call surfaces)

`resources/qml/voip/`:

- **`ElementCallPanel`** hosts the `WebEngineView`. It is instantiated by a
  `Loader` in `TimelineView.qml` whenever `ElementCall.active`, kept loaded for the
  whole call (so the webview, and therefore the live WebRTC session, survives room
  switches), and visible only in the call's room. The panel reflows **above** the
  timeline (the timeline's top anchor binds to the panel's bottom) rather than
  overlaying it. The webview uses the dedicated secure-origin profile,
  `playbackRequiresUserGesture: false`, `screenCaptureEnabled: true`, and an
  `onPermissionRequested` that grants the transient camera/microphone capture
  permissions Chromium raises for `getUserMedia`. The panel has a draggable bottom
  border to resize its height (in-memory, reset on hangup) and a true-OS-fullscreen
  mode (the webview reparents to the window overlay and the window goes fullscreen).
- **`ElementCallActiveBar`** is the global "back to call / end call" bar shown when
  you navigate to a different room during a call.
- **`ElementCallRingBar`** is the global incoming-call bar (Join / Decline) for a
  `ring`-type notification addressed to you.
- **`ElementCallBarButton`** is the shared flat bar button used across all the EC
  surfaces, density-matched to the room-header action buttons.

Room-list and tab-bar avatar indicators (a diffuse glow + corner badge) light when
a call is live in a room, in green for calls you have joined and the warning hue for
calls in rooms you have not. The timeline renders an `m.rtc.notification` as a
"started a call" tile with a Join button.

## The bridge: QWebChannel as a dumb pipe

Element Call expects to talk to its host with vanilla `window.postMessage`, not an
RPC channel. QtWebEngine has no `addJavascriptInterface`, so the supported way to
get a string out of the page is QWebChannel. Komai uses QWebChannel purely as a
JS→C++ transport for a `window.postMessage` interceptor; Element Call itself never
speaks QWebChannel.

The injected page script (`bridgeUserScripts`, run at `DocumentCreation` in the
main world, after `qwebchannel.js`):

- adds a `window.addEventListener('message')` that **filters** to only the messages
  the driver cares about: widget→host *requests* (`api: "fromWidget"`, no
  `response`) and host→widget *responses* (`api: "toWidget"` with `response`). This
  filter matters: messages Komai injects are echoed back as `message` events and
  must be dropped, or the driver sees its own output and its state machine breaks.
- forwards each surviving message as a JSON string to `komaiBridge.postMessageFromWidget`.
- also wires a double-click (toggle fullscreen, ignoring clicks on Element Call's
  own controls) and `Escape` (leave fullscreen) as fallbacks for when the webview
  holds keyboard focus.

`bridgeUserScripts` also returns one unrelated injection: a small `<style>` that hides
Element Call's own in-page fullscreen button (see
[Fullscreen is OS-window, not DOM](#fullscreen-is-os-window-not-dom)).

The host→widget direction is plain `runJavaScript("window.postMessage(...)")`
driven from the session's `messageToWidget` signal.

## URL tuning (why the fragment is edited)

`generate_webview_url` builds a correct base URL, but Komai appends a few fragment
parameters because the corresponding `matrix_sdk::widget` config fields are not
re-exported (so they cannot be set the clean way) and because the desktop embed
wants different defaults than Element Call's standalone "unknown intent" preset:

- `header=none` removes Element Call's branded header (Komai draws its own bar).
  `HeaderStyle` is not re-exported and EC 0.20.x ignores the deprecated `hideHeader`
  param, so the URL is the only lever.
- `confineToRoom=true` removes the "back to recents" / return-home navigation; the
  call is embedded in one room and leaving is driven by Komai's End-call button.
- `sendNotificationType=ring|notification` makes Element Call publish an MSC4075
  notification on call start (which also drives the "started a call" timeline tile).
- The widget is pointed at the `/room` SPA route (not `/`, which is Element Call's
  home page), so it opens the call for `roomId` directly.

These are appended only because the underlying config fields default to `None`, in
which case the SDK emits none of those keys, so appending is safe.

## Ringing and notifications

The "ring vs silent notification" distinction is **Element's product convention,
not a Matrix spec rule**; it is the embedder's choice. Komai makes it on both ends:

- **Emit side** (`runtime_element_call.rs`): a direct chat sends
  `sendNotificationType=ring` plus `waitForCallPickup=true&autoLeave=true` so the
  caller stops ringing out and leaves on decline/no-answer (Element's
  `StartNewCallDM` behavior); a group room sends `notification` (silent).
  "Direct" is Komai's own determination (`room_is_direct`: an `m.direct` target, or
  exactly two active members), so it matches what the rest of Komai treats as a DM,
  rather than the SDK's `Room::is_direct`.
- **Receive side** (`runtime_rtc.rs` + `ElementCallController`): Komai honors the
  wire `notification_type`, which resolves any "they think it's a DM, we don't"
  mismatch. A `ring` only rings if it is actually addressed to you
  (`m.mentions`); a silent group `notification` honors the room's notify mode
  (muted rooms stay silent; mentions-only rooms notify only on a personal or `@room`
  mention, exactly as a regular message would). Notifications are deduplicated by
  event id, suppressed when the call is already joined, and expired per the MSC4075
  `lifetime` (with the standard clock-skew fallback to `origin_server_ts` when the
  sender clock diverges too far).

Komai does not predict caller auto-leave from the wire (it is not expressed there);
the Join affordance is reactive on call liveness, which it learns from the room-list
snapshot. The ring is canceled on join, decline, expiry, or a confirmed
live→gone transition of the call.

## Build integration

Everything web-facing is gated behind the `ELEMENT_CALL` CMake option (default
`ON`). When on, the build links `Qt::WebEngineQuick` and `Qt::WebChannelQuick`,
defines `ELEMENT_CALL_AVAILABLE`, and appends `QtWebEngine`/`QtWebChannel` to the
QML module dependencies. When off, the option cleanly drops the whole QtWebEngine
dependency: the panel QML (which `import QtWebEngine`) is excluded from
`qmlcachegen`, and the C++ surfaces compile to no-ops. The Rust widget driver and
the RTC receive handler compile **unconditionally** (they pull no QtWebEngine), so
`-DELEMENT_CALL=OFF` is purely a UI/webview drop.

The driver-only crate cost is the `experimental-widgets` matrix-sdk feature plus
`tokio-util` (for `CancellationToken`) and `language-tags`.

### The embedded bundle

The bundle is the published `@element-hq/element-call-embedded` npm artifact (the
widget-only Vite build with relative asset URLs, no standalone/consent UI), pinned
in `bin/element-call/sources.lock.yml` by `version` + `sha256`. This mirrors the
emoji-pipeline pattern, not the committed-icons pattern:

- `bin/element-call/fetch.py` (stdlib-only) downloads the tarball into the
  gitignored `var/element-call/<version>/`, verifies the sha256 (printing the
  correct hash on mismatch), and extracts only `package/dist/` (path-traversal
  guarded, atomic swap). It is idempotent and offline-friendly.
- CMake runs `fetch.py` at configure time (the file set is dynamic, so the file
  list cannot be hardcoded), globs the dist tree, and embeds it via
  `qt_add_resources(... PREFIX "/element-call" ...)`. The bundle compiles **into**
  the binary, so there is no runtime asset dependency to ship per platform; only the
  build needs the bundle present.
- Renovate tracks the npm package and proposes `version` bumps; the `sha256` is
  updated with the bump. The lock's `version: 1` schema line does not match the
  RE2-safe `version:\s+"..."` extraction, by design.

## Packaging QtWebEngine per target

QtWebEngine (Chromium) adds roughly 120-180 MB per artifact and is sourced
differently on every platform. The bundle being compiled-in means only the *build*
needs `fetch.py` to have run (`just element-call-fetch`, a prerequisite of the
packaging recipes); the per-target work is all about supplying QtWebEngine itself.

- **Arch** (`PKGBUILD`): `qt6-webengine` as a build + runtime dependency (Arch ships
  libs, headers, cmake config, and the `QtWebEngineProcess` helper in one package).
- **Flatpak** (`cc.etke.komai.yaml`): the KDE runtime has no QtWebEngine, so the
  manifest layers `base: io.qt.qtwebengine.BaseApp` (`base-version` tracking the
  runtime), which seeds `/app` with the libs, QML plugin, `QtWebEngineProcess`, and
  Chromium paks. `-DCMAKE_PREFIX_PATH=/app` lets `find_package` locate the BaseApp's
  Qt6WebEngineQuick config, and `cleanup-BaseApp.sh` strips its dev tree from the
  final bundle. `libQt6WebEngineCore.so` pulls `libsnappy`/`libminizip` via
  `DT_NEEDED` from `/app/lib` (not the triplet subdir); a plain `-L` does not feed
  ld's search for an indirectly-needed lib's symbols, so the link uses
  `-Wl,-rpath-link,/app/lib -Wl,--allow-shlib-undefined`. The offline build also
  copies `var/element-call` into the sandbox.
- **AppImage** (`AppImageBuilder.yml` + `bin/build-docker`): the build container
  installs `qt6-webengine-dev` etc.; the bundle includes `libqt6webenginecore6`,
  `libqt6webenginecore6-bin` (the `QtWebEngineProcess` helper is a separate
  package), `libqt6webenginequick6`, `qml6-module-qtwebengine`, and
  `libqt6webengine6-data` (the Chromium paks, `icudtl.dat`, and locale paks). The
  compiled-in `/usr` prefix does not resolve inside the AppImage, so AppRun sets
  `QTWEBENGINEPROCESS_PATH`, `QTWEBENGINE_RESOURCES_PATH`, and
  `QTWEBENGINE_LOCALES_PATH` explicitly.
- **Snap** (`snapcraft.yaml`): the `kde-neon-6` extension's content snaps already
  ship QtWebEngine (libs, process, paks) for both build and runtime, so no manifest
  layering is needed; `fetch.py` runs in `override-build`.
- **Windows** (`build-windows-zip/action.yml`) and **macOS**
  (`build-macos-dmg/action.yml`): the aqt module list includes `qtwebengine
  qtwebchannel qtpositioning` (`qtwebchannel` provides `Qt::WebChannelQuick`;
  `qtpositioning` is a QtWebEngine dependency). `windeployqt` /  `macdeployqt`
  then bundle `QtWebEngineProcess.exe` / the nested `QtWebEngineProcess.app`, the
  paks, ICU, and locales automatically.

### The Chromium sandbox

Chromium's own user-namespace sandbox does not work inside several confined runtimes,
so `QTWEBENGINE_DISABLE_SANDBOX=1` is set for **AppImage** and **Snap** (strict
confinement blocks the userns sandbox; snapd's own AppArmor + seccomp confinement
still applies). Native and Flatpak builds leave it on.

### The Windows glob fix

`fetch.py` prints the dist directory as a native path. On Windows that path has
backslashes (`D:\a\...`), which broke the `CONFIGURE_DEPENDS` glob baked into
CMake's `VerifyGlobs.cmake` ("Invalid character escape '\a'"). The fix is a
`file(TO_CMAKE_PATH ...)` normalization before the `file(GLOB_RECURSE ...)`. macOS
and Linux are unaffected (their native paths use forward slashes).

## Implementation decisions and rejected alternatives

- **Native widget driver, not a JS SDK shim.** Every JS Matrix client (Element Web,
  Cinny, Sable) reimplements the widget driver on top of matrix-js-sdk. Komai has no
  JS SDK; rather than embed one, it uses `matrix_sdk::widget`, which is exactly the
  Element X path and handles capabilities, event relay, encrypted to-device, OpenID,
  and delayed events natively against the existing client.
- **QtWebEngine (Chromium), not a native MatrixRTC reimplementation.** matrix-rust-sdk
  gives the widget driver but no MatrixRTC media client, so a native UI would mean
  reimplementing the LiveKit client and the entire call interface. System webviews
  (WebKitGTK) have flaky desktop-Linux WebRTC and would mean maintaining three
  engines; CEF is the same weight with a worse Qt fit; lighter engines have no
  production WebRTC. A native client remains a far-future aspiration only.
- **Secure `komai-ec://` scheme, not `qrc://`/`file://`.** Chromium treats the
  latter as non-secure contexts, and `getUserMedia` requires a secure context. A
  custom scheme registered as secure (the Qt analog of Element X's
  `appassets.androidplatform.net` trick) serves the embedded bundle from a real,
  comparable origin.
- **QWebChannel as a dumb pipe, not Element Call's RPC transport.** Element Call
  speaks `window.postMessage`. QWebChannel is used only to carry intercepted
  postMessage strings out of the page (QtWebEngine has no `addJavascriptInterface`),
  with a filter so the driver never sees its own injected output.
- **Persistent per-Komai-profile webview profile, not off-the-record.** The default
  QML profile is in-memory, which wiped Element Call's `localStorage` (all its
  settings, including the saved mic/camera) and the per-origin device-id salt on
  every exit. A named persistent profile under the active Komai profile's dirs fixes
  it without leaking state between Komai profiles.
- **Capabilities auto-approved.** Element Call is a trusted first-party widget, so
  the provider grants the canned set without a consent prompt, matching Element X.

### Fullscreen is OS-window, not DOM

Komai's fullscreen makes the **OS window** borderless and fullscreen and reparents
the webview to cover it; it never puts the web content into the page's **DOM**
fullscreen (`element.requestFullscreen()`). DOM fullscreen would not enlarge the call
any further -- the webview already fills the window -- but it does make Chromium paint
its opaque-black `::backdrop` plus Element Call's near-black canvas around the call,
which looks worse than Element Call's normal call view at full size. (This is Element
Call's own look, reproducible in any Chromium browser embedding it, not a Komai bug.)

This forecloses keeping the two fullscreen notions in sync, and that was a deliberate
trade, not an oversight:

- **Two-way sync would force the dark backdrop.** Element Call reflects fullscreen
  *only* through `document.fullscreenElement` (`SpotlightTile.tsx` toggles its button
  on it). Making Element Call agree we are fullscreen therefore *requires* real DOM
  fullscreen -- which is exactly the dark backdrop we are avoiding. The clean look and
  two-way sync are mutually exclusive.
- **A native button cannot enter DOM fullscreen anyway.** `requestFullscreen()` needs
  a transient user activation (a real in-page gesture). Element Call's own button, a
  double-click on the video, or a key press in the page have one; a QML/native button
  click, `runJavaScript`, and synthetic `.click()` do not. So a header button could
  not drive DOM fullscreen even if we wanted it to.

Given Komai owns the fullscreen UI (header button + double-click + Escape + OSD), we
**disable the page's fullscreen support** (`settings.fullScreenSupportEnabled: false`,
so Chromium drops any `requestFullscreen()`) and **hide Element Call's own in-page
fullscreen button** with a CSS rule on its `aria-label="maximise"`
(`ElementCallWebProfile::bridgeUserScripts`), leaving a single, clean fullscreen
affordance. The two are belt-and-suspenders: if a future Element Call relabels the
button and the selector stops matching, disabled fullscreen support keeps the button an
inert no-op rather than a regression to the dark backdrop. The selector's fragility is
caught at the (already explicit) version bump by a marker check in
`bin/element-call/fetch.py`, which fails the build if `aria-label="maximise"` is no
longer present in the bundle.

## Call UI and UX decisions

Beyond the wiring, a number of choices shape how a call actually feels. They are
recorded here with the reasoning, since most are not obvious from the code alone.

- **A call lives in an in-room panel, not a separate window or floating overlay.**
  The panel reflows *above* the timeline (the timeline's top anchor binds to the
  panel's bottom) rather than overlaying it, so the room's scrollbar and content are
  never covered. It stays loaded for the whole call even when you switch rooms, which
  is what keeps the WebRTC session alive (destroying the webview would drop the call).
  A floating draggable picture-in-picture (Element Web's model) was considered and
  not pursued for the default; the in-room panel plus a global bar and room-list
  indicators already keep a call reachable from anywhere without DOM-reparenting
  gymnastics. Komai owns only collapsed / expanded / fullscreen; Element Call owns its
  own spotlight and grid layouts, so Komai does not build competing layout modes.
- **Closing a tab never ends a call; the call room is pinned against pruning.** A
  room with a live call (Element Call *or* legacy) is pinned so it cannot be evicted
  from the recently-closed timeline pool while the call is up, which would otherwise
  destroy the panel and silently tear the session down. There is deliberately no
  "prevent closing a tab with an active call" guard: closing a tab is non-destructive.
  The call keeps running with full audio/video while its tab is closed, the global bar
  and room-list indicators keep it reachable, and reopening the room tab restores the
  panel with nothing lost.
- **One consistent "call green" across every surface.** The in-room panel header, the
  global active bar, and the legacy active bar all use the same call green so an
  ongoing call reads the same wherever it appears. End-call actions use the theme's
  error red. The legacy 1:1 active bar was relocated to sit above the timeline (where
  the Element Call panel sits) so the two stacks present their "in a call" state in
  the same place.
- **Live-call presence: glow, badge, and a timeline tile.** A room with a live call
  gets a diffuse glow plus a corner badge on its avatar in both the room list and the
  tab bar. Calls you have joined glow in the call green; live calls in rooms you have
  *not* joined glow in the theme's warning hue (chosen over an attention hue, with
  error reserved for end-call). The glow follows the theme's hue but normalizes it to
  a bright, saturated value, because a raw semantic color (for example a dark warning
  on a light theme) washes out as a diffuse bloom. Element Call's start-of-call
  `m.rtc.notification` is also rendered as a "started a call" timeline tile with a
  Join button.
- **Incoming calls: ring for 1:1, silent for groups.** An addressed `ring`
  notification raises a global ring bar (Join / Decline) and plays the shared call
  ringtone; a silent group `notification` raises no ring. Both can raise a desktop
  notification with Join (and Decline, for rings) actions, suppressed when the app is
  focused and honoring the room's notify mode for group calls. Declining sends a real
  `m.rtc.decline` (which stops the ring on your other devices and makes a waiting DM
  caller leave) but does **not** bar you from joining later. See
  [Ringing and notifications](#ringing-and-notifications) for the wire details.
- **One composer call button with a menu.** When both call systems are enabled the
  composer shows a single call button that opens a small menu (Element Call / Legacy);
  when only one is enabled it acts directly. Element Call is offered in any room;
  legacy calls keep their existing DM / 2-person gate.
- **Leaving shows a busy state, and the drain is not shortened.** Hanging up runs
  Element Call's graceful drain (disconnect LiveKit, publish the leave membership),
  which takes a moment. Komai shows a "Leaving call…" busy state during it rather
  than freezing or cutting the drain short, because cutting it would leave a stale
  membership for other participants.
- **Dedicated Mute, Stop camera, and Fullscreen buttons in the panel header.** The
  panel header carries native Mute and Stop-camera toggles (mirroring Element Call's
  own state via `io.element.device_mute`) and a Fullscreen button, alongside the
  Collapse and End-call buttons. The mute and camera toggles matter most when the
  panel is collapsed to its thin header bar, where Element Call's own controls are not
  visible. The Fullscreen button surfaces a feature Element Call buries behind several
  clicks. These do not try to hide or replace Element Call's in-call controls: Element
  Call exposes no host API to pick an input *device* (selection happens inside
  Chromium) and its `showControls` is all-or-nothing, so device selection is left to
  Element Call's own settings.
- **Panel ergonomics: resize and fullscreen.** The panel has a draggable bottom
  border to set its height; that height is kept in memory across tab switches but
  deliberately resets on hangup and is **not** a persisted setting (a call is
  transient, so a sticky size would be surprising). Fullscreen is a true borderless OS
  window (the webview reparents and the window goes fullscreen); the header button and
  a double-click on the call view drive it, with Escape to exit. Fullscreen is an
  OS-window concept only, not the page's DOM fullscreen -- see
  [Fullscreen is OS-window, not DOM](#fullscreen-is-os-window-not-dom). In fullscreen,
  controls collapse into a dark translucent OSD bar pinned to the top of the screen
  (Mute / Stop camera / End call / Exit fullscreen); Exit fullscreen sits flush in the
  top-right corner so it is a large, easy pointer target (Fitts's law).
- **Settings → Calls leads with Element Call.** The Calls settings tab lists Element
  Call first, with the legacy sections greyed out (disabled, not hidden) when legacy
  calls are off (which is the default), reflecting that Element Call is the primary
  path.
- **`call.member` state churn is hidden from the timeline.** MatrixRTC membership
  state events (`org.matrix.msc3401.call.member`) are unrenderable noise, so they are
  hard-suppressed in the timeline alongside the legacy call status events.
- **Settings link colors use `TextEdit`, not `Text`.** A plain QML `Text` hardcodes
  link blue and ignores `palette.link`; the read-only `TextEdit` used for the Calls
  settings notes honors the Link palette role (and is selectable).

## Gotchas and limitations

- **No SFU, no call.** See [Hard requirements](#hard-requirements). The lobby loads
  but a call cannot connect without a MatrixRTC backend.
- **`registerUrlScheme()` runs before logging is up.** It executes before
  `QApplication` and before `komai::logging::init()`; logging there crashes the
  not-yet-initialized Rust tracing subscriber. Never log in that function.
- **`WebEngineView.profile` is a `QQuickWebEngineProfile*`**, not the C++
  `QWebEngineProfile*`. Assigning the C++ type silently fails and the view falls
  back to the default profile (no scheme handler), so `komai-ec://` aborts to a
  blank page. Install scheme handlers on the QML profile type.
- **Qt 6 `WebEngineScript` is a value type**, not a creatable QML element. The
  bridge scripts are built in C++ (`bridgeUserScripts`) and handed to QML.
- **E2EE follows the room, not the URL.** Element Call's effective encryption is
  per-participant only when the Matrix room is encrypted; in an unencrypted room the
  LiveKit publisher logs a benign "e2ee not configured" and the call connects
  unencrypted. `perParticipantE2EE=true` is correct and simply ignored for the
  choice. For an encrypted call, use an encrypted room.
- **`experimental-widgets` is an experimental SDK feature**, so its API can churn
  across matrix-sdk upgrades.
- **Async webview resize overflow.** QtWebEngine resizes its render surface
  asynchronously, so during a panel resize or fullscreen exit the webview could
  briefly paint past its slot onto the timeline beneath it. A scene-graph `clip: true`
  on the webview's slot cuts the overflow and resolves it.

## Related

- [Legacy calls user guide](../user-guide/features/legacy-calls.md) -- the parallel legacy 1:1 stack
- [Rust in Komai](rust.md) -- the CXX-bridge interop pattern
- [Notifications](notifications.md) -- the desktop-notification pipeline call
  notifications reuse
- [Emoji Architecture](emojis.md) -- the pinned-and-fetched bundle pattern this
  mirrors
- `src/rust/src/matrix_backend/runtime_element_call.rs` -- widget driver glue
- `src/rust/src/matrix_backend/runtime_rtc.rs` -- ring/decline receive side
- `src/voip/ElementCall*.{h,cpp}` -- webview host, session, controller
- `resources/qml/voip/ElementCall*.qml` -- call surfaces
- [Element Call](https://github.com/element-hq/element-call) -- the embedded web app
