/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDEXTRAACTIONS_H
#define PLAYERBOTS_ULDEXTRAACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include <string>
#include <utility>

class UldKillOrderMarkAction : public Action
{
public:
    UldKillOrderMarkAction(PlayerbotAI* botAI) : Action(botAI, "uld kill order mark") {}
    bool Execute(Event event) override;
};

class UldTankOffTankAddAction : public AttackAction
{
public:
    UldTankOffTankAddAction(PlayerbotAI* botAI) : AttackAction(botAI, "uld tank off-tank add") {}
    bool Execute(Event event) override;
};

class UldIgnisDragConstructAction : public MovementAction
{
public:
    UldIgnisDragConstructAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld ignis drag construct") {}
    bool Execute(Event event) override;
};

class UldXtSpreadAction : public MovementAction
{
public:
    UldXtSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld xt spread") {}
    bool Execute(Event event) override;
};

class UldXtAttackBotAddAction : public AttackAction
{
public:
    UldXtAttackBotAddAction(PlayerbotAI* botAI) : AttackAction(botAI, "uld xt attack bot add") {}
    bool Execute(Event event) override;
};

class UldXtAvoidVoidZoneAction : public MovementAction
{
public:
    UldXtAvoidVoidZoneAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld xt avoid void zone") {}
    bool Execute(Event event) override;
};

class UldAuriayaStackAction : public MovementAction
{
public:
    UldAuriayaStackAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld auriaya stack") {}
    bool Execute(Event event) override;
};

class UldInterruptAction : public Action
{
public:
    UldInterruptAction(PlayerbotAI* botAI) : Action(botAI, "uld interrupt") {}
    bool Execute(Event event) override;
};

class UldCouncilTankAction : public AttackAction
{
public:
    UldCouncilTankAction(PlayerbotAI* botAI) : AttackAction(botAI, "uld council tank") {}
    bool Execute(Event event) override;
};

class UldThorimAvoidSifFrostNovaAction : public MovementAction
{
public:
    UldThorimAvoidSifFrostNovaAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld thorim avoid sif frost nova") {}
    bool Execute(Event event) override;
};

class UldThorimAvoidSifBlizzardAction : public MovementAction
{
public:
    UldThorimAvoidSifBlizzardAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld thorim avoid sif blizzard") {}
    bool Execute(Event event) override;
};

class UldMimironAvoidFlamesAction : public MovementAction
{
public:
    UldMimironAvoidFlamesAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld mimiron avoid flames") {}
    bool Execute(Event event) override;
};

class UldMimironAvoidFrostBombAction : public MovementAction
{
public:
    UldMimironAvoidFrostBombAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld mimiron avoid frost bomb") {}
    bool Execute(Event event) override;
};

class UldVezaxKillVaporsAction : public AttackAction
{
public:
    UldVezaxKillVaporsAction(PlayerbotAI* botAI) : AttackAction(botAI, "uld vezax kill vapors") {}
    bool Execute(Event event) override;
};

class UldHodirToastyFireAction : public MovementAction
{
public:
    UldHodirToastyFireAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld hodir toasty fire") {}
    bool Execute(Event event) override;
};

// Sidesteps around Hodir, alternating sides so the bot stays where it fights.
class UldHodirShedBitingColdAction : public MovementAction
{
public:
    UldHodirShedBitingColdAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld hodir shed biting cold") {}
    bool Execute(Event event) override;

private:
    bool clockwise = false;
};

class UldHodirBreakIceAction : public AttackAction
{
public:
    UldHodirBreakIceAction(PlayerbotAI* botAI) : AttackAction(botAI, "uld hodir break ice") {}
    bool Execute(Event event) override;
};

class UldFreyaBreakRootsAction : public AttackAction
{
public:
    UldFreyaBreakRootsAction(PlayerbotAI* botAI) : AttackAction(botAI, "uld freya break roots") {}
    bool Execute(Event event) override;
};

class UldFreyaNaturesFuryAction : public MovementAction
{
public:
    UldFreyaNaturesFuryAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld freya nature's fury") {}
    bool Execute(Event event) override;
};

class UldFreyaAvoidSunBeamAction : public MovementAction
{
public:
    UldFreyaAvoidSunBeamAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld freya avoid sun beam") {}
    bool Execute(Event event) override;
};

class UldMimironAvoidMineAction : public MovementAction
{
public:
    UldMimironAvoidMineAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld mimiron avoid mine") {}
    bool Execute(Event event) override;
};

class UldYoggAvoidCloudAction : public MovementAction
{
public:
    UldYoggAvoidCloudAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld yogg avoid cloud") {}
    bool Execute(Event event) override;
};

// Attacks the named boss and taunts it off whoever has it.
class UldTauntBossAction : public AttackAction
{
public:
    UldTauntBossAction(PlayerbotAI* botAI, std::string const& name, std::string bossName)
        : AttackAction(botAI, name), bossName(std::move(bossName)) {}
    bool Execute(Event event) override;

private:
    std::string const bossName;
};

class UldAlgalonTauntAction : public AttackAction
{
public:
    UldAlgalonTauntAction(PlayerbotAI* botAI) : AttackAction(botAI, "uld algalon taunt") {}
    bool Execute(Event event) override;
};

class UldAlgalonEnterBlackHoleAction : public MovementAction
{
public:
    UldAlgalonEnterBlackHoleAction(PlayerbotAI* botAI) : MovementAction(botAI, "uld algalon enter black hole") {}
    bool Execute(Event event) override;
};

class UldAlgalonAttackStarAction : public AttackAction
{
public:
    UldAlgalonAttackStarAction(PlayerbotAI* botAI) : AttackAction(botAI, "uld algalon attack star") {}
    bool Execute(Event event) override;
};

#endif
