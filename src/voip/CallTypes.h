// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaType>

#include <cstdint>
#include <string>
#include <vector>

namespace komai::voip {

struct CallIceCandidate
{
    std::string sdpMid;
    uint16_t sdpMLineIndex = 0;
    std::string candidate;
};

using CallIceCandidateList = std::vector<CallIceCandidate>;

} // namespace komai::voip

Q_DECLARE_METATYPE(komai::voip::CallIceCandidate)
Q_DECLARE_METATYPE(komai::voip::CallIceCandidateList)
