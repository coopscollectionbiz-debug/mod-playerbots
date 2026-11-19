/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BWLACTIONCONTEXT_H
#define PLAYERBOTS_BWLACTIONCONTEXT_H

#include "Action.h"
#include "BWLActions.h"
#include "BossAuraActions.h"
#include "NamedObjectContext.h"

class RaidBwlActionContext : public NamedObjectContext<Action>
{
public:
    RaidBwlActionContext()
    {
        creators["bwl check onyxia scale cloak"] = &RaidBwlActionContext::bwl_check_onyxia_scale_cloak;
        creators["bwl turn off suppression device"] = &RaidBwlActionContext::bwl_turn_off_suppression_device;

        creators["bwl razorgore fire resistance"] = &RaidBwlActionContext::bwl_razorgore_fire_resistance_action;
        creators["bwl razorgore avoid aoe"] = &RaidBwlActionContext::bwl_razorgore_avoid_aoe;
        creators["bwl razorgore mark boss"] = &RaidBwlActionContext::bwl_razorgore_mark_boss;

        creators["bwl vaelastrasz fire resistance"] = &RaidBwlActionContext::bwl_vaelastrasz_fire_resistance_action;
        creators["bwl vaelastrasz move away"] = &RaidBwlActionContext::bwl_vaelastrasz_move_away;

        creators["bwl broodlord fire resistance"] = &RaidBwlActionContext::bwl_broodlord_fire_resistance_action;
        creators["bwl broodlord ranged move away"] = &RaidBwlActionContext::bwl_broodlord_ranged_move_away;

        creators["bwl firemaw fire resistance"] = &RaidBwlActionContext::bwl_firemaw_fire_resistance_action;
        creators["bwl firemaw avoid breath"] = &RaidBwlActionContext::bwl_firemaw_avoid_breath;
        creators["bwl ebonroc avoid breath"] = &RaidBwlActionContext::bwl_ebonroc_avoid_breath;
        creators["bwl flamegor fire resistance"] = &RaidBwlActionContext::bwl_flamegor_fire_resistance_action;
        creators["bwl flamegor avoid breath"] = &RaidBwlActionContext::bwl_flamegor_avoid_breath;

        creators["bwl chromaggus fire resistance"] = &RaidBwlActionContext::bwl_chromaggus_fire_resistance_action;
        creators["bwl use hourglass sand"] = &RaidBwlActionContext::bwl_use_hourglass_sand;
        creators["bwl nefarian rear flank"] = &RaidBwlActionContext::bwl_nefarian_rear_flank;
        creators["bwl nefarian kill corrupted totem"] = &RaidBwlActionContext::bwl_nefarian_kill_corrupted_totem;
        creators["bwl nefarian mt station"] = &RaidBwlActionContext::bwl_nefarian_mt_station;
        creators["bwl nefarian position"] = &RaidBwlActionContext::bwl_nefarian_position;
        creators["bwl nefarian dps target"] = &RaidBwlActionContext::bwl_nefarian_dps_target;
        creators["bwl nefarian ot target"] = &RaidBwlActionContext::bwl_nefarian_ot_target;
        creators["bwl nefarian tremor"] = &RaidBwlActionContext::bwl_nefarian_tremor;
        creators["bwl nefarian decurse tank"] = &RaidBwlActionContext::bwl_nefarian_decurse_tank;
        creators["bwl nefarian dispel polymorph"] = &RaidBwlActionContext::bwl_nefarian_dispel_polymorph;
        creators["bwl nefarian priest tank only"] = &RaidBwlActionContext::bwl_nefarian_priest_tank_only;
        creators["bwl nefarian hunter rearm"] = &RaidBwlActionContext::bwl_nefarian_hunter_rearm;
        creators["bwl nefarian rogue survive"] = &RaidBwlActionContext::bwl_nefarian_rogue_survive;
        creators["bwl nefarian mt sunder"] = &RaidBwlActionContext::bwl_nefarian_mt_sunder;

        creators["bwl death talon wyrmguard tank move away"] = &RaidBwlActionContext::bwl_death_talon_wyrmguard_tank_move_away;
        creators["bwl death talon wyrmguard ranged move away"] = &RaidBwlActionContext::bwl_death_talon_wyrmguard_ranged_move_away;
        creators["bwl wyrmguard tank"] = &RaidBwlActionContext::bwl_wyrmguard_tank;
        creators["bwl wyrmguard mark skull"] = &RaidBwlActionContext::bwl_wyrmguard_mark_skull;
        creators["bwl wyrmguard dps target"] = &RaidBwlActionContext::bwl_wyrmguard_dps_target;
        creators["bwl wyrmguard melee position"] = &RaidBwlActionContext::bwl_wyrmguard_melee_position;
        creators["bwl wyrmguard ebonroc keep out"] = &RaidBwlActionContext::bwl_wyrmguard_ebonroc_keep_out;
        creators["bwl wyrmguard healer position"] = &RaidBwlActionContext::bwl_wyrmguard_healer_position;
        creators["bwl threat hold"] = &RaidBwlActionContext::bwl_threat_hold;
    }

private:
    static Action* bwl_check_onyxia_scale_cloak(PlayerbotAI* botAI) { return new BwlOnyxiaScaleCloakAuraCheckAction(botAI); }
    static Action* bwl_turn_off_suppression_device(PlayerbotAI* botAI) { return new BwlTurnOffSuppressionDeviceAction(botAI); }
    static Action* bwl_razorgore_fire_resistance_action(PlayerbotAI* botAI) { return new BossFireResistanceAction(botAI, "razorgore the untamed"); }
    static Action* bwl_razorgore_avoid_aoe(PlayerbotAI* botAI) { return new BwlRazorgoreAvoidAoeAction(botAI); }
    static Action* bwl_razorgore_mark_boss(PlayerbotAI* botAI) { return new BwlRazorgoreMarkBossAction(botAI); }
    static Action* bwl_vaelastrasz_fire_resistance_action(PlayerbotAI* botAI) { return new BossFireResistanceAction(botAI, "vaelastrasz the corrupt"); }
    static Action* bwl_vaelastrasz_move_away(PlayerbotAI* botAI) { return new BwlVaelastraszMoveAwayAction(botAI); }
    static Action* bwl_broodlord_fire_resistance_action(PlayerbotAI* botAI) { return new BossFireResistanceAction(botAI, "broodlord lashlayer"); }
    static Action* bwl_broodlord_ranged_move_away(PlayerbotAI* botAI) { return new BwlBroodlordRangedMoveAwayAction(botAI); }
    static Action* bwl_firemaw_fire_resistance_action(PlayerbotAI* botAI) { return new BossFireResistanceAction(botAI, "firemaw"); }
    static Action* bwl_firemaw_avoid_breath(PlayerbotAI* botAI) { return new BwlBlackDrakeAvoidBreathAction(botAI, "firemaw"); }
    static Action* bwl_ebonroc_avoid_breath(PlayerbotAI* botAI) { return new BwlBlackDrakeAvoidBreathAction(botAI, "ebonroc"); }
    static Action* bwl_flamegor_fire_resistance_action(PlayerbotAI* botAI) { return new BossFireResistanceAction(botAI, "flamegor"); }
    static Action* bwl_flamegor_avoid_breath(PlayerbotAI* botAI) { return new BwlBlackDrakeAvoidBreathAction(botAI, "flamegor"); }
    static Action* bwl_chromaggus_fire_resistance_action(PlayerbotAI* botAI) { return new BossFireResistanceAction(botAI, "chromaggus"); }
    static Action* bwl_use_hourglass_sand(PlayerbotAI* botAI) { return new BwlUseHourglassSandAction(botAI); }
    static Action* bwl_nefarian_rear_flank(PlayerbotAI* botAI) { return new BossRearFlankAction(botAI, "nefarian"); }
    static Action* bwl_nefarian_kill_corrupted_totem(PlayerbotAI* botAI) { return new BwlNefarianKillCorruptedTotemAction(botAI); }
    static Action* bwl_nefarian_mt_station(PlayerbotAI* botAI) { return new BwlNefarianMtStationAction(botAI); }
    static Action* bwl_nefarian_position(PlayerbotAI* botAI) { return new BwlNefarianPositionAction(botAI); }
    static Action* bwl_nefarian_dps_target(PlayerbotAI* botAI) { return new BwlNefarianDpsTargetAction(botAI); }
    static Action* bwl_nefarian_ot_target(PlayerbotAI* botAI) { return new BwlNefarianOtTargetAction(botAI); }
    static Action* bwl_nefarian_tremor(PlayerbotAI* botAI) { return new BwlNefarianTremorAction(botAI); }
    static Action* bwl_nefarian_decurse_tank(PlayerbotAI* botAI) { return new BwlNefarianDecurseTankAction(botAI); }
    static Action* bwl_nefarian_dispel_polymorph(PlayerbotAI* botAI) { return new BwlNefarianDispelPolymorphAction(botAI); }
    static Action* bwl_nefarian_priest_tank_only(PlayerbotAI* botAI) { return new BwlNefarianPriestTankOnlyAction(botAI); }
    static Action* bwl_nefarian_hunter_rearm(PlayerbotAI* botAI) { return new BwlNefarianHunterRearmAction(botAI); }
    static Action* bwl_nefarian_rogue_survive(PlayerbotAI* botAI) { return new BwlNefarianRogueSurviveAction(botAI); }
    static Action* bwl_nefarian_mt_sunder(PlayerbotAI* botAI) { return new BwlNefarianMtSunderAction(botAI); }
    static Action* bwl_death_talon_wyrmguard_tank_move_away(PlayerbotAI* botAI) { return new BwlDeathTalonWyrmguardTankMoveAwayAction(botAI); }
    static Action* bwl_death_talon_wyrmguard_ranged_move_away(PlayerbotAI* botAI) { return new BwlDeathTalonWyrmguardRangedMoveAwayAction(botAI); }
    static Action* bwl_wyrmguard_tank(PlayerbotAI* botAI) { return new BwlWyrmguardTankAction(botAI); }
    static Action* bwl_wyrmguard_mark_skull(PlayerbotAI* botAI) { return new BwlWyrmguardMarkSkullAction(botAI); }
    static Action* bwl_wyrmguard_dps_target(PlayerbotAI* botAI) { return new BwlWyrmguardDpsTargetAction(botAI); }
    static Action* bwl_wyrmguard_melee_position(PlayerbotAI* botAI) { return new BwlWyrmguardMeleePositionAction(botAI); }
    static Action* bwl_wyrmguard_ebonroc_keep_out(PlayerbotAI* botAI) { return new BwlWyrmguardEbonrocKeepOutAction(botAI); }
    static Action* bwl_wyrmguard_healer_position(PlayerbotAI* botAI) { return new BwlWyrmguardHealerPositionAction(botAI); }
    static Action* bwl_threat_hold(PlayerbotAI* botAI) { return new BwlThreatHoldAction(botAI); }
};

#endif
