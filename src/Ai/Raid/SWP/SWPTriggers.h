/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPTRIGGERS_H
#define PLAYERBOTS_SWPTRIGGERS_H

#include "Trigger.h"

// General

class SwpKillOrderMarkTrigger : public Trigger
{
public:
    SwpKillOrderMarkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp kill order mark") {}
    bool IsActive() override;
};

class SwpCcMarkTrigger : public Trigger
{
public:
    SwpCcMarkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp cc mark") {}
    bool IsActive() override;
};

class SwpAvoidHazardTrigger : public Trigger
{
public:
    SwpAvoidHazardTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp avoid hazard") {}
    bool IsActive() override;
};

// Kalecgos

// Checked every second: the portal search and team pick walk the area and the raid.
class SwpKalecgosTakePortalTrigger : public Trigger
{
public:
    SwpKalecgosTakePortalTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp kalecgos take portal", 1000) {}
    bool IsActive() override;
};

class SwpKalecgosHealKalecTrigger : public Trigger
{
public:
    SwpKalecgosHealKalecTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp kalecgos heal kalec") {}
    bool IsActive() override;
};

class SwpKalecgosNotVictimTrigger : public Trigger
{
public:
    SwpKalecgosNotVictimTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp kalecgos not victim") {}
    bool IsActive() override;
};

// Brutallus

class SwpBrutallusTankSwapTrigger : public Trigger
{
public:
    SwpBrutallusTankSwapTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp brutallus tank swap") {}
    bool IsActive() override;
};

class SwpBrutallusSoakTrigger : public Trigger
{
public:
    SwpBrutallusSoakTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp brutallus soak") {}
    bool IsActive() override;
};

class SwpBrutallusNotSoakerTrigger : public Trigger
{
public:
    SwpBrutallusNotSoakerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp brutallus not soaker") {}
    bool IsActive() override;
};

class SwpBrutallusBurnTrigger : public Trigger
{
public:
    SwpBrutallusBurnTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp brutallus burn") {}
    bool IsActive() override;
};

// Felmyst

class SwpFelmystNotVictimTrigger : public Trigger
{
public:
    SwpFelmystNotVictimTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp felmyst not victim") {}
    bool IsActive() override;
};

class SwpFelmystVaporTrigger : public Trigger
{
public:
    SwpFelmystVaporTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp felmyst vapor") {}
    bool IsActive() override;
};

class SwpFelmystLaneTrigger : public Trigger
{
public:
    SwpFelmystLaneTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp felmyst lane") {}
    bool IsActive() override;
};

// Eredar Twins
// Checked every 2s: finding the nearest raid member walks the whole group.

class SwpTwinsSpreadTrigger : public Trigger
{
public:
    SwpTwinsSpreadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp twins spread", 2) {}
    bool IsActive() override;
};

class SwpSacrolashTankTrigger : public Trigger
{
public:
    SwpSacrolashTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp sacrolash tank") {}
    bool IsActive() override;
};

class SwpAlythessTankTrigger : public Trigger
{
public:
    SwpAlythessTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp alythess tank") {}
    bool IsActive() override;
};

// M'uru

class SwpVoidSentinelTankTrigger : public Trigger
{
public:
    SwpVoidSentinelTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp void sentinel tank") {}
    bool IsActive() override;
};

class SwpDarkFiendTrigger : public Trigger
{
public:
    SwpDarkFiendTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp dark fiend") {}
    bool IsActive() override;
};

// Kil'jaeden

class SwpHandTankTrigger : public Trigger
{
public:
    SwpHandTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp hand tank") {}
    bool IsActive() override;
};

class SwpHandVolleyTrigger : public Trigger
{
public:
    SwpHandVolleyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp hand volley") {}
    bool IsActive() override;
};

class SwpRangedAddTrigger : public Trigger
{
public:
    SwpRangedAddTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp ranged add") {}
    bool IsActive() override;
};

class SwpFireBloomTrigger : public Trigger
{
public:
    SwpFireBloomTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp fire bloom") {}
    bool IsActive() override;
};

class SwpTakeDragonOrbTrigger : public Trigger
{
public:
    SwpTakeDragonOrbTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp take dragon orb") {}
    bool IsActive() override;
};

class SwpRideDragonTrigger : public Trigger
{
public:
    SwpRideDragonTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp ride dragon") {}
    bool IsActive() override;
};

class SwpDarknessStackTrigger : public Trigger
{
public:
    SwpDarknessStackTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swp darkness stack") {}
    bool IsActive() override;
};

#endif
