/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "NaxxHelpers.h"
#include "Playerbots.h"

bool NaxxMoveFromPlaguedChampionAction::Execute(Event /*event*/)
{
    using namespace NaxxHelpers;
    Unit* champion = GetNearestNpc(botAI, NPC_PLAGUED_CHAMPION, PLAGUED_CHAMPION_SHOCK_DISTANCE);
    if (!champion)
        return false;

    return MoveAway(champion, PLAGUED_CHAMPION_SHOCK_DISTANCE - bot->GetExactDist2d(champion));
}
