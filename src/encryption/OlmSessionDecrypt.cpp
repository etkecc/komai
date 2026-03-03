// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <QDateTime>

#include <nlohmann/json.hpp>

#include "cache/Cache.h"
#include "logging/Logging.h"

namespace olm {

nlohmann::json
try_olm_decryption(const std::string &sender_key, const mtx::events::msg::OlmCipherContent &msg)
{
    auto session_ids = cache::getOlmSessions(sender_key);

    nhlog::crypto()->info("attempt to decrypt message with {} known session_ids",
                          session_ids.size());

    for (const auto &id : session_ids) {
        auto session = cache::getOlmSession(sender_key, id);

        if (!session) {
            nhlog::crypto()->warn("Unknown olm session: {}:{}", sender_key, id);
            continue;
        }

        mtx::crypto::BinaryBuf text;

        try {
            text = olm::client()->decrypt_message(session->get(), msg.type, msg.body);
            nhlog::crypto()->debug("Updated olm session: {}",
                                   mtx::crypto::session_id(session->get()));
            cache::saveOlmSession(
              sender_key, std::move(session.value()), QDateTime::currentMSecsSinceEpoch());
        } catch (const mtx::crypto::olm_exception &e) {
            nhlog::crypto()->debug("failed to decrypt olm message ({}, {}) with {}: {}",
                                   msg.type,
                                   sender_key,
                                   id,
                                   e.what());
            continue;
        } catch (const std::exception &e) {
            nhlog::crypto()->critical("failed to save session: {}", e.what());
            return {};
        }

        try {
            return nlohmann::json::parse(std::string_view((char *)text.data(), text.size()));
        } catch (const nlohmann::json::exception &e) {
            nhlog::crypto()->critical("failed to parse the decrypted session msg: {} {}",
                                      e.what(),
                                      std::string_view((char *)text.data(), text.size()));
        }
    }

    return {};
}

nlohmann::json
handle_pre_key_olm_message(const std::string &sender,
                           const std::string &sender_key,
                           const mtx::events::msg::OlmCipherContent &content)
{
    nhlog::crypto()->info("opening olm session with {}", sender);

    mtx::crypto::OlmSessionPtr inbound_session = nullptr;
    try {
        inbound_session = olm::client()->create_inbound_session_from(sender_key, content.body);

        // We also remove the one time key used to establish that
        // session so we'll have to update our copy of the account object.
        auto secret = cache::pickleSecret();
        if (!secret.empty())
            cache::saveOlmAccount(olm::client()->save(secret));
        else
            nhlog::crypto()->warn("skipping OLM account save: pickle secret unavailable");
    } catch (const mtx::crypto::olm_exception &e) {
        nhlog::crypto()->critical("failed to create inbound session with {}: {}", sender, e.what());
        return {};
    }

    if (!mtx::crypto::matches_inbound_session_from(
          inbound_session.get(), sender_key, content.body)) {
        nhlog::crypto()->warn("inbound olm session doesn't match sender's key ({})", sender);
        return {};
    }

    mtx::crypto::BinaryBuf output;
    try {
        output = olm::client()->decrypt_message(inbound_session.get(), content.type, content.body);
    } catch (const mtx::crypto::olm_exception &e) {
        nhlog::crypto()->critical("failed to decrypt olm message {}: {}", content.body, e.what());
        return {};
    }

    auto plaintext = nlohmann::json::parse(std::string((char *)output.data(), output.size()));
    nhlog::crypto()->debug("decrypted message: \n {}", plaintext.dump(2));

    try {
        nhlog::crypto()->debug("New olm session: {}",
                               mtx::crypto::session_id(inbound_session.get()));
        cache::saveOlmSession(
          sender_key, std::move(inbound_session), QDateTime::currentMSecsSinceEpoch());
    } catch (const std::exception &e) {
        nhlog::db()->warn("failed to save inbound olm session from {}: {}", sender, e.what());
    }

    return plaintext;
}

} // namespace olm
