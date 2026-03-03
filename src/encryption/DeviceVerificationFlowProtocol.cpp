// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DeviceVerificationFlow.h"

#include <algorithm>
#include <cassert>
#include <tuple>

#include <QDateTime>

#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include "cache/Cache.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "timeline/TimelineModel.h"
#include "utils/Utils.h"

namespace {

static constexpr std::string_view mac_method_alg_v1 = "hkdf-hmac-sha256";
static constexpr std::string_view mac_method_alg_v2 = "hkdf-hmac-sha256.v2";

}

void
DeviceVerificationFlow::handleStartMessage(const mtx::events::msg::KeyVerificationStart &msg,
                                           std::string)
{
    if (state_ == Failed || state_ == Success)
        return;

    if (msg.transaction_id.has_value()) {
        if (msg.transaction_id.value() != this->transaction_id)
            return;
    } else if (msg.relations.references()) {
        if (msg.relations.references() != this->relation.event_id)
            return;
    } else {
        return;
    }

    if (state_ == Failed)
        return;

    if (msg.from_device != this->deviceId.toStdString()) {
        cancelVerification(AcceptedOnOtherDevice);
        return;
    }

    nhlog::crypto()->info("verification: received start with mac methods {}",
                          fmt::join(msg.message_authentication_codes, ", "));

    // TODO(Nico): Replace with contains once we use C++23
    if (std::ranges::count(msg.key_agreement_protocols, "curve25519-hkdf-sha256") &&
        std::ranges::count(msg.hashes, "sha256")) {
        if (std::ranges::count(msg.message_authentication_codes, mac_method_alg_v2)) {
            this->mac_method = mac_method_alg_v2;
        } else if (std::ranges::count(msg.message_authentication_codes, mac_method_alg_v1)) {
            this->mac_method = mac_method_alg_v1;
        } else {
            this->cancelVerification(DeviceVerificationFlow::Error::UnknownMethod);
            return;
        }

        if (std::ranges::count(msg.short_authentication_string,
                               mtx::events::msg::SASMethods::Emoji)) {
            this->method = mtx::events::msg::SASMethods::Emoji;
        } else if (std::ranges::count(msg.short_authentication_string,
                                      mtx::events::msg::SASMethods::Decimal)) {
            this->method = mtx::events::msg::SASMethods::Decimal;
        } else {
            this->cancelVerification(DeviceVerificationFlow::Error::UnknownMethod);
            return;
        }

        if (!sender)
            this->canonical_json = nlohmann::json(msg).dump();
        else {
            // resolve glare
            if (std::tuple(this->toClient.to_string(), this->deviceId.toStdString()) <
                std::tuple(utils::localUser().toStdString(), http::client()->device_id())) {
                // treat this as if the user with the smaller mxid or smaller deviceid (if the mxid
                // was the same) was the sender of "start"
                this->canonical_json = nlohmann::json(msg).dump();
                this->sender         = false;
            }

            if (msg.method != mtx::events::msg::VerificationMethods::SASv1) {
                cancelVerification(DeviceVerificationFlow::Error::UnknownMethod);
                return;
            }
        }

        // If we didn't send "start", accept the verification (otherwise wait for the other side to
        // accept
        if (state_ != PromptStartVerification && !sender)
            this->acceptVerificationRequest();
    } else {
        this->cancelVerification(DeviceVerificationFlow::Error::UnknownMethod);
    }
}

