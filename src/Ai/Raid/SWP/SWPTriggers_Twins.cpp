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

bool TwinsConflagrationOnMeTrigger::IsActive()
{
    // Shared predicate with the movement multiplier, so the two can never disagree about whether a flee
    // is in progress and hand generic movement back mid-run.
    return TwinsShouldFleeConflagration(bot);
}

bool TwinsNearConflagrationTrigger::IsActive()
{
    return TwinsShouldClearConflagration(bot);
}

bool TwinsWrongDpsTargetTrigger::IsActive()
{
    if (!IsTwinsEncounterActive(bot))
        return false;

    Creature* sacrolash = FindSacrolash(bot);
    if (!sacrolash || !sacrolash->IsAlive())
        return false;  // she is down: Alythess is now the only correct target

    // Her assigned tank is the ONE bot supposed to be on Alythess - somebody has to hold her, because
    // Blaze lands on whoever does. Both Sacrolash tanks are expected on Sacrolash, so they fall through
    // and get corrected like any other bot.
    if (GetTwinsTankRole(bot, botAI) == TwinsTankRole::Alythess)
        return false;

    Creature* alythess = FindAlythess(bot);
    if (!alythess)
        return false;

    return bot->GetVictim() == alythess;
}

bool TwinsAlythessUntankedTrigger::IsActive()
{
    if (!IsTwinsEncounterActive(bot))
        return false;

    if (GetTwinsTankRole(bot, botAI) != TwinsTankRole::Alythess)
        return false;

    Creature* alythess = FindAlythess(bot);
    if (!alythess || !alythess->IsAlive())
        return false;

    return bot->GetVictim() != alythess;  // already on her - let the normal rotation build the threat
}

bool TwinsCotankConfoundedTrigger::IsActive()
{
    return TwinsShouldReliefTaunt(bot, botAI);
}

bool TwinsSacrolashOverextendedTrigger::IsActive()
{
    return TwinsShouldFixLeash(bot, botAI);
}

bool TwinsOutOfAlythessStackTrigger::IsActive()
{
    return TwinsShouldStackOnAlythess(bot);
}

bool TwinsBlazeFootworkTrigger::IsActive()
{
    return TwinsShouldWorkBlaze(bot);
}

bool TwinsNeedsShadowCleanseTrigger::IsActive()
{
    return TwinsShouldSeekShadow(bot);
}

bool TwinsPyrogenicsUpTrigger::IsActive()
{
    return TwinsShouldDispelPyrogenics(bot, botAI);
}
