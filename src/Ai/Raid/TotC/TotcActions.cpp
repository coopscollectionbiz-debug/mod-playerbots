/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TotcActions.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "TotcHelpers.h"
#include <array>

using namespace EncounterHelpers;
using namespace TotcHelpers;

// Shared

bool TotcInterruptAction::Execute(Event /*event*/)
{
    static constexpr std::array interruptSpells = {
        "kick", "pummel", "shield bash", "wind shear", "earth shock", "counterspell", "silencing shot",
        "mind freeze", };

    Unit* caster = GetTotcInterruptTarget(botAI);
    if (!caster)
        return false;

    for (char const* name : interruptSpells)
    {
        if (botAI->CanCastSpell(name, caster) && botAI->CastSpell(name, caster))
            return true;
    }

    return false;
}

// The Beasts of Northrend

bool TotcTauntGormokAction::Execute(Event event)
{
    Unit* gormok = AI_VALUE2(Unit*, "find target", "gormok the impaler");
    if (!gormok)
        return false;

    if (bot->GetVictim() != gormok)
        return Attack(gormok);

    return gormok->GetVictim() != bot && botAI->DoSpecificAction("taunt spell", event, true);
}

bool TotcKillSnoboldAction::Execute(Event /*event*/)
{
    Unit* snobold = GetNearestNpc(botAI, NPC_SNOBOLD_VASSAL, TOTC_SEARCH_DISTANCE, true);
    return snobold && Attack(snobold);
}

