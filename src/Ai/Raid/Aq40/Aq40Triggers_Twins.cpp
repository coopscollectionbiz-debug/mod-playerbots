/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Twin Emperors v2 triggers (overlay patch 0016). Spec (overlay repo):
// docs/superpowers/specs/2026-10-22-playerbot-aq40-twin-emperors-v2-design.md, §5.3-§5.8. Every predicate is
// in Aq40Helpers.cpp; these only adapt them. All are false while there is no Twins view (inert while the stations
// are provisional), except the kept hazard / housekeeping / pet triggers, which never needed it.

#include "Aq40Triggers.h"

#include "Aq40Helpers.h"
#include "Playerbots.h"

using namespace TempleOfAhnQirajHelpers;

bool Aq40TwinsNotInCombatTrigger::IsActive()
{
    return IsInAq40(bot) && !bot->IsInCombat() && !IsTwinsEncounterActive(bot);
}

bool Aq40TwinsInExplodeRadiusTrigger::IsActive() { return TwinsShouldClearExplode(bot, botAI); }
bool Aq40TwinsInBlizzardTrigger::IsActive() { return TwinsShouldClearBlizzard(bot, botAI); }
bool Aq40TwinsInBurstRangeTrigger::IsActive() { return TwinsInBurstRange(bot); }
bool Aq40TwinsWrongTargetTrigger::IsActive() { return TwinsShouldRetarget(bot, botAI); }
bool Aq40TwinsPetOnWrongTwinTrigger::IsActive() { return TwinsShouldDirectPet(bot, botAI); }
bool Aq40TwinsTankHealTrigger::IsActive() { return TwinsTankHealTarget(bot, botAI) != nullptr; }

bool Aq40TwinsInLandingZoneTrigger::IsActive()
{
    Position escape;
    return TwinsInLandingZone(bot, botAI, escape);
}

bool Aq40TwinsWrongVictimTrigger::IsActive()
{
    Position spot;
    return TwinsWrongVictim(bot, botAI, spot);
}

bool Aq40TwinsBossOffStationTrigger::IsActive()
{
    // One bot per side at most (the active melee tank holding Vek'nilash); the path step is computed here and again
    // in the action, never in the movement veto (TwinsShouldSuppressGenericMovement uses the cheap test).
    Position step;
    return TwinsShouldHoldStation(bot, botAI, step);
}

bool Aq40TwinsOffSpotTrigger::IsActive()
{
    Position spot;
    return GetTwinsSpot(bot, botAI, spot) && bot->GetExactDist2d(spot) > TWINS_SPOT_ARRIVE;
}

bool Aq40TwinsWarlockTankDueTrigger::IsActive() { return TwinsShouldWarlockTank(bot, botAI); }

// BWL's "bwl threat hold" / Huhuran's, mirrored: active only while the bot is attacking (a swing or an auto-repeat
// spell), so it fires once per breach and the threat multiplier then keeps the attack from restarting. Two
// reasons: holding fire on a twin (ruling R8, spec §5.4 - waiting tanks always, DPS on Vek'nilash until he is on
// his designated tank or 8 s, Revision 5), or the 0.90 hard stop against that twin's threat denominator (spec §5.6,
// TwinsThreatPair). Cheap
// map / role / attacking / entry tests before any view lookup. Healers are never active.
bool Aq40TwinsThreatHoldTrigger::IsActive()
{
    if (bot->GetMapId() != AQ40_MAP_ID || PlayerbotAI::IsHeal(bot))
        return false;
    if (!bot->GetVictim() && !bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
        return false;

    Unit* const current = AI_VALUE(Unit*, "current target");
    if (!current || (current->GetEntry() != NPC_VEKLOR && current->GetEntry() != NPC_VEKNILASH))
        return false;

    if (IsTwinsHoldingFire(bot, botAI))
        return true;
    if (AI_VALUE(bool, "neglect threat"))
        return false;
    return TwinsThreatHardStop(bot, current);
}

bool Aq40TwinsFormUpTrigger::IsActive()
{
    Position spot;
    return GetTwinsStagingSpot(bot, spot);
}