//! accepts a verification
void
DeviceVerificationFlow::acceptVerificationRequest()
{
    if (state_ == Failed || state_ == Success)
        return;

    if (acceptSent)
        return;

    if (mac_method.empty()) {
        nhlog::crypto()->critical("Ignoring start without mac method set!");
        return;
    } else {
        nhlog::crypto()->debug("Accepted verification using mac_method {}", mac_method);
    }

    acceptSent = true;

    mtx::events::msg::KeyVerificationAccept req;

    req.method                      = mtx::events::msg::VerificationMethods::SASv1;
    req.key_agreement_protocol      = "curve25519-hkdf-sha256";
    req.hash                        = "sha256";
    req.message_authentication_code = this->mac_method;
    if (this->method == mtx::events::msg::SASMethods::Emoji)
        req.short_authentication_string = {mtx::events::msg::SASMethods::Emoji};
    else if (this->method == mtx::events::msg::SASMethods::Decimal)
        req.short_authentication_string = {mtx::events::msg::SASMethods::Decimal};
    req.commitment = mtx::crypto::bin2base64_unpadded(
      mtx::crypto::sha256(this->sas->public_key() + this->canonical_json));

    send(req);
    setState(WaitingForKeys);
}
//! responds verification request
void
DeviceVerificationFlow::sendVerificationReady()
{
    mtx::events::msg::KeyVerificationReady req;

    req.from_device = http::client()->device_id();
    req.methods     = {mtx::events::msg::VerificationMethods::SASv1};

    send(req);
    setState(WaitingForKeys);
}
//! accepts a verification
void
DeviceVerificationFlow::sendVerificationDone()
{
    mtx::events::msg::KeyVerificationDone req;

    send(req);
}
//! starts the verification flow
void
DeviceVerificationFlow::startVerificationRequest()
{
    if (startSent)
        return;
    startSent = true;

    mtx::events::msg::KeyVerificationStart req;

    req.from_device                  = http::client()->device_id();
    req.method                       = mtx::events::msg::VerificationMethods::SASv1;
    req.key_agreement_protocols      = {"curve25519-hkdf-sha256"};
    req.hashes                       = {"sha256"};
    req.message_authentication_codes = {std::string(mac_method_alg_v1),
                                        std::string(mac_method_alg_v2)};
    req.short_authentication_string  = {mtx::events::msg::SASMethods::Decimal,
                                        mtx::events::msg::SASMethods::Emoji};

    if (this->type == DeviceVerificationFlow::Type::ToDevice) {
        mtx::requests::ToDeviceMessages<mtx::events::msg::KeyVerificationStart> body;
        req.transaction_id   = this->transaction_id;
        this->canonical_json = nlohmann::json(req).dump();
    } else if (this->type == DeviceVerificationFlow::Type::RoomMsg && model_) {
        req.relations.relations.push_back(this->relation);
        // Set synthesized to suppress legacy relation extensions.
        req.relations.synthesized = true;
        this->canonical_json      = nlohmann::json(req).dump();
    }
    send(req);
    setState(WaitingForOtherToAccept);
}
//! sends a verification request
void
DeviceVerificationFlow::sendVerificationRequest()
{
    mtx::events::msg::KeyVerificationRequest req;

    req.from_device = http::client()->device_id();
    req.methods     = {mtx::events::msg::VerificationMethods::SASv1};

    if (this->type == DeviceVerificationFlow::Type::ToDevice) {
        QDateTime currentTime = QDateTime::currentDateTimeUtc();

        req.timestamp = (uint64_t)currentTime.toMSecsSinceEpoch();

    } else if (this->type == DeviceVerificationFlow::Type::RoomMsg && model_) {
        req.to      = this->toClient.to_string();
        req.msgtype = "m.key.verification.request";
        // clang-format off
        // clang-format < 12 is buggy on this
        req.body    = "User is requesting to verify keys with you. However, your client does "
                      "not support this method, so you will need to use the legacy method of "
                      "key verification.";
        // clang-format on
    }

    send(req);
    setState(WaitingForOtherToAccept);
}
//! cancels a verification flow
void
DeviceVerificationFlow::cancelVerification(DeviceVerificationFlow::Error error_code)
{
    if (state_ == State::Success || state_ == State::Failed)
        return;

    mtx::events::msg::KeyVerificationCancel req;

    if (error_code == DeviceVerificationFlow::Error::UnknownMethod) {
        req.code   = "m.unknown_method";
        req.reason = "unknown method received";
    } else if (error_code == DeviceVerificationFlow::Error::MismatchedCommitment) {
        req.code   = "m.mismatched_commitment";
        req.reason = "commitment didn't match";
    } else if (error_code == DeviceVerificationFlow::Error::MismatchedSAS) {
        req.code   = "m.mismatched_sas";
        req.reason = "sas didn't match";
    } else if (error_code == DeviceVerificationFlow::Error::KeyMismatch) {
        req.code   = "m.key_match";
        req.reason = "keys did not match";
    } else if (error_code == DeviceVerificationFlow::Error::Timeout) {
        req.code   = "m.timeout";
        req.reason = "timed out";
    } else if (error_code == DeviceVerificationFlow::Error::User) {
        req.code   = "m.user";
        req.reason = "user cancelled the verification";
    } else if (error_code == DeviceVerificationFlow::Error::OutOfOrder) {
        req.code   = "m.unexpected_message";
        req.reason = "received messages out of order";
    }

    this->error_ = error_code;
    this->setState(Failed);
    emit errorChanged();

    // Don't cancel if the user accepted the request elsewhere, instead just silently stop
    if (error_code != AcceptedOnOtherDevice)
        send(req);
}
//! sends the verification key
void
DeviceVerificationFlow::sendVerificationKey()
{
    if (keySent)
        return;
    keySent = true;

    mtx::events::msg::KeyVerificationKey req;

    req.key = this->sas->public_key();

    send(req);
}

//! sends the mac of the keys
void
DeviceVerificationFlow::sendVerificationMac()
{
    if (macSent)
        return;
    macSent = true;

    nhlog::crypto()->debug("Sending mac using mac_method {}", mac_method);

    std::map<std::string, std::string> key_list;
    key_list["ed25519:" + http::client()->device_id()] = olm::client()->identity_keys().ed25519;

    // send our master key, if we trust it
    if (!this->our_trusted_master_key.empty())
        key_list["ed25519:" + our_trusted_master_key] = our_trusted_master_key;

    mtx::events::msg::KeyVerificationMac req = sas->calculate_mac(mac_method,
                                                                  http::client()->user_id(),
                                                                  http::client()->device_id(),
                                                                  this->toClient,
                                                                  this->deviceId.toStdString(),
                                                                  this->transaction_id,
                                                                  key_list);

    send(req);

    setState(WaitingForMac);
    acceptDevice();
}
//! Completes the verification flow
void
DeviceVerificationFlow::acceptDevice()
{
    if (!isMacVerified) {
        setState(WaitingForMac);
    } else if (state_ == WaitingForMac) {
        cache::markDeviceVerified(this->toClient.to_string(), this->deviceId.toStdString());
        this->sendVerificationDone();
        setState(Success);

        // Request secrets. We should probably check somehow, if a device knowns about the
        // secrets.
        if (utils::localUser().toStdString() == this->toClient.to_string() &&
            (!cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing) ||
             !cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing))) {
            olm::request_cross_signing_keys();
        }
    }
}

