// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstring>
#include <optional>
#include <string_view>

#include "CallDevices.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"

#ifdef GSTREAMER_AVAILABLE
extern "C"
{
#include "gst/gst.h"
}
#endif

CallDevices::CallDevices()
  : QObject()
{
}

#ifdef GSTREAMER_AVAILABLE
namespace {

struct AudioSource
{
    std::string name;
    GstDevice *device;
};

struct VideoSource
{
    struct Caps
    {
        std::string resolution;
        std::vector<std::string> frameRates;
    };
    std::string name;
    GstDevice *device;
    std::vector<Caps> caps;
};

std::vector<AudioSource> audioSources_;
std::vector<VideoSource> videoSources_;

using FrameRate = std::pair<int, int>;
std::optional<FrameRate>
getFrameRate(const GValue *value)
{
    if (GST_VALUE_HOLDS_FRACTION(value)) {
        gint num = gst_value_get_fraction_numerator(value);
        gint den = gst_value_get_fraction_denominator(value);
        return FrameRate{num, den};
    }
    return std::nullopt;
}

void
addFrameRate(std::vector<std::string> &rates, const FrameRate &rate)
{
    rates.push_back(std::to_string(rate.first) + "/" + std::to_string(rate.second));
}

// Pick the most useful capture frame rate from the camera's reported list.
// Cameras often list ascending (5/10/15/30) so blindly picking the first
// entry under-feeds the encoder; we prefer the highest frame rate that
// caps at 30fps (sweet spot for live encoding) and fall back to the
// highest available, then to the first entry as a last resort.
std::string
preferredFrameRate(const std::vector<std::string> &rates)
{
    if (rates.empty())
        return {};

    auto numericRate = [](const std::string &spec) -> double {
        const auto slash = spec.find('/');
        if (slash == std::string::npos)
            return 0.0;
        try {
            const double num = std::stod(spec.substr(0, slash));
            const double den = std::stod(spec.substr(slash + 1));
            return den > 0 ? num / den : 0.0;
        } catch (...) {
            return 0.0;
        }
    };

    const std::string *bestUnder30 = nullptr;
    double bestUnder30Rate         = 0.0;
    const std::string *bestAny     = nullptr;
    double bestAnyRate             = 0.0;
    for (const auto &spec : rates) {
        const double r = numericRate(spec);
        if (r > bestAnyRate) {
            bestAnyRate = r;
            bestAny     = &spec;
        }
        if (r > bestUnder30Rate && r <= 30.0) {
            bestUnder30Rate = r;
            bestUnder30     = &spec;
        }
    }
    if (bestUnder30)
        return *bestUnder30;
    if (bestAny)
        return *bestAny;
    return rates.front();
}

void
setDefaultDevice(bool isVideo)
{
    auto settings = UserSettings::instance();
    if (isVideo && settings->callsDevicesCamera().isEmpty()) {
        const VideoSource &camera = videoSources_.front();
        settings->setCallsDevicesCamera(QString::fromStdString(camera.name));
        settings->setCallsDevicesCameraResolution(
          QString::fromStdString(camera.caps.front().resolution));
        settings->setCallsDevicesCameraFrameRate(
          QString::fromStdString(preferredFrameRate(camera.caps.front().frameRates)));
    } else if (!isVideo && settings->callsDevicesMicrophone().isEmpty()) {
        settings->setCallsDevicesMicrophone(QString::fromStdString(audioSources_.front().name));
    }
}

void
addDevice(GstDevice *device)
{
    if (!device)
        return;

    gchar *name  = gst_device_get_display_name(device);
    gchar *type  = gst_device_get_device_class(device);
    bool isVideo = !std::strncmp(type, "Video", 5);
    g_free(type);
    komai::logging::ui()->debug("WebRTC: {} device added: {}", isVideo ? "video" : "audio", name);
    if (!isVideo) {
        audioSources_.push_back({name, device});
        g_free(name);
        setDefaultDevice(false);
        return;
    }

    GstCaps *gstcaps = gst_device_get_caps(device);
    if (!gstcaps) {
        komai::logging::ui()->debug("WebRTC: unable to get caps for {}", name);
        g_free(name);
        return;
    }

    VideoSource source{name, device, {}};
    g_free(name);
    guint nCaps = gst_caps_get_size(gstcaps);
    for (guint i = 0; i < nCaps; ++i) {
        GstStructure *structure  = gst_caps_get_structure(gstcaps, i);
        const gchar *struct_name = gst_structure_get_name(structure);
        if (!std::strcmp(struct_name, "video/x-raw")) {
            gint widthpx, heightpx;
            if (gst_structure_get(structure,
                                  "width",
                                  G_TYPE_INT,
                                  &widthpx,
                                  "height",
                                  G_TYPE_INT,
                                  &heightpx,
                                  nullptr)) {
                VideoSource::Caps caps;
                caps.resolution     = std::to_string(widthpx) + "x" + std::to_string(heightpx);
                const GValue *value = gst_structure_get_value(structure, "framerate");
                if (auto fr = getFrameRate(value); fr)
                    addFrameRate(caps.frameRates, *fr);
                else if (GST_VALUE_HOLDS_FRACTION_RANGE(value)) {
                    addFrameRate(caps.frameRates,
                                 *getFrameRate(gst_value_get_fraction_range_min(value)));
                    addFrameRate(caps.frameRates,
                                 *getFrameRate(gst_value_get_fraction_range_max(value)));
                } else if (GST_VALUE_HOLDS_LIST(value)) {
                    guint nRates = gst_value_list_get_size(value);
                    for (guint j = 0; j < nRates; ++j) {
                        const GValue *rate = gst_value_list_get_value(value, j);
                        if (auto frate = getFrameRate(rate); frate)
                            addFrameRate(caps.frameRates, *frate);
                    }
                }
                if (!caps.frameRates.empty())
                    source.caps.push_back(std::move(caps));
            }
        }
    }
    gst_caps_unref(gstcaps);
    videoSources_.push_back(std::move(source));
    setDefaultDevice(true);
}

template<typename T>
bool
removeDevice(T &sources, GstDevice *device, bool changed)
{
    if (auto it = std::find_if(
          sources.begin(), sources.end(), [device](const auto &s) { return s.device == device; });
        it != sources.end()) {
        komai::logging::ui()->debug(
          "WebRTC: device {}: {}", (changed ? "changed" : "removed"), it->name);
        gst_object_unref(device);
        sources.erase(it);
        return true;
    }
    return false;
}

void
removeDevice(GstDevice *device, bool changed)
{
    if (device) {
        if (removeDevice(audioSources_, device, changed) ||
            removeDevice(videoSources_, device, changed))
            return;
    }
}

gboolean
newBusMessage(GstBus *bus G_GNUC_UNUSED, GstMessage *msg, gpointer user_data G_GNUC_UNUSED)
{
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_DEVICE_ADDED: {
        GstDevice *device;
        gst_message_parse_device_added(msg, &device);
        addDevice(device);
        emit CallDevices::instance().devicesChanged();
        break;
    }
    case GST_MESSAGE_DEVICE_REMOVED: {
        GstDevice *device;
        gst_message_parse_device_removed(msg, &device);
        removeDevice(device, false);
        emit CallDevices::instance().devicesChanged();
        break;
    }
    case GST_MESSAGE_DEVICE_CHANGED: {
        GstDevice *device;
        GstDevice *oldDevice;
        gst_message_parse_device_changed(msg, &device, &oldDevice);
        removeDevice(oldDevice, true);
        addDevice(device);
        break;
    }
    default:
        break;
    }
    return TRUE;
}

