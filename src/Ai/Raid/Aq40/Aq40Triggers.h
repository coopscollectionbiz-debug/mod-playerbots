/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40TRIGGERS_H
#define PLAYERBOTS_AQ40TRIGGERS_H

#include "Trigger.h"

// EVERY NAME IS PREFIXED "aq40 twins". SWP registers ten "twins ..." names for the Eredar Twins,
// and NamedObjectContext creators are keyed by string in a shared registry - a duplicate silently
// shadows and the trigger never fires.

class Aq40TwinsNotInCombatTrigger : public Trigger
{
public:
    Aq40TwinsNotInCombatTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "aq40 twins not in combat") {}
    bool IsActive() override;
};

class Aq40TwinsInExplodeRadiusTrigger : public Trigger
{
public:
    Aq40TwinsInExplodeRadiusTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "aq40 twins in explode radius") {}
    bool IsActive() override;
};

class Aq40TwinsInBlizzardTrigger : public Trigger
{
public:
    Aq40TwinsInBlizzardTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "aq40 twins in blizzard") {}
    bool IsActive() override;
};

// Revision 7 §16.1: the bot is within 12.5 y of a living, non-passive Vek'lor (TwinsInBurstRange).
class Aq40TwinsInBurstRangeTrigger : public Trigger
{
public:
    Aq40TwinsInBurstRangeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 twins in burst range") {}
    bool IsActive() override;
};

class Aq40TwinsWrongTargetTrigger : public Trigger
{
public:
    Aq40TwinsWrongTargetTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "aq40 twins wrong target") {}
    bool IsActive() override;
};

// ---- v2 (spec 2026-10-22 §5.3-§5.6): fixed sides A/B, ready pickup tanks ----

// The 2 s landing window: the bot (unless it is that station's pickup tank) or its pet is inside a clear radius.
class Aq40TwinsInLandingZoneTrigger : public Trigger
{
public:
    Aq40TwinsInLandingZoneTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 twins in landing zone") {}
    bool IsActive() override;
};

// The bot is a twin's victim but not its designated active tank.
class Aq40TwinsWrongVictimTrigger : public Trigger
{
public:
    Aq40TwinsWrongVictimTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 twins wrong victim") {}
    bool IsActive() override;
};

// The active melee tank holds Vek'nilash and he is off his side's station.
class Aq40TwinsBossOffStationTrigger : public Trigger
{
public:
    Aq40TwinsBossOffStationTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 twins boss off station") {}
    bool IsActive() override;
};

// A healer, a waiting tank or the active warlock is away from its owned fixed spot.
class Aq40TwinsOffSpotTrigger : public Trigger
{
public:
    Aq40TwinsOffSpotTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 twins off spot") {}
    bool IsActive() override;
};

// Vek'lor's active warlock can cast on him from where it stands, not moving (ruling R13 F1; any spot, not only its
// casting spot).
class Aq40TwinsWarlockTankDueTrigger : public Trigger
{
public:
    Aq40TwinsWarlockTankDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 twins warlock tank due") {}
    bool IsActive() override;
};

// A running auto-attack / auto-shot / wand on a twin must stop: the 0.90 hard stop, or hold fire (ruling R8).
class Aq40TwinsThreatHoldTrigger : public Trigger
{
public:
    Aq40TwinsThreatHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 twins threat hold") {}
    bool IsActive() override;
};

class Aq40TwinsPetOnWrongTwinTrigger : public Trigger
{
public:
    Aq40TwinsPetOnWrongTwinTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "aq40 twins pet on wrong twin") {}
    bool IsActive() override;
};

// Revision 6 §15.1: a latched healer has a castable heal for a designated tank of its side (TwinsTankHealTarget).
class Aq40TwinsTankHealTrigger : public Trigger
{
public:
    Aq40TwinsTankHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 twins tank heal") {}
    bool IsActive() override;
};

