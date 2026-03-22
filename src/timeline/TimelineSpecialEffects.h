// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>
#include <QStringView>
#include <QVector>

#include <mtx/events/collections.hpp>

namespace timeline::effects {

enum class SpecialEffect
{
    Confetti,
    Rainfall,
    Lightning,
    KomaiLogo,
};

using SpecialEffects = QVector<SpecialEffect>;

SpecialEffects
detect(const mtx::events::collections::TimelineEvents &event);
void
appendUnique(SpecialEffects &target, SpecialEffect effect);
void
appendUnique(SpecialEffects &target, const SpecialEffects &effects);
bool
bodyHasTrigger(SpecialEffect effect, QStringView body);
QString
effectName(SpecialEffect effect);
QStringList
effectNames(const SpecialEffects &effects);

} // namespace timeline::effects
