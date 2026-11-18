/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOAMULTIPLIERS_H
#define PLAYERBOTS_VOAMULTIPLIERS_H

#include "Multiplier.h"

class ToravonFrostbiteMultiplier : public Multiplier
{
public:
    ToravonFrostbiteMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "toravon frostbite multiplier") {}
    float GetValue(Action* action) override;
};

#endif
