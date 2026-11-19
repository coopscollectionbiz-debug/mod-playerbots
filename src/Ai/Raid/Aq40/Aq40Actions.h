/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40ACTIONS_H
#define PLAYERBOTS_AQ40ACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"

class Aq40TwinsEraseTrackersAction : public Action
{
public:
    Aq40TwinsEraseTrackersAction(PlayerbotAI* botAI)
        : Action(botAI, "aq40 twins erase trackers") {}
    bool Execute(Event event) override;
};

class Aq40TwinsClearExplodeAction : public MovementAction
{
public:
    Aq40TwinsClearExplodeAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "aq40 twins clear explode") {}
    bool Execute(Event event) override;
};

class Aq40TwinsClearBlizzardAction : public MovementAction
{
public:
    Aq40TwinsClearBlizzardAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "aq40 twins clear blizzard") {}
    bool Execute(Event event) override;
};

// Revision 7 §16.1: step out to 16 y from Vek'lor on a stable sector bearing (GetTwinsBurstClearSpot).
class Aq40TwinsClearBurstAction : public MovementAction
{
public:
    Aq40TwinsClearBurstAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 twins clear burst") {}
    bool Execute(Event event) override;
};

class Aq40TwinsFocusTargetAction : public AttackAction
{
public:
    Aq40TwinsFocusTargetAction(PlayerbotAI* botAI)
        : AttackAction(botAI, "aq40 twins focus target") {}
    bool Execute(Event event) override;
};

// ---- v2 (spec 2026-10-22 §5.3-§5.6) ----

// Leave the landing zone on a stable sector bearing, and recall the pet (a designated warlock's pet is parked).
class Aq40TwinsLandingClearAction : public MovementAction
{
public:
    Aq40TwinsLandingClearAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 twins landing clear") {}
    bool Execute(Event event) override;
};

// A wrong victim never drags a twin: 12-40 y from Vek'lor, or walk Vek'nilash to his side's melee tank.
class Aq40TwinsVictimHoldAction : public MovementAction
{
public:
    Aq40TwinsVictimHoldAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 twins victim hold") {}
    bool Execute(Event event) override;
};

// The active melee tank backs Vek'nilash onto his side's station along a PathGenerator route, facing him.
class Aq40TwinsHoldStationAction : public MovementAction
{
public:
    Aq40TwinsHoldStationAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 twins hold station") {}
    bool Execute(Event event) override;
};

class Aq40TwinsHoldSpotAction : public MovementAction
{
public:
    Aq40TwinsHoldSpotAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 twins hold spot") {}
    bool Execute(Event event) override;
};

// Shadow Ward when it is ready and Vek'lor is on me, else Searing Pain on him; both by name.
class Aq40TwinsWarlockTankAction : public Action
{
public:
    Aq40TwinsWarlockTankAction(PlayerbotAI* botAI) : Action(botAI, "aq40 twins warlock tank") {}
    bool Execute(Event event) override;
};

// Stop the running swing / auto-repeat, keep the target (BWL BwlThreatHoldAction).
class Aq40TwinsThreatHoldAction : public Action
{
public:
    Aq40TwinsThreatHoldAction(PlayerbotAI* botAI) : Action(botAI, "aq40 twins threat hold") {}
    bool Execute(Event event) override;
};

class Aq40TwinsDirectPetsAction : public Action
{
public:
    Aq40TwinsDirectPetsAction(PlayerbotAI* botAI)
        : Action(botAI, "aq40 twins direct pets") {}
    bool Execute(Event event) override;
};

// Revision 6 §15.1: the class heal (Aq40TankHealSpellName), by name, on TwinsTankHealTarget.
class Aq40TwinsTankHealAction : public Action
{
public:
    Aq40TwinsTankHealAction(PlayerbotAI* botAI) : Action(botAI, "aq40 twins tank heal") {}
    bool Execute(Event event) override;
};

// Spec §5.5 form-up: walk to the staging spot and hold it (the hold spot shape); the formed-up whisper.
class Aq40TwinsStageAction : public MovementAction
{
public:
    Aq40TwinsStageAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 twins stage") {}
    bool Execute(Event event) override;
};

// Spec §5.5 "twins pull": warlock A opens on Vek'lor from its staging spot, A's casting spot (ruling R16). Everyone
// else, and any stale event, does nothing.
class Aq40TwinsPullAction : public Action
{
public:
    Aq40TwinsPullAction(PlayerbotAI* botAI) : Action(botAI, "aq40 twins pull") {}
    bool Execute(Event event) override;
};

// C'Thun.

