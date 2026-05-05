# 📞 Calls

Komai's call story today is **partial and disabled by default**. This page sets honest expectations and explains where things are headed.


## ⚠️ Status at a glance

| Path | Komai today |
| --- | --- |
| **Voice (1:1)** with another legacy-calls client | ✅ Works |
| **Video (1:1)** | ⚠️ Local capture works; the remote side does not receive frames (`framesReceived` stays at 0) |
| **Screen sharing (1:1)** | ⚠️ Same as video — local pipeline runs, remote doesn't receive |
| **Group calls (Element Call / MatrixRTC)** | ❌ Not implemented; no ETA |

[Element Call](https://github.com/element-hq/element-call) and the underlying [MatrixRTC](https://github.com/matrix-org/matrix-spec-proposals/pull/4143) framework are where the Matrix ecosystem is heading. Modern clients like [Element X](https://github.com/element-hq/element-x-android) have already dropped legacy calls entirely. Komai will follow that direction (see [Element Call](#-element-call)), but isn't there yet.


## 🛡️ Disabled by default

Legacy calls are off by default. To turn them on:

1. Open **Settings → Calls**.
2. Under **General**, toggle **Enable legacy calls** on.
3. Optionally enable **Use turn.matrix.org as fallback relay** if your homeserver doesn't provide a TURN/STUN server of its own.

Once enabled, you'll see the call buttons in the composer toolbar of any direct chat (or any 2-person room) and you'll be able to receive incoming legacy-call invites.


## 📞 Legacy calls — what does and doesn't work

Komai inherits its 1:1 voice/video/screen-share implementation from [Nheko](https://github.com/Nheko-Reborn/nheko), which uses GStreamer and `webrtcbin` to bridge to the legacy Matrix call protocol (`m.call.invite` / `m.call.answer` / `m.call.candidates`).

### ✅ Voice calls

Voice has been confirmed end-to-end against Element Web. The audio path uses `pulsesrc` / `autoaudiosink` and Opus over RTP, and rings, ICE negotiation, hold/resume, mute, and hangup all behave.

### ⚠️ Video and screen-sharing

Outgoing video and outgoing screen-share both **fail on the remote side**: the local camera/screen captures, the encoder runs, packets leave webrtcbin, but the remote client (tested against Element Web) reports `framesReceived: 0`. Incoming video decodes correctly when the remote sends it — the asymmetry is on Komai's send-side.

This was already partly broken in upstream Nheko and we have not fully untangled it. The legacy call code paths are accessible behind the toggle so contributors can keep poking at them, but they aren't dependable for day-to-day video calls today.

### 📋 Other limitations

- **No group calls.** Legacy Matrix calls are 1:1 only. If you try to call a multi-person room, the call button is disabled with a tooltip explaining why (rooms must be flagged direct, or contain exactly two members).


## 🌐 Element Call

[Element Call](https://github.com/element-hq/element-call) is the modern Matrix calling system: WebRTC SFU-backed group calls, end-to-end encrypted, with first-class clients on every major platform. It's based on [MSC4143](https://github.com/matrix-org/matrix-spec-proposals/pull/4143) (MatrixRTC) and is what newer clients implement.

**Komai does not implement Element Call yet.** There is no estimated date for that work; it's a substantial project (it would touch the Rust matrix-sdk side, the web-stack rendering for the call UI, and the device-management story for participating in MatrixRTC sessions). When it lands, we'll likely *also* drop the legacy 1:1 call code, since the broader Matrix ecosystem is consolidating around the new stack.

If you need group calling today, your best bet is to install Element Call standalone and join calls through the room widget there.
