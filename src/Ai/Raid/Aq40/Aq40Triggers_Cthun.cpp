/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Triggers.h"

#include "Aq40CthunHelpers.h"
#include "Aq40Helpers.h"
#include "Playerbots.h"

using namespace TempleOfAhnQirajHelpers;

bool Aq40CthunNotInCombatTrigger::IsActive()
{
    return IsInAq40(bot) && !bot->IsInCombat() && !IsCthunEncounterActive(bot);
}

bool Aq40CthunInStomachTrigger::IsActive() { return CthunShouldExitStomach(bot); }
bool Aq40CthunMountedInPlaceTrigger::IsActive() { return CthunShouldDismount(bot, botAI); }

bool Aq40CthunStomachAirborneTrigger::IsActive()
{
    float groundZ = 0.0f;
    return GetCthunAirborneState(bot, botAI, groundZ) != CthunAirborne::None;
}
bool Aq40CthunStomachFarFromTentacleTrigger::IsActive()
{
    Position s;
    return GetCthunStomachApproachSpot(bot, botAI, s);
}

bool Aq40CthunInGlarePathTrigger::IsActive() { return CthunShouldDodgeGlare(bot); }
bool Aq40CthunNearBeamTargetTrigger::IsActive() { return CthunShouldClearBeam(bot); }
bool Aq40CthunOpeningPullerTrigger::IsActive()
{
    Position s;
    return GetCthunPullerSpot(bot, s);
}

bool Aq40CthunWrongTargetTrigger::IsActive() { return CthunShouldRetarget(bot, botAI); }

bool Aq40CthunNotFormedTrigger::IsActive() { return CthunShouldFormUp(bot, botAI); }
