/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "EncounterHelpers.h"
#include "NaxxHelpers.h"
#include "Playerbots.h"

uint32 RotateAroundTheCenterPointAction::FindNearestWaypoint()
{
    float minDistance = 0;
    int ret = -1;
    for (uint32 i = 0; i < intervals; i++)
    {
        float w_x = waypoints[i].first, w_y = waypoints[i].second;
        float dis = bot->GetDistance2d(w_x, w_y);
        if (ret == -1 || dis < minDistance)
        {
            ret = i;
            minDistance = dis;
        }
    }
    return ret;
}

bool NaxxKillOrderMarkAction::Execute(Event /*event*/)
{
    return EncounterHelpers::MarkTargetWithSkull(bot, NaxxHelpers::GetKillOrderMarkTarget(botAI));
}

bool NaxxTauntBossAction::Execute(Event event)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", bossName);
    if (!boss)
        return false;

    if (bot->GetVictim() != boss)
        return Attack(boss);

    return boss->GetVictim() != bot && botAI->DoSpecificAction("taunt spell", event, true);
}
