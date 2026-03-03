// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DeviceVerificationFlow.h"

#include <tuple>

#include <QDateTime>
#include <QTimer>

#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "timeline/TimelineModel.h"
#include "utils/Utils.h"

static constexpr int TIMEOUT = 2 * 60 * 1000; // 2 minutes

static constexpr std::string_view mac_method_alg_v1 = "hkdf-hmac-sha256";
static constexpr std::string_view mac_method_alg_v2 = "hkdf-hmac-sha256.v2";

DeviceVerificationFlow::DeviceVerificationFlow(QObject *,
                                               DeviceVerificationFlow::Type flow_type,
                                               TimelineModel *model,
                                               const QString &userID,
                                               const std::vector<QString> &deviceIds_)
  : sender(false)
  , type(flow_type)
  , deviceIds(std::move(deviceIds_))
  , model_(model)
{
    nhlog::crypto()->debug("CREATING NEW FLOW, {}, {}", static_cast<int>(flow_type), (void *)this);
    if (deviceIds.size() == 1)
        deviceId = deviceIds.front();

    timeout = new QTimer(this);
    timeout->setSingleShot(true);
    this->sas           = olm::client()->sas_init();
    this->isMacVerified = false;

    auto user_id_  = userID.toStdString();
    this->toClient = mtx::identifiers::parse<mtx::identifiers::User>(user_id_);
    cache::queryKeys(
      user_id_, [user_id_, this](const UserKeyCache &res, mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("failed to query device keys: {},{}",
                                 mtx::errors::to_string(err->matrix_error.errcode),
                                 static_cast<int>(err->status_code));
              return;
          }

          if (!this->deviceId.isEmpty() &&
              (res.device_keys.find(deviceId.toStdString()) == res.device_keys.end())) {
              nhlog::net()->warn("no devices retrieved {}", user_id_);
              return;
          }

          this->their_keys = res;
      });

    cache::queryKeys(http::client()->user_id().to_string(),
                     [this](const UserKeyCache &res, mtx::http::RequestErr err) {
                         if (err) {
                             nhlog::net()->warn("failed to query device keys: {},{}",
                                                mtx::errors::to_string(err->matrix_error.errcode),
                                                static_cast<int>(err->status_code));
                             return;
                         }

                         if (res.master_keys.keys.empty())
                             return;

                         if (auto status =
                               cache::verificationStatus(http::client()->user_id().to_string());
                             status && status->user_verified == crypto::Trust::Verified)
                             this->our_trusted_master_key = res.master_keys.keys.begin()->second;
                     });

    if (model) {
        connect(this->model_,
                &TimelineModel::updateFlowEventId,
                this,
                [this](const std::string &event_id_) {
                    this->relation.rel_type = mtx::common::RelationType::Reference;
                    this->relation.event_id = event_id_;
                    this->transaction_id    = event_id_;
                });
    }

    connect(timeout, &QTimer::timeout, this, [this]() {
        nhlog::crypto()->info("verification: timeout");
        if (state_ != Success && state_ != Failed)
            this->cancelVerification(DeviceVerificationFlow::Error::Timeout);
    });

    connect(ChatPage::instance(),
            &ChatPage::receivedDeviceVerificationStart,
            this,
            &DeviceVerificationFlow::handleStartMessage);
    connect(ChatPage::instance(),
            &ChatPage::receivedDeviceVerificationAccept,
            this,
            &DeviceVerificationFlow::handleVerificationAccept);

    connect(ChatPage::instance(),
            &ChatPage::receivedDeviceVerificationCancel,
            this,
            &DeviceVerificationFlow::handleVerificationCancel);

    connect(ChatPage::instance(),
            &ChatPage::receivedDeviceVerificationKey,
            this,
            &DeviceVerificationFlow::handleVerificationKey);

    connect(ChatPage::instance(),
            &ChatPage::receivedDeviceVerificationMac,
            this,
            &DeviceVerificationFlow::handleVerificationMac);

    connect(ChatPage::instance(),
            &ChatPage::receivedDeviceVerificationReady,
            this,
            &DeviceVerificationFlow::handleVerificationReady);

    connect(ChatPage::instance(),
            &ChatPage::receivedDeviceVerificationDone,
            this,
            &DeviceVerificationFlow::handleVerificationDone);

    timeout->start(TIMEOUT);
}

QString
DeviceVerificationFlow::state()
{
    switch (state_) {
    case PromptStartVerification:
        return QStringLiteral("PromptStartVerification");
    case CompareEmoji:
        return QStringLiteral("CompareEmoji");
    case CompareNumber:
        return QStringLiteral("CompareNumber");
    case WaitingForKeys:
        return QStringLiteral("WaitingForKeys");
    case WaitingForOtherToAccept:
        return QStringLiteral("WaitingForOtherToAccept");
    case WaitingForMac:
        return QStringLiteral("WaitingForMac");
    case Success:
        return QStringLiteral("Success");
    case Failed:
        return QStringLiteral("Failed");
    default:
        return QString();
    }
}

void
DeviceVerificationFlow::next()
{
    if (sender) {
        switch (state_) {
        case PromptStartVerification:
            sendVerificationRequest();
            break;
        case CompareEmoji:
        case CompareNumber:
            sendVerificationMac();
            break;
        case WaitingForKeys:
        case WaitingForOtherToAccept:
        case WaitingForMac:
        case Success:
        case Failed:
            nhlog::db()->error("verification: Invalid state transition!");
            break;
        }
    } else {
        switch (state_) {
        case PromptStartVerification:
            if (canonical_json.empty())
                sendVerificationReady();
            else // legacy path without request and ready
                acceptVerificationRequest();
            break;
        case CompareEmoji:
            [[fallthrough]];
        case CompareNumber:
            sendVerificationMac();
            break;
        case WaitingForKeys:
        case WaitingForOtherToAccept:
        case WaitingForMac:
        case Success:
        case Failed:
            nhlog::db()->error("verification: Invalid state transition!");
            break;
        }
    }
}

QString
DeviceVerificationFlow::getUserId()
{
    return QString::fromStdString(this->toClient.to_string());
}

QString
DeviceVerificationFlow::getDeviceId()
{
    return this->deviceId;
}

bool
DeviceVerificationFlow::getSender()
{
    return this->sender;
}

std::vector<int>
DeviceVerificationFlow::getSasList()
{
    return this->sasList;
}

bool
DeviceVerificationFlow::isSelfVerification() const
{
    return this->toClient.to_string() == http::client()->user_id().to_string();
}

void
DeviceVerificationFlow::setEventId(const std::string &event_id_)
{
    this->relation.rel_type = mtx::common::RelationType::Reference;
    this->relation.event_id = event_id_;
    this->transaction_id    = event_id_;
}

#include "moc_DeviceVerificationFlow.cpp"
