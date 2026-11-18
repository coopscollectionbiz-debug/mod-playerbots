/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDSTRATEGY_H
#define PLAYERBOTS_ULDSTRATEGY_H

#include "Strategy.h"

class RaidUlduarStrategy : public Strategy
{
public:
    RaidUlduarStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    virtual std::string const getName() override { return "ulduar"; }
    virtual void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    virtual void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

// Empty switch the Ulduar code checks: bots start the hard modes they control (XT-002's heart, the Iron
// Council's order, General Vezax's vapors). Added in chat with "co +uld hard mode".
class UlduarHardModeStrategy : public Strategy
{
public:
    UlduarHardModeStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    std::string const getName() override { return "uld hard mode"; }
};

#endif
