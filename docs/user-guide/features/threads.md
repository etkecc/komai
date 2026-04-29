# 💬 Threads

Threads let you branch a side-conversation off a message without losing the main flow of the room.

Komai uses **[Discord](https://discord.com/)-style threads**. By default, thread replies appear inline in the main timeline alongside everything else. Clicking a thread opens it on its own, so you can read just that conversation in isolation.

If you'd rather hide thread replies from the main timeline, see [Collapse thread replies](#-collapse-thread-replies) below.

See a [🖼️ Screenshot of a room with threads in the main timeline](../screenshots/timeline-with-collapse-thread-replies-threads.webp).


## ✨ Starting a thread

To start a new thread or reply in an existing one:

- Click the **Message actions** icon shown next to every message, then click the thread button in the actions toolbar -- it's labelled **New thread** for messages that aren't part of a thread, and **Reply in thread** for messages that are
- In Selection mode, press `t` on the focused or selected message (see [⌨️ Keyboard Shortcuts](keyboard-shortcuts.md))

When you're composing a thread reply, the composer shows a **Replying in a thread** indicator above the input.


## 🧵 Viewing a single thread

Clicking the **%n thread reply(s)** indicator on a thread root opens that thread in isolation -- the timeline switches to show only the root message and its replies, with a **Thread by … (N replies)** bar at the top. Press `Escape` (or click the close button on the bar) to return to the main timeline.

See a [🖼️ Screenshot of a thread opened in isolation](../screenshots/timeline-thread-view.webp).


## 🗂️ Browsing all threads in a room

The **Threads** button on the room header bar opens the **Threads** dialog -- a paginated list of every thread in the current room. Use it to discover threads you may have missed, find a thread you participated in earlier, or see everything that's currently being discussed in parallel. Click any entry to jump straight into that thread.

The dialog supports filtering between all threads in the room and just the threads you've participated in.

See a [🖼️ Screenshot of the Threads dialog](../screenshots/timeline-threads-list.webp).


## 📥 Collapse thread replies

By default, thread replies are interleaved into the main timeline.
This is good, because you can't miss messages. New messages always appear at the bottom of the timeline for you to see, regardless of their association (old thread; new thread; new thread).

The downside is: if a busy room has long, off-topic threads, this can drown out the main conversation.

**Collapse thread replies** hides non-root thread messages from the main timeline, leaving only the thread root visible. To read or continue a thread, click its **%n thread reply(s)** indicator and the thread opens in its own view. The downside is that if some old thread is revived by a new message, you may never learn about it. See [⚠️ Caveat: per-thread unread tracking](#%EF%B8%8F-caveat-per-thread-unread-tracking) below.

See a [🖼️ Screenshot of a collapsed thread root in the main timeline](../screenshots/timeline-thread-collapse-thread-replies.webp).


### ⚙️ Global setting

The global toggle lives in **[Settings](../settings/README.md) → Timeline → Collapse thread replies**. It's off by default.

Config key: `timeline.threads.collapse_replies.global`


### 🏠 Per-room override

You can override the global setting on a per-room basis from **Room Info → Preferences → Collapse thread replies**, which offers three choices:

- **Global Default (currently: On / Off)** -- follow the global setting (this is the default for every room)
- **On** -- collapse replies in this room regardless of the global setting
- **Off** -- show replies inline in this room regardless of the global setting

This is handy when you want collapsing on globally but kept off in a few small rooms where threads are short and worth seeing in context (or vice versa).

Config key: `timeline.threads.collapse_replies.by_room` (a map of room ID → boolean)


### ⚠️ Caveat: per-thread unread tracking

Komai does **not** currently track unread state per thread. When replies are collapsed, you may miss new replies to older threads because the main timeline won't surface them.

If you rely heavily on threads, consider leaving collapsing off in the rooms where you do, and using the per-room override to keep it on elsewhere.