class Aq40CthunEraseTrackersAction : public Action
{
public:
    Aq40CthunEraseTrackersAction(PlayerbotAI* botAI) : Action(botAI, "aq40 cthun erase trackers") {}
    bool Execute(Event event) override;
};

class Aq40CthunExitStomachAction : public MovementAction
{
public:
    Aq40CthunExitStomachAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "aq40 cthun exit stomach") {}
    bool Execute(Event event) override;
};

class Aq40CthunDismountAction : public Action
{
public:
    Aq40CthunDismountAction(PlayerbotAI* botAI) : Action(botAI, "aq40 cthun dismount") {}
    bool Execute(Event event) override;
};

class Aq40CthunDropToFloorAction : public MovementAction
{
public:
    Aq40CthunDropToFloorAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "aq40 cthun drop to floor") {}
    bool Execute(Event event) override;
};

class Aq40CthunStomachCloseInAction : public MovementAction
{
public:
    Aq40CthunStomachCloseInAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "aq40 cthun stomach close in") {}
    bool Execute(Event event) override;
};

class Aq40CthunDodgeGlareAction : public MovementAction
{
public:
    Aq40CthunDodgeGlareAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 cthun dodge glare") {}
    bool Execute(Event event) override;
};

class Aq40CthunClearBeamAction : public MovementAction
{
public:
    Aq40CthunClearBeamAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 cthun clear beam") {}
    bool Execute(Event event) override;
};

class Aq40CthunRunToFarSideAction : public MovementAction
{
public:
    Aq40CthunRunToFarSideAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "aq40 cthun run to far side") {}
    bool Execute(Event event) override;
};

class Aq40CthunFocusTargetAction : public AttackAction
{
public:
    Aq40CthunFocusTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, "aq40 cthun focus target") {}
    bool Execute(Event event) override;
};

class Aq40CthunFormUpAction : public MovementAction
{
public:
    Aq40CthunFormUpAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 cthun form up") {}
    bool Execute(Event event) override;
};

// Princess Huhuran.

class Aq40HuhuranEraseTrackersAction : public Action
{
public:
    Aq40HuhuranEraseTrackersAction(PlayerbotAI* botAI) : Action(botAI, "aq40 huhuran erase trackers") {}
    bool Execute(Event event) override;
};

// The MT's drag onto P (spec §4.2; mirrors BWL BwlNefarianMtStationAction).
class Aq40HuhuranParkBossAction : public AttackAction
{
public:
    Aq40HuhuranParkBossAction(PlayerbotAI* botAI) : AttackAction(botAI, "aq40 huhuran park boss") {}
    bool Execute(Event event) override;

private:
    uint32 backAnchorMs = 0;  // 0 = not tracking; when the MT was last at backAnchorX/Y
    uint32 backLastMs = 0;    // last Back tick (a gap restarts the tracking)
    float backAnchorX = 0.0f;
    float backAnchorY = 0.0f;
};

class Aq40HuhuranOffTankEngageAction : public AttackAction
{
public:
    Aq40HuhuranOffTankEngageAction(PlayerbotAI* botAI) : AttackAction(botAI, "aq40 huhuran off tank engage") {}
    bool Execute(Event event) override;

private:
    uint32 refusedLogMs = 0;  // last "off-tank spot move refused" log line; 0 = never
};

class Aq40HuhuranHoldSpotAction : public MovementAction
{
public:
    Aq40HuhuranHoldSpotAction(PlayerbotAI* botAI) : MovementAction(botAI, "aq40 huhuran hold spot") {}
    bool Execute(Event event) override;
};

// Rev 2 (spec §7.2 #2): stop the running auto-attack/auto-shot, keep the target (BWL BwlThreatHoldAction).
class Aq40HuhuranThreatHoldAction : public Action
{
public:
    Aq40HuhuranThreatHoldAction(PlayerbotAI* botAI) : Action(botAI, "aq40 huhuran threat hold") {}
    bool Execute(Event event) override;
};

// Rev 2 (spec §7.2 #4): the class fast heal on the picked tank, cast by name.
class Aq40HuhuranTankHealAction : public Action
{
public:
    Aq40HuhuranTankHealAction(PlayerbotAI* botAI) : Action(botAI, "aq40 huhuran tank heal") {}
    bool Execute(Event event) override;
};

// Rev 3 (spec §8.2 #2): the MT's Shield Wall / Divine Protection / Barkskin, then Last Stand < 50 %, once each.
class Aq40HuhuranMtBerserkDefensiveAction : public Action
{
public:
    Aq40HuhuranMtBerserkDefensiveAction(PlayerbotAI* botAI) : Action(botAI, "aq40 huhuran mt berserk defensive") {}
    bool Execute(Event event) override;
};

#endif
