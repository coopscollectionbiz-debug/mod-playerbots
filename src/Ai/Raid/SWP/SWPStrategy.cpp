/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPStrategy.h"
#include "SWPMultipliers.h"

static void Add(std::vector<TriggerNode*>& triggers, char const* trigger, char const* action, float relevance)
{
    triggers.push_back(new TriggerNode(trigger, { NextAction(action, relevance) }));
}

void RaidSunwellPlateauStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Every boss: kill order, crowd control, and void zones wider than the generic avoidance covers
    Add(triggers, "swp kill order mark", "swp kill order mark", ACTION_RAID);
    Add(triggers, "swp cc mark", "swp cc mark", ACTION_RAID);
    Add(triggers, "swp avoid hazard", "swp avoid hazard", ACTION_RAID + 2);

    // Kalecgos
    Add(triggers, "swp kalecgos take portal", "swp kalecgos take portal", ACTION_RAID + 1);
    Add(triggers, "swp kalecgos heal kalec", "swp kalecgos heal kalec", ACTION_RAID);
    Add(triggers, "swp kalecgos not victim", "swp kalecgos flank", ACTION_MOVE + 4);

    // Brutallus
    Add(triggers, "swp brutallus tank swap", "swp brutallus taunt", ACTION_RAID);
    Add(triggers, "swp brutallus soak", "swp brutallus soak", ACTION_MOVE + 4);
    Add(triggers, "swp brutallus not soaker", "swp brutallus flank", ACTION_MOVE + 4);
    Add(triggers, "swp brutallus burn", "swp brutallus burn spread", ACTION_RAID + 1);

    // Felmyst
    Add(triggers, "swp felmyst not victim", "swp felmyst flank", ACTION_MOVE + 4);
    Add(triggers, "swp felmyst vapor", "swp felmyst lead vapor", ACTION_RAID + 1);
    Add(triggers, "swp felmyst lane", "swp felmyst dodge lane", ACTION_RAID + 3);

    // Eredar Twins
    Add(triggers, "swp twins spread", "swp twins spread", ACTION_RAID);
    Add(triggers, "swp sacrolash tank", "swp sacrolash taunt", ACTION_RAID);
    Add(triggers, "swp alythess tank", "swp alythess taunt", ACTION_RAID);

    // M'uru
    Add(triggers, "swp void sentinel tank", "swp void sentinel tank", ACTION_RAID);
    Add(triggers, "swp dark fiend", "swp dispel dark fiend", ACTION_RAID + 1);

    // Kil'jaeden
    Add(triggers, "swp hand tank", "swp hand tank", ACTION_RAID);
    Add(triggers, "swp hand volley", "swp hand volley move away", ACTION_RAID);
    Add(triggers, "swp ranged add", "swp attack ranged add", ACTION_RAID);
    Add(triggers, "swp fire bloom", "swp fire bloom spread", ACTION_RAID + 1);
    Add(triggers, "swp take dragon orb", "swp use dragon orb", ACTION_RAID + 1);
    Add(triggers, "swp ride dragon", "swp ride dragon", ACTION_RAID + 4);
    Add(triggers, "swp darkness stack", "swp stack on dragon", ACTION_RAID + 3);
}

void RaidSunwellPlateauStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new KalecgosMultiplier(botAI));
    multipliers.push_back(new BrutallusMultiplier(botAI));
    multipliers.push_back(new FelmystBreathMultiplier(botAI));
    multipliers.push_back(new SunwellAssignedTankMultiplier(botAI));
    multipliers.push_back(new KiljaedenDragonMultiplier(botAI));
}