template<typename T>
std::vector<std::string>
deviceNames(T &sources, const std::string &defaultDevice)
{
    std::vector<std::string> ret;
    ret.reserve(sources.size());
    for (const auto &s : sources)
        ret.push_back(s.name);

    // move default device to top of the list
    if (auto it = std::find(ret.begin(), ret.end(), defaultDevice); it != ret.end())
        std::swap(ret.front(), *it);

    return ret;
}

std::optional<VideoSource>
getVideoSource(const std::string &cameraName)
{
    if (auto it = std::find_if(videoSources_.cbegin(),
                               videoSources_.cend(),
                               [&cameraName](const auto &s) { return s.name == cameraName; });
        it != videoSources_.cend()) {
        return *it;
    }
    return std::nullopt;
}

std::pair<int, int>
tokenise(std::string_view str, char delim)
{
    std::pair<int, int> ret;
    ret.first  = std::atoi(str.data());
    auto pos   = str.find_first_of(delim);
    ret.second = std::atoi(str.data() + pos + 1);
    return ret;
}
}

static GstDeviceMonitor *monitor = nullptr;

bool
CallDevices::ensureInitialized(std::string *errorMessage)
{
    if (!gst_is_initialized()) {
        GError *error = nullptr;
        if (!gst_init_check(nullptr, nullptr, &error)) {
            std::string strError("WebRTC: failed to initialise GStreamer: ");
            if (error) {
                strError += error->message;
                g_error_free(error);
            }
            komai::logging::ui()->error(strError);
            if (errorMessage)
                *errorMessage = strError;
            return false;
        }

        // pipewiresrc (from PipeWire's gst-pipewire plugin) constructs caps
        // with a degenerate `GstIntRange[N, N]` during portal stream
        // negotiation. GStreamer's int-range constructor asserts start <
        // end, producing a torrent of "range start is not smaller than end"
        // criticals during every screen-share request. The pipeline still
        // works -- it's a noisy upstream bug -- so we filter just that
        // exact assertion text out of the GStreamer log domain rather than
        // the user's terminal.
        g_log_set_handler(
          "GStreamer",
          GLogLevelFlags(G_LOG_LEVEL_CRITICAL | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION),
          [](const gchar *log_domain,
             GLogLevelFlags log_level,
             const gchar *message,
             gpointer user_data) {
              if (message) {
                  if (std::strstr(message, "gst_value_collect_int_range") ||
                      std::strstr(message, "range start is not smaller than end")) {
                      return;
                  }
              }
              g_log_default_handler(log_domain, log_level, message, user_data);
          },
          nullptr);

        gchar *version = gst_version_string();
        komai::logging::ui()->info("WebRTC: initialised {}", version);
        g_free(version);
    }

    init();
    return true;
}

