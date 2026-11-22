/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <cmath>
#include "NaxxActions.h"
#include "NaxxHelpers.h"
#include "NaxxTriggers.h"
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
    // Diagnostic (2026-11-21, temporary): the rotation circle (3281,-3310 r35) is the WotLK room centre; IP's Naxx40
    // Grobbulus spawns at 3205,-3341 z320. Log where he and his tank are, every 10 s.
    {
        static uint32 lastMs = 0;
        uint32 const nowMs = getMSTime();
        if (!lastMs || getMSTimeDiff(lastMs, nowMs) > 10000)
        {
            lastMs = nowMs ? nowMs : 1;
            Unit* boss = AI_VALUE(Unit*, "boss target");
            uint32 const wp = FindNearestWaypoint();
            LOG_INFO("module", "[Naxx40 Grobbulus] tank {} at {:.1f} {:.1f} {:.1f}; boss {} at {:.1f} {:.1f} {:.1f}; nearest waypoint {} "
                     "({:.1f} {:.1f}), {:.1f} yd from the circle centre",
                     bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                     boss ? boss->GetEntry() : 0u, boss ? boss->GetPositionX() : 0.0f, boss ? boss->GetPositionY() : 0.0f,
                     boss ? boss->GetPositionZ() : 0.0f, wp, waypoints[wp].first, waypoints[wp].second,
                     bot->GetExactDist2d(3281.23f, -3310.38f));
        }
    }
    uint32 current = FindNearestWaypoint();
    if (clockwise)
        return (current + 1) % intervals;

    return (current + intervals - 1) % intervals;
}

bool GrobbulusRotateAction::Execute(Event /*event*/)
{
    // The circle's waypoints are on the room floor (z 292.6). The base action moves at the bot's own Z, so a tank who
    // picked Grobbulus up on the entrance ledge (z 320, IP Naxx40) asked for a point 28 yd up in the air, the move
    // failed and he stayed on the ledge while the clouds stacked (11-21 diag: wipes held at 3211,-3320 z320; the kill
    // reached the floor and rotated waypoints 2-7). Ask for the floor so the path goes down the ramp.
    constexpr float kFloorZ = 292.6f;
    uint32 const next = GetCurrWaypoint();
    return MoveTo(bot->GetMapId(), waypoints[next].first, waypoints[next].second, kFloorZ, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool GrobbulusAvoidSprayAction::Execute(Event /*event*/)
{
    // "find target", as the trigger uses: "boss target" is empty for most of the raid and the action FAILED every
    // time it was pushed (Playerbots.log 11-21 17:4x).
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss || boss->GetVictim() == bot)
        return false;
    if (bot->GetExactDist2d(boss) > NaxxGrobbulusSpray::Radius || !boss->HasInArc(NaxxGrobbulusSpray::Arc, bot))
        return false;

    // Behind him on the side the bot is already on, at its own role distance; same height and in sight of him.
    float const range = botAI->IsRanged(bot) ? 22.0f : 4.0f;
    float const back = boss->GetOrientation() + M_PI;
    float const side = Position::NormalizeOrientation(boss->GetAngle(bot) - boss->GetOrientation()) < M_PI ? 1.0f : -1.0f;
    static float const offsets[] = {0.6f, 0.3f, 0.9f, 0.0f};
    for (float off : offsets)
    {
        float const a = back - side * off;
        float x = boss->GetPositionX() + cos(a) * range;
        float y = boss->GetPositionY() + sin(a) * range;
        float z = boss->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);
        if (std::fabs(z - boss->GetPositionZ()) > 4.0f || !boss->IsWithinLOS(x, y, z + 2.0f))
            continue;
        return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    }
    // No clean spot behind him: step sideways out of the cone on the bot's own side.
    float const a = boss->GetOrientation() + side * (NaxxGrobbulusSpray::Arc / 2.0f + 0.5f);
    float const d = std::max(bot->GetExactDist2d(boss), 6.0f);
    float x = boss->GetPositionX() + cos(a) * d;
    float y = boss->GetPositionY() + sin(a) * d;
    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);
    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool GrobbulusAvoidCloudAction::Execute(Event /*event*/)
{
    // The nearest spot round the bot that is 4 yd clear of every cloud and, when possible, out of the spray cone, at
    // its own height. Rings of 8, 14 and 20 yd, 16 bearings each.
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    float bestX = 0.0f, bestY = 0.0f, bestZ = 0.0f, bestScore = -1000.0f;
    bool found = false;
    for (float ring : {8.0f, 14.0f, 20.0f})
    {
        for (int i = 0; i < 16; ++i)
        {
            float const a = i * static_cast<float>(M_PI) / 8.0f;
            float x = bot->GetPositionX() + cos(a) * ring;
            float y = bot->GetPositionY() + sin(a) * ring;
            float z = bot->GetPositionZ();
            bot->UpdateAllowedPositionZ(x, y, z);
            if (std::fabs(z - bot->GetPositionZ()) > 4.0f || !bot->IsWithinLOS(x, y, z + 2.0f))
                continue;
            float const gap = NaxxHelpers::GrobbulusCloudGap(bot, x, y);
            if (gap < 4.0f)
                continue;
            float score = std::min(gap, 15.0f) - ring * 0.5f;
            if (boss)
            {
                Position const spot(x, y, z);
                if (boss->GetExactDist2d(x, y) <= NaxxGrobbulusSpray::Radius && boss->HasInArc(NaxxGrobbulusSpray::Arc, &spot))
                    score -= 20.0f;
                // Healers and ranged stay within 30 yd of him; melee within 10.
                float const keep = botAI->IsRanged(bot) ? 30.0f : 10.0f;
                score -= std::max(0.0f, boss->GetExactDist2d(x, y) - keep);
            }
            if (!found || score > bestScore)
            {
                found = true;
                bestScore = score;
                bestX = x;
                bestY = y;
                bestZ = z;
            }
        }
        if (found)
            break;
    }
    if (!found)
        return false;
    return MoveTo(bot->GetMapId(), bestX, bestY, bestZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
}
