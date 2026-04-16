// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#include <QGuiApplication>

#include "CallManager.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"

#ifdef XCB_AVAILABLE
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#endif

#ifdef Q_OS_WINDOWS
#include <Windows.h>
#endif

#ifdef GSTREAMER_AVAILABLE
extern "C"
{
#include "gst/gst.h"
}
#endif

using webrtc::ScreenShareType;

#ifdef GSTREAMER_AVAILABLE
namespace {

GstElement *pipe_        = nullptr;
unsigned int busWatchId_ = 0;

void
close_preview_stream()
{
    if (pipe_) {
        gst_element_set_state(GST_ELEMENT(pipe_), GST_STATE_NULL);
        gst_object_unref(pipe_);
        pipe_ = nullptr;
    }
    if (busWatchId_) {
        g_source_remove(busWatchId_);
        busWatchId_ = 0;
    }
}

gboolean
newBusMessage(GstBus *bus G_GNUC_UNUSED, GstMessage *msg, gpointer G_GNUC_UNUSED)
{
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        close_preview_stream();
        break;
    case GST_MESSAGE_ERROR: {
        GError *err     = nullptr;
        gchar *dbg_info = nullptr;
        gst_message_parse_error(msg, &err, &dbg_info);
        komai::logging::ui()->error("GST error: {}", dbg_info);
        g_error_free(err);
        g_free(dbg_info);
        close_preview_stream();
        break;
    }
    default:
        break;
    }
    return TRUE;
}

GstElement *
make_preview_sink()
{
    if (QGuiApplication::platformName() == QStringLiteral("wayland")) {
        return gst_element_factory_make("waylandsink", nullptr);
    } else if (QGuiApplication::platformName() == QStringLiteral("windows")) {
        return gst_element_factory_make("d3d11videosink", nullptr);
    } else {
        return gst_element_factory_make("ximagesink", nullptr);
    }
}

} // namespace
#endif

bool
CallManager::screenShareReady() const
{
#ifdef GSTREAMER_AVAILABLE
    if (screenShareUsesWindowPicker(screenShareType_)) {
        return true;
    } else {
        return ScreenCastPortal::instance().ready();
    }
#else
    return false;
#endif
}

QStringList
CallManager::screenShareTypeList()
{
    QStringList ret;
    ret.reserve(3);
    for (ScreenShareType type : screenShareTypes_) {
        switch (type) {
        case ScreenShareType::X11:
            ret.append(tr("X11"));
            break;
        case ScreenShareType::D3D11:
            ret.append("DirectX 11");
            break;
        case ScreenShareType::XDP:
            ret.append(tr("PipeWire"));
            break;
        }
    }

    return ret;
}

