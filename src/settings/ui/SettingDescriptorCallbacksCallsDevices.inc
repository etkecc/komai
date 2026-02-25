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
    return vecToList(CallDevices::instance().names(false, i->microphone().toStdString()));
}

static QVariant
callMicrophoneValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callMicrophoneValuesList().indexOf(i->microphone());
}

static bool
setCallMicrophoneValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyStringListSelection(v, callMicrophoneValuesList(), [i](const QString &choice) {
        i->setMicrophone(choice);
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
    return vecToList(CallDevices::instance().names(true, i->camera().toStdString()));
}

static QVariant
callCameraValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraValuesList().indexOf(i->camera());
}

static bool
setCallCameraValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyStringListSelection(v, callCameraValuesList(), [i](const QString &choice) {
        i->setCamera(choice);
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
    return vecToList(CallDevices::instance().resolutions(i->camera().toStdString()));
}

static QVariant
callCameraResolutionValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraResolutionValuesList().indexOf(i->cameraResolution());
}

static bool
setCallCameraResolutionValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyStringListSelection(
      v, callCameraResolutionValuesList(), [i](const QString &choice) { i->setCameraResolution(choice); });
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

    return vecToList(CallDevices::instance().frameRates(i->camera().toStdString(),
                                                        i->cameraResolution().toStdString()));
}

static QVariant
callCameraFrameRateValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraFrameRateValuesList().indexOf(i->cameraFrameRate());
}

static bool
setCallCameraFrameRateValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyStringListSelection(
      v, callCameraFrameRateValuesList(), [i](const QString &choice) { i->setCameraFrameRate(choice); });
}

static QVariant
callCameraFrameRateValues()
{
    return callCameraFrameRateValuesList();
}
