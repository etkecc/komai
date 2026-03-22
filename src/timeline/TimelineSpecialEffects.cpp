// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineSpecialEffects.h"

#include <array>
#include <span>
#include <string_view>

#include <QString>
#include <QStringView>

namespace timeline::effects {
namespace {

constexpr std::string_view CONFETTI_MSGTYPE = "nic.custom.confetti";
constexpr std::string_view RAINFALL_MSGTYPE = "io.element.effect.rainfall";

const std::array<QStringView, 2> CONFETTI_TRIGGERS = {
  QStringView(u"🎉"),
  QStringView(u"🎊"),
};
const std::array<QStringView, 3> RAINFALL_TRIGGERS = {
  QStringView(u"🌧"),
  QStringView(u"🌦"),
  QStringView(u"☔"),
};
const std::array<QStringView, 1> LIGHTNING_TRIGGERS = {
  QStringView(u"⚡"),
};
const std::array<QStringView, 1> STORM_TRIGGERS = {
  QStringView(u"⛈"),
};
const std::array<QStringView, 2> KOMAI_LOGO_TRIGGERS = {
  QStringView(u"🦁"),
  QStringView(u"⛩️"),
};

bool
containsAny(QStringView body, std::span<const QStringView> triggers)
{
    for (const auto trigger : triggers) {
        if (body.contains(trigger))
            return true;
    }

    return false;
}

std::span<const QStringView>
triggersFor(SpecialEffect effect)
{
    switch (effect) {
    case SpecialEffect::Confetti:
        return CONFETTI_TRIGGERS;
    case SpecialEffect::Rainfall:
        return RAINFALL_TRIGGERS;
    case SpecialEffect::Lightning:
        return LIGHTNING_TRIGGERS;
    case SpecialEffect::KomaiLogo:
        return KOMAI_LOGO_TRIGGERS;
    }

    return {};
}

std::span<const QStringView>
combinedTriggersFor(SpecialEffect effect)
{
    switch (effect) {
    case SpecialEffect::Rainfall:
    case SpecialEffect::Lightning:
        return STORM_TRIGGERS;
    case SpecialEffect::Confetti:
    case SpecialEffect::KomaiLogo:
        return {};
    }

    return {};
}

void
addContentTriggeredEffects(SpecialEffects &effects, QStringView body)
{
    if (bodyHasTrigger(SpecialEffect::Confetti, body))
        appendUnique(effects, SpecialEffect::Confetti);

    if (bodyHasTrigger(SpecialEffect::Rainfall, body))
        appendUnique(effects, SpecialEffect::Rainfall);

    if (bodyHasTrigger(SpecialEffect::Lightning, body))
        appendUnique(effects, SpecialEffect::Lightning);

    if (bodyHasTrigger(SpecialEffect::KomaiLogo, body))
        appendUnique(effects, SpecialEffect::KomaiLogo);
}

} // namespace

SpecialEffects
detect(const mtx::events::collections::TimelineEvents &event)
{
    using namespace mtx::events;

    SpecialEffects effects;

    if (auto text = std::get_if<RoomEvent<msg::Text>>(&event)) {
        addContentTriggeredEffects(effects, QString::fromStdString(text->content.body));
    } else if (auto unknown = std::get_if<RoomEvent<msg::Unknown>>(&event)) {
        addContentTriggeredEffects(effects, QString::fromStdString(unknown->content.body));
    } else if (auto effect = std::get_if<RoomEvent<msg::ElementEffect>>(&event)) {
        if (effect->content.msgtype == CONFETTI_MSGTYPE) {
            appendUnique(effects, SpecialEffect::Confetti);
        } else if (effect->content.msgtype == RAINFALL_MSGTYPE) {
            appendUnique(effects, SpecialEffect::Rainfall);
        }
    }

    return effects;
}

void
appendUnique(SpecialEffects &target, SpecialEffect effect)
{
    if (!target.contains(effect))
        target.push_back(effect);
}

void
appendUnique(SpecialEffects &target, const SpecialEffects &effects)
{
    for (const auto effect : effects)
        appendUnique(target, effect);
}

bool
bodyHasTrigger(SpecialEffect effect, QStringView body)
{
    return containsAny(body, triggersFor(effect)) || containsAny(body, combinedTriggersFor(effect));
}

QString
effectName(SpecialEffect effect)
{
    switch (effect) {
    case SpecialEffect::Confetti:
        return QStringLiteral("confetti");
    case SpecialEffect::Rainfall:
        return QStringLiteral("rainfall");
    case SpecialEffect::Lightning:
        return QStringLiteral("lightning");
    case SpecialEffect::KomaiLogo:
        return QStringLiteral("komaiLogo");
    }

    return {};
}

QStringList
effectNames(const SpecialEffects &effects)
{
    QStringList names;
    names.reserve(effects.size());

    for (const auto effect : effects)
        names.push_back(effectName(effect));

    return names;
}

} // namespace timeline::effects
