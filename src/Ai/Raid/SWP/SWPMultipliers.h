/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPMULTIPLIERS_H
#define PLAYERBOTS_SWPMULTIPLIERS_H

#include "Multiplier.h"

class KalecgosMultiplier : public Multiplier
{
public:
    KalecgosMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "kalecgos multiplier") {}
    float GetValue(Action* action) override;
};

class BrutallusMultiplier : public Multiplier
{
public:
    BrutallusMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "brutallus multiplier") {}
    float GetValue(Action* action) override;
};

class FelmystBreathMultiplier : public Multiplier
{
public:
    FelmystBreathMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "felmyst breath multiplier") {}
    float GetValue(Action* action) override;
};

// Tanks given a fixed add or twin don't follow the raid's target instead.
class SunwellAssignedTankMultiplier : public Multiplier
{
public:
    SunwellAssignedTankMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sunwell assigned tank multiplier") {}
    float GetValue(Action* action) override;
};

class KiljaedenDragonMultiplier : public Multiplier
{
public:
    KiljaedenDragonMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "kiljaeden dragon multiplier") {}
    float GetValue(Action* action) override;
};

#endif
