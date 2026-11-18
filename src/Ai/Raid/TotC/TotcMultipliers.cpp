/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TotcMultipliers.h"
#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "TotcHelpers.h"

using namespace EncounterHelpers;
using namespace TotcHelpers;

float TrialOfTheCrusaderMultiplier::GetValue(Action* action)
{
    if (bot->GetMapId() != TOTC_MAP_ID)
        return 1.0f;

    // Gormok: the tank with the Impale stacks lets the other tank keep him rather than taunting back.
    if (IsTauntAction(bot, action) && GetDifficultyAuraStacks(bot, SPELL_IMPALE) >= IMPALE_SWAP_STACKS &&
        AI_VALUE2(Unit*, "find target", "gormok the impaler"))
    {
        return 0.0f;
    }

    return 1.0f;
}
