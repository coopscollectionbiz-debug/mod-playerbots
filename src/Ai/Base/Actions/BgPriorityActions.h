// Ai/Base/Actions/BgPriorityActions.h   (overlay patch 0019)
#ifndef _PLAYERBOT_BGPRIORITYACTIONS_H
#define _PLAYERBOT_BGPRIORITYACTIONS_H
#include "AiObjectContext.h"
#include "AttackAction.h"
#include "BgKillTargetValue.h"
#include "MovementActions.h"
#include "Player.h"

class PlayerbotAI;

// overlay patch 0019: true while the bot's current victim IS a standing director pick, so stock
// target choosers must leave the target slot alone. Both values are nullptr when the module is
// off / not in a live BG, so stock behaviour is byte-identical when disabled.
// Spelled out rather than via AI_VALUE: that macro lives in Playerbots.h, which this header does
// not (and should not) pull in — the expansion is exactly this.
inline bool BgDirectorOwnsVictim(Player* bot, AiObjectContext* context)
{
    Unit* v = bot->GetVictim();
    if (!v) return false;
    return v == context->GetValue<Unit*>("bg kill target")->Get()
        || v == context->GetValue<Unit*>("bg peel target")->Get();
}

// Switch to the director-ranked kill target, with a per-bot retarget cooldown so the bot commits.
class AttackBgPriorityTargetAction : public AttackAction
{
public:
    AttackBgPriorityTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, "attack bg priority target") {}
    std::string const GetTargetName() override { return "bg kill target"; }
    bool isUseful() override;
    bool Execute(Event event) override;
private:
    uint32 _lastSwitchMs = 0;
};

// Melee only: snare/stun the enemy melee on our FC/healer, then turn on it.
class BgPeelForAllyAction : public AttackAction
{
public:
    BgPeelForAllyAction(PlayerbotAI* botAI) : AttackAction(botAI, "bg peel for ally") {}
    std::string const GetTargetName() override { return "bg peel target"; }
    bool isUseful() override;
    bool Execute(Event event) override;
private:
    uint32 _lastPeelMs = 0;
};

// Healers with a squad stay within 20y of the squad's living melee anchor instead of the flag.
class BgHealerFollowSquadAction : public MovementAction
{
public:
    BgHealerFollowSquadAction(PlayerbotAI* botAI) : MovementAction(botAI, "bg healer follow squad") {}
    bool isUseful() override;
    bool Execute(Event event) override;
};
#endif
