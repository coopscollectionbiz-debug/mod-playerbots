/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPTRIGGERCONTEXT_H
#define PLAYERBOTS_SWPTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "SWPTriggers.h"

class RaidSunwellPlateauTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidSunwellPlateauTriggerContext()
    {
        creators["swp kill order mark"] = &RaidSunwellPlateauTriggerContext::kill_order_mark;
        creators["swp cc mark"] = &RaidSunwellPlateauTriggerContext::cc_mark;
        creators["swp avoid hazard"] = &RaidSunwellPlateauTriggerContext::avoid_hazard;
        creators["swp kalecgos take portal"] = &RaidSunwellPlateauTriggerContext::kalecgos_take_portal;
        creators["swp kalecgos heal kalec"] = &RaidSunwellPlateauTriggerContext::kalecgos_heal_kalec;
        creators["swp kalecgos not victim"] = &RaidSunwellPlateauTriggerContext::kalecgos_not_victim;
        creators["swp brutallus tank swap"] = &RaidSunwellPlateauTriggerContext::brutallus_tank_swap;
        creators["swp brutallus soak"] = &RaidSunwellPlateauTriggerContext::brutallus_soak;
        creators["swp brutallus not soaker"] = &RaidSunwellPlateauTriggerContext::brutallus_not_soaker;
        creators["swp brutallus burn"] = &RaidSunwellPlateauTriggerContext::brutallus_burn;
        creators["swp felmyst not victim"] = &RaidSunwellPlateauTriggerContext::felmyst_not_victim;
        creators["swp felmyst vapor"] = &RaidSunwellPlateauTriggerContext::felmyst_vapor;
        creators["swp felmyst lane"] = &RaidSunwellPlateauTriggerContext::felmyst_lane;
        creators["swp twins spread"] = &RaidSunwellPlateauTriggerContext::twins_spread;
        creators["swp sacrolash tank"] = &RaidSunwellPlateauTriggerContext::sacrolash_tank;
        creators["swp alythess tank"] = &RaidSunwellPlateauTriggerContext::alythess_tank;
        creators["swp void sentinel tank"] = &RaidSunwellPlateauTriggerContext::void_sentinel_tank;
        creators["swp dark fiend"] = &RaidSunwellPlateauTriggerContext::dark_fiend;
        creators["swp hand tank"] = &RaidSunwellPlateauTriggerContext::hand_tank;
        creators["swp hand volley"] = &RaidSunwellPlateauTriggerContext::hand_volley;
        creators["swp ranged add"] = &RaidSunwellPlateauTriggerContext::ranged_add;
        creators["swp fire bloom"] = &RaidSunwellPlateauTriggerContext::fire_bloom;
        creators["swp take dragon orb"] = &RaidSunwellPlateauTriggerContext::take_dragon_orb;
        creators["swp ride dragon"] = &RaidSunwellPlateauTriggerContext::ride_dragon;
        creators["swp darkness stack"] = &RaidSunwellPlateauTriggerContext::darkness_stack;
    }

private:
    static Trigger* kill_order_mark(PlayerbotAI* botAI) { return new SwpKillOrderMarkTrigger(botAI); }
    static Trigger* cc_mark(PlayerbotAI* botAI) { return new SwpCcMarkTrigger(botAI); }
    static Trigger* avoid_hazard(PlayerbotAI* botAI) { return new SwpAvoidHazardTrigger(botAI); }
    static Trigger* kalecgos_take_portal(PlayerbotAI* botAI) { return new SwpKalecgosTakePortalTrigger(botAI); }
    static Trigger* kalecgos_heal_kalec(PlayerbotAI* botAI) { return new SwpKalecgosHealKalecTrigger(botAI); }
    static Trigger* kalecgos_not_victim(PlayerbotAI* botAI) { return new SwpKalecgosNotVictimTrigger(botAI); }
    static Trigger* brutallus_tank_swap(PlayerbotAI* botAI) { return new SwpBrutallusTankSwapTrigger(botAI); }
    static Trigger* brutallus_soak(PlayerbotAI* botAI) { return new SwpBrutallusSoakTrigger(botAI); }
    static Trigger* brutallus_not_soaker(PlayerbotAI* botAI) { return new SwpBrutallusNotSoakerTrigger(botAI); }
    static Trigger* brutallus_burn(PlayerbotAI* botAI) { return new SwpBrutallusBurnTrigger(botAI); }
    static Trigger* felmyst_not_victim(PlayerbotAI* botAI) { return new SwpFelmystNotVictimTrigger(botAI); }
    static Trigger* felmyst_vapor(PlayerbotAI* botAI) { return new SwpFelmystVaporTrigger(botAI); }
    static Trigger* felmyst_lane(PlayerbotAI* botAI) { return new SwpFelmystLaneTrigger(botAI); }
    static Trigger* twins_spread(PlayerbotAI* botAI) { return new SwpTwinsSpreadTrigger(botAI); }
    static Trigger* sacrolash_tank(PlayerbotAI* botAI) { return new SwpSacrolashTankTrigger(botAI); }
    static Trigger* alythess_tank(PlayerbotAI* botAI) { return new SwpAlythessTankTrigger(botAI); }
    static Trigger* void_sentinel_tank(PlayerbotAI* botAI) { return new SwpVoidSentinelTankTrigger(botAI); }
    static Trigger* dark_fiend(PlayerbotAI* botAI) { return new SwpDarkFiendTrigger(botAI); }
    static Trigger* hand_tank(PlayerbotAI* botAI) { return new SwpHandTankTrigger(botAI); }
    static Trigger* hand_volley(PlayerbotAI* botAI) { return new SwpHandVolleyTrigger(botAI); }
    static Trigger* ranged_add(PlayerbotAI* botAI) { return new SwpRangedAddTrigger(botAI); }
    static Trigger* fire_bloom(PlayerbotAI* botAI) { return new SwpFireBloomTrigger(botAI); }
    static Trigger* take_dragon_orb(PlayerbotAI* botAI) { return new SwpTakeDragonOrbTrigger(botAI); }
    static Trigger* ride_dragon(PlayerbotAI* botAI) { return new SwpRideDragonTrigger(botAI); }
    static Trigger* darkness_stack(PlayerbotAI* botAI) { return new SwpDarknessStackTrigger(botAI); }
};

#endif
