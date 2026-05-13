// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

namespace komai::mac {

// Wraps NSApp.delegate so a dock-icon click on a running komai with no visible windows reaches us
// as a Qt-thread callback. macOS surfaces this as NSApplicationDelegate's
// applicationShouldHandleReopen:hasVisibleWindows: which Qt doesn't translate into any QEvent or
// signal, so without this wrapper a "start in tray" launch is unreachable from the dock until the
// user finds the menu-bar item. The wrapper installs itself in front of Qt's existing delegate and
// forwards every other selector unchanged. Idempotent — only the first call wires anything up.
void
installReopenHandler(std::function<void()> onReopen);

} // namespace komai::mac
