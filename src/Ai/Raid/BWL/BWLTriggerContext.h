/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BWLTRIGGERCONTEXT_H
#define PLAYERBOTS_BWLTRIGGERCONTEXT_H

#include "BWLTriggers.h"
#include "BossAuraTriggers.h"
#include "NamedObjectContext.h"

class RaidBwlTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidBwlTriggerContext()
    {
        creators["bwl suppression device"] = &RaidBwlTriggerContext::bwl_suppression_device;

        creators["bwl razorgore fire resistance"] = &RaidBwlTriggerContext::bwl_razorgore_fire_resistance_trigger;
        creators["bwl razorgore not mind controlled"] = &RaidBwlTriggerContext::bwl_razorgore_not_mind_controlled;

        creators["bwl vaelastrasz fire resistance"] = &RaidBwlTriggerContext::bwl_vaelastrasz_fire_resistance_trigger;
        creators["bwl vaelastrasz positioning"] = &RaidBwlTriggerContext::bwl_vaelastrasz_positioning;
        creators["bwl vaelastrasz burning adrenaline"] = &RaidBwlTriggerContext::bwl_vaelastrasz_burning_adrenaline;

        creators["bwl broodlord fire resistance"] = &RaidBwlTriggerContext::bwl_broodlord_fire_resistance_trigger;
        creators["bwl broodlord ranged too close"] = &RaidBwlTriggerContext::bwl_broodlord_ranged_too_close;

        creators["bwl firemaw fire resistance"] = &RaidBwlTriggerContext::bwl_firemaw_fire_resistance_trigger;
        creators["bwl firemaw not victim"] = &RaidBwlTriggerContext::bwl_firemaw_not_victim;
        creators["bwl ebonroc not victim"] = &RaidBwlTriggerContext::bwl_ebonroc_not_victim;
        creators["bwl flamegor fire resistance"] = &RaidBwlTriggerContext::bwl_flamegor_fire_resistance_trigger;
        creators["bwl flamegor not victim"] = &RaidBwlTriggerContext::bwl_flamegor_not_victim;

        creators["bwl chromaggus fire resistance"] = &RaidBwlTriggerContext::bwl_chromaggus_fire_resistance_trigger;
        creators["bwl affliction bronze"] = &RaidBwlTriggerContext::bwl_affliction_bronze;

        creators["bwl nefarian positioning"] = &RaidBwlTriggerContext::bwl_nefarian_positioning;
        creators["bwl nefarian wild magic"] = &RaidBwlTriggerContext::bwl_nefarian_wild_magic;
        creators["bwl nefarian corrupted totem"] = &RaidBwlTriggerContext::bwl_nefarian_corrupted_totem;
        creators["bwl nefarian mt station"] = &RaidBwlTriggerContext::bwl_nefarian_mt_station;
        creators["bwl nefarian position"] = &RaidBwlTriggerContext::bwl_nefarian_position;
        creators["bwl nefarian dps target"] = &RaidBwlTriggerContext::bwl_nefarian_dps_target;
        creators["bwl nefarian ot target"] = &RaidBwlTriggerContext::bwl_nefarian_ot_target;
        creators["bwl nefarian tremor"] = &RaidBwlTriggerContext::bwl_nefarian_tremor;
        creators["bwl nefarian decurse tank"] = &RaidBwlTriggerContext::bwl_nefarian_decurse_tank;
        creators["bwl nefarian dispel polymorph"] = &RaidBwlTriggerContext::bwl_nefarian_dispel_polymorph;
        creators["bwl nefarian priest tank only"] = &RaidBwlTriggerContext::bwl_nefarian_priest_tank_only;
        creators["bwl nefarian hunter rearm"] = &RaidBwlTriggerContext::bwl_nefarian_hunter_rearm;
        creators["bwl nefarian rogue survive"] = &RaidBwlTriggerContext::bwl_nefarian_rogue_survive;
        creators["bwl nefarian mt sunder"] = &RaidBwlTriggerContext::bwl_nefarian_mt_sunder;

        creators["bwl death talon wyrmguard tank"] = &RaidBwlTriggerContext::bwl_death_talon_wyrmguard_tank;
        creators["bwl death talon wyrmguard ranged"] = &RaidBwlTriggerContext::bwl_death_talon_wyrmguard_ranged;
        creators["bwl wyrmguard tank"] = &RaidBwlTriggerContext::bwl_wyrmguard_tank;
        creators["bwl wyrmguard mark skull"] = &RaidBwlTriggerContext::bwl_wyrmguard_mark_skull;
        creators["bwl wyrmguard dps target"] = &RaidBwlTriggerContext::bwl_wyrmguard_dps_target;
        creators["bwl wyrmguard melee position"] = &RaidBwlTriggerContext::bwl_wyrmguard_melee_position;
        creators["bwl wyrmguard melee behind"] = &RaidBwlTriggerContext::bwl_wyrmguard_melee_behind;
        creators["bwl wyrmguard ebonroc keep out"] = &RaidBwlTriggerContext::bwl_wyrmguard_ebonroc_keep_out;
        creators["bwl wyrmguard healer position"] = &RaidBwlTriggerContext::bwl_wyrmguard_healer_position;
        creators["bwl threat hold"] = &RaidBwlTriggerContext::bwl_threat_hold;
    }

