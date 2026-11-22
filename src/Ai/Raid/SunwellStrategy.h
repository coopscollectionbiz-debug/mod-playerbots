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
    }
};

#endif
