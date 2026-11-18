/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TOTCTRIGGERS_H
#define PLAYERBOTS_TOTCTRIGGERS_H

#include "Trigger.h"

// Shared
class TotcInterruptTrigger : public Trigger
{
public:
    TotcInterruptTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc interrupt") {}
    bool IsActive() override;
};

// The Beasts of Northrend
class TotcGormokImpaleTrigger : public Trigger
{
public:
    TotcGormokImpaleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc gormok impale") {}
    bool IsActive() override;
};

class TotcSnoboldVassalTrigger : public Trigger
{
public:
    TotcSnoboldVassalTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc snobold vassal") {}
    bool IsActive() override;
};

class TotcBurningBileTrigger : public Trigger
{
public:
    TotcBurningBileTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc burning bile") {}
    bool IsActive() override;
};

class TotcSlimePoolTrigger : public Trigger
{
public:
    TotcSlimePoolTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc slime pool") {}
    bool IsActive() override;
};

class TotcIcehowlTrampleTrigger : public Trigger
{
public:
    TotcIcehowlTrampleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc icehowl trample") {}
    bool IsActive() override;
};

class TotcJormungarTankTrigger : public Trigger
{
public:
    TotcJormungarTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc jormungar tank") {}
    bool IsActive() override;
};

class TotcJormungarKillOrderTrigger : public Trigger
{
public:
    TotcJormungarKillOrderTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc jormungar kill order") {}
    bool IsActive() override;
};

class TotcJormungarMeleeTrigger : public Trigger
{
public:
    TotcJormungarMeleeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc jormungar melee") {}
    bool IsActive() override;
};

// Lord Jaraxxus
class TotcLegionFlameTrigger : public Trigger
{
public:
    TotcLegionFlameTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc legion flame") {}
    bool IsActive() override;
};

class TotcJaraxxusAddTrigger : public Trigger
{
public:
    TotcJaraxxusAddTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc jaraxxus add") {}
    bool IsActive() override;
};

// The Faction Champions
class TotcChampionKillOrderTrigger : public Trigger
{
public:
    TotcChampionKillOrderTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc champion kill order") {}
    bool IsActive() override;
};

class TotcChampionCrowdControlTrigger : public Trigger
{
public:
    TotcChampionCrowdControlTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc champion crowd control") {}
    bool IsActive() override;
};

// The Twin Val'kyr
class TotcTwinsWrongEssenceTrigger : public Trigger
{
public:
    TotcTwinsWrongEssenceTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc twins wrong essence") {}
    bool IsActive() override;
};

class TotcTwinsTargetTrigger : public Trigger
{
public:
    TotcTwinsTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc twins target") {}
    bool IsActive() override;
};

class TotcTwinsOppositeOrbTrigger : public Trigger
{
public:
    TotcTwinsOppositeOrbTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc twins opposite orb") {}
    bool IsActive() override;
};

// Anub'arak
class TotcPursuedByAnubarakTrigger : public Trigger
{
public:
    TotcPursuedByAnubarakTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc pursued by anub'arak") {}
    bool IsActive() override;
};

class TotcSpikeNearbyTrigger : public Trigger
{
public:
    TotcSpikeNearbyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc spike nearby") {}
    bool IsActive() override;
};

class TotcBurrowerUntankedTrigger : public Trigger
{
public:
    TotcBurrowerUntankedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc burrower untanked") {}
    bool IsActive() override;
};

class TotcFrostSphereTrigger : public Trigger
{
public:
    TotcFrostSphereTrigger(PlayerbotAI* botAI) : Trigger(botAI, "totc frost sphere") {}
    bool IsActive() override;
};

#endif
