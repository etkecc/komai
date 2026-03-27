// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <cstdint>

class SSOHandler final : public QObject
{
    Q_OBJECT

public:
    SSOHandler(QObject *parent = nullptr);

    ~SSOHandler();

    QString url() const;

signals:
    void callbackSuccess(const QString &callbackQuery, const QString &loginToken);
    void ssoFailed();

private:
    void pollStatus();

    static std::string loadSvgLogo();
    static std::string pageHtml(bool success);

    QTimer pollTimer;
    uint64_t listenerId = 0;
    QString callbackUrl;
};
