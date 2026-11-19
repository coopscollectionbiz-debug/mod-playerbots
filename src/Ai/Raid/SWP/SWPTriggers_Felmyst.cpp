/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPTriggers.h"

#include "Playerbots.h"
#include "EncounterHelpers.h"

using namespace EncounterHelpers;

#include "SWPHelpers.h"

using namespace SunwellPlateauHelpers;

// Shared gate: the FLIGHT phase of a live Felmyst encounter, with this bot a participant.
//
// Both halves of the boss test are required. `IsInCombat` alone would miss the phase entirely, and
// the airborne flag alone fires long BEFORE the pull - she flies an idle waypoint loop with
// MOVEMENTFLAG_DISABLE_GRAVITY set from the moment the instance loads, so keying off the flag by
// itself would send the raid to the far end of the room while players were still walking in.
static bool FelmystAirPhaseActive(Player* bot, Creature** bossOut = nullptr)
{
    if (!IsInSunwell(bot))
        return false;

    Creature* boss = FindFelmyst(bot);
    if (!boss || !boss->IsAlive() || !boss->IsInCombat())
        return false;

    if (!IsFelmystAirborne(boss))
        return false;

    // Deliberately generous (see FELMYST_PARTICIPANT_RANGE): she is ~28y up and can be at the far end
    // of a ~230y room while the raid is at the other, so a tight range would switch the fog hop off
    // for exactly the bots that still need it.
    if (bot->GetExactDist2d(boss->GetPositionX(), boss->GetPositionY()) > FELMYST_PARTICIPANT_RANGE)
        return false;

    if (bossOut)
        *bossOut = boss;
    return true;
}

bool FelmystInFogLaneTrigger::IsActive()
{
    Creature* boss = nullptr;
    if (!FelmystAirPhaseActive(bot, &boss))
        return false;

    // Only fires when a lane is actually live AND this bot is standing in it - so the raid holds
    // position for the whole vapour half of the flight phase and hops only when it must.
    //
    // NO PROXIMITY TOLERANCE HERE, deliberately. This used to also require the bot to be more than
    // FELMYST_STATION_TOLERANCE (6y) from its refuge, and since the refuge sits only FELMYST_FOG_MARGIN
    // (6y) beyond the kill radius, "arrived" could mean 6y short of the refuge = exactly 20.4y from the
    // line = inside the fog. A bot was charmed while logging dist 6.6. GetFelmystFogStation already
    // returns false once the bot is clear of the band, and that test carries the margin, so it is the
    // only arrival condition that cannot round the wrong way.
    Position station;
    return GetFelmystFogStation(bot, botAI, boss, station);
}

bool FelmystNearEncapsulateTrigger::IsActive()
{
    // Deliberately NOT gated on FelmystAirPhaseActive: this one fires during the GROUND phase, and the
    // shared predicate is what keeps the trigger and the movement multiplier in agreement.
    if (!IsInSunwell(bot))
        return false;

    return FelmystShouldFleeEncapsulate(bot, FindFelmyst(bot));
}

bool FelmystCharmedRaiderLooseTrigger::IsActive()
{
    if (!IsInSunwell(bot))
        return false;

    Creature* boss = FindFelmyst(bot);
    if (!boss || !boss->IsAlive())
        return false;

    // Healers are left out: the raid still needs healing while it burns the charmed bot down, and a
    // healer contributes almost nothing to the kill.
    if (botAI->IsHeal(bot))
        return false;

    Player* charmed = FindFelmystCharmedRaider(bot);
    if (!charmed)
        return false;

    return bot->GetVictim() != charmed;  // already on it - let the normal rotation do the damage
}

bool FelmystGasNovaDispellableTrigger::IsActive()
{
    if (!IsInSunwell(bot))
        return false;

    // Not gated on the flight phase: Gas Nova's DoT is 30s and outlives the takeoff that cancels the
    // scheduler, so it is still worth stripping in the air.
    return FelmystShouldMassDispel(bot, botAI, FindFelmyst(bot));
}

bool FelmystStrayFromRaidTrigger::IsActive()
{
    if (!IsInSunwell(bot))
        return false;

    return FelmystShouldRegroup(bot, botAI, FindFelmyst(bot));
}

bool FelmystChasedByVaporTrigger::IsActive()
{
    Creature* boss = nullptr;
    if (!FelmystAirPhaseActive(bot, &boss))
        return false;

    if (!FindFelmystVaporChasingMe(bot))
        return false;

    // Clearing the fog outranks kiting: both vapours are summoned before the first strafe, so these
    // should never overlap, but if they do, the charm is terminal and the vapour is only damage.
    Position station;
    return !GetFelmystFogStation(bot, botAI, boss, station);
}