void
CallDevices::init()
{
    if (!monitor) {
        monitor       = gst_device_monitor_new();
        GstCaps *caps = gst_caps_new_empty_simple("audio/x-raw");
        gst_device_monitor_add_filter(monitor, "Audio/Source", caps);
        gst_device_monitor_add_filter(monitor, "Audio/Duplex", caps);
        gst_caps_unref(caps);
        caps = gst_caps_new_empty_simple("video/x-raw");
        gst_device_monitor_add_filter(monitor, "Video/Source", caps);
        gst_device_monitor_add_filter(monitor, "Video/Duplex", caps);
        gst_caps_unref(caps);

        GstBus *bus = gst_device_monitor_get_bus(monitor);
        gst_bus_add_watch(bus, newBusMessage, nullptr);
        gst_object_unref(bus);
        if (!gst_device_monitor_start(monitor)) {
            komai::logging::ui()->error("WebRTC: failed to start device monitor");
            return;
        }
    }
}

void
CallDevices::deinit()
{
    if (monitor) {
        gst_device_monitor_stop(monitor);
        gst_object_unref(monitor);
        monitor = nullptr;
    }
}

bool
CallDevices::haveMic() const
{
    return !audioSources_.empty();
}

bool
CallDevices::haveCamera() const
{
    return !videoSources_.empty();
}

std::vector<std::string>
CallDevices::names(bool isVideo, const std::string &defaultDevice) const
{
    return isVideo ? deviceNames(videoSources_, defaultDevice)
                   : deviceNames(audioSources_, defaultDevice);
}

