// Ai/Base/Value/BgKillTargetValue.h   (mod-playerbots, added by overlay patch 0019)
#ifndef _PLAYERBOT_BGKILLTARGETVALUE_H
#define _PLAYERBOT_BGKILLTARGETVALUE_H
#include "Battleground.h"
#include "Player.h"
#include "Value.h"
#include <vector>

class PlayerbotAI;

// The ONE live-BG gate shared by the 0019 value, actions and triggers: a real (non-arena)
// battleground that has actually started. Everything 0019 adds no-ops outside it.
inline bool InLiveOpenWorldBg(Player* bot)
{
    Battleground* bg = bot->GetBattleground();
    return bg && !bg->isArena() && bg->GetStatus() == STATUS_IN_PROGRESS;
}

// Deterministic BG kill target: enemy FC/capturer/siege vehicle > whoever is attacking ME > an
// enemy healer within my role's reach (unless I am under pressure) > <35% HP > attacker of our
// FC/healer — and a current victim already under 35% is finished before anything but tier 1.
// nullptr when nothing qualifies, so the stock "dps assist" target stands. Pure ranking lives in
// mod-battleground-bots/src/BgKillRank.h (unit-tested); this class only builds the candidates.
class BgKillTargetValue : public UnitCalculatedValue
{
public:
    BgKillTargetValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "bg kill target", 1000) {}
    Unit* Calculate() override;

private:
    bool IsHealerLike(Player* p) const;
    bool Unreachable(Player* target, std::vector<Player*> const& enemies) const;
};

// Enemy melee within 15y hitting our flag carrier or one of our healers (peel target).
class BgPeelTargetValue : public UnitCalculatedValue
{
public:
    BgPeelTargetValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "bg peel target", 1000) {}
    Unit* Calculate() override;
};
#endif
