/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TOTCMULTIPLIERS_H
#define PLAYERBOTS_TOTCMULTIPLIERS_H

#include "Multiplier.h"

class TrialOfTheCrusaderMultiplier : public Multiplier
{
public:
    TrialOfTheCrusaderMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "trial of the crusader") {}
    float GetValue(Action* action) override;
};

#endif
