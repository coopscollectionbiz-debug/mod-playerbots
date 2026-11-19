/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPMULTIPLIERS_H
#define PLAYERBOTS_SWPMULTIPLIERS_H

#include "Multiplier.h"

class KalecgosDpsBalanceMultiplier : public Multiplier
{
public:
    KalecgosDpsBalanceMultiplier(PlayerbotAI* ai) : Multiplier(ai, "kalecgos dps balance multiplier") {}
    float GetValue(Action* action) override;
};

class BrutallusTankHoldMultiplier : public Multiplier
{
public:
    BrutallusTankHoldMultiplier(PlayerbotAI* ai) : Multiplier(ai, "brutallus tank hold multiplier") {}
    float GetValue(Action* action) override;
};

class FelmystAirMovementMultiplier : public Multiplier
{
public:
    FelmystAirMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "felmyst air movement multiplier") {}
    float GetValue(Action* action) override;
};

class TwinsMovementMultiplier : public Multiplier
{
public:
    TwinsMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "twins movement multiplier") {}
    float GetValue(Action* action) override;
};

class MuruMovementMultiplier : public Multiplier
{
public:
    MuruMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "muru movement multiplier") {}
    float GetValue(Action* action) override;
};

// Stops the engine's own target choosers from undoing this fight's kill order.
//
// THIS IS WHAT WAS ACTUALLY BREAKING THE ADD DAMAGE, and no amount of tuning inside `muru focus target`
// could have fixed it. `DpsAssistStrategy` binds "not dps target active" -> "dps assist" at relevance
// 50.0f, and "dps target" resolves to what the raid as a whole is on - the boss. Our focus action sits at
// 61, so it runs first, sets the add, and returns Attack()'s result. The moment the bot IS on the add,
// Attack() returns FALSE (it reports "already attacking"), the engine falls through to the next action in
// the queue, and `dps assist` at 50 puts the boss straight back.
//
// The result is a two-tick boss <-> add oscillation that no log line inside our own action can show,
// because from its point of view everything worked. Watched from the client it looks exactly as reported:
// targets flickering between the boss and an add, melee never leaving the boss to travel to an add (the
// generic movement they rely on chases whatever "current target" says, and half the time that is the
// boss), and nothing actually dying.
//
// Same idiom as BrutallusTankHoldMultiplier: keep the strategy's decision from being un-made a few
// relevance points lower down.
class MuruTargetHoldMultiplier : public Multiplier
{
public:
    MuruTargetHoldMultiplier(PlayerbotAI* ai) : Multiplier(ai, "muru target hold multiplier") {}
    float GetValue(Action* action) override;
};

class KjMovementMultiplier : public Multiplier
{
public:
    KjMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "kj movement multiplier") {}
    float GetValue(Action* action) override;
};

// The M'uru lesson, applied before it can cost anything this time: a strategy that dictates a target MUST
// veto the engine's own target choosers, because high relevance alone does not hold. `DpsAssistStrategy`
// binds "not dps target active" -> "dps assist" at 50.0f, AttackAction::Attack returns FALSE the moment
// the bot is already on the target it asked for, and the engine then falls through to the next queued
// action - which puts the raid's target back. The result is a two-tick oscillation no log line inside our
// own action can show.
class KjTargetHoldMultiplier : public Multiplier
{
public:
    KjTargetHoldMultiplier(PlayerbotAI* ai) : Multiplier(ai, "kj target hold multiplier") {}
    float GetValue(Action* action) override;
};


#endif
