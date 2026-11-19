#ifndef _PLAYERBOT_WINTERGRASPSIEGESTRATEGY_H
#define _PLAYERBOT_WINTERGRASPSIEGESTRATEGY_H

#include "Action.h"
#include "Strategy.h"
#include "Trigger.h"

// Phase 3: attacker bots build + pilot demolishers to break the Wintergrasp keep and (once the last
// door is down) click the Titan relic to win. Self-governed by the WG vehicle cap minus a human
// reserve. Firing uses the IOC-proven path: set the "bg siege" position to the wall, then
// PlayerbotAI::CastVehicleSpell(HurlBoulder, vehicleBase) so the projectile arcs to the wall.

// Fires while the bot is an attacker in a live WG battle (or already crewing a siege vehicle).
class WgSiegeActiveTrigger : public Trigger
{
public:
    WgSiegeActiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wg siege active") {}
    bool IsActive() override;
};

// One consolidated action driving the whole loop: build -> drive to wall -> fire -> (relic -> win).
class WintergraspSiegeAction : public Action
{
public:
    WintergraspSiegeAction(PlayerbotAI* botAI) : Action(botAI, "wg siege") {}
    bool Execute(Event event) override;

private:
    bool TryBuild();          // not in a vehicle: build a demolisher if rank + cap allow
    bool DoSiegeCombat();     // in a demolisher: drive to the wall and fire, or handle the relic
    bool DoRelic();           // last door down: reach + use the Titan relic (attacker win)
};

class WintergraspSiegeStrategy : public Strategy
{
public:
    WintergraspSiegeStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    uint32 GetType() const override { return STRATEGY_TYPE_GENERIC; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "wg siege"; }
};

#endif