QStringList
CallManager::windowList()
{
    if (std::none_of(screenShareTypes_.begin(), screenShareTypes_.end(), [](ScreenShareType type) {
            return screenShareUsesWindowPicker(type);
        })) {
        return {};
    }

    windows_.clear();
    windows_.push_back({tr("Entire screen"), 0});

#ifdef XCB_AVAILABLE
    std::unique_ptr<xcb_connection_t, std::function<void(xcb_connection_t *)>> connection(
      xcb_connect(nullptr, nullptr), [](xcb_connection_t *c) { xcb_disconnect(c); });
    if (xcb_connection_has_error(connection.get())) {
        komai::logging::ui()->error("Failed to connect to X server");
        return {};
    }

    xcb_ewmh_connection_t ewmh;
    if (!xcb_ewmh_init_atoms_replies(
          &ewmh, xcb_ewmh_init_atoms(connection.get(), &ewmh), nullptr)) {
        komai::logging::ui()->error("Failed to connect to EWMH server");
        return {};
    }
    std::unique_ptr<xcb_ewmh_connection_t, std::function<void(xcb_ewmh_connection_t *)>>
      ewmhconnection(&ewmh, [](xcb_ewmh_connection_t *c) { xcb_ewmh_connection_wipe(c); });

    for (int i = 0; i < ewmh.nb_screens; i++) {
        xcb_ewmh_get_windows_reply_t clients;
        if (!xcb_ewmh_get_client_list_reply(
              &ewmh, xcb_ewmh_get_client_list(&ewmh, i), &clients, nullptr)) {
            komai::logging::ui()->error("Failed to request window list");
            return {};
        }

        for (uint32_t w = 0; w < clients.windows_len; w++) {
            xcb_window_t window = clients.windows[w];

            std::string name;
            xcb_ewmh_get_utf8_strings_reply_t data;
            auto getName = [](xcb_ewmh_get_utf8_strings_reply_t *r) {
                std::string name(r->strings, r->strings_len);
                xcb_ewmh_get_utf8_strings_reply_wipe(r);
                return name;
            };

            xcb_get_property_cookie_t cookie = xcb_ewmh_get_wm_name(&ewmh, window);
            if (xcb_ewmh_get_wm_name_reply(&ewmh, cookie, &data, nullptr))
                name = getName(&data);

            cookie = xcb_ewmh_get_wm_visible_name(&ewmh, window);
            if (xcb_ewmh_get_wm_visible_name_reply(&ewmh, cookie, &data, nullptr))
                name = getName(&data);

            windows_.push_back({QString::fromStdString(name), window});
        }
        xcb_ewmh_get_windows_reply_wipe(&clients);
    }
#endif
#ifdef Q_OS_WINDOWS
    for (HWND windowHandle = GetTopWindow(nullptr); windowHandle != nullptr;
         windowHandle      = GetNextWindow(windowHandle, GW_HWNDNEXT)) {
        if (!IsWindowVisible(windowHandle))
            continue;

        int titleLength = GetWindowTextLengthW(windowHandle);
        if (titleLength == 0)
            continue;

        if (GetWindowLong(windowHandle, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
            continue;

        TITLEBARINFO titleInfo;
        titleInfo.cbSize = sizeof(titleInfo);
        GetTitleBarInfo(windowHandle, &titleInfo);
        if (titleInfo.rgstate[0] & STATE_SYSTEM_INVISIBLE)
            continue;

        wchar_t *windowTitle = new wchar_t[titleLength + 1];
        GetWindowTextW(windowHandle, windowTitle, titleLength + 1);

        windows_.push_back(
          {QString::fromWCharArray(windowTitle), reinterpret_cast<uint64_t>(windowHandle)});
    }
#endif
    QStringList ret;
    assert(windows_.size() < std::numeric_limits<int>::max());
    ret.reserve(static_cast<int>(windows_.size()));
    for (const auto &w : windows_)
        ret.append(w.first);

    return ret;
}

void
CallManager::previewWindow(unsigned int index) const
{
#ifdef GSTREAMER_AVAILABLE
    if (!gst_is_initialized())
        return;

    if (pipe_ != nullptr) {
        komai::logging::ui()->warn("Preview already started");
        return;
    }

    if (screenShareType_ == ScreenShareType::X11 &&
        (windows_.empty() || index >= windows_.size())) {
        komai::logging::ui()->error("X11 screencast not available");
        return;
    }

    if (screenShareType_ == ScreenShareType::D3D11 &&
        (windows_.empty() || index >= windows_.size())) {
        komai::logging::ui()->error("D3D11 screencast not available");
        return;
    }

    auto settings = ChatPage::instance()->userSettings();

    pipe_ = gst_pipeline_new(nullptr);

    GstElement *videoconvert = gst_element_factory_make("videoconvert", nullptr);
    GstElement *videoscale   = gst_element_factory_make("videoscale", nullptr);
    GstElement *capsfilter   = gst_element_factory_make("capsfilter", nullptr);
    GstElement *preview_sink = make_preview_sink();
    GstElement *videorate    = gst_element_factory_make("videorate", nullptr);

    gst_bin_add_many(
      GST_BIN(pipe_), videorate, videoconvert, videoscale, capsfilter, preview_sink, nullptr);

    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "framerate",
                                        GST_TYPE_FRACTION,
                                        settings->callsScreenshareFrameRate(),
                                        1,
                                        nullptr);
    g_object_set(capsfilter, "caps", caps, nullptr);
    gst_caps_unref(caps);

    GstElement *screencastsrc = nullptr;
    if (screenShareType_ == ScreenShareType::X11) {
        GstElement *ximagesrc = gst_element_factory_make("ximagesrc", nullptr);
        if (!ximagesrc) {
            komai::logging::ui()->error("Failed to create ximagesrc");
            gst_object_unref(pipe_);
            pipe_ = nullptr;
            return;
        }
        g_object_set(ximagesrc, "use-damage", FALSE, nullptr);
        g_object_set(ximagesrc, "xid", windows_[index].second, nullptr);
        g_object_set(ximagesrc, "show-pointer", settings->callsScreenshareShowCursor(), nullptr);
        g_object_set(ximagesrc, "do-timestamp", (gboolean)1, nullptr);

        gst_bin_add(GST_BIN(pipe_), ximagesrc);
        screencastsrc = ximagesrc;
    } else if (screenShareType_ == ScreenShareType::D3D11) {
        GstElement *d3d11screensrc = gst_element_factory_make("d3d11screencapturesrc", nullptr);
        if (!d3d11screensrc) {
            komai::logging::ui()->error("Failed to create d3d11screencapturesrc");
            gst_object_unref(pipe_);
            pipe_ = nullptr;
            return;
        }
        g_object_set(d3d11screensrc, "window-handle", windows_[index].second, nullptr);
        g_object_set(
          d3d11screensrc, "show-cursor", settings->callsScreenshareShowCursor(), nullptr);

        gst_bin_add(GST_BIN(pipe_), d3d11screensrc);
        screencastsrc = d3d11screensrc;
    } else {
        ScreenCastPortal &sc_portal            = ScreenCastPortal::instance();
        const ScreenCastPortal::Stream *stream = sc_portal.getStream();
        if (stream == nullptr) {
            komai::logging::ui()->error("xdg-desktop-portal stream not started");
            gst_object_unref(pipe_);
            pipe_ = nullptr;
            return;
        }
        GstElement *pipewiresrc = gst_element_factory_make("pipewiresrc", nullptr);
        g_object_set(pipewiresrc, "fd", (gint)stream->fd.fileDescriptor(), nullptr);
        std::string path = std::to_string(stream->nodeId);
        g_object_set(pipewiresrc, "path", path.c_str(), nullptr);
        g_object_set(pipewiresrc, "do-timestamp", (gboolean)1, nullptr);

        gst_bin_add(GST_BIN(pipe_), pipewiresrc);
        screencastsrc = pipewiresrc;
    }

    if (!gst_element_link_many(
          screencastsrc, videorate, videoconvert, videoscale, capsfilter, preview_sink, nullptr)) {
        komai::logging::ui()->error("Failed to link preview window elements");
        gst_object_unref(pipe_);
        pipe_ = nullptr;
        return;
    }

    if (gst_element_set_state(pipe_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        komai::logging::ui()->error("Unable to start preview pipeline");
        gst_object_unref(pipe_);
        pipe_ = nullptr;
        return;
    }

    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipe_));
    busWatchId_ = gst_bus_add_watch(bus, newBusMessage, nullptr);
    gst_object_unref(bus);
#else
    (void)index;
#endif
}

void
CallManager::setupScreenShareXDP()
{
#ifdef GSTREAMER_AVAILABLE
    ScreenCastPortal &sc_portal = ScreenCastPortal::instance();
    sc_portal.init();
#endif
}

void
CallManager::setScreenShareType(unsigned int index)
{
#ifdef GSTREAMER_AVAILABLE
    closeScreenShare();
    if (index >= screenShareTypes_.size())
        komai::logging::ui()->error("WebRTC: Screen share type index out of range");
    screenShareType_ = screenShareTypes_[index];
    emit screenShareChanged();
#else
    (void)index;
#endif
}

void
CallManager::closeScreenShare()
{
#ifdef GSTREAMER_AVAILABLE
    close_preview_stream();
    if (!isOnCall()) {
        ScreenCastPortal &sc_portal = ScreenCastPortal::instance();
        sc_portal.close();
    }
#endif
}
