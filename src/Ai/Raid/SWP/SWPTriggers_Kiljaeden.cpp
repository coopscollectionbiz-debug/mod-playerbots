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

// Every one of these delegates to the same predicate the movement multiplier reads - the Twins lesson:
// if a trigger and the veto could disagree about whether a move is owed, generic movement gets handed
// back mid-run and the bot shuttles in and out of the hazard.

bool KjInArmageddonTrigger::IsActive()
{
    return KjShouldClearArmageddon(bot);
}

bool KjPilotingDrakeTrigger::IsActive()
{
    return GetKjDrake(bot) != nullptr;
}

bool KjDarknessIncomingTrigger::IsActive()
{
    return KjShouldStackForDarkness(bot, botAI);
}

bool KjOrbAvailableTrigger::IsActive()
{
    return KjShouldClaimOrb(bot, botAI);
}

bool KjFireBloomOnMeTrigger::IsActive()
{
    return KjShouldQuarantineFireBloom(bot, botAI);
}

bool KjBossOffAnchorTrigger::IsActive()
{
    return KjShouldHoldAnchor(bot, botAI);
}

bool KjOutOfStationTrigger::IsActive()
{
    return KjShouldHoldStation(bot, botAI);
}

bool KjWrongTargetTrigger::IsActive()
{
    return KjShouldRetarget(bot, botAI);
}
