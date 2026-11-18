/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "NaxxHelpers.h"
#include "Playerbots.h"

bool NaxxAttackWebWrapAction::Execute(Event /*event*/)
{
    Unit* cocoon = NaxxHelpers::GetNearestNpc(botAI, NaxxHelpers::NPC_WEB_WRAP, NaxxHelpers::NAXX_ADD_SEARCH_DISTANCE);
    return cocoon && Attack(cocoon);
}
