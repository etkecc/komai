// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DeviceVerificationFlow.h"

#include <algorithm>

#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include <mtx/secret_storage.hpp>

#include "Olm.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "utils/Utils.h"

namespace {

static constexpr std::string_view mac_method_alg_v1 = "hkdf-hmac-sha256";
static constexpr std::string_view mac_method_alg_v2 = "hkdf-hmac-sha256.v2";

}

void
DeviceVerificationFlow::handleVerificationAccept(const mtx::events::msg::KeyVerificationAccept &msg)
{
    if (state_ == Failed || state_ == Success)
        return;

    nhlog::crypto()->info("verification: received accept with mac methods {}",
                          fmt::join(msg.message_authentication_code, ", "));
    if (msg.transaction_id.has_value()) {
        if (msg.transaction_id.value() != this->transaction_id)
            return;
    } else if (msg.relations.references()) {
        if (msg.relations.references() != this->relation.event_id)
            return;
    }

    if (msg.key_agreement_protocol == "curve25519-hkdf-sha256" && msg.hash == "sha256" &&
        (msg.message_authentication_code == mac_method_alg_v1 ||
         msg.message_authentication_code == mac_method_alg_v2)) {
        this->commitment = msg.commitment;
        if (std::find(msg.short_authentication_string.begin(),
                      msg.short_authentication_string.end(),
                      mtx::events::msg::SASMethods::Emoji) !=
            msg.short_authentication_string.end()) {
            this->method = mtx::events::msg::SASMethods::Emoji;
        } else {
            this->method = mtx::events::msg::SASMethods::Decimal;
        }
        this->mac_method = msg.message_authentication_code;
        this->sendVerificationKey();
    } else {
        this->cancelVerification(DeviceVerificationFlow::Error::UnknownMethod);
    }
}

void
DeviceVerificationFlow::handleVerificationCancel(const mtx::events::msg::KeyVerificationCancel &msg)
{
    nhlog::crypto()->info("verification: received cancel, {} : {}", msg.code, msg.reason);
    if (msg.transaction_id.has_value()) {
        if (msg.transaction_id.value() != this->transaction_id)
            return;
    } else if (msg.relations.references()) {
        if (msg.relations.references() != this->relation.event_id)
            return;
    }
    error_ = User;
    emit errorChanged();
    setState(Failed);
}

void
DeviceVerificationFlow::handleVerificationKey(const mtx::events::msg::KeyVerificationKey &msg)
{
    if (state_ == Failed || state_ == Success)
        return;

    nhlog::crypto()->info(
      "verification: received key, sender {}, state {}", sender, state().toStdString());
    if (msg.transaction_id.has_value()) {
        if (msg.transaction_id.value() != this->transaction_id)
            return;
    } else if (msg.relations.references()) {
        if (msg.relations.references() != this->relation.event_id)
            return;
    }

    if (sender) {
        if (state_ != WaitingForOtherToAccept && state_ != WaitingForKeys) {
            this->cancelVerification(OutOfOrder);
            return;
        }
    } else {
        if (state_ != WaitingForKeys) {
            this->cancelVerification(OutOfOrder);
            return;
        }
    }

    this->sas->set_their_key(msg.key);
    std::string info;
    if (this->sender == true) {
        info = "MATRIX_KEY_VERIFICATION_SAS|" + http::client()->user_id().to_string() + "|" +
               http::client()->device_id() + "|" + this->sas->public_key() + "|" +
               this->toClient.to_string() + "|" + this->deviceId.toStdString() + "|" + msg.key +
               "|" + this->transaction_id;
    } else {
        info = "MATRIX_KEY_VERIFICATION_SAS|" + this->toClient.to_string() + "|" +
               this->deviceId.toStdString() + "|" + msg.key + "|" +
               http::client()->user_id().to_string() + "|" + http::client()->device_id() + "|" +
               this->sas->public_key() + "|" + this->transaction_id;
    }

    nhlog::ui()->info("Info is: '{}'", info);

    if (this->sender == false) {
        this->sendVerificationKey();
    } else {
        if (this->commitment !=
            mtx::crypto::bin2base64_unpadded(mtx::crypto::sha256(msg.key + this->canonical_json))) {
            this->cancelVerification(DeviceVerificationFlow::Error::MismatchedCommitment);
            return;
        }
    }

    if (this->method == mtx::events::msg::SASMethods::Emoji) {
        this->sasList = this->sas->generate_bytes_emoji(info);
        setState(CompareEmoji);
    } else if (this->method == mtx::events::msg::SASMethods::Decimal) {
        this->sasList = this->sas->generate_bytes_decimal(info);
        setState(CompareNumber);
    }
}

