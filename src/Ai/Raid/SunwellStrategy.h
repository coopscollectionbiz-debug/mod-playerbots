/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Sunwell Plateau (580) has no playerbots raid strategy. This one only hosts the raid resistance buff
// (RaidResistanceBuff.h; its trigger and action are registered in the shared Naxx/AQ40 contexts).

#ifndef PLAYERBOTS_SUNWELLSTRATEGY_H
#define PLAYERBOTS_SUNWELLSTRATEGY_H

#include "Strategy.h"

class RaidSunwellStrategy : public Strategy
{
public:
    RaidSunwellStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    std::string const getName() override { return "sunwell"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override
    {
        triggers.push_back(
            new TriggerNode("raid resistance buff", { NextAction("raid resistance buff", ACTION_RAID + 5) }));
        // Eredar Twins: out of Alythess's Blaze patches, and ranged/healers spread against Conflagration (SunwellActions.h).
        triggers.push_back(
            new TriggerNode("eredar blaze", { NextAction("eredar move out of blaze", ACTION_RAID + 3) }));
        triggers.push_back(
            new TriggerNode("eredar conflagration spread", { NextAction("eredar conflagration spread", ACTION_RAID + 2) }));
    }
};

#endif
