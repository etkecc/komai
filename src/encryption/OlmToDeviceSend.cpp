// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include "logging/Logging.h"

namespace olm {

//! Send encrypted to device messages, targets is a map from userid to device ids or {} for all
//! devices
void
send_encrypted_to_device_messages(const std::map<std::string, std::vector<std::string>> &targets,
                                  const mtx::events::collections::DeviceEvents &event,
                                  bool force_new_session)
{
    (void)event;
    (void)force_new_session;
    nhlog::crypto()->warn(
      "Ignoring legacy encrypted to-device send for {} target users; this flow is not "
      "migrated to the matrix-sdk backend yet",
      targets.size());
}

} // namespace olm