void
DeviceVerificationFlow::handleVerificationMac(const mtx::events::msg::KeyVerificationMac &msg)
{
    if (state_ == Failed || state_ == Success)
        return;

    nhlog::crypto()->info("verification: received mac");
    if (msg.transaction_id.has_value()) {
        if (msg.transaction_id.value() != this->transaction_id)
            return;
    } else if (msg.relations.references()) {
        if (msg.relations.references() != this->relation.event_id)
            return;
    }

    std::map<std::string, std::string> key_list;
    std::string key_string;
    for (const auto &mac : msg.mac) {
        for (const auto &[deviceid, key] : their_keys.device_keys) {
            (void)deviceid;
            if (key.keys.count(mac.first))
                key_list[mac.first] = key.keys.at(mac.first);
        }

        if (their_keys.master_keys.keys.count(mac.first))
            key_list[mac.first] = their_keys.master_keys.keys[mac.first];
        if (their_keys.user_signing_keys.keys.count(mac.first))
            key_list[mac.first] = their_keys.user_signing_keys.keys[mac.first];
        if (their_keys.self_signing_keys.keys.count(mac.first))
            key_list[mac.first] = their_keys.self_signing_keys.keys[mac.first];
    }
    auto macs = sas->calculate_mac(mac_method,
                                   toClient,
                                   this->deviceId.toStdString(),
                                   http::client()->user_id(),
                                   http::client()->device_id(),
                                   this->transaction_id,
                                   key_list);

    for (const auto &[key, mac] : macs.mac) {
        if (mac != msg.mac.at(key)) {
            this->cancelVerification(DeviceVerificationFlow::Error::KeyMismatch);
            return;
        }
    }

    if (msg.keys == macs.keys) {
        mtx::requests::KeySignaturesUpload req;
        if (utils::localUser().toStdString() == this->toClient.to_string()) {
            // self verification, sign master key with device key, if we verified it
            for (const auto &mac : msg.mac) {
                if (their_keys.master_keys.keys.count(mac.first)) {
                    nlohmann::json j = their_keys.master_keys;
                    j.erase("signatures");
                    j.erase("unsigned");
                    mtx::crypto::CrossSigningKeys master_key =
                      j.get<mtx::crypto::CrossSigningKeys>();
                    master_key.signatures[utils::localUser().toStdString()]
                                         ["ed25519:" + http::client()->device_id()] =
                      olm::client()->sign_message(j.dump());
                    req
                      .signatures[utils::localUser().toStdString()][master_key.keys.at(mac.first)] =
                      master_key;
                } else if (mac.first == "ed25519:" + this->deviceId.toStdString()) {
                    // Sign their device key with self signing key

                    auto device_id = this->deviceId.toStdString();

                    if (their_keys.device_keys.count(device_id)) {
                        nlohmann::json j = their_keys.device_keys.at(device_id);
                        j.erase("signatures");
                        j.erase("unsigned");

                        auto secret =
                          cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing);
                        if (!secret)
                            continue;
                        auto ssk = mtx::crypto::PkSigning::from_seed(*secret);

                        mtx::crypto::DeviceKeys dev = j.get<mtx::crypto::DeviceKeys>();
                        dev.signatures[utils::localUser().toStdString()]
                                      ["ed25519:" + ssk.public_key()] = ssk.sign(j.dump());

                        req.signatures[utils::localUser().toStdString()][device_id] = dev;
                    }
                }
            }
        } else {
            // Sign their master key with user signing key
            for (const auto &mac : msg.mac) {
                if (their_keys.master_keys.keys.count(mac.first)) {
                    nlohmann::json j = their_keys.master_keys;
                    j.erase("signatures");
                    j.erase("unsigned");

                    auto secret =
                      cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing);
                    if (!secret)
                        continue;
                    auto usk = mtx::crypto::PkSigning::from_seed(*secret);

                    mtx::crypto::CrossSigningKeys master_key =
                      j.get<mtx::crypto::CrossSigningKeys>();
                    master_key
                      .signatures[utils::localUser().toStdString()]["ed25519:" + usk.public_key()] =
                      usk.sign(j.dump());

                    req.signatures[toClient.to_string()][master_key.keys.at(mac.first)] =
                      master_key;
                }
            }
        }

        if (!req.signatures.empty()) {
            nhlog::crypto()->debug("Signatures to send: {}", nlohmann::json(req).dump(2));
            http::client()->keys_signatures_upload(
              req, [](const mtx::responses::KeySignaturesUpload &res, mtx::http::RequestErr err) {
                  if (err) {
                      nhlog::net()->error("failed to upload signatures: {},{}",
                                          mtx::errors::to_string(err->matrix_error.errcode),
                                          static_cast<int>(err->status_code));
                  }

                  for (const auto &error : res.errors) {
                      const auto &user_id = error.first;
                      for (const auto &key_error : error.second) {
                          const auto &key_id = key_error.first;
                          const auto &e      = key_error.second;

                          nhlog::net()->error("signature error for user {} and key "
                                              "id {}: {}, {}",
                                              user_id,
                                              key_id,
                                              mtx::errors::to_string(e.errcode),
                                              e.error);
                      }
                  }
              });
        }

        this->isMacVerified = true;
        this->acceptDevice();
    } else {
        this->cancelVerification(DeviceVerificationFlow::Error::KeyMismatch);
    }
}

