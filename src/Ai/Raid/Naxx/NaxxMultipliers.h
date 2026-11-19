/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXMULTIPLIERS_H
#define PLAYERBOTS_NAXXMULTIPLIERS_H

#include "Multiplier.h"
#include "NaxxBossHelper.h"

class GrobbulusMultiplier : public Multiplier
{
public:
    GrobbulusMultiplier(PlayerbotAI* ai) : Multiplier(ai, "grobbulus") {}

public:
    float GetValue(Action* action) override;
};

class HeiganDanceMultiplier : public Multiplier
{
public:
    explicit HeiganDanceMultiplier(PlayerbotAI* ai) : Multiplier(ai, "heigan dance"), helper(ai) {}

public:
    float GetValue(Action* action) override;

private:
    HeiganBossHelper helper;
};

class LoathebGenericMultiplier : public Multiplier
{
public:
    LoathebGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "loatheb generic") {}

public:
    float GetValue(Action* action) override;
};

class ThaddiusGenericMultiplier : public Multiplier
{
public:
    ThaddiusGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thaddius generic"), helper(ai) {}

public:
    float GetValue(Action* action) override;

private:
    ThaddiusBossHelper helper;
};

class SapphironGenericMultiplier : public Multiplier
{
public:
    SapphironGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "sapphiron generic"), helper(ai) {}

    float GetValue(Action* action) override;

private:
    SapphironBossHelper helper;
};

class InstructorRazuviousGenericMultiplier : public Multiplier
{
public:
    InstructorRazuviousGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "instructor razuvious generic"), helper(ai) {}
    float GetValue(Action* action) override;

private:
    RazuviousBossHelper helper;
};

class KelthuzadGenericMultiplier : public Multiplier
{
public:
    KelthuzadGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "kelthuzad generic"), helper(ai) {}
    float GetValue(Action* action) override;

private:
    KelthuzadBossHelper helper;
};

class AnubrekhanGenericMultiplier : public Multiplier
{
public:
    AnubrekhanGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "anubrekhan generic") {}

public:
    float GetValue(Action* action) override;
};

class FourHorsemenGenericMultiplier : public Multiplier
{
public:
    FourHorsemenGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "four horsemen generic") {}

public:
    float GetValue(Action* action) override;
};

// Naxx40 Four Horsemen (overlay patch 0022; spec 2026-10-27): keeps stock target choosers and movers from undoing the
// strategy's placement for every latched bot, holds latched DPS under the horseman's active tank's threat, and keeps
// pets off a horseman their owner has left. NaxxMultipliers.cpp.
class Naxx40HorsemenMultiplier : public Multiplier
{
public:
    Naxx40HorsemenMultiplier(PlayerbotAI* ai) : Multiplier(ai, "naxx40 horsemen") {}

public:
    float GetValue(Action* action) override;
};

// Naxx40 Loatheb (overlay patch 0022; spec 2026-10-30 §4.2 / §4.8): keeps latched healers from spending their one heal
// per Corrupted Mind lockout on stock positive spells, and keeps upstream's Loatheb chooser from pulling a DPS bot off
// the spore it is killing. Registered after LoathebGenericMultiplier. NaxxMultipliers.cpp.
class Naxx40LoathebMultiplier : public Multiplier
{
public:
    Naxx40LoathebMultiplier(PlayerbotAI* ai) : Multiplier(ai, "naxx40 loatheb") {}

public:
    float GetValue(Action* action) override;
};

// Naxx40 Anub'Rekhan (overlay patch 0022; spec 2026-10-30-playerbot-naxx40-anubrekhan §4): vetoes upstream's
// "anub'rekhan position" (the swarm-phase clump at the room centre) for every bot holding a Naxx40Anub slot; tanks hold
// none, so the main tank's kite is untouched. NaxxMultipliers.cpp.
class Naxx40AnubMultiplier : public Multiplier
{
public:
    Naxx40AnubMultiplier(PlayerbotAI* ai) : Multiplier(ai, "naxx40 anub") {}

public:
    float GetValue(Action* action) override;
};

// Naxx40 Patchwerk (overlay patch 0022; spec 2026-11-01 §3.1-§3.2): the opener hold (non-tanks hold fire until the
// roles latch), the per-role threat caps after it, and no taunts from latched tanks other than the MT.
// NaxxMultipliers.cpp.
class Naxx40PatchwerkMultiplier : public Multiplier
{
public:
    Naxx40PatchwerkMultiplier(PlayerbotAI* ai) : Multiplier(ai, "naxx40 patchwerk") {}

public:
    float GetValue(Action* action) override;
};

// class GothikGenericMultiplier : public Multiplier
// {
// public:
//     GothikGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "gothik generic") {}

// public:
//     float GetValue(Action* action) override;
// };

class GluthGenericMultiplier : public Multiplier
{
public:
    GluthGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "gluth generic"), helper(ai) {}
    float GetValue(Action* action) override;

private:
    GluthBossHelper helper;
};

// Mechanics added from the WotLK 10/25-player guides: Faerlina's Worshippers, Gothik's side split,
// Patchwerk's Hateful Strike soakers and Chains of Kel'Thuzad.
class NaxxRaidMechanicsMultiplier : public Multiplier
{
public:
    NaxxRaidMechanicsMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "naxx raid mechanics") {}
    float GetValue(Action* action) override;
};

#endif