std::vector<std::string>
CallDevices::resolutions(const std::string &cameraName) const
{
    std::vector<std::string> ret;
    if (auto s = getVideoSource(cameraName); s) {
        ret.reserve(s->caps.size());
        for (const auto &c : s->caps)
            ret.push_back(c.resolution);
    }
    return ret;
}

std::vector<std::string>
CallDevices::frameRates(const std::string &cameraName, const std::string &resolution) const
{
    if (auto s = getVideoSource(cameraName); s) {
        if (auto it = std::find_if(s->caps.cbegin(),
                                   s->caps.cend(),
                                   [&](const auto &c) { return c.resolution == resolution; });
            it != s->caps.cend())
            return it->frameRates;
    }
    return {};
}

GstDevice *
CallDevices::audioDevice() const
{
    std::string name = UserSettings::instance()->callsDevicesMicrophone().toStdString();
    if (auto it = std::find_if(audioSources_.cbegin(),
                               audioSources_.cend(),
                               [&name](const auto &s) { return s.name == name; });
        it != audioSources_.cend()) {
        komai::logging::ui()->debug("WebRTC: microphone: {}", name);
        return it->device;
    } else {
        komai::logging::ui()->error("WebRTC: unknown microphone: {}", name);
        return nullptr;
    }
}

GstDevice *
CallDevices::videoDevice(std::pair<int, int> &resolution, std::pair<int, int> &frameRate) const
{
    auto settings    = UserSettings::instance();
    std::string name = settings->callsDevicesCamera().toStdString();
    if (auto s = getVideoSource(name); s) {
        komai::logging::ui()->debug("WebRTC: camera: {}", name);
        resolution = tokenise(settings->callsDevicesCameraResolution().toStdString(), 'x');
        frameRate  = tokenise(settings->callsDevicesCameraFrameRate().toStdString(), '/');

        // Migrate legacy stored values that were captured by the old
        // "first frame rate the camera reports" default. UVC cameras
        // commonly enumerate ascending (5/10/15/30), so existing profiles
        // ended up at 5fps which is way too low to feed vp8enc; the
        // remote ends up with bytesReceived > 0 but framesReceived = 0.
        // Substitute a sensible upper bound from the camera's actual
        // caps when the stored value is implausibly low for live video.
        const double storedRate = frameRate.second > 0 ? static_cast<double>(frameRate.first) /
                                                           static_cast<double>(frameRate.second)
                                                       : 0.0;
        if (storedRate < 10.0) {
            for (const auto &cap : s->caps) {
                if (cap.resolution != settings->callsDevicesCameraResolution().toStdString())
                    continue;
                const auto preferred = preferredFrameRate(cap.frameRates);
                if (preferred.empty())
                    break;
                auto upgraded = tokenise(preferred, '/');
                if (upgraded.second > 0 &&
                    static_cast<double>(upgraded.first) / upgraded.second > storedRate) {
                    komai::logging::ui()->info(
                      "WebRTC: stored camera frame rate {}/{} is too low for "
                      "live video; using {} from device caps instead",
                      frameRate.first,
                      frameRate.second,
                      preferred);
                    frameRate = upgraded;
                }
                break;
            }
        }

        komai::logging::ui()->debug(
          "WebRTC: camera resolution: {}x{}", resolution.first, resolution.second);
        komai::logging::ui()->debug(
          "WebRTC: camera frame rate: {}/{}", frameRate.first, frameRate.second);
        return s->device;
    } else {
        komai::logging::ui()->error("WebRTC: unknown camera: {}", name);
        return nullptr;
    }
}

#else

bool
CallDevices::ensureInitialized(std::string *errorMessage)
{
    (void)errorMessage;
    return false;
}

bool
CallDevices::haveMic() const
{
    return false;
}

bool
CallDevices::haveCamera() const
{
    return false;
}

std::vector<std::string>
CallDevices::names(bool, const std::string &) const
{
    return {};
}

std::vector<std::string>
CallDevices::resolutions(const std::string &) const
{
    return {};
}

std::vector<std::string>
CallDevices::frameRates(const std::string &, const std::string &) const
{
    return {};
}

#endif

#include "moc_CallDevices.cpp"
