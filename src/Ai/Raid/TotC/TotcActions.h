/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TOTCACTIONS_H
#define PLAYERBOTS_TOTCACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"

// Shared
class TotcInterruptAction : public Action
{
public:
    TotcInterruptAction(PlayerbotAI* botAI) : Action(botAI, "totc interrupt") {}
    bool Execute(Event event) override;
};

// The Beasts of Northrend
class TotcTauntGormokAction : public AttackAction
{
public:
    TotcTauntGormokAction(PlayerbotAI* botAI) : AttackAction(botAI, "totc taunt gormok") {}
    bool Execute(Event event) override;
};

class TotcKillSnoboldAction : public AttackAction
{
public:
    TotcKillSnoboldAction(PlayerbotAI* botAI) : AttackAction(botAI, "totc kill snobold") {}
    bool Execute(Event event) override;
};

class TotcCureParalyticToxinAction : public MovementAction
{
public:
    TotcCureParalyticToxinAction(PlayerbotAI* botAI) : MovementAction(botAI, "totc cure paralytic toxin") {}
    bool Execute(Event event) override;
};

class TotcAvoidSlimePoolAction : public MovementAction
{
public:
    TotcAvoidSlimePoolAction(PlayerbotAI* botAI) : MovementAction(botAI, "totc avoid slime pool") {}
    bool Execute(Event event) override;
};

class TotcDodgeTrampleAction : public MovementAction
{
public:
    TotcDodgeTrampleAction(PlayerbotAI* botAI) : MovementAction(botAI, "totc dodge trample") {}
    bool Execute(Event event) override;
};

class TotcTankJormungarAction : public AttackAction
{
public:
    TotcTankJormungarAction(PlayerbotAI* botAI) : AttackAction(botAI, "totc tank jormungar") {}
    bool Execute(Event event) override;
};

class TotcAttackAcidmawAction : public AttackAction
{
public:
    TotcAttackAcidmawAction(PlayerbotAI* botAI) : AttackAction(botAI, "totc attack acidmaw") {}
    bool Execute(Event event) override;
};

class TotcJormungarMeleePositionAction : public MovementAction
{
public:
    TotcJormungarMeleePositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "totc jormungar melee position") {}
    bool Execute(Event event) override;
};

// Lord Jaraxxus
class TotcSpreadFromRaidAction : public MovementAction
{
public:
    TotcSpreadFromRaidAction(PlayerbotAI* botAI) : MovementAction(botAI, "totc spread from raid") {}
    bool Execute(Event event) override;
};

class TotcKillJaraxxusAddAction : public AttackAction
{
public:
    TotcKillJaraxxusAddAction(PlayerbotAI* botAI) : AttackAction(botAI, "totc kill jaraxxus add") {}
    bool Execute(Event event) override;
};

// The Faction Champions
class TotcMarkChampionAction : public Action
{
public:
    TotcMarkChampionAction(PlayerbotAI* botAI) : Action(botAI, "totc mark champion") {}
    bool Execute(Event event) override;
};

class TotcCrowdControlChampionAction : public Action
{
public:
    TotcCrowdControlChampionAction(PlayerbotAI* botAI) : Action(botAI, "totc crowd control champion") {}
    bool Execute(Event event) override;
};

// The Twin Val'kyr
class TotcTakeEssenceAction : public MovementAction
{
public:
    TotcTakeEssenceAction(PlayerbotAI* botAI) : MovementAction(botAI, "totc take essence") {}
    bool Execute(Event event) override;
};

class TotcAttackTwinAction : public AttackAction
{
public:
    TotcAttackTwinAction(PlayerbotAI* botAI) : AttackAction(botAI, "totc attack twin") {}
    bool Execute(Event event) override;
};

class TotcAvoidOrbAction : public MovementAction
{
public:
    TotcAvoidOrbAction(PlayerbotAI* botAI) : MovementAction(botAI, "totc avoid orb") {}
    bool Execute(Event event) override;
};

// Anub'arak
class TotcKiteSpikeAction : public MovementAction
{
public:
    TotcKiteSpikeAction(PlayerbotAI* botAI) : MovementAction(botAI, "totc kite spike") {}
    bool Execute(Event event) override;
};

class TotcAvoidSpikeAction : public MovementAction
{
public:
    TotcAvoidSpikeAction(PlayerbotAI* botAI) : MovementAction(botAI, "totc avoid spike") {}
    bool Execute(Event event) override;
};

class TotcTankBurrowerAction : public AttackAction
{
public:
    TotcTankBurrowerAction(PlayerbotAI* botAI) : AttackAction(botAI, "totc tank burrower") {}
    bool Execute(Event event) override;
};

class TotcKillFrostSphereAction : public AttackAction
{
public:
    TotcKillFrostSphereAction(PlayerbotAI* botAI) : AttackAction(botAI, "totc kill frost sphere") {}
    bool Execute(Event event) override;
};

#endif
