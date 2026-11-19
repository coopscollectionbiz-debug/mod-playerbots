/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SWPACTIONCONTEXT_H
#define PLAYERBOTS_SWPACTIONCONTEXT_H

#include "NamedObjectContext.h"
#include "SWPActions.h"

class RaidSunwellPlateauActionContext : public NamedObjectContext<Action>
{
public:
    RaidSunwellPlateauActionContext()
    {
        // General
        creators["sunwell erase timers and trackers"] =
            &RaidSunwellPlateauActionContext::sunwell_erase_timers_and_trackers;

        // Kalecgos
        creators["kalecgos melee flank boss"] =
            &RaidSunwellPlateauActionContext::kalecgos_melee_flank_boss;
        creators["kalecgos tank position boss"] =
            &RaidSunwellPlateauActionContext::kalecgos_tank_position_boss;
        creators["kalecgos disperse ranged"] =
            &RaidSunwellPlateauActionContext::kalecgos_disperse_ranged;
        creators["kalecgos enter spectral rift"] =
            &RaidSunwellPlateauActionContext::kalecgos_enter_spectral_rift;
        creators["kalecgos return to platform"] =
            &RaidSunwellPlateauActionContext::kalecgos_return_to_platform;
        creators["kalecgos tank pick up sathrovarr"] =
            &RaidSunwellPlateauActionContext::kalecgos_tank_pick_up_sathrovarr;
        creators["kalecgos attack sathrovarr"] =
            &RaidSunwellPlateauActionContext::kalecgos_attack_sathrovarr;
        creators["kalecgos heal kalec"] =
            &RaidSunwellPlateauActionContext::kalecgos_heal_kalec;

        // Brutallus
        creators["brutallus taunt boss"] =
            &RaidSunwellPlateauActionContext::brutallus_taunt_boss;
        creators["brutallus move to station"] =
            &RaidSunwellPlateauActionContext::brutallus_move_to_station;
        creators["brutallus purge burn"] =
            &RaidSunwellPlateauActionContext::brutallus_purge_burn;

        // Felmyst
        creators["felmyst leave fog lane"] =
            &RaidSunwellPlateauActionContext::felmyst_leave_fog_lane;
        creators["felmyst flee encapsulate"] =
            &RaidSunwellPlateauActionContext::felmyst_flee_encapsulate;
        creators["felmyst attack charmed raider"] =
            &RaidSunwellPlateauActionContext::felmyst_attack_charmed_raider;
        creators["felmyst mass dispel gas nova"] =
            &RaidSunwellPlateauActionContext::felmyst_mass_dispel_gas_nova;
        creators["felmyst kite vapor"] =
            &RaidSunwellPlateauActionContext::felmyst_kite_vapor;
        creators["felmyst regroup"] =
            &RaidSunwellPlateauActionContext::felmyst_regroup;

        // Eredar Twins
        creators["twins flee conflagration"] =
            &RaidSunwellPlateauActionContext::twins_flee_conflagration;
        creators["twins clear conflagration"] =
            &RaidSunwellPlateauActionContext::twins_clear_conflagration;
        creators["twins focus sacrolash"] =
            &RaidSunwellPlateauActionContext::twins_focus_sacrolash;
        creators["twins tank alythess"] =
            &RaidSunwellPlateauActionContext::twins_tank_alythess;
        creators["twins blaze footwork"] =
            &RaidSunwellPlateauActionContext::twins_blaze_footwork;
        creators["twins seek shadow blades"] =
            &RaidSunwellPlateauActionContext::twins_seek_shadow_blades;
        creators["twins dispel pyrogenics"] =
            &RaidSunwellPlateauActionContext::twins_dispel_pyrogenics;
        creators["twins relief taunt sacrolash"] =
            &RaidSunwellPlateauActionContext::twins_relief_taunt_sacrolash;
        creators["twins pull sacrolash back"] =
            &RaidSunwellPlateauActionContext::twins_pull_sacrolash_back;
        creators["twins stack on alythess"] =
            &RaidSunwellPlateauActionContext::twins_stack_on_alythess;

        // M'uru / Entropius
        creators["muru clear darkness"] =
            &RaidSunwellPlateauActionContext::muru_clear_darkness;
        creators["muru dispel dark fiend"] =
            &RaidSunwellPlateauActionContext::muru_dispel_dark_fiend;
        creators["muru clear void zone"] =
            &RaidSunwellPlateauActionContext::muru_clear_void_zone;
        creators["muru flee singularity"] =
            &RaidSunwellPlateauActionContext::muru_flee_singularity;
        creators["muru leave shadow pulse"] =
            &RaidSunwellPlateauActionContext::muru_leave_shadow_pulse;
        creators["muru tank add"] =
            &RaidSunwellPlateauActionContext::muru_tank_add;
        creators["muru focus target"] =
            &RaidSunwellPlateauActionContext::muru_focus_target;

        // Kil'jaeden
        creators["kj clear armageddon"] =
            &RaidSunwellPlateauActionContext::kj_clear_armageddon;
        creators["kj quarantine fire bloom"] =
            &RaidSunwellPlateauActionContext::kj_quarantine_fire_bloom;
        creators["kj stack for darkness"] =
            &RaidSunwellPlateauActionContext::kj_stack_for_darkness;
        creators["kj hold anchor"] =
            &RaidSunwellPlateauActionContext::kj_hold_anchor;
        creators["kj hold station"] =
            &RaidSunwellPlateauActionContext::kj_hold_station;
        creators["kj claim orb"] =
            &RaidSunwellPlateauActionContext::kj_claim_orb;
        creators["kj drive drake"] =
            &RaidSunwellPlateauActionContext::kj_drive_drake;
        creators["kj focus target"] =
            &RaidSunwellPlateauActionContext::kj_focus_target;
    }

private:
    static Action* sunwell_erase_timers_and_trackers(
        PlayerbotAI* botAI) { return new SunwellEraseTimersAndTrackersAction(botAI); }
    static Action* kalecgos_melee_flank_boss(
        PlayerbotAI* botAI) { return new KalecgosMeleeFlankBossAction(botAI); }
    static Action* kalecgos_tank_position_boss(
        PlayerbotAI* botAI) { return new KalecgosTankPositionBossAction(botAI); }
    static Action* kalecgos_disperse_ranged(
        PlayerbotAI* botAI) { return new KalecgosDisperseRangedAction(botAI); }
    static Action* kalecgos_enter_spectral_rift(
        PlayerbotAI* botAI) { return new KalecgosEnterSpectralRiftAction(botAI); }
    static Action* kalecgos_return_to_platform(
        PlayerbotAI* botAI) { return new KalecgosReturnToPlatformAction(botAI); }
    static Action* kalecgos_tank_pick_up_sathrovarr(
        PlayerbotAI* botAI) { return new KalecgosTankPickUpSathrovarrAction(botAI); }
    static Action* kalecgos_attack_sathrovarr(
        PlayerbotAI* botAI) { return new KalecgosAttackSathrovarrAction(botAI); }
    static Action* kalecgos_heal_kalec(
        PlayerbotAI* botAI) { return new KalecgosHealKalecAction(botAI); }
    static Action* brutallus_taunt_boss(
        PlayerbotAI* botAI) { return new BrutallusTauntBossAction(botAI); }
    static Action* brutallus_move_to_station(
        PlayerbotAI* botAI) { return new BrutallusMoveToStationAction(botAI); }
    static Action* brutallus_purge_burn(
        PlayerbotAI* botAI) { return new BrutallusPurgeBurnAction(botAI); }
    static Action* felmyst_leave_fog_lane(
        PlayerbotAI* botAI) { return new FelmystLeaveFogLaneAction(botAI); }
    static Action* felmyst_flee_encapsulate(
        PlayerbotAI* botAI) { return new FelmystFleeEncapsulateAction(botAI); }
    static Action* felmyst_attack_charmed_raider(
        PlayerbotAI* botAI) { return new FelmystAttackCharmedRaiderAction(botAI); }
    static Action* felmyst_mass_dispel_gas_nova(
        PlayerbotAI* botAI) { return new FelmystMassDispelGasNovaAction(botAI); }
    static Action* felmyst_kite_vapor(
        PlayerbotAI* botAI) { return new FelmystKiteVaporAction(botAI); }
    static Action* felmyst_regroup(
        PlayerbotAI* botAI) { return new FelmystRegroupAction(botAI); }
    static Action* twins_flee_conflagration(
        PlayerbotAI* botAI) { return new TwinsFleeConflagrationAction(botAI); }
    static Action* twins_clear_conflagration(
        PlayerbotAI* botAI) { return new TwinsClearConflagrationAction(botAI); }
    static Action* twins_focus_sacrolash(
        PlayerbotAI* botAI) { return new TwinsFocusSacrolashAction(botAI); }
    static Action* twins_tank_alythess(
        PlayerbotAI* botAI) { return new TwinsTankAlythessAction(botAI); }
    static Action* twins_blaze_footwork(
        PlayerbotAI* botAI) { return new TwinsBlazeFootworkAction(botAI); }
    static Action* twins_seek_shadow_blades(
        PlayerbotAI* botAI) { return new TwinsSeekShadowBladesAction(botAI); }
    static Action* twins_dispel_pyrogenics(
        PlayerbotAI* botAI) { return new TwinsDispelPyrogenicsAction(botAI); }
    static Action* twins_relief_taunt_sacrolash(
        PlayerbotAI* botAI) { return new TwinsReliefTauntSacrolashAction(botAI); }
    static Action* twins_pull_sacrolash_back(
        PlayerbotAI* botAI) { return new TwinsPullSacrolashBackAction(botAI); }
    static Action* twins_stack_on_alythess(
        PlayerbotAI* botAI) { return new TwinsStackOnAlythessAction(botAI); }
    static Action* muru_clear_darkness(
        PlayerbotAI* botAI) { return new MuruClearDarknessAction(botAI); }
    static Action* muru_dispel_dark_fiend(
        PlayerbotAI* botAI) { return new MuruDispelDarkFiendAction(botAI); }
    static Action* muru_clear_void_zone(
        PlayerbotAI* botAI) { return new MuruClearVoidZoneAction(botAI); }
    static Action* muru_flee_singularity(
        PlayerbotAI* botAI) { return new MuruFleeSingularityAction(botAI); }
    static Action* muru_leave_shadow_pulse(
        PlayerbotAI* botAI) { return new MuruLeaveShadowPulseAction(botAI); }
    static Action* muru_tank_add(
        PlayerbotAI* botAI) { return new MuruTankAddAction(botAI); }
    static Action* muru_focus_target(
        PlayerbotAI* botAI) { return new MuruFocusTargetAction(botAI); }
    static Action* kj_clear_armageddon(
        PlayerbotAI* botAI) { return new KjClearArmageddonAction(botAI); }
    static Action* kj_quarantine_fire_bloom(
        PlayerbotAI* botAI) { return new KjQuarantineFireBloomAction(botAI); }
    static Action* kj_stack_for_darkness(
        PlayerbotAI* botAI) { return new KjStackForDarknessAction(botAI); }
    static Action* kj_hold_anchor(
        PlayerbotAI* botAI) { return new KjHoldAnchorAction(botAI); }
    static Action* kj_hold_station(
        PlayerbotAI* botAI) { return new KjHoldStationAction(botAI); }
    static Action* kj_claim_orb(
        PlayerbotAI* botAI) { return new KjClaimOrbAction(botAI); }
    static Action* kj_drive_drake(
        PlayerbotAI* botAI) { return new KjDriveDrakeAction(botAI); }
    static Action* kj_focus_target(
        PlayerbotAI* botAI) { return new KjFocusTargetAction(botAI); }
};

#endif
