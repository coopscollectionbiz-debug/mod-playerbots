/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TotcTriggers.h"
#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "TotcHelpers.h"

using namespace EncounterHelpers;
using namespace TotcHelpers;

// Shared

bool TotcInterruptTrigger::IsActive()
{
    if (bot->GetMapId() != TOTC_MAP_ID)
        return false;

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_ROGUE:
        case CLASS_SHAMAN:
        case CLASS_MAGE:
        case CLASS_HUNTER:
        case CLASS_DEATH_KNIGHT:
            break;
        default:
            return false;
    }

    return GetTotcInterruptTarget(botAI) != nullptr;
}

// The Beasts of Northrend

bool TotcGormokImpaleTrigger::IsActive()
{
    // Guides taunt Gormok off a tank at 3-4 Impale stacks.
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* gormok = AI_VALUE2(Unit*, "find target", "gormok the impaler");
    Unit* victim = gormok ? gormok->GetVictim() : nullptr;
    if (!victim || victim == bot || GetDifficultyAuraStacks(victim, SPELL_IMPALE) < IMPALE_SWAP_STACKS)
        return false;

    return GetTakeoverTank(bot, victim, [](Player const* tank)
        { return GetDifficultyAuraStacks(tank, SPELL_IMPALE) >= IMPALE_SWAP_STACKS; }) == bot;
}

bool TotcSnoboldVassalTrigger::IsActive()
{
    // Guides kill the Snobold Vassals that jump onto players' heads.
    if (!PlayerbotAI::IsDps(bot) || !AI_VALUE2(Unit*, "find target", "gormok the impaler"))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->GetEntry() == NPC_SNOBOLD_VASSAL)
        return false;

    return GetNearestNpc(botAI, NPC_SNOBOLD_VASSAL, TOTC_SEARCH_DISTANCE, true) != nullptr;
}

bool TotcBurningBileTrigger::IsActive()
{
    // Guides take Burning Bile to the players Paralytic Toxin is slowing, which cleanses them.
    if (!HasDifficultyAura(bot, SPELL_BURNING_BILE))
        return false;

    constexpr float CURE_DISTANCE = 3.0f;
    Player* paralyzed = GetParalyzedGroupMember(bot);
    return paralyzed && bot->GetExactDist2d(paralyzed) > CURE_DISTANCE;
}

bool TotcSlimePoolTrigger::IsActive()
{
    // The worms' Slime Pools grow; the distance is ours.
    constexpr float SLIME_POOL_AVOID_DISTANCE = 8.0f;
    return IsTotcEncounter(bot) && GetNearestNpc(botAI, NPC_SLIME_POOL, SLIME_POOL_AVOID_DISTANCE) != nullptr;
}

bool TotcIcehowlTrampleTrigger::IsActive()
{
    // Guides use the speed Massive Crash gives to get out of Trample's path.
    Position destination;
    return IsTotcEncounter(bot) && GetTrampleDodgePosition(botAI, destination);
}

bool TotcJormungarTankTrigger::IsActive()
{
    // A tank picks its worm back up whenever it is mobile and on someone else.
    Unit* worm = GetAssignedJormungar(botAI);
    if (!worm || !IsJormungarMobile(worm))
        return false;

    return bot->GetVictim() != worm || worm->GetVictim() != bot;
}

bool TotcJormungarKillOrderTrigger::IsActive()
{
    // Guides burn Acidmaw first; Dreadscale enrages when he dies.
    if (!PlayerbotAI::IsDps(bot))
        return false;

    Unit* acidmaw = GetNearestNpc(botAI, NPC_ACIDMAW, TOTC_SEARCH_DISTANCE, true);
    return acidmaw && AI_VALUE(Unit*, "current target") != acidmaw;
}

bool TotcJormungarMeleeTrigger::IsActive()
{
    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsTank(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || (target->GetEntry() != NPC_ACIDMAW && target->GetEntry() != NPC_DREADSCALE))
        return false;

    Position destination;
    return GetJormungarMeleePosition(botAI, target, destination);
}

// Lord Jaraxxus

