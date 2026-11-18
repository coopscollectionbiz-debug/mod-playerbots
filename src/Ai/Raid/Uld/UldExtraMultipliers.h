/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDEXTRAMULTIPLIERS_H
#define PLAYERBOTS_ULDEXTRAMULTIPLIERS_H

#include "Multiplier.h"

// Ignis' constructs, XT-002's heart, Auriaya's Feral Defender and Algalon's Phase Punch and Big Bang.
class UlduarExtraMultiplier : public Multiplier
{
public:
    UlduarExtraMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "ulduar extra") {}
    float GetValue(Action* action) override;
};

#endif
