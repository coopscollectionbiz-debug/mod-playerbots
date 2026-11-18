/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TotcStrategy.h"
#include "TotcMultipliers.h"

void RaidTrialOfTheCrusaderStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Shared
    triggers.push_back(new TriggerNode("totc interrupt", { NextAction("totc interrupt", ACTION_EMERGENCY + 1) }));

    // The Beasts of Northrend
    triggers.push_back(new TriggerNode("totc gormok impale", { NextAction("totc taunt gormok", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("totc snobold vassal", { NextAction("totc kill snobold", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("totc burning bile",
        { NextAction("totc cure paralytic toxin", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("totc slime pool", { NextAction("totc avoid slime pool", ACTION_MOVE + 5) }));
    triggers.push_back(new TriggerNode("totc icehowl trample",
        { NextAction("totc dodge trample", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode("totc jormungar tank",
        { NextAction("totc tank jormungar", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("totc jormungar kill order",
        { NextAction("totc attack acidmaw", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("totc jormungar melee",
        { NextAction("totc jormungar melee position", ACTION_MOVE + 4) }));

    // Lord Jaraxxus
    triggers.push_back(new TriggerNode("totc legion flame", { NextAction("totc spread from raid", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("totc jaraxxus add", { NextAction("totc kill jaraxxus add", ACTION_RAID + 1) }));

    // The Faction Champions
    triggers.push_back(new TriggerNode("totc champion kill order", { NextAction("totc mark champion", ACTION_RAID) }));

    triggers.push_back(new TriggerNode("totc champion crowd control",
        { NextAction("totc crowd control champion", ACTION_RAID + 1) }));

    // The Twin Val'kyr
    triggers.push_back(new TriggerNode("totc twins wrong essence",
        { NextAction("totc take essence", ACTION_EMERGENCY + 2) }));
    triggers.push_back(new TriggerNode("totc twins target", { NextAction("totc attack twin", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("totc twins opposite orb", { NextAction("totc avoid orb", ACTION_MOVE + 5) }));

    // Anub'arak
    triggers.push_back(new TriggerNode("totc pursued by anub'arak",
        { NextAction("totc kite spike", ACTION_EMERGENCY + 5) }));
    triggers.push_back(new TriggerNode("totc spike nearby", { NextAction("totc avoid spike", ACTION_EMERGENCY + 4) }));
    triggers.push_back(new TriggerNode("totc burrower untanked",
        { NextAction("totc tank burrower", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("totc frost sphere", { NextAction("totc kill frost sphere", ACTION_RAID + 2) }));
}

void RaidTrialOfTheCrusaderStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new TrialOfTheCrusaderMultiplier(botAI));
}
