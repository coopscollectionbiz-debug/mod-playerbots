/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DruidShapeshiftActions.h"
#include "Playerbots.h"

bool CastBearFormAction::isUseful()
{
    return CastBuffSpellAction::isUseful() && !botAI->HasAura("dire bear form", GetTarget());
}

bool CastBearFormAction::isPossible()
{
    return CastBuffSpellAction::isPossible() && !botAI->HasAura("dire bear form", GetTarget());
}

std::vector<NextAction> CastDireBearFormAction::getAlternatives()
{
    return NextAction::merge({NextAction("bear form")}, CastSpellAction::getAlternatives());
}

bool CastTravelFormAction::isUseful()
{
    bool firstmount = bot->GetLevel() >= 20;

    // useful if no mount or with wsg flag
    return !bot->IsMounted() && (!firstmount || (bot->HasAura(23333) || bot->HasAura(23335) || bot->HasAura(34976))) &&
           !botAI->HasAura("dash", bot);
}

bool CastCasterFormAction::Execute(Event /*event*/)
{
    botAI->RemoveShapeshift();
    return true;
}

bool CastCasterFormAction::isUseful()
{
    // A tank druid in combat stays a bear: every druid heal / cure / buff has caster form as a prerequisite, so one
    // between swings left bear form and the next boss hit killed it at caster health (Nightbane and Morogrim tanks died
    // at 98-100% to one 8-9k swing, 2026-11-21; the Naxx40 Horsemen veto is the same rule for its latched tanks).
    if (bot->IsInCombat() && PlayerbotAI::IsTank(bot) && botAI->HasAnyAuraOf(bot, "dire bear form", "bear form", nullptr))
        return false;
    return botAI->HasAnyAuraOf(GetTarget(), "dire bear form", "bear form", "cat form", "travel form", "aquatic form",
                               "flight form", "swift flight form", "moonkin form", nullptr) &&
           AI_VALUE2(uint8, "mana", "self target") > sPlayerbotAIConfig.mediumHealth;
}

bool CastCancelDruidAction::Execute(Event /*event*/)
{
    botAI->RemoveAura(auraName);
    return true;
}

bool CastCancelDruidAction::isUseful() { return bot->HasAura(auraId); }

bool CastTreeFormAction::isUseful()
{
    constexpr uint32 SPELL_TREE_OF_LIFE = 33891;
    return GetTarget() && CastSpellAction::isUseful() && !bot->HasAura(SPELL_TREE_OF_LIFE);
}
