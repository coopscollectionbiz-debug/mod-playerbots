/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPTriggers.h"
#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "SWPHelpers.h"
#include "SpellAuras.h"

using namespace EncounterHelpers;
using namespace SunwellPlateauHelpers;

static bool IsTankPlayer(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && PlayerbotAI::IsTank(player);
}

// General

bool SwpKillOrderMarkTrigger::IsActive()
{
    // One marker per raid; encounter-only, so trash keeps normal targeting.
    return bot->IsInCombat() && IsEncounterInProgress(bot, MAP_SUNWELL_PLATEAU) &&
           IsMechanicTrackerBot(bot, MAP_SUNWELL_PLATEAU) && GetKillOrderMarkTarget(botAI);
}

bool SwpCcMarkTrigger::IsActive()
{
    return bot->IsInCombat() && IsEncounterInProgress(bot, MAP_SUNWELL_PLATEAU) &&
           IsMechanicTrackerBot(bot, MAP_SUNWELL_PLATEAU) && GetCcMarkTarget(botAI);
}

bool SwpAvoidHazardTrigger::IsActive()
{
    return GetNearestHazard(botAI) != nullptr;
}

// Kalecgos

bool SwpKalecgosTakePortalTrigger::IsActive()
{
    return IsKalecgosPortalTeamMember(botAI);
}

bool SwpKalecgosHealKalecTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot) || !IsInSpectralRealm(bot))
        return false;

    Unit* kalec = FindCreatureAnyRealm(bot, NPC_KALEC);
    return kalec && kalec->GetHealthPct() < KALEC_HEAL_BELOW_PCT;
}

bool SwpKalecgosNotVictimTrigger::IsActive()
{
    // Frost Breath covers his front and Tail Lash his rear; everyone but his tank stands at his sides.
    if (PlayerbotAI::IsTank(bot) || IsInSpectralRealm(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "kalecgos");
    return boss && boss->GetEntry() == NPC_KALECGOS && boss->GetVictim() != bot;
}

// Brutallus

static bool IsBrutallusSwapStacked(Unit const* tank)
{
    return GetAuraStackAmount(tank, SPELL_METEOR_SLASH) >= BRUTALLUS_SWAP_STACKS ||
           tank->HasAura(SPELL_BRUTALLUS_STOMP);
}

bool SwpBrutallusTankSwapTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "brutallus");
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    if (!victim || victim == bot)
        return false;

    // Guides taunt once the other tank has more than 2 Meteor Slash stacks or the Stomp debuff.
    if (IsTankPlayer(victim) && !IsBrutallusSwapStacked(victim))
        return false;

    return GetTakeoverTank(bot, victim, [](Player const* tank) { return IsBrutallusSwapStacked(tank); }) == bot;
}

bool SwpBrutallusSoakTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "brutallus");
    if (!IsBrutallusSoaker(bot, boss))
        return false;

    return !IsInBrutallusSoakSpot(bot, boss);
}

bool SwpBrutallusNotSoakerTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "brutallus");
    return boss && boss->GetVictim() != bot && !IsBrutallusSoaker(bot, boss);
}

bool SwpBrutallusBurnTrigger::IsActive()
{
    // The burned player leaves the group so Burn doesn't spread.
    return bot->HasAura(SPELL_BURN) && GetNearestPlayerInRadius(bot, GetBurnSpreadRadius(bot)) != nullptr;
}

// Felmyst

bool SwpFelmystNotVictimTrigger::IsActive()
{
    // Cleave hits her front: only the tank stands there, melee behind and ranged at her sides.
    if (PlayerbotAI::IsTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "felmyst");
    return boss && !boss->IsFlying() && boss->GetVictim() != bot;
}

bool SwpFelmystVaporTrigger::IsActive()
{
    // The Demonic Vapor follows its target and leaves a trail; that player leads it away from the raid.
    Unit* vapor = GetNearestNpc(botAI, NPC_DEMONIC_VAPOR, SWP_ADD_SEARCH_DISTANCE);
    if (!vapor || !IsDemonicVaporTarget(vapor, bot))
        return false;

    return GetNearestPlayerInRadius(bot, FELMYST_VAPOR_LEAD_DISTANCE) != nullptr;
}

