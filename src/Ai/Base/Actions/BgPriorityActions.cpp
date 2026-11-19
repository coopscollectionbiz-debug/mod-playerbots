// Ai/Base/Actions/BgPriorityActions.cpp
#include "BgPriorityActions.h"
#include "BattlegroundBotsApi.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Timer.h"

// The live-BG gate is the single InLiveOpenWorldBg() in BgKillTargetValue.h (pulled in by
// BgPriorityActions.h) — it is stricter than InBattleground() && !InArena() because it also
// requires STATUS_IN_PROGRESS, matching the values these actions read.

bool AttackBgPriorityTargetAction::isUseful() { return InLiveOpenWorldBg(bot) && BattlegroundBotsApi::Enabled(); }

bool AttackBgPriorityTargetAction::Execute(Event /*event*/)
{
    uint32 now = getMSTime();
    if (now - _lastSwitchMs < BattlegroundBotsApi::Knobs().retargetCooldownMs) return false;
    Unit* target = AI_VALUE(Unit*, "bg kill target");
    if (!target || bot->GetVictim() == target) return false;
    if (!Attack(target)) return false;
    _lastSwitchMs = now;
    return true;
}

bool BgPeelForAllyAction::isUseful()
{
    return InLiveOpenWorldBg(bot) && BattlegroundBotsApi::Enabled()
        && (PlayerbotAI::IsTank(bot, true) || PlayerbotAI::IsMelee(bot, true));
}

bool BgPeelForAllyAction::Execute(Event /*event*/)
{
    uint32 now = getMSTime();
    if (now - _lastPeelMs < BattlegroundBotsApi::Knobs().retargetCooldownMs) return false;
    Unit* target = AI_VALUE(Unit*, "bg peel target");
    if (!target) return false;
    static char const* const kPeels[] = { "hamstring", "frost shock", "concussive shot", "earthbind totem",
                                          "hammer of justice", "intimidating shout", "death grip" };
    bool cast = false;
    for (char const* name : kPeels)
        if (botAI->CanCastSpell(name, target)) { cast = botAI->CastSpell(name, target); if (cast) break; }
    bool switched = bot->GetVictim() != target && Attack(target);
    if (!cast && !switched) return false;
    _lastPeelMs = now;
    return true;
}

bool BgHealerFollowSquadAction::isUseful()
{
    if (!InLiveOpenWorldBg(bot) || !BattlegroundBotsApi::Enabled() || !PlayerbotAI::IsHeal(bot, true)) return false;
    ObjectGuid anchor;
    return BattlegroundBotsApi::GetSquadAnchor(bot, anchor);
}

bool BgHealerFollowSquadAction::Execute(Event /*event*/)
{
    ObjectGuid guid;
    if (!BattlegroundBotsApi::GetSquadAnchor(bot, guid)) return false;
    Unit* anchor = botAI->GetUnit(guid);
    if (!anchor || !anchor->IsAlive() || bot->GetDistance(anchor) <= 20.0f) return false;
    return MoveNear(anchor, 8.0f);
}