// Spec §5.5 form-up: out of combat before the pull, the bot has a staging spot (always active while it does, so
// the staging action can also send the formed-up whisper). The "twins pull" chat command is a plain
// ChatCommandTrigger, registered in Aq40TriggerContext.h.
class Aq40TwinsFormUpTrigger : public Trigger
{
public:
    Aq40TwinsFormUpTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 twins form up") {}
    bool IsActive() override;
};

// C'Thun. Every name is prefixed "aq40 cthun" for the same shared-registry reason as above.

class Aq40CthunNotInCombatTrigger : public Trigger
{
public:
    Aq40CthunNotInCombatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 cthun not in combat") {}
    bool IsActive() override;
};

class Aq40CthunInStomachTrigger : public Trigger
{
public:
    Aq40CthunInStomachTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 cthun in stomach") {}
    bool IsActive() override;
};

class Aq40CthunMountedInPlaceTrigger : public Trigger
{
public:
    Aq40CthunMountedInPlaceTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 cthun mounted in place") {}
    bool IsActive() override;
};

class Aq40CthunStomachAirborneTrigger : public Trigger
{
public:
    Aq40CthunStomachAirborneTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "aq40 cthun stomach airborne") {}
    bool IsActive() override;
};

class Aq40CthunStomachFarFromTentacleTrigger : public Trigger
{
public:
    Aq40CthunStomachFarFromTentacleTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "aq40 cthun stomach far from tentacle") {}
    bool IsActive() override;
};

class Aq40CthunInGlarePathTrigger : public Trigger
{
public:
    Aq40CthunInGlarePathTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 cthun in glare path") {}
    bool IsActive() override;
};

class Aq40CthunNearBeamTargetTrigger : public Trigger
{
public:
    Aq40CthunNearBeamTargetTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "aq40 cthun near beam target") {}
    bool IsActive() override;
};

class Aq40CthunOpeningPullerTrigger : public Trigger
{
public:
    Aq40CthunOpeningPullerTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "aq40 cthun opening puller") {}
    bool IsActive() override;
};

class Aq40CthunWrongTargetTrigger : public Trigger
{
public:
    Aq40CthunWrongTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 cthun wrong target") {}
    bool IsActive() override;
};

class Aq40CthunNotFormedTrigger : public Trigger
{
public:
    Aq40CthunNotFormedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 cthun not formed") {}
    bool IsActive() override;
};

// Princess Huhuran. Every name is prefixed "aq40 huhuran" for the same shared-registry reason as above.

class Aq40HuhuranNotInCombatTrigger : public Trigger
{
public:
    Aq40HuhuranNotInCombatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 huhuran not in combat") {}
    bool IsActive() override;
};

class Aq40HuhuranMtHasWorkTrigger : public Trigger
{
public:
    Aq40HuhuranMtHasWorkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 huhuran mt has work") {}
    bool IsActive() override;
};

class Aq40HuhuranOffTankIdleTrigger : public Trigger
{
public:
    Aq40HuhuranOffTankIdleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 huhuran off tank idle") {}
    bool IsActive() override;
};

class Aq40HuhuranOffSpotTrigger : public Trigger
{
public:
    Aq40HuhuranOffSpotTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 huhuran off spot") {}
    bool IsActive() override;
};

// Rev 2 (spec §7.2 #2): the hard stop of a running auto-attack/auto-shot at 0.90 of the MT's threat (BWL's
// "bwl threat hold", mirrored).
class Aq40HuhuranThreatHoldTrigger : public Trigger
{
public:
    Aq40HuhuranThreatHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 huhuran threat hold") {}
    bool IsActive() override;
};

// Rev 2 (spec §7.2 #4): a Berserk tank healer has a castable fast heal for a tank under 90 %.
class Aq40HuhuranTankHealTrigger : public Trigger
{
public:
    Aq40HuhuranTankHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 huhuran tank heal") {}
    bool IsActive() override;
};

// Rev 3 (spec §8.2 #2): the MT has an unused, castable Berserk defensive.
class Aq40HuhuranMtBerserkDefensiveTrigger : public Trigger
{
public:
    Aq40HuhuranMtBerserkDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 huhuran mt berserk defensive") {}
    bool IsActive() override;
};

#endif