bool SwpFelmystLaneTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "felmyst");
    if (!boss)
        boss = GetNearestNpc(botAI, NPC_FELMYST, SWP_ADD_SEARCH_DISTANCE);

    return IsInFelmystLane(bot, GetFelmystBreathLane(boss));
}

// Eredar Twins

bool SwpTwinsSpreadTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    if (!AI_VALUE2(Unit*, "find target", "lady sacrolash") &&
        !AI_VALUE2(Unit*, "find target", "grand warlock alythess"))
        return false;

    return GetNearestPlayerInRadius(bot, TWINS_SPREAD_DISTANCE) != nullptr;
}

bool SwpSacrolashTankTrigger::IsActive()
{
    if (!IsSacrolashTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "lady sacrolash");
    if (!boss)
        return false;

    // Confounding Blow drops her threat on the tank, so the other Sacrolash tank takes over.
    Unit* victim = boss->GetVictim();
    if (victim && !IsTankPlayer(victim))
    {
        Player const* alythessTank = GetAlythessTank(bot);
        return GetTakeoverTank(bot, victim, [alythessTank](Player const* tank) { return tank == alythessTank; }) ==
               bot;
    }

    return bot->GetVictim() != boss;
}

bool SwpAlythessTankTrigger::IsActive()
{
    if (GetAlythessTank(bot) != bot)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "grand warlock alythess");
    return boss && (boss->GetVictim() != bot || bot->GetVictim() != boss);
}

// M'uru

bool SwpVoidSentinelTankTrigger::IsActive()
{
    if (GetGroupAssistTank(bot, 0) != bot)
        return false;

    Unit* sentinel = GetNearestNpc(botAI, NPC_VOID_SENTINEL, SWP_ADD_SEARCH_DISTANCE);
    return sentinel && (sentinel->GetVictim() != bot || bot->GetVictim() != sentinel);
}

bool SwpDarkFiendTrigger::IsActive()
{
    // Dark Fiends die to a dispel or purge.
    if (!bot->IsClass(CLASS_PRIEST) && !bot->IsClass(CLASS_SHAMAN) && !bot->IsClass(CLASS_MAGE))
        return false;

    return GetNearestNpc(botAI, NPC_DARK_FIEND, botAI->GetRange("spell")) != nullptr;
}

// Kil'jaeden

bool SwpHandTankTrigger::IsActive()
{
    Unit* hand = GetAssignedHand(botAI);
    if (!hand)
        return false;

    if (hand->GetVictim() != bot || bot->GetVictim() != hand)
        return true;

    return IsTooCloseToOtherHand(botAI, hand);
}

bool SwpHandVolleyTrigger::IsActive()
{
    // Guides keep the raid outside the Hands' 30y Shadow Bolt Volley.
    if (!PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsTank(bot))
        return false;

    return GetNearestNpc(botAI, NPC_HAND_OF_THE_DECEIVER, HAND_VOLLEY_DISTANCE) != nullptr;
}

bool SwpRangedAddTrigger::IsActive()
{
    // Ranged DPS kill the imps before they reach the raid, and the Shield Orbs as they orbit.
    if (!PlayerbotAI::IsRangedDps(bot))
        return false;

    Unit* victim = bot->GetVictim();
    if (victim && victim->IsAlive() &&
        (victim->GetEntry() == NPC_VOLATILE_FELFIRE_FIEND || victim->GetEntry() == NPC_SHIELD_ORB))
        return false;

    return GetRangedAddTarget(botAI) != nullptr;
}

bool SwpFireBloomTrigger::IsActive()
{
    return bot->HasAura(SPELL_FIRE_BLOOM) && GetNearestPlayerInRadius(bot, FIRE_BLOOM_SPREAD_DISTANCE) != nullptr;
}

bool SwpTakeDragonOrbTrigger::IsActive()
{
    if (GetDragonRider(bot) != bot || GetPossessedDrake(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "kil'jaeden");
    return boss && boss->IsAlive() && GetUsableBlueFlightOrb(bot) != nullptr;
}

bool SwpRideDragonTrigger::IsActive()
{
    return GetPossessedDrake(bot) != nullptr;
}

bool SwpDarknessStackTrigger::IsActive()
{
    if (GetPossessedDrake(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "kil'jaeden");
    if (!IsUsingDarkness(boss))
        return false;

    Unit* drake = GetShieldDrake(botAI);
    return drake && bot->GetExactDist2d(drake) > DRAKE_SHIELD_STACK_DISTANCE;
}
