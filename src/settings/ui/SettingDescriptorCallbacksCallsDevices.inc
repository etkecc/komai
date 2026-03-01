// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QStringList
callMicrophoneValuesList()
{
    auto i = I;
    if (!i)
        return {};
    if (!CallDevices::instance().ensureInitialized())
        return {};
    return vecToList(CallDevices::instance().names(false, i->callsDevicesMicrophone().toStdString()));
}

static QVariant
callMicrophoneValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callMicrophoneValuesList().indexOf(i->callsDevicesMicrophone());
}

static bool
setCallMicrophoneValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyStringListSelection(v, callMicrophoneValuesList(), [i](const QString &choice) {
        i->setCallsDevicesMicrophone(choice);
    });
}

static QVariant
callMicrophoneValues()
{
    return callMicrophoneValuesList();
}

static QStringList
callCameraValuesList()
{
    auto i = I;
    if (!i)
        return {};
    if (!CallDevices::instance().ensureInitialized())
        return {};
    return vecToList(CallDevices::instance().names(true, i->callsDevicesCamera().toStdString()));
}

static QVariant
callCameraValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraValuesList().indexOf(i->callsDevicesCamera());
}

static bool
setCallCameraValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyStringListSelection(v, callCameraValuesList(), [i](const QString &choice) {
        i->setCallsDevicesCamera(choice);
    });
}

static QVariant
callCameraValues()
{
    return callCameraValuesList();
}

static QStringList
callCameraResolutionValuesList()
{
    auto i = I;
    if (!i)
        return {};
    if (!CallDevices::instance().ensureInitialized())
        return {};
    return vecToList(CallDevices::instance().resolutions(i->callsDevicesCamera().toStdString()));
}

static QVariant
callCameraResolutionValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraResolutionValuesList().indexOf(i->callsDevicesCameraResolution());
}

static bool
setCallCameraResolutionValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyStringListSelection(
      v, callCameraResolutionValuesList(), [i](const QString &choice) { i->setCallsDevicesCameraResolution(choice); });
}

static QVariant
callCameraResolutionValues()
{
    return callCameraResolutionValuesList();
}

static QStringList
callCameraFrameRateValuesList()
{
    auto i = I;
    if (!i)
        return {};
    if (!CallDevices::instance().ensureInitialized())
        return {};

    return vecToList(CallDevices::instance().frameRates(i->callsDevicesCamera().toStdString(),
                                                        i->callsDevicesCameraResolution().toStdString()));
}

static QVariant
callCameraFrameRateValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraFrameRateValuesList().indexOf(i->callsDevicesCameraFrameRate());
}

static bool
setCallCameraFrameRateValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyStringListSelection(
      v, callCameraFrameRateValuesList(), [i](const QString &choice) { i->setCallsDevicesCameraFrameRate(choice); });
}

static QVariant
callCameraFrameRateValues()
{
    return callCameraFrameRateValuesList();
}
