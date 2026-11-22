/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <cmath>
#include "Map.h"
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

// The waypoints moved out to the wall: real raids kite him along the outer wall with the tank's back to it, so he faces
// the wall and Slime Spray (45 yd frontal cone) hits only the tank while the raid stands in the middle behind him
// (research 11-21, reports/Vanilla raid boss bot strategies.md). The inherited circle (r 35 round the room centre) left
// the raid in front of him. Measured once: along each of the 8 bearings the farthest floor point (z within 2 of
// 292.6) in sight of the centre, up to 60 yd, then WALL_INSET in from it; a bearing with nothing past 35 keeps 35.
static constexpr float GROBBULUS_CENTER_X = 3281.23f;
static constexpr float GROBBULUS_CENTER_Y = -3310.38f;
static constexpr float GROBBULUS_FLOOR_Z = 292.6f;

static std::vector<std::pair<float, float>> sGrobbulusWall;

static void PlaceGrobbulusWaypointsOnWall(Player* bot, std::vector<std::pair<float, float>>& waypoints)
{
    std::vector<std::pair<float, float>>& wall = sGrobbulusWall;
    if (wall.empty())
    {
        constexpr float WALL_INSET = 4.0f;
        Map* map = bot->GetMap();
        for (size_t i = 0; i < waypoints.size(); ++i)
        {
            float const a = std::atan2(waypoints[i].second - GROBBULUS_CENTER_Y, waypoints[i].first - GROBBULUS_CENTER_X);
            float best = 35.0f + WALL_INSET;
            for (float r = 60.0f; r > 35.0f + WALL_INSET; r -= 1.0f)
            {
                float const x = GROBBULUS_CENTER_X + std::cos(a) * r;
                float const y = GROBBULUS_CENTER_Y + std::sin(a) * r;
                float const z = map->GetHeight(bot->GetPhaseMask(), x, y, GROBBULUS_FLOOR_Z + 5.0f);
                if (z <= INVALID_HEIGHT || std::fabs(z - GROBBULUS_FLOOR_Z) > 2.0f)
                    continue;
                if (!map->isInLineOfSight(GROBBULUS_CENTER_X, GROBBULUS_CENTER_Y, GROBBULUS_FLOOR_Z + 2.0f, x, y, z + 2.0f,
                                          bot->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
                    continue;
                best = r;
                break;
            }
            float const r = best - WALL_INSET;
            wall.emplace_back(GROBBULUS_CENTER_X + std::cos(a) * r, GROBBULUS_CENTER_Y + std::sin(a) * r);
            LOG_INFO("module", "[Naxx40 Grobbulus] wall waypoint {} at {:.1f} {:.1f} (r {:.1f})", i, wall.back().first,
                     wall.back().second, r);
        }
    }
    if (wall.size() == waypoints.size())
        waypoints = wall;
}

bool GrobbulusRotateAction::Execute(Event /*event*/)
{
    PlaceGrobbulusWaypointsOnWall(bot, waypoints);
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

// The wall radius on a bearing from the room centre, from the nearest measured wall waypoint (+ the 4 yd inset).
static float GrobbulusWallRadiusAt(float angle)
{
    float best = 39.0f, bestDiff = 10.0f;
    for (auto const& w : sGrobbulusWall)
    {
        float const a = std::atan2(w.second - GROBBULUS_CENTER_Y, w.first - GROBBULUS_CENTER_X);
        float const d = std::fabs(Position::NormalizeOrientation(a - angle + static_cast<float>(M_PI)) - static_cast<float>(M_PI));
        if (d < bestDiff)
        {
            bestDiff = d;
            best = std::hypot(w.first - GROBBULUS_CENTER_X, w.second - GROBBULUS_CENTER_Y) + 4.0f;
        }
    }
    return best;
}

bool GrobbulusTankFaceWallAction::Execute(Event /*event*/)
{
    // Between clouds: stand between him and the wall, so he faces it and Slime Spray only reaches the tank. The rotation
    // left him on the wall side of the tank, facing the raid (11-21 diag: at waypoint 2 the boss stood 4.6 yd farther
    // out than the tank; Fallout Slime still killed 29).
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss || sGrobbulusWall.empty())
        return false;
    float const a = std::atan2(boss->GetPositionY() - GROBBULUS_CENTER_Y, boss->GetPositionX() - GROBBULUS_CENTER_X);
    float const wallR = GrobbulusWallRadiusAt(a);
    float const bossR = std::hypot(boss->GetPositionX() - GROBBULUS_CENTER_X, boss->GetPositionY() - GROBBULUS_CENTER_Y);
    float const r = std::min(wallR - 1.5f, bossR + 3.0f);
    float x = GROBBULUS_CENTER_X + std::cos(a) * r;
    float y = GROBBULUS_CENTER_Y + std::sin(a) * r;
    float z = GROBBULUS_FLOOR_Z;
    bot->UpdateAllowedPositionZ(x, y, z);
    if (bot->GetExactDist2d(x, y) < 1.5f)
        return false;
    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool GrobbulusTankFacingWrong(Player* bot, PlayerbotAI* botAI, Unit* boss)
{
    if (!boss || !boss->IsInCombat() || boss->GetVictim() != bot || !botAI->IsMainTank(bot) || sGrobbulusWall.empty())
        return false;
    if (std::fabs(boss->GetPositionZ() - GROBBULUS_FLOOR_Z) > 3.0f || bot->GetExactDist2d(boss) > 10.0f ||
        boss->isMoving() || bot->isMoving())
        return false;
    float const out = std::atan2(boss->GetPositionY() - GROBBULUS_CENTER_Y, boss->GetPositionX() - GROBBULUS_CENTER_X);
    float const diff =
        std::fabs(Position::NormalizeOrientation(boss->GetOrientation() - out + static_cast<float>(M_PI)) - static_cast<float>(M_PI));
    return diff > static_cast<float>(M_PI) / 3.0f;  // more than 60 degrees off "out at the wall"
}
