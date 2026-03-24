// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QTimer>
#include <cstdint>
#include <string>

class SSOHandler final : public QObject
{
    Q_OBJECT

public:
    SSOHandler(QObject *parent = nullptr);

    ~SSOHandler();

    std::string url() const;

signals:
    void ssoSuccess(std::string token);
    void ssoFailed();

private:
    void pollStatus();

    static std::string loadSvgLogo();
    static std::string pageHtml(bool success);

    QTimer pollTimer;
    uint64_t listenerId = 0;
    std::string callbackUrl;
};
