// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <QDateTime>
#include <QMap>

#include <nlohmann/json.hpp>

#include "cache/Cache.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"

namespace olm {

//! Send encrypted to device messages, targets is a map from userid to device ids or {} for all
//! devices
void
send_encrypted_to_device_messages(const std::map<std::string, std::vector<std::string>> &targets,
                                  const mtx::events::collections::DeviceEvents &event,
                                  bool force_new_session)
{
    static QMap<std::pair<std::string, std::string>, qint64> rateLimit;

    nlohmann::json ev_json = std::visit([](const auto &e) { return nlohmann::json(e); }, event);

    std::map<std::string, std::vector<std::string>> keysToQuery;
    mtx::requests::ClaimKeys claims;
    std::map<mtx::identifiers::User, std::map<std::string, mtx::events::msg::OlmEncrypted>>
      messages;
    std::map<std::string, std::map<std::string, DevicePublicKeys>> pks;

    auto our_curve = olm::client()->identity_keys().curve25519;

    {
        auto currentTime = QDateTime::currentSecsSinceEpoch();
        std::vector<std::pair<std::string, mtx::crypto::OlmSessionPtr>> sessionsToPersist;

        for (const auto &[user, devices] : targets) {
            auto deviceKeys = cache::userKeys(user);

            // no keys for user, query them
            if (!deviceKeys) {
                keysToQuery[user] = devices;
                continue;
            }

            auto deviceTargets = devices;
            if (devices.empty()) {
                deviceTargets.clear();
                deviceTargets.reserve(deviceKeys->device_keys.size());
                for (const auto &[device, keys] : deviceKeys->device_keys) {
                    (void)keys;
                    deviceTargets.push_back(device);
                }
            }

            for (const auto &device : deviceTargets) {
                if (!deviceKeys->device_keys.count(device)) {
                    keysToQuery[user] = {};
                    break;
                }

                const auto &d = deviceKeys->device_keys.at(device);

                if (!d.keys.count("curve25519:" + device) || !d.keys.count("ed25519:" + device)) {
                    nhlog::crypto()->warn("Skipping device {} since it has no keys!", device);
                    continue;
                }

                auto device_curve = d.keys.at("curve25519:" + device);
                if (device_curve == our_curve) {
                    nhlog::crypto()->warn("Skipping our own device, since sending "
                                          "ourselves olm messages makes no sense.");
                    continue;
                }

                auto session = cache::getLatestOlmSession(device_curve);
                if (!session || force_new_session) {
                    if (rateLimit.value(std::pair(user, device)) + 60 * 60 * 10 < currentTime) {
                        claims.one_time_keys[user][device] = mtx::crypto::SIGNED_CURVE25519;
                        pks[user][device].ed25519          = d.keys.at("ed25519:" + device);
                        pks[user][device].curve25519       = d.keys.at("curve25519:" + device);

                        rateLimit.insert(std::pair(user, device), currentTime);
                    } else {
                        nhlog::crypto()->warn("Not creating new session with {}:{} "
                                              "because of rate limit",
                                              user,
                                              device);
                    }
                    continue;
                }

                messages[mtx::identifiers::parse<mtx::identifiers::User>(user)][device] =
                  olm::client()
                    ->create_olm_encrypted_content(session->get(),
                                                   ev_json,
                                                   UserId(user),
                                                   d.keys.at("ed25519:" + device),
                                                   device_curve)
                    .get<mtx::events::msg::OlmEncrypted>();
                sessionsToPersist.emplace_back(d.keys.at("curve25519:" + device),
                                               std::move(*session));
            }
        }

        if (!sessionsToPersist.empty()) {
            try {
                nhlog::crypto()->debug("Updated olm sessions: {}", sessionsToPersist.size());
                cache::saveOlmSessions(std::move(sessionsToPersist), currentTime);
            } catch (const mtx::crypto::olm_exception &e) {
                nhlog::crypto()->critical("failed to pickle outbound olm session: {}", e.what());
            } catch (const std::exception &e) {
                nhlog::db()->critical("failed to save outbound olm session: {}", e.what());
            }
        }
    }

    if (!messages.empty())
        http::client()->send_to_device<mtx::events::msg::OlmEncrypted>(
          http::client()->generate_txn_id(), messages, [](mtx::http::RequestErr err) {
              if (err) {
                  nhlog::net()->warn("failed to send "
                                     "send_to_device "
                                     "message: {}",
                                     err->matrix_error.error);
              }
          });

    auto BindPks = [ev_json](decltype(pks) pks_temp) {
        return [pks = pks_temp, ev_json](const mtx::responses::ClaimKeys &res,
                                         mtx::http::RequestErr) {
            std::map<mtx::identifiers::User, std::map<std::string, mtx::events::msg::OlmEncrypted>>
              messages;
            auto currentTime = QDateTime::currentSecsSinceEpoch();
            std::vector<std::pair<std::string, mtx::crypto::OlmSessionPtr>> sessionsToPersist;

            for (const auto &[user_id, retrieved_devices] : res.one_time_keys) {
                nhlog::net()->debug("claimed keys for {}", user_id);
                if (retrieved_devices.size() == 0) {
                    nhlog::net()->debug("no one-time keys found for user_id: {}", user_id);
                    continue;
                }

                for (const auto &rd : retrieved_devices) {
                    const auto device_id = rd.first;

                    nhlog::net()->debug("{} : \n {}", device_id, rd.second.dump(2));

                    if (rd.second.empty() || !rd.second.begin()->contains("key")) {
                        nhlog::net()->warn("Skipping device {} as it has no key.", device_id);
                        continue;
                    }

                    auto otk = rd.second.begin()->at("key").get<std::string>();

                    auto sign_key = pks.at(user_id).at(device_id).ed25519;
                    auto id_key   = pks.at(user_id).at(device_id).curve25519;

                    // Verify signature
                    {
                        auto signedKey = *rd.second.begin();
                        std::string signature =
                          signedKey["signatures"][user_id].value("ed25519:" + device_id, "");

                        if (signature.empty() || !mtx::crypto::ed25519_verify_signature(
                                                   sign_key, signedKey, signature)) {
                            nhlog::net()->warn("Skipping device {} as its one time key "
                                               "has an invalid signature.",
                                               device_id);
                            continue;
                        }
                    }

                    auto session = olm::client()->create_outbound_session(id_key, otk);

                    messages[mtx::identifiers::parse<mtx::identifiers::User>(user_id)][device_id] =
                      olm::client()
                        ->create_olm_encrypted_content(
                          session.get(), ev_json, UserId(user_id), sign_key, id_key)
                        .get<mtx::events::msg::OlmEncrypted>();

                    sessionsToPersist.emplace_back(id_key, std::move(session));
                }
                nhlog::net()->info("send_to_device: {}", user_id);
            }

            if (!sessionsToPersist.empty()) {
                try {
                    nhlog::crypto()->debug("Updated (new) olm sessions: {}",
                                           sessionsToPersist.size());
                    cache::saveOlmSessions(std::move(sessionsToPersist), currentTime);
                } catch (const mtx::crypto::olm_exception &e) {
                    nhlog::crypto()->critical("failed to pickle outbound olm session: {}",
                                              e.what());
                } catch (const std::exception &e) {
                    nhlog::db()->critical("failed to save outbound olm session: {}", e.what());
                }
            }

            if (!messages.empty())
                http::client()->send_to_device<mtx::events::msg::OlmEncrypted>(
                  http::client()->generate_txn_id(), messages, [](mtx::http::RequestErr err) {
                      if (err) {
                          nhlog::net()->warn("failed to send "
                                             "send_to_device "
                                             "message: {}",
                                             err->matrix_error.error);
                      }
                  });
        };
    };

    if (!claims.one_time_keys.empty())
        http::client()->claim_keys(claims, BindPks(pks));

    if (!keysToQuery.empty()) {
        mtx::requests::QueryKeys req;
        req.device_keys = keysToQuery;
        http::client()->query_keys(
          req,
          [ev_json, BindPks, our_curve](const mtx::responses::QueryKeys &res,
                                        mtx::http::RequestErr err) {
              if (err) {
                  nhlog::net()->warn("failed to query device keys: {} {}",
                                     err->matrix_error.error,
                                     static_cast<int>(err->status_code));
                  return;
              }

              nhlog::net()->info("queried keys");

              cache::updateUserKeys(cache::nextBatchToken(), res);

              mtx::requests::ClaimKeys claim_keys;

              std::map<std::string, std::map<std::string, DevicePublicKeys>> deviceKeys;

              for (const auto &user : res.device_keys) {
                  for (const auto &dev : user.second) {
                      const auto user_id   = ::UserId(dev.second.user_id);
                      const auto device_id = DeviceId(dev.second.device_id);

                      if (user_id.get() == http::client()->user_id().to_string() &&
                          device_id.get() == http::client()->device_id())
                          continue;

                      const auto device_keys = dev.second.keys;
                      const auto curveKey    = "curve25519:" + device_id.get();
                      const auto edKey       = "ed25519:" + device_id.get();

                      if ((device_keys.find(curveKey) == device_keys.end()) ||
                          (device_keys.find(edKey) == device_keys.end())) {
                          nhlog::net()->debug("ignoring malformed keys for device {}",
                                              device_id.get());
                          continue;
                      }

                      DevicePublicKeys pks;
                      pks.ed25519    = device_keys.at(edKey);
                      pks.curve25519 = device_keys.at(curveKey);

                      if (pks.curve25519 == our_curve) {
                          nhlog::crypto()->warn("Skipping our own device, since sending "
                                                "ourselves olm messages makes no sense.");
                          continue;
                      }

                      try {
                          if (!mtx::crypto::verify_identity_signature(
                                dev.second, device_id, user_id)) {
                              nhlog::crypto()->warn("failed to verify identity keys: {}",
                                                    nlohmann::json(dev.second).dump(2));
                              continue;
                          }
                      } catch (const nlohmann::json::exception &e) {
                          nhlog::crypto()->warn("failed to parse device key json: {}", e.what());
                          continue;
                      } catch (const mtx::crypto::olm_exception &e) {
                          nhlog::crypto()->warn("failed to verify device key json: {}", e.what());
                          continue;
                      }

                      auto currentTime = QDateTime::currentSecsSinceEpoch();
                      if (rateLimit.value(std::pair(user.first, device_id.get())) + 60 * 60 * 10 <
                          currentTime) {
                          deviceKeys[user_id].emplace(device_id, pks);
                          claim_keys.one_time_keys[user.first][device_id] =
                            mtx::crypto::SIGNED_CURVE25519;

                          rateLimit.insert(std::pair(user.first, device_id.get()), currentTime);
                      } else {
                          nhlog::crypto()->warn("Not creating new session with {}:{} "
                                                "because of rate limit",
                                                user.first,
                                                device_id.get());
                          continue;
                      }

                      nhlog::net()->info("{}", device_id.get());
                      nhlog::net()->info("  curve25519 {}", pks.curve25519);
                      nhlog::net()->info("  ed25519 {}", pks.ed25519);
                  }
              }

              if (!claim_keys.one_time_keys.empty())
                  http::client()->claim_keys(claim_keys, BindPks(deviceKeys));
          });
    }
}

} // namespace olm
