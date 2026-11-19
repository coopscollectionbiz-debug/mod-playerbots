/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40MULTIPLIERS_H
#define PLAYERBOTS_AQ40MULTIPLIERS_H

#include "Multiplier.h"

// Named TwinEmperors*, not Twins*: SWP already defines TwinsMovementMultiplier for the Eredar
// Twins and both live in the same binary.

class TwinEmperorsMovementMultiplier : public Multiplier
{
public:
    TwinEmperorsMovementMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "twin emperors movement multiplier") {}
    float GetValue(Action* action) override;
};

// A strategy that dictates a target MUST veto the engine's own target choosers; high relevance
// alone does not hold. DpsAssistStrategy binds "not dps target active" -> "dps assist" at 50.0f,
// AttackAction::Attack returns FALSE the moment the bot is already on the target it asked for, and
// the engine then falls through to the next queued action - which puts the raid's target back.
// Here that would mean a caster hammering a physical-immune Vek'nilash for the whole fight.
class TwinEmperorsTargetHoldMultiplier : public Multiplier
{
public:
    TwinEmperorsTargetHoldMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "twin emperors target hold multiplier") {}
    float GetValue(Action* action) override;
};

// Spec §5.6 + ruling R8: no DPS, spare tank or waiting tank becomes a twin's top threat over its designated active
// tank, and nobody fires on a twin while holding fire (the Huhuran threat-multiplier shape, copied).
class TwinEmperorsThreatMultiplier : public Multiplier
{
public:
    TwinEmperorsThreatMultiplier(PlayerbotAI* ai) : Multiplier(ai, "twin emperors threat multiplier") {}
    float GetValue(Action* action) override;
};

// C'Thun. Same two shapes as the first two Twin Emperors multipliers above.

class Aq40CthunMovementMultiplier : public Multiplier
{
public:
    Aq40CthunMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "aq40 cthun movement multiplier") {}
    float GetValue(Action* action) override;
};

class Aq40CthunTargetHoldMultiplier : public Multiplier
{
public:
    Aq40CthunTargetHoldMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "aq40 cthun target hold multiplier") {}
    float GetValue(Action* action) override;
};

// No cast may START that would still be running when a Green Beam lands on us or Dark Glare can
// reach us: a casting bot evaluates no trigger until the cast ends.
class Aq40CthunCastSafetyMultiplier : public Multiplier
{
public:
    Aq40CthunCastSafetyMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "aq40 cthun cast safety multiplier") {}
    float GetValue(Action* action) override;
};

// Princess Huhuran.

// Ring holders: no generic mover while they own a spot, on it or walking to it. Off-tanks (rev 2): none
// while they have a melee spot beside the MT after the opener hold. The MT: none while
// "aq40 huhuran park boss" has work.
class Aq40HuhuranMovementMultiplier : public Multiplier
{
public:
    Aq40HuhuranMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "aq40 huhuran movement multiplier") {}
    float GetValue(Action* action) override;
};

// The opener hold (rev 2, spec §7.2 #1): non-tank bots do not attack her until she has been parked once this
// attempt, failing open after 45 s.
class Aq40HuhuranTargetHoldMultiplier : public Multiplier
{
public:
    Aq40HuhuranTargetHoldMultiplier(PlayerbotAI* ai) : Multiplier(ai, "aq40 huhuran target hold multiplier") {}
    float GetValue(Action* action) override;
};

// She is taunt-immune: no taunt (single-target or AoE) while she is the bot's current target (BWL Nefarian rule).
class Aq40HuhuranTauntMultiplier : public Multiplier
{
public:
    Aq40HuhuranTauntMultiplier(PlayerbotAI* ai) : Multiplier(ai, "aq40 huhuran taunt multiplier") {}
    float GetValue(Action* action) override;
};

// Rev 2 (spec §7.2 #2): threat discipline on her for DPS and off-tanks, against the MT's threat (BWL's
// BwlThreatDisciplineMultiplier, mirrored).
class Aq40HuhuranThreatMultiplier : public Multiplier
{
public:
    Aq40HuhuranThreatMultiplier(PlayerbotAI* ai) : Multiplier(ai, "aq40 huhuran threat multiplier") {}
    float GetValue(Action* action) override;
};

// Rev 3 (spec §8.2 #1): DPS / raid / racial / trinket / tank cooldowns held until Berserk, by exact action name.
class Aq40HuhuranCooldownHoldMultiplier : public Multiplier
{
public:
    Aq40HuhuranCooldownHoldMultiplier(PlayerbotAI* ai) : Multiplier(ai, "aq40 huhuran cooldown hold multiplier") {}
    float GetValue(Action* action) override;
};

#endif
