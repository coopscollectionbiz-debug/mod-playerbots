/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "ObjectGuid.h"
#include "Playerbots.h"

bool AnubrekhanChooseTargetAction::Execute(Event /*event*/)
{
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    Unit* target = nullptr;
    Unit* target_boss = nullptr;
    std::vector<Unit*> target_guards;
    Unit* loose_scarab = nullptr;
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;
        if (botAI->EqualLowercaseName(unit->GetName(), "crypt guard"))
            target_guards.push_back(unit);

        if (botAI->EqualLowercaseName(unit->GetName(), "anub'rekhan"))
            target_boss = unit;

        // Corpse Scarabs (five from every raid member who dies) on someone other than a tank. One on a
        // healer comes first: a dead healer starts the next five.
        if (botAI->EqualLowercaseName(unit->GetName(), "corpse scarab") && unit->IsAlive() && unit->GetVictim() &&
            unit->GetVictim()->ToPlayer() && !botAI->IsTank(unit->GetVictim()->ToPlayer()))
        {
            bool const onHealer = botAI->IsHeal(unit->GetVictim()->ToPlayer());
            bool const bestOnHealer = loose_scarab && loose_scarab->GetVictim() && loose_scarab->GetVictim()->ToPlayer() &&
                                      botAI->IsHeal(loose_scarab->GetVictim()->ToPlayer());
            if (!loose_scarab || (onHealer && !bestOnHealer) ||
                (onHealer == bestOnHealer && bot->GetExactDist(unit) < bot->GetExactDist(loose_scarab)))
                loose_scarab = unit;
        }
    }
    // Every damage dealer, melee and ranged, clears the scarabs off the raid first: they come in fives and
    // finish off the healers and casters they land on, and each death brings five more. Melee alone was
    // too slow (Corpse Scarabs: 28 of 45 deaths in the 11-19 item level 70 wipes). An off-tank with no Crypt
    // Guard loose takes them too, so they end up on plate instead of cloth.
    bool guardLoose = false;
    for (Unit* g : target_guards)
        if (g->GetVictim() && g->GetVictim()->ToPlayer() && !botAI->IsTank(g->GetVictim()->ToPlayer()))
            guardLoose = true;
    bool const takesScarabs = (!botAI->IsTank(bot) && !botAI->IsHeal(bot)) ||
                              (botAI->IsAssistTank(bot) && !botAI->IsMainTank(bot) && !guardLoose);
    if (loose_scarab && takesScarabs)
    {
        if (context->GetValue<Unit*>("current target")->Get() == loose_scarab)
            return false;
        return Attack(loose_scarab);
    }
    if (botAI->IsMainTank(bot))
        target = target_boss;
    else
    {
        if (target_guards.size() == 0)
            target = target_boss;
        else
        {
            if (botAI->IsAssistTank(bot))
            {
                for (Unit* t : target_guards)
                {
                    if (target == nullptr || (target->GetVictim() && target->GetVictim()->ToPlayer() &&
                                              botAI->IsTank(target->GetVictim()->ToPlayer())))
                        target = t;
                }
            }
            else
            {
                for (Unit* t : target_guards)
                {
                    if (target == nullptr || target->GetHealthPct() > t->GetHealthPct())
                        target = t;
                }
            }
        }
    }
    if (context->GetValue<Unit*>("current target")->Get() == target)
        return false;

    return Attack(target);
}

bool AnubrekhanPositionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anub'rekhan");
    if (!boss)
        return false;

    bool inPhase = botAI->HasAura("locust swarm", boss) || boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (inPhase)
    {
        if (botAI->IsMainTank(bot))
        {
            uint32 nearest = FindNearestWaypoint();
            uint32 next_point;
            next_point = (nearest + 1) % intervals;

            return MoveTo(bot->GetMapId(), waypoints[next_point].first, waypoints[next_point].second,
                          bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
        else
            return MoveInside(533, 3272.49f, -3476.27f, bot->GetPositionZ(), 3.0f, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}
