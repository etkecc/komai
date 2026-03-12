// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <QObject>
#include <QRandomGenerator>
#include <QTimer>

#include <nlohmann/json.hpp>

#include <variant>

#include <mtx/responses/common.hpp>
#include <mtx/secret_storage.hpp>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
auto client_ = std::make_unique<mtx::crypto::OlmClient>();

std::map<std::string, std::string> request_id_to_secret_name;
}

namespace olm {

mtx::crypto::OlmClient *
client()
{
    return client_.get();
}

static void
handle_secret_request(const mtx::events::DeviceEvent<mtx::events::msg::SecretRequest> *e,
                      const std::string &sender)
{
    using namespace mtx::events;

    if (e->content.action != mtx::events::msg::RequestAction::Request)
        return;

    auto local_user = http::client()->user_id();

    if (sender != local_user.to_string())
        return;

    auto verificationStatus = cache::verificationStatus(local_user.to_string());

    if (!verificationStatus)
        return;

    auto deviceKeys = cache::userKeys(local_user.to_string());
    if (!deviceKeys)
        return;

    if (std::find(verificationStatus->verified_devices.begin(),
                  verificationStatus->verified_devices.end(),
                  e->content.requesting_device_id) == verificationStatus->verified_devices.end())
        return;

    // this is a verified device
    mtx::events::DeviceEvent<mtx::events::msg::SecretSend> secretSend;
    secretSend.type               = EventType::SecretSend;
    secretSend.content.request_id = e->content.request_id;

    auto secret = cache::secret(e->content.name);
    if (!secret)
        return;
    secretSend.content.secret = secret.value();

    // Randomly delay reply to workaround olm session generation races
    QTimer::singleShot(QRandomGenerator::global()->bounded(0, 3000),
                       ChatPage::instance(),
                       [local_user, e = *e, secretSend] {
                           send_encrypted_to_device_messages(
                             {{local_user.to_string(), {{e.content.requesting_device_id}}}},
                             secretSend);

                           nhlog::net()->info("Sent secret '{}' to ({},{})",
                                              e.content.name,
                                              local_user.to_string(),
                                              e.content.requesting_device_id);
                       });
}

void
handle_to_device_messages(const std::vector<mtx::events::collections::DeviceEvents> &msgs)
{
    if (msgs.empty())
        return;
    nhlog::crypto()->info("received {} to_device messages", msgs.size());
    nlohmann::json j_msg;

    for (const auto &msg : msgs) {
        j_msg = std::visit([](auto &e) { return nlohmann::json(e); }, std::move(msg));
        if (j_msg.count("type") == 0) {
            nhlog::crypto()->warn("received message with no type field: {}", j_msg.dump(2));
            continue;
        }

        std::string msg_type = j_msg.at("type").get<std::string>();

        if (msg_type == to_string(mtx::events::EventType::RoomEncrypted)) {
            try {
                olm::OlmMessage olm_msg = j_msg.get<olm::OlmMessage>();
                cache::queryKeys(
                  olm_msg.sender, [olm_msg](const UserKeyCache &userKeys, mtx::http::RequestErr e) {
                      if (e) {
                          nhlog::crypto()->error(
                            "Failed to query user keys, dropping olm message: {}", e);
                          return;
                      }
                      handle_olm_message(std::move(olm_msg), userKeys);
                  });
            } catch (const nlohmann::json::exception &e) {
                nhlog::crypto()->warn(
                  "parsing error for olm message: {} {}", e.what(), j_msg.dump(2));
            } catch (const std::invalid_argument &e) {
                nhlog::crypto()->warn(
                  "validation error for olm message: {} {}", e.what(), j_msg.dump(2));
            }

        } else if (msg_type == to_string(mtx::events::EventType::RoomKeyRequest)) {
            nhlog::crypto()->warn("handling key request event: {}", j_msg.dump(2));
            try {
                mtx::events::DeviceEvent<mtx::events::msg::KeyRequest> req =
                  j_msg.get<mtx::events::DeviceEvent<mtx::events::msg::KeyRequest>>();
                if (req.content.action == mtx::events::msg::RequestAction::Request)
                    handle_key_request_message(req);
                else
                    nhlog::crypto()->warn("ignore key request (unhandled action): {}",
                                          req.content.request_id);
            } catch (const nlohmann::json::exception &e) {
                nhlog::crypto()->warn(
                  "parsing error for key_request message: {} {}", e.what(), j_msg.dump(2));
            }
        } else if (msg_type == to_string(mtx::events::EventType::KeyVerificationAccept)) {
            auto message =
              std::get<mtx::events::DeviceEvent<mtx::events::msg::KeyVerificationAccept>>(msg);
            ChatPage::instance()->receivedDeviceVerificationAccept(message.content);
        } else if (msg_type == to_string(mtx::events::EventType::KeyVerificationRequest)) {
            auto message =
              std::get<mtx::events::DeviceEvent<mtx::events::msg::KeyVerificationRequest>>(msg);
            ChatPage::instance()->receivedDeviceVerificationRequest(message.content,
                                                                    message.sender);
        } else if (msg_type == to_string(mtx::events::EventType::KeyVerificationCancel)) {
            auto message =
              std::get<mtx::events::DeviceEvent<mtx::events::msg::KeyVerificationCancel>>(msg);
            ChatPage::instance()->receivedDeviceVerificationCancel(message.content);
        } else if (msg_type == to_string(mtx::events::EventType::KeyVerificationKey)) {
            auto message =
              std::get<mtx::events::DeviceEvent<mtx::events::msg::KeyVerificationKey>>(msg);
            ChatPage::instance()->receivedDeviceVerificationKey(message.content);
        } else if (msg_type == to_string(mtx::events::EventType::KeyVerificationMac)) {
            auto message =
              std::get<mtx::events::DeviceEvent<mtx::events::msg::KeyVerificationMac>>(msg);
            ChatPage::instance()->receivedDeviceVerificationMac(message.content);
        } else if (msg_type == to_string(mtx::events::EventType::KeyVerificationStart)) {
            auto message =
              std::get<mtx::events::DeviceEvent<mtx::events::msg::KeyVerificationStart>>(msg);
            ChatPage::instance()->receivedDeviceVerificationStart(message.content, message.sender);
        } else if (msg_type == to_string(mtx::events::EventType::KeyVerificationReady)) {
            auto message =
              std::get<mtx::events::DeviceEvent<mtx::events::msg::KeyVerificationReady>>(msg);
            ChatPage::instance()->receivedDeviceVerificationReady(message.content);
        } else if (msg_type == to_string(mtx::events::EventType::KeyVerificationDone)) {
            auto message =
              std::get<mtx::events::DeviceEvent<mtx::events::msg::KeyVerificationDone>>(msg);
            ChatPage::instance()->receivedDeviceVerificationDone(message.content);
        } else if (auto e =
                     std::get_if<mtx::events::DeviceEvent<mtx::events::msg::SecretRequest>>(&msg)) {
            handle_secret_request(e, e->sender);
        } else {
            nhlog::crypto()->warn("unhandled event: {}", j_msg.dump(2));
        }
    }
}

void
handle_olm_message(const OlmMessage &msg, const UserKeyCache &otherUserDeviceKeys)
{
    nhlog::crypto()->info("sender    : {}", msg.sender);
    nhlog::crypto()->info("sender_key: {}", msg.sender_key);

    if (msg.sender_key == olm::client()->identity_keys().ed25519) {
        nhlog::crypto()->warn("Ignoring olm message from ourselves!");
        return;
    }

    const auto my_key = olm::client()->identity_keys().curve25519;

    bool failed_decryption = false;

    for (const auto &cipher : msg.ciphertext) {
        // We skip messages not meant for the current device.
        if (cipher.first != my_key) {
            nhlog::crypto()->debug(
              "Skipping message for {} since we are {}.", cipher.first, my_key);
            continue;
        }

        const auto type = cipher.second.type;
        nhlog::crypto()->info("type: {}", type == 0 ? "OLM_PRE_KEY" : "OLM_MESSAGE");

        auto payload = try_olm_decryption(msg.sender_key, cipher.second);

        if (payload.is_null()) {
            // Check for PRE_KEY message
            if (cipher.second.type == 0) {
                payload = handle_pre_key_olm_message(msg.sender, msg.sender_key, cipher.second);
            } else {
                nhlog::crypto()->error("Undecryptable olm message!");
                failed_decryption = true;
                continue;
            }
        }

        if (!payload.is_null()) {
            mtx::events::collections::DeviceEvents device_event;

            // Other properties are included in order to prevent an attacker from
            // publishing someone else's curve25519 keys as their own and subsequently
            // claiming to have sent messages which they didn't. sender must correspond
            // to the user who sent the event, recipient to the local user, and
            // recipient_keys to the local ed25519 key.
            std::string receiver_ed25519 = payload["recipient_keys"]["ed25519"].get<std::string>();
            if (receiver_ed25519.empty() ||
                receiver_ed25519 != olm::client()->identity_keys().ed25519) {
                nhlog::crypto()->warn("Decrypted event doesn't include our ed25519: {}",
                                      payload.dump());
                return;
            }
            std::string receiver = payload["recipient"].get<std::string>();
            if (receiver.empty() || receiver != http::client()->user_id().to_string()) {
                nhlog::crypto()->warn("Decrypted event doesn't include our user_id: {}",
                                      payload.dump());
                return;
            }

            // Clients must confirm that the sender_key and the ed25519 field value
            // under the keys property match the keys returned by /keys/query for the
            // given user, and must also verify the signature of the payload. Without
            // this check, a client cannot be sure that the sender device owns the
            // private part of the ed25519 key it claims to have in the Olm payload.
            // This is crucial when the ed25519 key corresponds to a verified device.
            std::string sender_ed25519 = payload["keys"]["ed25519"].get<std::string>();
            if (sender_ed25519.empty()) {
                nhlog::crypto()->warn("Decrypted event doesn't include sender ed25519: {}",
                                      payload.dump());
                return;
            }

            bool from_their_device = false;
            for (const auto &[device_id, key] : otherUserDeviceKeys.device_keys) {
                auto c_key = key.keys.find("curve25519:" + device_id);
                auto e_key = key.keys.find("ed25519:" + device_id);

                if (c_key == key.keys.end() || e_key == key.keys.end()) {
                    nhlog::crypto()->warn("Skipping device {} as we have no keys for it.",
                                          device_id);
                } else if (c_key->second == msg.sender_key && e_key->second == sender_ed25519) {
                    from_their_device = true;
                    break;
                }
            }
            if (!from_their_device) {
                nhlog::crypto()->warn("Decrypted event isn't sent from a device "
                                      "listed by that user! {}",
                                      payload.dump());
                return;
            }

            {
                std::string msg_type       = payload["type"].get<std::string>();
                nlohmann::json event_array = nlohmann::json::array();
                event_array.push_back(payload);

                std::vector<mtx::events::collections::DeviceEvents> temp_events;
                mtx::responses::utils::parse_device_events(event_array, temp_events);
                if (temp_events.empty()) {
                    nhlog::crypto()->warn("Decrypted unknown event: {}", payload.dump());
                    return;
                }
                device_event = temp_events.at(0);
            }

            using namespace mtx::events;
            if (auto e1 = std::get_if<DeviceEvent<msg::KeyVerificationAccept>>(&device_event)) {
                ChatPage::instance()->receivedDeviceVerificationAccept(e1->content);
            } else if (auto e2 =
                         std::get_if<DeviceEvent<msg::KeyVerificationRequest>>(&device_event)) {
                ChatPage::instance()->receivedDeviceVerificationRequest(e2->content, e2->sender);
            } else if (auto e3 =
                         std::get_if<DeviceEvent<msg::KeyVerificationCancel>>(&device_event)) {
                ChatPage::instance()->receivedDeviceVerificationCancel(e3->content);
            } else if (auto e4 = std::get_if<DeviceEvent<msg::KeyVerificationKey>>(&device_event)) {
                ChatPage::instance()->receivedDeviceVerificationKey(e4->content);
            } else if (auto e5 = std::get_if<DeviceEvent<msg::KeyVerificationMac>>(&device_event)) {
                ChatPage::instance()->receivedDeviceVerificationMac(e5->content);
            } else if (auto e6 =
                         std::get_if<DeviceEvent<msg::KeyVerificationStart>>(&device_event)) {
                ChatPage::instance()->receivedDeviceVerificationStart(e6->content, e6->sender);
            } else if (auto e7 =
                         std::get_if<DeviceEvent<msg::KeyVerificationReady>>(&device_event)) {
                ChatPage::instance()->receivedDeviceVerificationReady(e7->content);
            } else if (auto e8 =
                         std::get_if<DeviceEvent<msg::KeyVerificationDone>>(&device_event)) {
                ChatPage::instance()->receivedDeviceVerificationDone(e8->content);
            } else if (auto roomKey = std::get_if<DeviceEvent<msg::RoomKey>>(&device_event)) {
                create_inbound_megolm_session(*roomKey, msg.sender_key, sender_ed25519);
            } else if (auto forwardedRoomKey =
                         std::get_if<DeviceEvent<msg::ForwardedRoomKey>>(&device_event)) {
                forwardedRoomKey->content.forwarding_curve25519_key_chain.push_back(msg.sender_key);
                import_inbound_megolm_session(*forwardedRoomKey);
            } else if (auto e = std::get_if<DeviceEvent<msg::SecretSend>>(&device_event)) {
                auto local_user = http::client()->user_id();

                if (msg.sender != local_user.to_string())
                    return;

                auto secret_name_it = request_id_to_secret_name.find(e->content.request_id);

                if (secret_name_it != request_id_to_secret_name.end()) {
                    auto secret_name = secret_name_it->second;
                    request_id_to_secret_name.erase(secret_name_it);

                    nhlog::crypto()->info("Received secret: {}", secret_name);

                    mtx::events::msg::SecretRequest secretRequest{};
                    secretRequest.action = mtx::events::msg::RequestAction::Cancellation;
                    secretRequest.requesting_device_id = http::client()->device_id();
                    secretRequest.request_id           = e->content.request_id;

                    auto verificationStatus = cache::verificationStatus(local_user.to_string());

                    if (!verificationStatus)
                        return;

                    auto deviceKeys = cache::userKeys(local_user.to_string());
                    if (!deviceKeys)
                        return;

                    std::string sender_device_id;
                    for (auto &[dev, key] : deviceKeys->device_keys) {
                        if (key.keys["curve25519:" + dev] == msg.sender_key) {
                            sender_device_id = dev;
                            break;
                        }
                    }
                    if (!verificationStatus->verified_devices.count(sender_device_id) ||
                        !verificationStatus->verified_device_keys.count(msg.sender_key) ||
                        verificationStatus->verified_device_keys.at(msg.sender_key) !=
                          crypto::Trust::Verified) {
                        nhlog::net()->critical(
                          "Received secret from unverified device {}! Ignoring!", sender_device_id);
                        return;
                    }

                    std::map<mtx::identifiers::User,
                             std::map<std::string, mtx::events::msg::SecretRequest>>
                      body;

                    for (const auto &dev : verificationStatus->verified_devices) {
                        if (dev != secretRequest.requesting_device_id && dev != sender_device_id)
                            body[local_user][dev] = secretRequest;
                    }

                    if (!body.empty()) {
                        http::client()->send_to_device<mtx::events::msg::SecretRequest>(
                          http::client()->generate_txn_id(),
                          body,
                          [secret_name](mtx::http::RequestErr err) {
                              if (err) {
                                  nhlog::net()->error("Failed to send request cancellation "
                                                      "for secrect "
                                                      "'{}'",
                                                      secret_name);
                              }
                          });
                    }

                    nhlog::crypto()->info("Storing secret {}", secret_name);
                    cache::storeSecret(secret_name, e->content.secret);
                }

            } else if (auto sec_req = std::get_if<DeviceEvent<msg::SecretRequest>>(&device_event)) {
                handle_secret_request(sec_req, msg.sender);
            }

            return;
        } else {
            failed_decryption = true;
        }
    }

    if (failed_decryption) {
        try {
            std::map<std::string, std::vector<std::string>> targets;
            for (const auto &[device_id, key] : otherUserDeviceKeys.device_keys) {
                if (key.keys.at("curve25519:" + device_id) == msg.sender_key)
                    targets[msg.sender].push_back(device_id);
            }

            send_encrypted_to_device_messages(
              targets, mtx::events::DeviceEvent<mtx::events::msg::Dummy>{}, true);
            nhlog::crypto()->info(
              "Recovering from broken olm channel with {}:{}", msg.sender, msg.sender_key);
        } catch (std::exception &e) {
            nhlog::crypto()->error("Failed to recover from broken olm sessions: {}", e.what());
        }
    }
}

void
request_cross_signing_keys()
{
    mtx::events::msg::SecretRequest secretRequest{};
    secretRequest.action               = mtx::events::msg::RequestAction::Request;
    secretRequest.requesting_device_id = http::client()->device_id();

    auto local_user = http::client()->user_id();

    auto verificationStatus = cache::verificationStatus(local_user.to_string());

    if (!verificationStatus)
        return;

    auto request = [&](std::string secretName) {
        secretRequest.name       = secretName;
        secretRequest.request_id = "ss." + http::client()->generate_txn_id();

        request_id_to_secret_name[secretRequest.request_id] = secretRequest.name;

        std::map<mtx::identifiers::User, std::map<std::string, mtx::events::msg::SecretRequest>>
          body;

        for (const auto &dev : verificationStatus->verified_devices) {
            if (dev != secretRequest.requesting_device_id)
                body[local_user][dev] = secretRequest;
        }

        if (body.empty()) {
            nhlog::net()->warn("No verified devices to request {} from.", secretName);
            return;
        }

        http::client()->send_to_device<mtx::events::msg::SecretRequest>(
          http::client()->generate_txn_id(),
          body,
          [request_id = secretRequest.request_id, secretName](mtx::http::RequestErr err) {
              if (err) {
                  nhlog::net()->error("Failed to send request for secrect '{}'", secretName);
                  // Cancel request on UI thread
                  QTimer::singleShot(0, ChatPage::instance(), [request_id]() {
                      request_id_to_secret_name.erase(request_id);
                  });
                  return;
              }
          });

        for (const auto &dev : verificationStatus->verified_devices) {
            if (dev != secretRequest.requesting_device_id)
                body[local_user][dev].action = mtx::events::msg::RequestAction::Cancellation;
        }

        // timeout after 15 min
        QTimer::singleShot(15 * 60 * 1000, ChatPage::instance(), [secretRequest, body]() {
            if (request_id_to_secret_name.count(secretRequest.request_id)) {
                request_id_to_secret_name.erase(secretRequest.request_id);
                http::client()->send_to_device<mtx::events::msg::SecretRequest>(
                  http::client()->generate_txn_id(),
                  body,
                  [secretRequest](mtx::http::RequestErr err) {
                      if (err) {
                          nhlog::net()->error("Failed to cancel request for secrect '{}'",
                                              secretRequest.name);
                          return;
                      }
                  });
            }
        });
    };

    request(std::string(mtx::secret_storage::secrets::cross_signing_master));
    request(std::string(mtx::secret_storage::secrets::cross_signing_self_signing));
    request(std::string(mtx::secret_storage::secrets::cross_signing_user_signing));
    request(std::string(mtx::secret_storage::secrets::megolm_backup_v1));
}

} // namespace olm

#include "moc_Olm.cpp"
