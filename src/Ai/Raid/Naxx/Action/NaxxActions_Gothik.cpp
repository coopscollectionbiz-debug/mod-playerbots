/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "NaxxHelpers.h"
#include "Playerbots.h"

bool NaxxGothikGoToSideAction::Execute(Event /*event*/)
{
    using namespace NaxxHelpers;
    Position const spot = GetGothikSideSpot(IsGothikDeadSideMember(bot));
    float const z = bot->GetMapWaterOrGroundLevel(spot.GetPositionX(), spot.GetPositionY(), bot->GetPositionZ());
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), z, false, false, false, false,
                  MovementPriority::MOVEMENT_NORMAL, true, false);
}
