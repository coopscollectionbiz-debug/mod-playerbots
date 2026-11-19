// Ai/Base/Trigger/BgDirectorTriggers.cpp
#include "BgDirectorTriggers.h"
#include "BattlegroundBotsApi.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool BgPriorityTargetNearTrigger::IsActive()
{
    Unit* t = AI_VALUE(Unit*, "bg kill target");
    if (!t || bot->GetVictim() == t) return false;
    // Defer to the peel job: while the bot is already on its peel target, the two nodes (51 and
    // 52) would otherwise take turns pulling the victim back and forth every tick.
    if (Unit* p = AI_VALUE(Unit*, "bg peel target"))
        if (bot->GetVictim() == p) return false;
    return true;
}

bool BgAllyNeedsPeelTrigger::IsActive() { return AI_VALUE(Unit*, "bg peel target") != nullptr; }

bool BgHealerSquadFarTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot, true)) return false;
    ObjectGuid guid;
    if (!BattlegroundBotsApi::GetSquadAnchor(bot, guid)) return false;
    Unit* anchor = botAI->GetUnit(guid);
    return anchor && anchor->IsAlive() && bot->GetDistance(anchor) > 20.0f;
}
