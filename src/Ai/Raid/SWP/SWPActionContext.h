/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPACTIONCONTEXT_H
#define PLAYERBOTS_SWPACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "SWPActions.h"
#include "SWPHelpers.h"

class RaidSunwellPlateauActionContext : public NamedObjectContext<Action>
{
public:
    RaidSunwellPlateauActionContext()
    {
        creators["swp kill order mark"] = &RaidSunwellPlateauActionContext::kill_order_mark;
        creators["swp cc mark"] = &RaidSunwellPlateauActionContext::cc_mark;
        creators["swp avoid hazard"] = &RaidSunwellPlateauActionContext::avoid_hazard;
        creators["swp kalecgos take portal"] = &RaidSunwellPlateauActionContext::kalecgos_take_portal;
        creators["swp kalecgos heal kalec"] = &RaidSunwellPlateauActionContext::kalecgos_heal_kalec;
        creators["swp kalecgos flank"] = &RaidSunwellPlateauActionContext::kalecgos_flank;
        creators["swp brutallus taunt"] = &RaidSunwellPlateauActionContext::brutallus_taunt;
        creators["swp brutallus soak"] = &RaidSunwellPlateauActionContext::brutallus_soak;
        creators["swp brutallus flank"] = &RaidSunwellPlateauActionContext::brutallus_flank;
        creators["swp brutallus burn spread"] = &RaidSunwellPlateauActionContext::brutallus_burn_spread;
        creators["swp felmyst flank"] = &RaidSunwellPlateauActionContext::felmyst_flank;
        creators["swp felmyst lead vapor"] = &RaidSunwellPlateauActionContext::felmyst_lead_vapor;
        creators["swp felmyst dodge lane"] = &RaidSunwellPlateauActionContext::felmyst_dodge_lane;
        creators["swp twins spread"] = &RaidSunwellPlateauActionContext::twins_spread;
        creators["swp sacrolash taunt"] = &RaidSunwellPlateauActionContext::sacrolash_taunt;
        creators["swp alythess taunt"] = &RaidSunwellPlateauActionContext::alythess_taunt;
        creators["swp void sentinel tank"] = &RaidSunwellPlateauActionContext::void_sentinel_tank;
        creators["swp dispel dark fiend"] = &RaidSunwellPlateauActionContext::dispel_dark_fiend;
        creators["swp hand tank"] = &RaidSunwellPlateauActionContext::hand_tank;
        creators["swp hand volley move away"] = &RaidSunwellPlateauActionContext::hand_volley_move_away;
        creators["swp attack ranged add"] = &RaidSunwellPlateauActionContext::attack_ranged_add;
        creators["swp fire bloom spread"] = &RaidSunwellPlateauActionContext::fire_bloom_spread;
        creators["swp use dragon orb"] = &RaidSunwellPlateauActionContext::use_dragon_orb;
        creators["swp ride dragon"] = &RaidSunwellPlateauActionContext::ride_dragon;
        creators["swp stack on dragon"] = &RaidSunwellPlateauActionContext::stack_on_dragon;
    }

private:
    static Action* kill_order_mark(PlayerbotAI* botAI) { return new SwpKillOrderMarkAction(botAI); }
    static Action* cc_mark(PlayerbotAI* botAI) { return new SwpCcMarkAction(botAI); }
    static Action* avoid_hazard(PlayerbotAI* botAI) { return new SwpAvoidHazardAction(botAI); }
    static Action* kalecgos_take_portal(PlayerbotAI* botAI) { return new SwpKalecgosTakePortalAction(botAI); }
    static Action* kalecgos_heal_kalec(PlayerbotAI* botAI) { return new SwpKalecgosHealKalecAction(botAI); }

    // Frost Breath covers his front and Tail Lash his rear, so everyone stands at his sides.
    static Action* kalecgos_flank(PlayerbotAI* botAI)
    {
        return new SwpBossFlankAction(botAI, "kalecgos", ANGLE_45_DEG, ANGLE_45_DEG * 3.0f, false);
    }

    static Action* brutallus_taunt(PlayerbotAI* botAI)
    {
        return new SwpTauntBossAction(botAI, "swp brutallus taunt", "brutallus", true);
    }
    static Action* brutallus_soak(PlayerbotAI* botAI) { return new SwpBrutallusSoakAction(botAI); }

    // Only the soak group stands in the Meteor Slash cone; melee sit behind him.
    static Action* brutallus_flank(PlayerbotAI* botAI)
    {
        return new SwpBossFlankAction(botAI, "brutallus", ANGLE_90_DEG, ANGLE_45_DEG * 3.5f, true);
    }
    static Action* brutallus_burn_spread(PlayerbotAI* botAI) { return new SwpBrutallusBurnSpreadAction(botAI); }

    // Cleave hits her front: melee go behind her, ranged to her sides.
    static Action* felmyst_flank(PlayerbotAI* botAI)
    {
        if (PlayerbotAI::IsRanged(botAI->GetBot()))
            return new SwpBossFlankAction(botAI, "felmyst", ANGLE_90_DEG * 0.75f, ANGLE_90_DEG * 1.25f, true);
        return new SwpBossFlankAction(botAI, "felmyst", ANGLE_90_DEG, ANGLE_45_DEG * 3.5f, true);
    }
    static Action* felmyst_lead_vapor(PlayerbotAI* botAI)
    {
        return new SwpSpreadAction(botAI, "swp felmyst lead vapor",
                                   SunwellPlateauHelpers::FELMYST_VAPOR_LEAD_DISTANCE);
    }
    static Action* felmyst_dodge_lane(PlayerbotAI* botAI) { return new SwpFelmystDodgeLaneAction(botAI); }

    static Action* twins_spread(PlayerbotAI* botAI)
    {
        return new SwpSpreadAction(botAI, "swp twins spread", SunwellPlateauHelpers::TWINS_SPREAD_DISTANCE);
    }
    static Action* sacrolash_taunt(PlayerbotAI* botAI)
    {
        return new SwpTauntBossAction(botAI, "swp sacrolash taunt", "lady sacrolash", false);
    }
    static Action* alythess_taunt(PlayerbotAI* botAI)
    {
        return new SwpTauntBossAction(botAI, "swp alythess taunt", "grand warlock alythess", true);
    }

    static Action* void_sentinel_tank(PlayerbotAI* botAI) { return new SwpVoidSentinelTankAction(botAI); }
    static Action* dispel_dark_fiend(PlayerbotAI* botAI) { return new SwpDispelDarkFiendAction(botAI); }

    static Action* hand_tank(PlayerbotAI* botAI) { return new SwpHandTankAction(botAI); }
    static Action* hand_volley_move_away(PlayerbotAI* botAI) { return new SwpHandVolleyMoveAwayAction(botAI); }
    static Action* attack_ranged_add(PlayerbotAI* botAI) { return new SwpAttackRangedAddAction(botAI); }
    static Action* fire_bloom_spread(PlayerbotAI* botAI)
    {
        return new SwpSpreadAction(botAI, "swp fire bloom spread", SunwellPlateauHelpers::FIRE_BLOOM_SPREAD_DISTANCE);
    }
    static Action* use_dragon_orb(PlayerbotAI* botAI) { return new SwpUseDragonOrbAction(botAI); }
    static Action* ride_dragon(PlayerbotAI* botAI) { return new SwpRideDragonAction(botAI); }
    static Action* stack_on_dragon(PlayerbotAI* botAI) { return new SwpStackOnDragonAction(botAI); }
};

#endif
