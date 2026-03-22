// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <iostream>
#include <string_view>

#include <QStringList>

#include <mtx/events/messages/elementeffect.hpp>
#include <mtx/events/messages/text.hpp>
#include <mtx/events/messages/unknown.hpp>

#include "timeline/TimelineSpecialEffects.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
containsEffect(const timeline::effects::SpecialEffects &effects, timeline::effects::SpecialEffect effect)
{
    return std::find(effects.begin(), effects.end(), effect) != effects.end();
}

mtx::events::RoomEvent<mtx::events::msg::Text>
makeTextEvent(std::string_view body)
{
    mtx::events::RoomEvent<mtx::events::msg::Text> event;
    event.content.body    = std::string(body);
    event.content.msgtype = "m.text";
    return event;
}

mtx::events::RoomEvent<mtx::events::msg::Unknown>
makeUnknownEvent(std::string_view body)
{
    mtx::events::RoomEvent<mtx::events::msg::Unknown> event;
    event.content.body    = std::string(body);
    event.content.msgtype = "org.example.unknown";
    return event;
}

mtx::events::RoomEvent<mtx::events::msg::ElementEffect>
makeElementEffectEvent(std::string_view msgtype)
{
    mtx::events::RoomEvent<mtx::events::msg::ElementEffect> event;
    event.content.msgtype = std::string(msgtype);
    return event;
}

bool
testContentTriggeredEffects()
{
    bool ok = true;

    const auto confetti =
      timeline::effects::detect(mtx::events::collections::TimelineEvents{makeTextEvent("hi 🎊")});
    ok &= expect(containsEffect(confetti, timeline::effects::SpecialEffect::Confetti),
                 "confetti emoji triggers confetti effect");

    const auto komaiLogo =
      timeline::effects::detect(mtx::events::collections::TimelineEvents{makeUnknownEvent("🦁")});
    ok &= expect(containsEffect(komaiLogo, timeline::effects::SpecialEffect::KomaiLogo),
                 "unknown message body can trigger komai logo effect");

    const auto rainfall =
      timeline::effects::detect(mtx::events::collections::TimelineEvents{makeTextEvent("bring ☔☔")});
    ok &= expect(containsEffect(rainfall, timeline::effects::SpecialEffect::Rainfall),
                 "rain emojis trigger rainfall effect");

    const auto lightning =
      timeline::effects::detect(mtx::events::collections::TimelineEvents{makeTextEvent("⚡ now")});
    ok &= expect(containsEffect(lightning, timeline::effects::SpecialEffect::Lightning),
                 "lightning emoji triggers lightning effect");

    const auto rainfallVariant = timeline::effects::detect(
      mtx::events::collections::TimelineEvents{makeUnknownEvent("later 🌧️ then 🌦️ then ⛈️")});
    ok &= expect(containsEffect(rainfallVariant, timeline::effects::SpecialEffect::Rainfall),
                 "rain emoji variants trigger rainfall effect");

    const auto combined = timeline::effects::detect(
      mtx::events::collections::TimelineEvents{makeTextEvent("🎉⚡🌧🦁")});
    ok &= expect(containsEffect(combined, timeline::effects::SpecialEffect::Confetti),
                 "combined body keeps confetti effect");
    ok &= expect(containsEffect(combined, timeline::effects::SpecialEffect::Rainfall),
                 "combined body keeps rainfall effect");
    ok &= expect(containsEffect(combined, timeline::effects::SpecialEffect::Lightning),
                 "combined body keeps lightning effect");
    ok &= expect(containsEffect(combined, timeline::effects::SpecialEffect::KomaiLogo),
                 "combined body keeps komai logo effect");

    return ok;
}

bool
testExplicitEffectMsgtypes()
{
    bool ok = true;

    const auto confetti = timeline::effects::detect(
      mtx::events::collections::TimelineEvents{makeElementEffectEvent("nic.custom.confetti")});
    ok &= expect(containsEffect(confetti, timeline::effects::SpecialEffect::Confetti),
                 "explicit confetti msgtype triggers confetti");

    const auto rainfall = timeline::effects::detect(
      mtx::events::collections::TimelineEvents{makeElementEffectEvent("io.element.effect.rainfall")});
    ok &= expect(containsEffect(rainfall, timeline::effects::SpecialEffect::Rainfall),
                 "explicit rainfall msgtype triggers rainfall");

    return ok;
}

bool
testEffectNamesFollowInsertionOrder()
{
    timeline::effects::SpecialEffects effects;
    timeline::effects::appendUnique(effects, timeline::effects::SpecialEffect::Confetti);
    timeline::effects::appendUnique(effects, timeline::effects::SpecialEffect::Lightning);
    timeline::effects::appendUnique(effects, timeline::effects::SpecialEffect::KomaiLogo);

    const QStringList names = timeline::effects::effectNames(effects);

    bool ok = true;
    ok &= expect(names.size() == 3, "three effect names are returned");
    ok &= expect(names.at(0) == QStringLiteral("confetti"),
                 "first effect name matches insertion order");
    ok &= expect(names.at(1) == QStringLiteral("lightning"),
                 "second effect name matches insertion order");
    ok &= expect(names.at(2) == QStringLiteral("komaiLogo"),
                 "second effect name matches insertion order");
    return ok;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testContentTriggeredEffects();
    ok &= testExplicitEffectMsgtypes();
    ok &= testEffectNamesFollowInsertionOrder();
    return ok ? 0 : 1;
}