bool TotcCureParalyticToxinAction::Execute(Event /*event*/)
{
    Player* paralyzed = GetParalyzedGroupMember(bot);
    return paralyzed && MoveTo(paralyzed->GetMapId(), paralyzed->GetPositionX(), paralyzed->GetPositionY(),
                               paralyzed->GetPositionZ(), false, false, false, false,
                               MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool TotcAvoidSlimePoolAction::Execute(Event /*event*/)
{
    constexpr float SLIME_POOL_AVOID_DISTANCE = 8.0f;
    Unit* pool = GetNearestNpc(botAI, NPC_SLIME_POOL, SLIME_POOL_AVOID_DISTANCE);
    return pool && FleePosition(pool->GetPosition(), SLIME_POOL_AVOID_DISTANCE);
}

bool TotcDodgeTrampleAction::Execute(Event /*event*/)
{
    Position destination;
    if (!GetTrampleDodgePosition(botAI, destination))
        return false;

    return MoveTo(bot->GetMapId(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool TotcTankJormungarAction::Execute(Event event)
{
    Unit* worm = GetAssignedJormungar(botAI);
    if (!worm)
        return false;

    if (bot->GetVictim() != worm)
        return Attack(worm);

    return worm->GetVictim() != bot && botAI->DoSpecificAction("taunt spell", event, true);
}

bool TotcAttackAcidmawAction::Execute(Event /*event*/)
{
    Unit* acidmaw = GetNearestNpc(botAI, NPC_ACIDMAW, TOTC_SEARCH_DISTANCE, true);
    return acidmaw && Attack(acidmaw);
}

bool TotcJormungarMeleePositionAction::Execute(Event /*event*/)
{
    Position destination;
    if (!GetJormungarMeleePosition(botAI, AI_VALUE(Unit*, "current target"), destination))
        return false;

    return MoveTo(bot->GetMapId(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(),
                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

// Lord Jaraxxus

bool TotcSpreadFromRaidAction::Execute(Event /*event*/)
{
    Player* nearestPlayer = GetNearestPlayerInRadius(bot, TOTC_SPREAD_DISTANCE);
    return nearestPlayer && FleePosition(nearestPlayer->GetPosition(), TOTC_SPREAD_DISTANCE);
}

bool TotcKillJaraxxusAddAction::Execute(Event /*event*/)
{
    Unit* add = GetJaraxxusAddToKill(botAI);
    return add && Attack(add);
}

// The Faction Champions

bool TotcMarkChampionAction::Execute(Event /*event*/)
{
    // Keep a living champion skull so the raid doesn't split as health values cross.
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Unit* target = GetFactionChampionKillTarget(botAI);
    Unit* skull = botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::skullIndex));
    if (!target || (skull && skull->IsAlive() && IsFactionChampionHealer(skull->GetEntry()) ==
                                                     IsFactionChampionHealer(target->GetEntry())))
    {
        return false;
    }

    return MarkTargetWithSkull(bot, target);
}

bool TotcCrowdControlChampionAction::Execute(Event /*event*/)
{
    char const* spell = GetChampionCrowdControlSpell(bot);
    Unit* champion = spell ? GetAssignedChampionToControl(botAI) : nullptr;
    return champion && botAI->CastSpell(spell, champion);
}

// The Twin Val'kyr

bool TotcTakeEssenceAction::Execute(Event /*event*/)
{
    bool const light = WantsLightEssence(botAI);
    Unit* essence = GetNearestNpc(botAI, light ? NPC_LIGHT_ESSENCE : NPC_DARK_ESSENCE, TOTC_SEARCH_DISTANCE * 2.0f);
    if (!essence)
        return false;

    if (bot->GetDistance(essence) > INTERACTION_DISTANCE)
    {
        return MoveTo(essence->GetMapId(), essence->GetPositionX(), essence->GetPositionY(), essence->GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_FORCED, true, false);
    }

    // The essence is taken by talking to the portal.
    WorldPacket gossipHello(CMSG_GOSSIP_HELLO);
    gossipHello << essence->GetGUID();
    bot->GetSession()->HandleGossipHelloOpcode(gossipHello);
    return true;
}

bool TotcAttackTwinAction::Execute(Event /*event*/)
{
    Unit* twin = GetTwinToAttack(botAI);
    return twin && Attack(twin);
}

bool TotcAvoidOrbAction::Execute(Event /*event*/)
{
    Unit* orb = GetOppositeOrbInRange(botAI);
    return orb && MoveAway(orb, TWINS_ORB_AVOID_DISTANCE);
}

// Anub'arak

bool TotcKiteSpikeAction::Execute(Event /*event*/)
{
    // Guides run the pursued player through Permafrost, which stops the spike; otherwise away from it.
    constexpr float ON_PERMAFROST_DISTANCE = 2.0f;
    if (Unit* permafrost = GetNearestPermafrost(botAI))
    {
        if (bot->GetExactDist2d(permafrost) <= ON_PERMAFROST_DISTANCE)
            return false;

        return MoveTo(permafrost->GetMapId(), permafrost->GetPositionX(), permafrost->GetPositionY(),
                      permafrost->GetPositionZ(), false, false, false, true, MovementPriority::MOVEMENT_FORCED,
                      true, false);
    }

    constexpr float SPIKE_KITE_DISTANCE = 15.0f;
    Unit* spike = GetNearestNpc(botAI, NPC_ANUBARAK_SPIKE, TOTC_SEARCH_DISTANCE);
    return spike && FleePosition(spike->GetPosition(), SPIKE_KITE_DISTANCE);
}

bool TotcAvoidSpikeAction::Execute(Event /*event*/)
{
    Unit* spike = GetNearestNpc(botAI, NPC_ANUBARAK_SPIKE, SPIKE_AVOID_DISTANCE);
    return spike && MoveAway(spike, SPIKE_AVOID_DISTANCE);
}

bool TotcTankBurrowerAction::Execute(Event event)
{
    Unit* burrower = GetUntankedNpc(botAI, NPC_NERUBIAN_BURROWER);
    if (!burrower)
        return false;

    if (bot->GetVictim() != burrower)
        return Attack(burrower);

    return burrower->GetVictim() != bot && botAI->DoSpecificAction("taunt spell", event, true);
}

bool TotcKillFrostSphereAction::Execute(Event /*event*/)
{
    constexpr float PERMAFROST_NEARBY_DISTANCE = 30.0f;
    Unit* sphere = GetNearestNpc(botAI, NPC_FROST_SPHERE, PERMAFROST_NEARBY_DISTANCE, true);
    return sphere && Attack(sphere);
}