void
DeviceVerificationFlow::handleVerificationReady(const mtx::events::msg::KeyVerificationReady &msg)
{
    if (state_ == Failed || state_ == Success)
        return;

    nhlog::crypto()->info("verification: received ready {}", (void *)this);
    if (!sender) {
        if (msg.from_device != this->deviceId.toStdString()) {
            nhlog::crypto()->debug("Accepted by {}, we are communicating with {}",
                                   msg.from_device,
                                   this->deviceId.toStdString());
            cancelVerification(AcceptedOnOtherDevice);
        }

        return;
    }

    if (msg.transaction_id.has_value()) {
        if (msg.transaction_id.value() != this->transaction_id)
            return;

        if (this->deviceId.isEmpty() && this->deviceIds.size() > 1) {
            auto from = QString::fromStdString(msg.from_device);
            if (std::find(deviceIds.begin(), deviceIds.end(), from) != deviceIds.end()) {
                mtx::events::msg::KeyVerificationCancel req{};
                req.code           = "m.user";
                req.reason         = "accepted by other device";
                req.transaction_id = this->transaction_id;
                mtx::requests::ToDeviceMessages<mtx::events::msg::KeyVerificationCancel> body;

                for (const auto &d : this->deviceIds) {
                    if (d != from)
                        body[this->toClient][d.toStdString()] = req;
                }

                http::client()->send_to_device(
                  http::client()->generate_txn_id(), body, [](mtx::http::RequestErr err) {
                      if (err)
                          nhlog::net()->warn("failed to send verification to_device message: {} {}",
                                             err->matrix_error.error,
                                             static_cast<int>(err->status_code));
                  });

                this->deviceId  = from;
                this->deviceIds = {from};
            }
        }
    } else if (msg.relations.references()) {
        if (msg.relations.references() != this->relation.event_id)
            return;
        else {
            this->deviceId = QString::fromStdString(msg.from_device);
        }
    } else {
        return;
    }
    nhlog::crypto()->info("verification: received ready sending start {}", (void *)this);
    this->startVerificationRequest();
}

void
DeviceVerificationFlow::handleVerificationDone(const mtx::events::msg::KeyVerificationDone &msg)
{
    nhlog::crypto()->info("verification: received done");
    if (msg.transaction_id.has_value()) {
        if (msg.transaction_id.value() != this->transaction_id)
            return;
    } else if (msg.relations.references()) {
        if (msg.relations.references() != this->relation.event_id)
            return;
    }
    nhlog::ui()->info("Flow done on other side");
}