void
DeviceVerificationFlow::unverify()
{
    cache::markDeviceUnverified(this->toClient.to_string(), this->deviceId.toStdString());

    emit refreshProfile();
}

QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::NewInRoomVerification(QObject *parent_,
                                              TimelineModel *timelineModel_,
                                              const mtx::events::msg::KeyVerificationRequest &msg,
                                              const QString &other_user_,
                                              const QString &event_id_)
{
    QSharedPointer<DeviceVerificationFlow> flow(
      new DeviceVerificationFlow(parent_,
                                 Type::RoomMsg,
                                 timelineModel_,
                                 other_user_,
                                 {QString::fromStdString(msg.from_device)}));

    flow->setEventId(event_id_.toStdString());

    if (std::find(msg.methods.begin(),
                  msg.methods.end(),
                  mtx::events::msg::VerificationMethods::SASv1) == msg.methods.end()) {
        flow->cancelVerification(UnknownMethod);
    }

    return flow;
}
QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::NewToDeviceVerification(QObject *parent_,
                                                const mtx::events::msg::KeyVerificationRequest &msg,
                                                const QString &other_user_,
                                                const QString &txn_id_)
{
    QSharedPointer<DeviceVerificationFlow> flow(new DeviceVerificationFlow(
      parent_, Type::ToDevice, nullptr, other_user_, {QString::fromStdString(msg.from_device)}));
    flow->transaction_id = txn_id_.toStdString();

    if (std::find(msg.methods.begin(),
                  msg.methods.end(),
                  mtx::events::msg::VerificationMethods::SASv1) == msg.methods.end()) {
        flow->cancelVerification(UnknownMethod);
    }

    return flow;
}
QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::NewToDeviceVerification(QObject *parent_,
                                                const mtx::events::msg::KeyVerificationStart &msg,
                                                const QString &other_user_,
                                                const QString &txn_id_)
{
    QSharedPointer<DeviceVerificationFlow> flow(new DeviceVerificationFlow(
      parent_, Type::ToDevice, nullptr, other_user_, {QString::fromStdString(msg.from_device)}));
    flow->transaction_id = txn_id_.toStdString();

    flow->handleStartMessage(msg, "");

    return flow;
}
QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::InitiateUserVerification(QObject *parent_,
                                                 TimelineModel *timelineModel_,
                                                 const QString &userid)
{
    QSharedPointer<DeviceVerificationFlow> flow(
      new DeviceVerificationFlow(parent_, Type::RoomMsg, timelineModel_, userid, {}));
    flow->sender = true;
    return flow;
}
QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::InitiateDeviceVerification(QObject *parent_,
                                                   const QString &userid,
                                                   const std::vector<QString> &devices)
{
    assert(!devices.empty());

    QSharedPointer<DeviceVerificationFlow> flow(
      new DeviceVerificationFlow(parent_, Type::ToDevice, nullptr, userid, devices));

    flow->sender         = true;
    flow->transaction_id = http::client()->generate_txn_id();

    return flow;
}
template<typename T>
void
DeviceVerificationFlow::send(T msg)
{
    if (this->type == DeviceVerificationFlow::Type::ToDevice) {
        mtx::requests::ToDeviceMessages<T> body;
        msg.transaction_id = this->transaction_id;
        for (const auto &d : deviceIds)
            body[this->toClient][d.toStdString()] = msg;

        http::client()->send_to_device<T>(
          http::client()->generate_txn_id(), body, [](mtx::http::RequestErr err) {
              if (err)
                  nhlog::net()->warn("failed to send verification to_device message: {} {}",
                                     err->matrix_error.error,
                                     static_cast<int>(err->status_code));
          });
    } else if (this->type == DeviceVerificationFlow::Type::RoomMsg && model_) {
        if constexpr (!std::is_same_v<T, mtx::events::msg::KeyVerificationRequest>) {
            msg.relations.relations.push_back(this->relation);
            // Set synthesized to suppress legacy relation extensions.
            msg.relations.synthesized = true;
        }
        (model_)->sendMessageEvent(msg, mtx::events::to_device_content_to_type<T>);
    }

    nhlog::net()->debug("Sent verification step: {} in state: {}",
                        mtx::events::to_string(mtx::events::to_device_content_to_type<T>),
                        state().toStdString());
}
