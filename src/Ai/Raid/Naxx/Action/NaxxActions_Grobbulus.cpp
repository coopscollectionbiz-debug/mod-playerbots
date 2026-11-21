/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <cmath>
#include "NaxxActions.h"
#include "Playerbots.h"

bool GrobbulusGoBehindAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE(Unit*, "boss target");
    if (!boss)
        return false;

    // Position* pos = boss->GetPosition();
    float orientation = boss->GetOrientation() + M_PI + delta_angle;
    float x = boss->GetPositionX();
    float y = boss->GetPositionY();
    float z = boss->GetPositionZ();
    float rx = x + cos(orientation) * distance;
    float ry = y + sin(orientation) * distance;
    return MoveTo(bot->GetMapId(), rx, ry, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

bool GrobbulusMoveAwayAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE(Unit*, "boss target");
    if (!boss)
        return false;

    const float currentDistance = bot->GetExactDist2d(boss);
    if (currentDistance >= distance)
        return false;

    // A spot the boss can see, at the same height: a point behind a wall sent the injected bot round the corridors
    // (Naxx40 runs ended 180-205 yd away in the Living Poison hall, tr-20261121-085338-1). Try angles either side.
    const float base = boss->GetAngle(bot);
    static float const offsets[] = {0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f, 2.0f, -2.0f};
    for (float off : offsets)
    {
        float x = boss->GetPositionX() + cos(base + off) * distance;
        float y = boss->GetPositionY() + sin(base + off) * distance;
        float z = boss->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);
        if (std::fabs(z - boss->GetPositionZ()) > 4.0f || !boss->IsWithinLOS(x, y, z + 2.0f) ||
            !bot->IsWithinLOS(x, y, z + 2.0f))
            continue;
        return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

uint32 GrobbulusRotateAction::GetCurrWaypoint()
{
    uint32 current = FindNearestWaypoint();
    if (clockwise)
        return (current + 1) % intervals;

    return (current + intervals - 1) % intervals;
}