private:
    static Trigger* bwl_suppression_device(PlayerbotAI* botAI) { return new BwlSuppressionDeviceTrigger(botAI); }
    static Trigger* bwl_razorgore_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "razorgore the untamed"); }
    static Trigger* bwl_razorgore_not_mind_controlled(PlayerbotAI* botAI) { return new BwlRazorgoreNotMindControlledTrigger(botAI); }
    static Trigger* bwl_vaelastrasz_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "vaelastrasz the corrupt"); }
    static Trigger* bwl_vaelastrasz_positioning(PlayerbotAI* botAI) { return new BwlVaelastraszPositioningTrigger(botAI); }
    static Trigger* bwl_vaelastrasz_burning_adrenaline(PlayerbotAI* botAI) { return new BwlVaelastraszBurningAdrenalineTrigger(botAI); }
    static Trigger* bwl_broodlord_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "broodlord lashlayer"); }
    static Trigger* bwl_broodlord_ranged_too_close(PlayerbotAI* botAI) { return new BwlBroodlordRangedTooCloseTrigger(botAI); }
    static Trigger* bwl_firemaw_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "firemaw"); }
    static Trigger* bwl_firemaw_not_victim(PlayerbotAI* botAI) { return new BwlBlackDrakeNotVictimTrigger(botAI, "firemaw"); }
    static Trigger* bwl_ebonroc_not_victim(PlayerbotAI* botAI) { return new BwlBlackDrakeNotVictimTrigger(botAI, "ebonroc"); }
    static Trigger* bwl_flamegor_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "flamegor"); }
    static Trigger* bwl_flamegor_not_victim(PlayerbotAI* botAI) { return new BwlBlackDrakeNotVictimTrigger(botAI, "flamegor"); }
    static Trigger* bwl_chromaggus_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "chromaggus"); }
    static Trigger* bwl_affliction_bronze(PlayerbotAI* botAI) { return new BwlAfflictionBronzeTrigger(botAI); }
    static Trigger* bwl_nefarian_wild_magic(PlayerbotAI* botAI) { return new BwlNefarianWildMagicTrigger(botAI); }
    static Trigger* bwl_nefarian_positioning(PlayerbotAI* botAI) { return new BwlNefarianPositioningTrigger(botAI); }
    static Trigger* bwl_nefarian_corrupted_totem(PlayerbotAI* botAI) { return new BwlNefarianCorruptedTotemTrigger(botAI); }
    static Trigger* bwl_nefarian_mt_station(PlayerbotAI* botAI) { return new BwlNefarianMtStationTrigger(botAI); }
    static Trigger* bwl_nefarian_position(PlayerbotAI* botAI) { return new BwlNefarianPositionTrigger(botAI); }
    static Trigger* bwl_nefarian_dps_target(PlayerbotAI* botAI) { return new BwlNefarianDpsTargetTrigger(botAI); }
    static Trigger* bwl_nefarian_ot_target(PlayerbotAI* botAI) { return new BwlNefarianOtTargetTrigger(botAI); }
    static Trigger* bwl_nefarian_tremor(PlayerbotAI* botAI) { return new BwlNefarianTremorTrigger(botAI); }
    static Trigger* bwl_nefarian_decurse_tank(PlayerbotAI* botAI) { return new BwlNefarianDecurseTankTrigger(botAI); }
    static Trigger* bwl_nefarian_dispel_polymorph(PlayerbotAI* botAI) { return new BwlNefarianDispelPolymorphTrigger(botAI); }
    static Trigger* bwl_nefarian_priest_tank_only(PlayerbotAI* botAI) { return new BwlNefarianPriestTankOnlyTrigger(botAI); }
    static Trigger* bwl_nefarian_hunter_rearm(PlayerbotAI* botAI) { return new BwlNefarianHunterRearmTrigger(botAI); }
    static Trigger* bwl_nefarian_rogue_survive(PlayerbotAI* botAI) { return new BwlNefarianRogueSurviveTrigger(botAI); }
    static Trigger* bwl_nefarian_mt_sunder(PlayerbotAI* botAI) { return new BwlNefarianMtSunderTrigger(botAI); }
    static Trigger* bwl_death_talon_wyrmguard_tank(PlayerbotAI* botAI) { return new BwlDeathTalonWyrmguardTankTrigger(botAI); }
    static Trigger* bwl_death_talon_wyrmguard_ranged(PlayerbotAI* botAI) { return new BwlDeathTalonWyrmguardRangedTrigger(botAI); }
    static Trigger* bwl_wyrmguard_tank(PlayerbotAI* botAI) { return new BwlWyrmguardTankTrigger(botAI); }
    static Trigger* bwl_wyrmguard_mark_skull(PlayerbotAI* botAI) { return new BwlWyrmguardMarkSkullTrigger(botAI); }
    static Trigger* bwl_wyrmguard_dps_target(PlayerbotAI* botAI) { return new BwlWyrmguardDpsTargetTrigger(botAI); }
    static Trigger* bwl_wyrmguard_melee_position(PlayerbotAI* botAI) { return new BwlWyrmguardMeleePositionTrigger(botAI); }
    static Trigger* bwl_wyrmguard_melee_behind(PlayerbotAI* botAI) { return new BwlWyrmguardMeleeBehindTrigger(botAI); }
    static Trigger* bwl_wyrmguard_ebonroc_keep_out(PlayerbotAI* botAI) { return new BwlWyrmguardEbonrocKeepOutTrigger(botAI); }
    static Trigger* bwl_wyrmguard_healer_position(PlayerbotAI* botAI) { return new BwlWyrmguardHealerPositionTrigger(botAI); }
    static Trigger* bwl_threat_hold(PlayerbotAI* botAI) { return new BwlThreatHoldTrigger(botAI); }
};

#endif
