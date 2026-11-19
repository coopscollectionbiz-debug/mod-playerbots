/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BWLMULTIPLIERS_H
#define PLAYERBOTS_BWLMULTIPLIERS_H

#include "Multiplier.h"

class RazorgoreTankMultiplier : public Multiplier
{
public:
    RazorgoreTankMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "razorgore tank multiplier") {}
    float GetValue(Action* action) override;
};

class VaelastraszTankMultiplier : public Multiplier
{
public:
    VaelastraszTankMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "vaelastrasz tank multiplier") {}
    float GetValue(Action* action) override;
};

class VaelastraszBurningAdrenalineMultiplier : public Multiplier
{
public:
    VaelastraszBurningAdrenalineMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "vaelastrasz burning adrenaline multiplier") {}
    float GetValue(Action* action) override;
};

class NefarianCorruptedHealingMultiplier : public Multiplier
{
public:
    NefarianCorruptedHealingMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "nefarian corrupted healing multiplier") {}
    float GetValue(Action* action) override;
};

class NefarianCorruptedTotemMultiplier : public Multiplier
{
public:
    NefarianCorruptedTotemMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "nefarian corrupted totem multiplier") {}
    float GetValue(Action* action) override;
};

// Death Talon Wyrmguard packs (overlay patch 0020)
class BwlWyrmguardMultiplier : public Multiplier
{
public:
    BwlWyrmguardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "bwl wyrmguard multiplier") {}
    float GetValue(Action* action) override;
};

// Nefarian phases 2 and 3 (overlay patch 0020, spec 2026-10-07)
class BwlNefarianMultiplier : public Multiplier
{
public:
    BwlNefarianMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "bwl nefarian multiplier") {}
    float GetValue(Action* action) override;
};

// BWL-wide DPS threat discipline (overlay patch 0020, spec 2026-10-08)
class BwlThreatDisciplineMultiplier : public Multiplier
{
public:
    BwlThreatDisciplineMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "bwl threat discipline multiplier") {}
    float GetValue(Action* action) override;
};

#endif