bool TotcLegionFlameTrigger::IsActive()
{
    // Guides keep moving with Legion Flame, away from everyone else.
    return HasDifficultyAura(bot, SPELL_LEGION_FLAME) && GetNearestPlayerInRadius(bot, TOTC_SPREAD_DISTANCE);
}

bool TotcJaraxxusAddTrigger::IsActive()
{
    if (!PlayerbotAI::IsDps(bot) || !AI_VALUE2(Unit*, "find target", "lord jaraxxus"))
        return false;

    Unit* add = GetJaraxxusAddToKill(botAI);
    return add && AI_VALUE(Unit*, "current target") != add;
}

// The Faction Champions

bool TotcChampionKillOrderTrigger::IsActive()
{
    // One marker per raid.
    return IsTotcEncounter(bot) && IsMechanicTrackerBot(bot, TOTC_MAP_ID) && GetFactionChampionKillTarget(botAI);
}

bool TotcChampionCrowdControlTrigger::IsActive()
{
    // Guides hold the champions they aren't killing with crowd control.
    char const* spell = GetChampionCrowdControlSpell(bot);
    if (!spell || !IsTotcEncounter(bot))
        return false;

    Unit* champion = GetAssignedChampionToControl(botAI);
    return champion && !botAI->HasAura(spell, champion) && botAI->CanCastSpell(spell, champion);
}

// The Twin Val'kyr

bool TotcTwinsWrongEssenceTrigger::IsActive()
{
    if (!AI_VALUE2(Unit*, "find target", "fjola lightbane") && !AI_VALUE2(Unit*, "find target", "eydis darkbane"))
        return false;

    return !HasEssence(bot, WantsLightEssence(botAI));
}

bool TotcTwinsTargetTrigger::IsActive()
{
    if (!PlayerbotAI::IsDps(bot))
        return false;

    Unit* twin = GetTwinToAttack(botAI);
    return twin && AI_VALUE(Unit*, "current target") != twin;
}

bool TotcTwinsOppositeOrbTrigger::IsActive()
{
    // An orb of the other color explodes on touch; guides largely leave the orbs alone.
    return IsTotcEncounter(bot) && GetOppositeOrbInRange(botAI) != nullptr;
}

// Anub'arak

bool TotcPursuedByAnubarakTrigger::IsActive()
{
    return bot->GetMapId() == TOTC_MAP_ID && HasDifficultyAura(bot, SPELL_PURSUED);
}

bool TotcSpikeNearbyTrigger::IsActive()
{
    // Everyone but the pursued player stays out of the spike's path.
    if (bot->GetMapId() != TOTC_MAP_ID || HasDifficultyAura(bot, SPELL_PURSUED))
        return false;

    return GetNearestNpc(botAI, NPC_ANUBARAK_SPIKE, SPIKE_AVOID_DISTANCE) != nullptr;
}

bool TotcBurrowerUntankedTrigger::IsActive()
{
    // Guides hold the Nerubian Burrowers on the off-tanks.
    if (!PlayerbotAI::IsAssistTank(bot) || !AI_VALUE2(Unit*, "find target", "anub'arak"))
        return false;

    return GetUntankedNpc(botAI, NPC_NERUBIAN_BURROWER) != nullptr;
}

bool TotcFrostSphereTrigger::IsActive()
{
    // A dead Frost Sphere leaves Permafrost, which stops the Pursuing Spike; ranged make one near the
    // raid while Anub'arak is submerged.
    if (!PlayerbotAI::IsRangedDps(bot))
        return false;

    Unit* anubarak = GetNearestNpc(botAI, NPC_ANUBARAK, TOTC_SEARCH_DISTANCE);
    if (!IsAnubarakSubmerged(anubarak))
        return false;

    constexpr float PERMAFROST_NEARBY_DISTANCE = 30.0f;
    Unit* permafrost = GetNearestPermafrost(botAI);
    if (permafrost && bot->GetExactDist2d(permafrost) < PERMAFROST_NEARBY_DISTANCE)
        return false;

    return GetNearestNpc(botAI, NPC_FROST_SPHERE, PERMAFROST_NEARBY_DISTANCE, true) != nullptr;
}
