// Ai/Base/Trigger/BgDirectorTriggers.h   (overlay patch 0019)
#ifndef _PLAYERBOT_BGDIRECTORTRIGGERS_H
#define _PLAYERBOT_BGDIRECTORTRIGGERS_H
#include "Trigger.h"

class PlayerbotAI;

class BgPriorityTargetNearTrigger : public Trigger
{
public:
    BgPriorityTargetNearTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bg priority target near", 1) {}
    bool IsActive() override;
};

class BgAllyNeedsPeelTrigger : public Trigger
{
public:
    BgAllyNeedsPeelTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bg ally needs peel", 1) {}
    bool IsActive() override;
};

class BgHealerSquadFarTrigger : public Trigger
{
public:
    BgHealerSquadFarTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bg healer squad far", 2) {}
    bool IsActive() override;
};
#endif
