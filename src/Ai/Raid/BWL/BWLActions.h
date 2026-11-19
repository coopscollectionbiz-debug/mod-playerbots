/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BWLACTIONS_H
#define PLAYERBOTS_BWLACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"

// General

class BwlOnyxiaScaleCloakAuraCheckAction : public Action
{
public:
    BwlOnyxiaScaleCloakAuraCheckAction(PlayerbotAI* botAI) : Action(botAI, "bwl onyxia scale cloak aura check") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class BwlTurnOffSuppressionDeviceAction : public Action
{
public:
    BwlTurnOffSuppressionDeviceAction(PlayerbotAI* botAI) : Action(botAI, "bwl turn off suppression device") {}
    bool Execute(Event event) override;
};

// Razorgore the Untamed

class BwlRazorgoreAvoidAoeAction : public MovementAction
{
public:
    BwlRazorgoreAvoidAoeAction(PlayerbotAI* botAI) : MovementAction(botAI, "bwl razorgore avoid aoe") {}
    bool Execute(Event event) override;
};

class BwlRazorgoreMarkBossAction : public AttackAction
{
public:
    BwlRazorgoreMarkBossAction(PlayerbotAI* botAI) : AttackAction(botAI, "bwl razorgore mark boss") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Vaelastrasz the Corrupt

class BwlVaelastraszMoveAwayAction : public MovementAction
{
public:
    BwlVaelastraszMoveAwayAction(PlayerbotAI* botAI) : MovementAction(botAI, "bwl vaelastrasz move away") {}
    bool Execute(Event event) override;

private:
    bool CalculateFleeDirection(Unit const* boss, float& fleeX, float& fleeY) const;
    bool MoveAlongFleeDirection(Unit const* boss, float fleeX, float fleeY);
};

// Broodlord Lashlayer

class BwlBroodlordRangedMoveAwayAction : public MovementAction
{
public:
    BwlBroodlordRangedMoveAwayAction(PlayerbotAI* botAI) : MovementAction(botAI, "bwl broodlord ranged move away") {}
    bool Execute(Event event) override;
};

// Firemaw / Ebonroc / Flamegor

class BwlBlackDrakeAvoidBreathAction : public BossRearFlankAction
{
public:
    BwlBlackDrakeAvoidBreathAction(PlayerbotAI* botAI, std::string const& bossName,
                                   float minAngle = ANGLE_45_DEG)
        : BossRearFlankAction(botAI, bossName, 0.0f, minAngle, M_PI) {}
    bool isUseful() override;
};

// Chromaggus

class BwlUseHourglassSandAction : public Action
{
public:
    BwlUseHourglassSandAction(PlayerbotAI* botAI) : Action(botAI, "bwl use hourglass sand") {}
    bool Execute(Event event) override;
};

// Nefarian

class BwlNefarianKillCorruptedTotemAction : public AttackAction
{
public:
    BwlNefarianKillCorruptedTotemAction(PlayerbotAI* botAI) : AttackAction(botAI, "bwl nefarian kill corrupted totem") {}
    bool Execute(Event event) override;
};

// Nefarian phases 2 and 3 (overlay patch 0020, spec 2026-10-07)

class BwlNefarianMtStationAction : public AttackAction
{
public:
    BwlNefarianMtStationAction(PlayerbotAI* botAI) : AttackAction(botAI, "bwl nefarian mt station") {}
    bool Execute(Event event) override;

private:
    // Stuck fallback for the Back step (fix round 3, G1); per bot (one action object per bot context).
    uint32 backAnchorMs = 0;   // 0 = not tracking; when the MT was last at backAnchorX/Y
    uint32 backLastMs = 0;     // last Back tick (a gap restarts the tracking)
    float backAnchorX = 0.0f;
    float backAnchorY = 0.0f;
};

class BwlNefarianPositionAction : public MovementAction
{
public:
    BwlNefarianPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "bwl nefarian position") {}
    bool Execute(Event event) override;
};

class BwlNefarianDpsTargetAction : public AttackAction
{
public:
    BwlNefarianDpsTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, "bwl nefarian dps target") {}
    bool Execute(Event event) override;
};

class BwlNefarianOtTargetAction : public AttackAction
{
public:
    BwlNefarianOtTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, "bwl nefarian ot target") {}
    bool Execute(Event event) override;
};

