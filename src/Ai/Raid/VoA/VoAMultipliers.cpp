/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "VoAMultipliers.h"
#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "VoATriggers.h"

float ToravonFrostbiteMultiplier::GetValue(Action* action)
{
    // The stacked tank lets Frostbite fall off instead of taunting him straight back.
    if (!EncounterHelpers::IsTauntAction(bot, action) ||
        EncounterHelpers::GetAuraStackAmount(bot, SPELL_TORAVON_FROSTBITE) < TORAVON_FROSTBITE_SWAP_STACKS)
        return 1.0f;

    return AI_VALUE2(Unit*, "find target", "toravon the ice watcher") ? 0.0f : 1.0f;
}
