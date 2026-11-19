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
        // General
        creators["sunwell bot is not in combat"] =
            &RaidSunwellPlateauTriggerContext::sunwell_bot_is_not_in_combat;

        // Kalecgos
        creators["kalecgos melee in breath or tail arc"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_melee_in_breath_or_tail_arc;
        creators["kalecgos tank position boss"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_tank_position_boss;
        creators["kalecgos ranged clumped"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_ranged_clumped;
        creators["kalecgos spectral realm needs tank"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_spectral_realm_needs_tank;
        creators["kalecgos spectral realm needs healer"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_spectral_realm_needs_healer;
        creators["kalecgos spectral realm needs dps"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_spectral_realm_needs_dps;
        creators["kalecgos stray off platform"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_stray_off_platform;
        creators["kalecgos sathrovarr loose in spectral realm"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_sathrovarr_loose;
        creators["kalecgos spectral dps wrong target"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_spectral_dps_wrong_target;
        creators["kalecgos kalec needs healing"] =
            &RaidSunwellPlateauTriggerContext::kalecgos_kalec_needs_healing;

        // Brutallus
        creators["brutallus out of station"] =
            &RaidSunwellPlateauTriggerContext::brutallus_out_of_station;
        creators["brutallus tank swap"] =
            &RaidSunwellPlateauTriggerContext::brutallus_tank_swap;
        creators["brutallus burning"] =
            &RaidSunwellPlateauTriggerContext::brutallus_burning;
        creators["brutallus can purge burn"] =
            &RaidSunwellPlateauTriggerContext::brutallus_can_purge_burn;

        // Felmyst
        creators["felmyst in fog lane"] =
            &RaidSunwellPlateauTriggerContext::felmyst_in_fog_lane;
        creators["felmyst near encapsulate"] =
            &RaidSunwellPlateauTriggerContext::felmyst_near_encapsulate;
        creators["felmyst charmed raider loose"] =
            &RaidSunwellPlateauTriggerContext::felmyst_charmed_raider_loose;
        creators["felmyst gas nova dispellable"] =
            &RaidSunwellPlateauTriggerContext::felmyst_gas_nova_dispellable;
        creators["felmyst chased by vapor"] =
            &RaidSunwellPlateauTriggerContext::felmyst_chased_by_vapor;
        creators["felmyst stray from raid"] =
            &RaidSunwellPlateauTriggerContext::felmyst_stray_from_raid;

        // Eredar Twins
        creators["twins conflagration on me"] =
            &RaidSunwellPlateauTriggerContext::twins_conflagration_on_me;
        creators["twins near conflagration"] =
            &RaidSunwellPlateauTriggerContext::twins_near_conflagration;
        creators["twins wrong dps target"] =
            &RaidSunwellPlateauTriggerContext::twins_wrong_dps_target;
        creators["twins alythess untanked"] =
            &RaidSunwellPlateauTriggerContext::twins_alythess_untanked;
        creators["twins blaze footwork"] =
            &RaidSunwellPlateauTriggerContext::twins_blaze_footwork;
        creators["twins needs shadow cleanse"] =
            &RaidSunwellPlateauTriggerContext::twins_needs_shadow_cleanse;
        creators["twins pyrogenics up"] =
            &RaidSunwellPlateauTriggerContext::twins_pyrogenics_up;
        creators["twins cotank confounded"] =
            &RaidSunwellPlateauTriggerContext::twins_cotank_confounded;
        creators["twins sacrolash overextended"] =
            &RaidSunwellPlateauTriggerContext::twins_sacrolash_overextended;
        creators["twins out of alythess stack"] =
            &RaidSunwellPlateauTriggerContext::twins_out_of_alythess_stack;

        // M'uru / Entropius
        creators["muru in darkness"] =
            &RaidSunwellPlateauTriggerContext::muru_in_darkness;
        creators["muru dark fiend up"] =
            &RaidSunwellPlateauTriggerContext::muru_dark_fiend_up;
        creators["muru in void zone"] =
            &RaidSunwellPlateauTriggerContext::muru_in_void_zone;
        creators["muru near singularity"] =
            &RaidSunwellPlateauTriggerContext::muru_near_singularity;
        creators["muru in shadow pulse"] =
            &RaidSunwellPlateauTriggerContext::muru_in_shadow_pulse;
        creators["muru add untanked"] =
            &RaidSunwellPlateauTriggerContext::muru_add_untanked;
        creators["muru wrong target"] =
            &RaidSunwellPlateauTriggerContext::muru_wrong_target;

        // Kil'jaeden
        creators["kj in armageddon"] =
            &RaidSunwellPlateauTriggerContext::kj_in_armageddon;
        creators["kj piloting drake"] =
            &RaidSunwellPlateauTriggerContext::kj_piloting_drake;
        creators["kj darkness incoming"] =
            &RaidSunwellPlateauTriggerContext::kj_darkness_incoming;
        creators["kj orb available"] =
            &RaidSunwellPlateauTriggerContext::kj_orb_available;
        creators["kj fire bloom on me"] =
            &RaidSunwellPlateauTriggerContext::kj_fire_bloom_on_me;
        creators["kj boss off anchor"] =
            &RaidSunwellPlateauTriggerContext::kj_boss_off_anchor;
        creators["kj out of station"] =
            &RaidSunwellPlateauTriggerContext::kj_out_of_station;
        creators["kj wrong target"] =
            &RaidSunwellPlateauTriggerContext::kj_wrong_target;
    }

private:
    static Trigger* sunwell_bot_is_not_in_combat(
        PlayerbotAI* botAI) { return new SunwellBotIsNotInCombatTrigger(botAI); }
    static Trigger* kalecgos_melee_in_breath_or_tail_arc(
        PlayerbotAI* botAI) { return new KalecgosMeleeInBreathOrTailArcTrigger(botAI); }
    static Trigger* kalecgos_tank_position_boss(
        PlayerbotAI* botAI) { return new KalecgosTankPositionBossTrigger(botAI); }
    static Trigger* kalecgos_ranged_clumped(
        PlayerbotAI* botAI) { return new KalecgosRangedClumpedTrigger(botAI); }
    static Trigger* kalecgos_spectral_realm_needs_tank(
        PlayerbotAI* botAI) { return new KalecgosSpectralRealmNeedsTankTrigger(botAI); }
    static Trigger* kalecgos_spectral_realm_needs_healer(
        PlayerbotAI* botAI) { return new KalecgosSpectralRealmNeedsHealerTrigger(botAI); }
    static Trigger* kalecgos_spectral_realm_needs_dps(
        PlayerbotAI* botAI) { return new KalecgosSpectralRealmNeedsDpsTrigger(botAI); }
    static Trigger* kalecgos_stray_off_platform(
        PlayerbotAI* botAI) { return new KalecgosStrayOffPlatformTrigger(botAI); }
    static Trigger* kalecgos_sathrovarr_loose(
        PlayerbotAI* botAI) { return new KalecgosSathrovarrLooseTrigger(botAI); }
    static Trigger* kalecgos_spectral_dps_wrong_target(
        PlayerbotAI* botAI) { return new KalecgosSpectralDpsWrongTargetTrigger(botAI); }
    static Trigger* kalecgos_kalec_needs_healing(
        PlayerbotAI* botAI) { return new KalecgosKalecNeedsHealingTrigger(botAI); }
    static Trigger* brutallus_out_of_station(
        PlayerbotAI* botAI) { return new BrutallusOutOfStationTrigger(botAI); }
    static Trigger* brutallus_tank_swap(
        PlayerbotAI* botAI) { return new BrutallusTankSwapTrigger(botAI); }
    static Trigger* brutallus_burning(
        PlayerbotAI* botAI) { return new BrutallusBurningTrigger(botAI); }
    static Trigger* brutallus_can_purge_burn(
        PlayerbotAI* botAI) { return new BrutallusCanPurgeBurnTrigger(botAI); }
    static Trigger* felmyst_in_fog_lane(
        PlayerbotAI* botAI) { return new FelmystInFogLaneTrigger(botAI); }
    static Trigger* felmyst_near_encapsulate(
        PlayerbotAI* botAI) { return new FelmystNearEncapsulateTrigger(botAI); }
    static Trigger* felmyst_charmed_raider_loose(
        PlayerbotAI* botAI) { return new FelmystCharmedRaiderLooseTrigger(botAI); }
    static Trigger* felmyst_gas_nova_dispellable(
        PlayerbotAI* botAI) { return new FelmystGasNovaDispellableTrigger(botAI); }
    static Trigger* felmyst_chased_by_vapor(
        PlayerbotAI* botAI) { return new FelmystChasedByVaporTrigger(botAI); }
    static Trigger* felmyst_stray_from_raid(
        PlayerbotAI* botAI) { return new FelmystStrayFromRaidTrigger(botAI); }
    static Trigger* twins_conflagration_on_me(
        PlayerbotAI* botAI) { return new TwinsConflagrationOnMeTrigger(botAI); }
    static Trigger* twins_near_conflagration(
        PlayerbotAI* botAI) { return new TwinsNearConflagrationTrigger(botAI); }
    static Trigger* twins_wrong_dps_target(
        PlayerbotAI* botAI) { return new TwinsWrongDpsTargetTrigger(botAI); }
    static Trigger* twins_alythess_untanked(
        PlayerbotAI* botAI) { return new TwinsAlythessUntankedTrigger(botAI); }
    static Trigger* twins_blaze_footwork(
        PlayerbotAI* botAI) { return new TwinsBlazeFootworkTrigger(botAI); }
    static Trigger* twins_needs_shadow_cleanse(
        PlayerbotAI* botAI) { return new TwinsNeedsShadowCleanseTrigger(botAI); }
    static Trigger* twins_pyrogenics_up(
        PlayerbotAI* botAI) { return new TwinsPyrogenicsUpTrigger(botAI); }
    static Trigger* twins_cotank_confounded(
        PlayerbotAI* botAI) { return new TwinsCotankConfoundedTrigger(botAI); }
    static Trigger* twins_sacrolash_overextended(
        PlayerbotAI* botAI) { return new TwinsSacrolashOverextendedTrigger(botAI); }
    static Trigger* twins_out_of_alythess_stack(
        PlayerbotAI* botAI) { return new TwinsOutOfAlythessStackTrigger(botAI); }
    static Trigger* muru_in_darkness(
        PlayerbotAI* botAI) { return new MuruInDarknessTrigger(botAI); }
    static Trigger* muru_dark_fiend_up(
        PlayerbotAI* botAI) { return new MuruDarkFiendUpTrigger(botAI); }
    static Trigger* muru_in_void_zone(
        PlayerbotAI* botAI) { return new MuruInVoidZoneTrigger(botAI); }
    static Trigger* muru_near_singularity(
        PlayerbotAI* botAI) { return new MuruNearSingularityTrigger(botAI); }
    static Trigger* muru_in_shadow_pulse(
        PlayerbotAI* botAI) { return new MuruInShadowPulseTrigger(botAI); }
    static Trigger* muru_add_untanked(
        PlayerbotAI* botAI) { return new MuruAddUntankedTrigger(botAI); }
    static Trigger* muru_wrong_target(
        PlayerbotAI* botAI) { return new MuruWrongTargetTrigger(botAI); }
    static Trigger* kj_in_armageddon(
        PlayerbotAI* botAI) { return new KjInArmageddonTrigger(botAI); }
    static Trigger* kj_piloting_drake(
        PlayerbotAI* botAI) { return new KjPilotingDrakeTrigger(botAI); }
    static Trigger* kj_darkness_incoming(
        PlayerbotAI* botAI) { return new KjDarknessIncomingTrigger(botAI); }
    static Trigger* kj_orb_available(
        PlayerbotAI* botAI) { return new KjOrbAvailableTrigger(botAI); }
    static Trigger* kj_fire_bloom_on_me(
        PlayerbotAI* botAI) { return new KjFireBloomOnMeTrigger(botAI); }
    static Trigger* kj_boss_off_anchor(
        PlayerbotAI* botAI) { return new KjBossOffAnchorTrigger(botAI); }
    static Trigger* kj_out_of_station(
        PlayerbotAI* botAI) { return new KjOutOfStationTrigger(botAI); }
    static Trigger* kj_wrong_target(
        PlayerbotAI* botAI) { return new KjWrongTargetTrigger(botAI); }
};

#endif
