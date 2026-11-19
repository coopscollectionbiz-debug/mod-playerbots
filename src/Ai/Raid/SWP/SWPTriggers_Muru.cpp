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

// Every one of these delegates to the same predicate the movement multiplier reads. That is deliberate
// and it is the Twins lesson: if a trigger and the veto could disagree about whether a move is owed,
// generic movement gets handed back mid-run and the bot shuttles in and out of the hazard.

bool MuruInDarknessTrigger::IsActive()
{
    return MuruShouldClearDarkness(bot, botAI);
}

bool MuruDarkFiendUpTrigger::IsActive()
{
    return MuruShouldDispelFiend(bot, botAI);
}

bool MuruInVoidZoneTrigger::IsActive()
{
    return MuruShouldClearVoidZone(bot, botAI);
}

bool MuruNearSingularityTrigger::IsActive()
{
    return MuruShouldFleeSingularity(bot, botAI);
}

bool MuruInShadowPulseTrigger::IsActive()
{
    return MuruShouldAvoidShadowPulse(bot, botAI);
}

bool MuruAddUntankedTrigger::IsActive()
{
    return MuruShouldTankAdds(bot, botAI);
}

bool MuruWrongTargetTrigger::IsActive()
{
    return MuruShouldRetarget(bot, botAI);
}
