/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPACTIONS_H
#define PLAYERBOTS_SWPACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"

// General

class SwpKillOrderMarkAction : public Action
{
public:
    SwpKillOrderMarkAction(PlayerbotAI* botAI) : Action(botAI, "swp kill order mark") {}
    bool Execute(Event event) override;
};

class SwpCcMarkAction : public Action
{
public:
    SwpCcMarkAction(PlayerbotAI* botAI) : Action(botAI, "swp cc mark") {}
    bool Execute(Event event) override;
};

class SwpAvoidHazardAction : public MovementAction
{
public:
    SwpAvoidHazardAction(PlayerbotAI* botAI) : MovementAction(botAI, "swp avoid hazard") {}
    bool Execute(Event event) override;
};

// Attacks the named boss and taunts it off whoever has it, or only off a non-tank.
class SwpTauntBossAction : public AttackAction
{
public:
    SwpTauntBossAction(PlayerbotAI* botAI, std::string const& name, std::string bossName, bool tauntOffTanks)
        : AttackAction(botAI, name), bossName(std::move(bossName)), tauntOffTanks(tauntOffTanks) {}
    bool Execute(Event event) override;

private:
    std::string const bossName;
    bool const tauntOffTanks;
};

// Keeps the bot out of a boss's front, at melee range or, for ranged, at range.
class SwpBossFlankAction : public BossRearFlankAction
{
public:
    SwpBossFlankAction(PlayerbotAI* botAI, std::string const& bossName, float minAngle, float maxAngle,
                       bool frontOnly);
    bool isUseful() override;

private:
    bool const frontOnly;
};

// Moves away from the rest of the raid until no one is within the distance.
class SwpSpreadAction : public MovementAction
{
public:
    SwpSpreadAction(PlayerbotAI* botAI, std::string const& name, float distance)
        : MovementAction(botAI, name), distance(distance) {}
    bool Execute(Event event) override;

private:
    float const distance;
};

// Kalecgos

class SwpKalecgosTakePortalAction : public MovementAction
{
public:
    SwpKalecgosTakePortalAction(PlayerbotAI* botAI) : MovementAction(botAI, "swp kalecgos take portal") {}
    bool Execute(Event event) override;
};

class SwpKalecgosHealKalecAction : public Action
{
public:
    SwpKalecgosHealKalecAction(PlayerbotAI* botAI) : Action(botAI, "swp kalecgos heal kalec") {}
    bool Execute(Event event) override;
};

// Brutallus

class SwpBrutallusSoakAction : public MovementAction
{
public:
    SwpBrutallusSoakAction(PlayerbotAI* botAI) : MovementAction(botAI, "swp brutallus soak") {}
    bool Execute(Event event) override;
};

class SwpBrutallusBurnSpreadAction : public MovementAction
{
public:
    SwpBrutallusBurnSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "swp brutallus burn spread") {}
    bool Execute(Event event) override;
};

// Felmyst

class SwpFelmystDodgeLaneAction : public MovementAction
{
public:
    SwpFelmystDodgeLaneAction(PlayerbotAI* botAI) : MovementAction(botAI, "swp felmyst dodge lane") {}
    bool Execute(Event event) override;
};

// M'uru

class SwpVoidSentinelTankAction : public AttackAction
{
public:
    SwpVoidSentinelTankAction(PlayerbotAI* botAI) : AttackAction(botAI, "swp void sentinel tank") {}
    bool Execute(Event event) override;
};

class SwpDispelDarkFiendAction : public Action
{
public:
    SwpDispelDarkFiendAction(PlayerbotAI* botAI) : Action(botAI, "swp dispel dark fiend") {}
    bool Execute(Event event) override;
};

// Kil'jaeden

class SwpHandTankAction : public AttackAction
{
public:
    SwpHandTankAction(PlayerbotAI* botAI) : AttackAction(botAI, "swp hand tank") {}
    bool Execute(Event event) override;
};

class SwpHandVolleyMoveAwayAction : public MovementAction
{
public:
    SwpHandVolleyMoveAwayAction(PlayerbotAI* botAI) : MovementAction(botAI, "swp hand volley move away") {}
    bool Execute(Event event) override;
};

class SwpAttackRangedAddAction : public AttackAction
{
public:
    SwpAttackRangedAddAction(PlayerbotAI* botAI) : AttackAction(botAI, "swp attack ranged add") {}
    bool Execute(Event event) override;
};

class SwpUseDragonOrbAction : public MovementAction
{
public:
    SwpUseDragonOrbAction(PlayerbotAI* botAI) : MovementAction(botAI, "swp use dragon orb") {}
    bool Execute(Event event) override;
};

class SwpRideDragonAction : public Action
{
public:
    SwpRideDragonAction(PlayerbotAI* botAI) : Action(botAI, "swp ride dragon") {}
    bool Execute(Event event) override;
};

class SwpStackOnDragonAction : public MovementAction
{
public:
    SwpStackOnDragonAction(PlayerbotAI* botAI) : MovementAction(botAI, "swp stack on dragon") {}
    bool Execute(Event event) override;
};

#endif