class BwlNefarianTremorAction : public Action
{
public:
    BwlNefarianTremorAction(PlayerbotAI* botAI) : Action(botAI, "bwl nefarian tremor") {}
    bool Execute(Event event) override;
};

class BwlNefarianDecurseTankAction : public Action
{
public:
    BwlNefarianDecurseTankAction(PlayerbotAI* botAI) : Action(botAI, "bwl nefarian decurse tank") {}
    bool Execute(Event event) override;
};

class BwlNefarianDispelPolymorphAction : public Action
{
public:
    BwlNefarianDispelPolymorphAction(PlayerbotAI* botAI) : Action(botAI, "bwl nefarian dispel polymorph") {}
    bool Execute(Event event) override;
};

class BwlNefarianPriestTankOnlyAction : public Action
{
public:
    BwlNefarianPriestTankOnlyAction(PlayerbotAI* botAI) : Action(botAI, "bwl nefarian priest tank only") {}
    bool Execute(Event event) override;
};

class BwlNefarianHunterRearmAction : public Action
{
public:
    BwlNefarianHunterRearmAction(PlayerbotAI* botAI) : Action(botAI, "bwl nefarian hunter rearm") {}
    bool Execute(Event event) override;

private:
    uint32 lastTryMs = 0;
};

class BwlNefarianRogueSurviveAction : public Action
{
public:
    BwlNefarianRogueSurviveAction(PlayerbotAI* botAI) : Action(botAI, "bwl nefarian rogue survive") {}
    bool Execute(Event event) override;
};

class BwlNefarianMtSunderAction : public Action
{
public:
    BwlNefarianMtSunderAction(PlayerbotAI* botAI) : Action(botAI, "bwl nefarian mt sunder") {}
    bool Execute(Event event) override;
};

// Trash

class BwlDeathTalonWyrmguardTankMoveAwayAction : public MovementAction
{
public:
    BwlDeathTalonWyrmguardTankMoveAwayAction(PlayerbotAI* botAI) : MovementAction(botAI, "bwl death talon wyrmguard tank move away") {}
    Unit* GetTarget() override;
    bool isUseful() override;
    bool Execute(Event event) override;
};

class BwlDeathTalonWyrmguardRangedMoveAwayAction : public MovementAction
{
public:
    BwlDeathTalonWyrmguardRangedMoveAwayAction(PlayerbotAI* botAI) : MovementAction(botAI, "bwl death talon wyrmguard ranged move away") {}
    Unit* GetTarget() override;
    bool Execute(Event event) override;
};

// Death Talon Wyrmguard packs (overlay patch 0020)

class BwlWyrmguardTankAction : public AttackAction
{
public:
    BwlWyrmguardTankAction(PlayerbotAI* botAI) : AttackAction(botAI, "bwl wyrmguard tank") {}
    bool Execute(Event event) override;
};

class BwlWyrmguardMarkSkullAction : public Action
{
public:
    BwlWyrmguardMarkSkullAction(PlayerbotAI* botAI) : Action(botAI, "bwl wyrmguard mark skull") {}
    bool Execute(Event event) override;
};

class BwlWyrmguardDpsTargetAction : public AttackAction
{
public:
    BwlWyrmguardDpsTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, "bwl wyrmguard dps target") {}
    bool Execute(Event event) override;
};

class BwlWyrmguardMeleePositionAction : public MovementAction
{
public:
    BwlWyrmguardMeleePositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "bwl wyrmguard melee position") {}
    bool Execute(Event event) override;
};

class BwlWyrmguardEbonrocKeepOutAction : public MovementAction
{
public:
    BwlWyrmguardEbonrocKeepOutAction(PlayerbotAI* botAI) : MovementAction(botAI, "bwl wyrmguard ebonroc keep out") {}
    bool Execute(Event event) override;
};

class BwlWyrmguardHealerPositionAction : public MovementAction
{
public:
    BwlWyrmguardHealerPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "bwl wyrmguard healer position") {}
    bool Execute(Event event) override;

private:
    uint32 noSpotMs = 0;   // when a full candidate sweep last found nothing better; 0 = none
    uint32 movingSinceMs = 0;   // when the current walk to a chosen spot started; 0 = none
};

// BWL-wide DPS threat discipline (overlay patch 0020, spec 2026-10-08)
class BwlThreatHoldAction : public Action
{
public:
    BwlThreatHoldAction(PlayerbotAI* botAI) : Action(botAI, "bwl threat hold") {}
    bool Execute(Event event) override;
};

#endif
