/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDTRIGGERCONTEXT_H
#define PLAYERBOTS_ULDTRIGGERCONTEXT_H

#include "BossAuraTriggers.h"
#include "NamedObjectContext.h"
#include "UldExtraTriggers.h"
#include "UldTriggers.h"

class RaidUlduarTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidUlduarTriggerContext()
    {
        creators["flame leviathan on vehicle"] = &RaidUlduarTriggerContext::flame_leviathan_on_vehicle;
        creators["flame leviathan vehicle near"] = &RaidUlduarTriggerContext::flame_leviathan_vehicle_near;
        creators["razorscale flying alone"] = &RaidUlduarTriggerContext::razorscale_flying_alone;
        creators["razorscale avoid devouring flames"] = &RaidUlduarTriggerContext::razorscale_avoid_devouring_flames;
        creators["razorscale avoid sentinel"] = &RaidUlduarTriggerContext::razorscale_avoid_sentinel;
        creators["razorscale avoid whirlwind"] = &RaidUlduarTriggerContext::razorscale_avoid_whirlwind;
        creators["razorscale grounded"] = &RaidUlduarTriggerContext::razorscale_grounded;
        creators["razorscale harpoon trigger"] = &RaidUlduarTriggerContext::razorscale_harpoon_trigger;
        creators["razorscale fuse armor trigger"] = &RaidUlduarTriggerContext::razorscale_fuse_armor_trigger;
        creators["razorscale fire resistance trigger"] = &RaidUlduarTriggerContext::razorscale_fire_resistance_trigger;
        creators["ignis fire resistance trigger"] = &RaidUlduarTriggerContext::ignis_fire_resistance_trigger;
        creators["iron assembly lightning tendrils trigger"] = &RaidUlduarTriggerContext::iron_assembly_lightning_tendrils_trigger;
        creators["iron assembly overload trigger"] = &RaidUlduarTriggerContext::iron_assembly_overload_trigger;
        creators["iron assembly rune of power trigger"] = &RaidUlduarTriggerContext::iron_assembly_rune_of_power_trigger;
        creators["kologarn mark dps target trigger"] = &RaidUlduarTriggerContext::kologarn_mark_dps_target_trigger;
        creators["kologarn fall from floor trigger"] = &RaidUlduarTriggerContext::kologarn_fall_from_floor_trigger;
        creators["kologarn nature resistance trigger"] = &RaidUlduarTriggerContext::kologarn_nature_resistance_trigger;
        creators["kologarn rubble slowdown trigger"] = &RaidUlduarTriggerContext::kologarn_rubble_slowdown_trigger;
        creators["kologarn eyebeam trigger"] = &RaidUlduarTriggerContext::kologarn_eyebeam_trigger;
        creators["kologarn rti target trigger"] = &RaidUlduarTriggerContext::kologarn_rti_target_trigger;
        creators["kologarn crunch armor trigger"] = &RaidUlduarTriggerContext::kologarn_crunch_armor_trigger;
        creators["kologarn attack dps target trigger"] = &RaidUlduarTriggerContext::kologarn_attack_dps_target_trigger;
        creators["auriaya fall from floor trigger"] = &RaidUlduarTriggerContext::auriaya_fall_from_floor_trigger;
        creators["hodir biting cold"] = &RaidUlduarTriggerContext::hodir_biting_cold;
        creators["hodir near snowpacked icicle"] = &RaidUlduarTriggerContext::hodir_near_snowpacked_icicle;
        creators["hodir frost resistance trigger"] = &RaidUlduarTriggerContext::hodir_frost_resistance_trigger;
        creators["freya near nature bomb"] = &RaidUlduarTriggerContext::freya_near_nature_bomb;
        creators["freya fire resistance trigger"] = &RaidUlduarTriggerContext::freya_fire_resistance_trigger;
        creators["freya nature resistance trigger"] = &RaidUlduarTriggerContext::freya_nature_resistance_trigger;
        creators["freya mark dps target trigger"] = &RaidUlduarTriggerContext::freya_mark_dps_target_trigger;
        creators["freya move to healing spore trigger"] = &RaidUlduarTriggerContext::freya_move_to_healing_spore_trigger;
        creators["thorim frost resistance trigger"] = &RaidUlduarTriggerContext::thorim_frost_resistance_trigger;
        creators["thorim nature resistance trigger"] = &RaidUlduarTriggerContext::thorim_nature_resistance_trigger;
        creators["thorim unbalancing strike trigger"] = &RaidUlduarTriggerContext::thorim_unbalancing_strike_trigger;
        creators["thorim mark dps target trigger"] = &RaidUlduarTriggerContext::thorim_mark_dps_target_trigger;
        creators["thorim arena positioning trigger"] = &RaidUlduarTriggerContext::thorim_arena_positioning_trigger;
        creators["thorim gauntlet positioning trigger"] = &RaidUlduarTriggerContext::thorim_gauntlet_positioning_trigger;
        creators["thorim fall from floor trigger"] = &RaidUlduarTriggerContext::thorim_fall_from_floor_trigger;
        creators["thorim phase 2 positioning trigger"] = &RaidUlduarTriggerContext::thorim_phase2_positioning_trigger;
        creators["mimiron fire resistance trigger"] = &RaidUlduarTriggerContext::mimiron_fire_resistance_trigger;
        creators["mimiron shock blast trigger"] = &RaidUlduarTriggerContext::mimiron_shock_blast_trigger;
        creators["mimiron phase 1 positioning trigger"] = &RaidUlduarTriggerContext::mimiron_phase_1_positioning_trigger;
        creators["mimiron p3wx2 laser barrage trigger"] = &RaidUlduarTriggerContext::mimiron_p3wx2_laser_barrage_trigger;
        creators["mimiron rapid burst trigger"] = &RaidUlduarTriggerContext::mimiron_rapid_burst_trigger;
        creators["mimiron aerial command unit trigger"] = &RaidUlduarTriggerContext::mimiron_aerial_command_unit_trigger;
        creators["mimiron rocket strike trigger"] = &RaidUlduarTriggerContext::mimiron_rocket_strike_trigger;
        creators["mimiron phase 4 mark dps trigger"] = &RaidUlduarTriggerContext::mimiron_phase_4_mark_dps_trigger;
        creators["mimiron cheat trigger"] = &RaidUlduarTriggerContext::mimiron_cheat_trigger;
        creators["vezax cheat trigger"] = &RaidUlduarTriggerContext::vezax_cheat_trigger;
        creators["vezax shadow crash trigger"] = &RaidUlduarTriggerContext::vezax_shadow_crash_trigger;
        creators["vezax mark of the faceless trigger"] = &RaidUlduarTriggerContext::vezax_mark_of_the_faceless_trigger;
        creators["vezax shadow resistance trigger"] = &RaidUlduarTriggerContext::vezax_shadow_resistance_trigger;
        creators["sara shadow resistance trigger"] = &RaidUlduarTriggerContext::sara_shadow_resistance_trigger;
        creators["yogg-saron shadow resistance trigger"] = &RaidUlduarTriggerContext::yogg_saron_shadow_resistance_trigger;
        creators["yogg-saron ominous cloud cheat trigger"] = &RaidUlduarTriggerContext::yogg_saron_ominous_cloud_cheat_trigger;
        creators["yogg-saron guardian positioning trigger"] = &RaidUlduarTriggerContext::yogg_saron_guardian_positioning_trigger;
        creators["yogg-saron sanity trigger"] = &RaidUlduarTriggerContext::yogg_saron_sanity_trigger;
        creators["yogg-saron death orb trigger"] = &RaidUlduarTriggerContext::yogg_saron_death_orb_trigger;
        creators["yogg-saron malady of the mind trigger"] = &RaidUlduarTriggerContext::yogg_saron_malady_of_the_mind_trigger;
        creators["yogg-saron mark target trigger"] = &RaidUlduarTriggerContext::yogg_saron_mark_target_trigger;
        creators["yogg-saron brain link trigger"] = &RaidUlduarTriggerContext::yogg_saron_brain_link_trigger;
        creators["yogg-saron move to enter portal trigger"] = &RaidUlduarTriggerContext::yogg_saron_move_to_enter_portal_trigger;
        creators["yogg-saron use portal trigger"] = &RaidUlduarTriggerContext::yogg_saron_use_portal_trigger;
        creators["uld kill order mark"] = &RaidUlduarTriggerContext::uld_kill_order_mark;
        creators["uld off-tank add"] = &RaidUlduarTriggerContext::uld_off_tank_add;
        creators["uld ignis construct"] = &RaidUlduarTriggerContext::uld_ignis_construct;
        creators["uld razorscale melee"] = &RaidUlduarTriggerContext::uld_razorscale_melee;
        creators["uld yogg ominous cloud"] = &RaidUlduarTriggerContext::uld_yogg_ominous_cloud;
        creators["uld mimiron proximity mine"] = &RaidUlduarTriggerContext::uld_mimiron_proximity_mine;
        creators["uld freya iron roots"] = &RaidUlduarTriggerContext::uld_freya_iron_roots;
        creators["uld freya nature's fury"] = &RaidUlduarTriggerContext::uld_freya_natures_fury;
        creators["uld freya sun beam"] = &RaidUlduarTriggerContext::uld_freya_sun_beam;
        creators["uld hodir toasty fire"] = &RaidUlduarTriggerContext::uld_hodir_toasty_fire;
        creators["uld hodir ice block"] = &RaidUlduarTriggerContext::uld_hodir_ice_block;
        creators["uld hodir biting cold"] = &RaidUlduarTriggerContext::uld_hodir_biting_cold;
        creators["uld thorim unbalancing strike"] = &RaidUlduarTriggerContext::uld_thorim_unbalancing_strike;
        creators["uld hodir frozen blows"] = &RaidUlduarTriggerContext::uld_hodir_frozen_blows;
        creators["uld kologarn crunch armor"] = &RaidUlduarTriggerContext::uld_kologarn_crunch_armor;
        creators["uld ignis melee"] = &RaidUlduarTriggerContext::uld_ignis_melee;
        creators["uld xt debuff"] = &RaidUlduarTriggerContext::uld_xt_debuff;
        creators["uld xt bot add"] = &RaidUlduarTriggerContext::uld_xt_bot_add;
        creators["uld xt void zone"] = &RaidUlduarTriggerContext::uld_xt_void_zone;
        creators["uld thorim sif frost nova"] = &RaidUlduarTriggerContext::uld_thorim_sif_frost_nova;
        creators["uld thorim sif blizzard"] = &RaidUlduarTriggerContext::uld_thorim_sif_blizzard;
        creators["uld mimiron flames"] = &RaidUlduarTriggerContext::uld_mimiron_flames;
        creators["uld mimiron frost bomb"] = &RaidUlduarTriggerContext::uld_mimiron_frost_bomb;
        creators["uld vezax saronite vapors"] = &RaidUlduarTriggerContext::uld_vezax_saronite_vapors;
        creators["uld auriaya stack"] = &RaidUlduarTriggerContext::uld_auriaya_stack;
        creators["uld interrupt"] = &RaidUlduarTriggerContext::uld_interrupt;
        creators["uld council tank"] = &RaidUlduarTriggerContext::uld_council_tank;
        creators["uld algalon phase punch"] = &RaidUlduarTriggerContext::uld_algalon_phase_punch;
        creators["uld algalon big bang"] = &RaidUlduarTriggerContext::uld_algalon_big_bang;
        creators["uld algalon star"] = &RaidUlduarTriggerContext::uld_algalon_star;
        creators["yogg-saron fall from floor trigger"] = &RaidUlduarTriggerContext::yogg_saron_fall_from_floor_trigger;
        creators["yogg-saron boss room movement cheat trigger"] = &RaidUlduarTriggerContext::yogg_saron_boss_room_movement_cheat_trigger;
        creators["yogg-saron illusion room trigger"] = &RaidUlduarTriggerContext::yogg_saron_illusion_room_trigger;
        creators["yogg-saron move to exit portal trigger"] = &RaidUlduarTriggerContext::yogg_saron_move_to_exit_portal_trigger;
        creators["yogg-saron lunatic gaze trigger"] = &RaidUlduarTriggerContext::yogg_saron_lunatic_gaze_trigger;
        creators["yogg-saron phase 3 positioning trigger"] = &RaidUlduarTriggerContext::yogg_saron_phase_3_positioning_trigger;
    }

private:
    static Trigger* flame_leviathan_on_vehicle(PlayerbotAI* botAI) { return new FlameLeviathanOnVehicleTrigger(botAI); }
    static Trigger* flame_leviathan_vehicle_near(PlayerbotAI* botAI) { return new FlameLeviathanVehicleNearTrigger(botAI); }
    static Trigger* razorscale_flying_alone(PlayerbotAI* botAI) { return new RazorscaleFlyingAloneTrigger(botAI); }
    static Trigger* razorscale_avoid_devouring_flames(PlayerbotAI* botAI) { return new RazorscaleDevouringFlamesTrigger(botAI); }
    static Trigger* razorscale_avoid_sentinel(PlayerbotAI* botAI) { return new RazorscaleAvoidSentinelTrigger(botAI); }
    static Trigger* razorscale_avoid_whirlwind(PlayerbotAI* botAI) { return new RazorscaleAvoidWhirlwindTrigger(botAI); }
    static Trigger* razorscale_grounded(PlayerbotAI* botAI) { return new RazorscaleGroundedTrigger(botAI); }
    static Trigger* razorscale_harpoon_trigger(PlayerbotAI* botAI) { return new RazorscaleHarpoonAvailableTrigger(botAI); }
    static Trigger* razorscale_fuse_armor_trigger(PlayerbotAI* botAI) { return new RazorscaleFuseArmorTrigger(botAI); }
    static Trigger* razorscale_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "razorscale"); }
    static Trigger* ignis_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "ignis the furnace master"); }
    static Trigger* iron_assembly_lightning_tendrils_trigger(PlayerbotAI* botAI) { return new IronAssemblyLightningTendrilsTrigger(botAI); }
    static Trigger* iron_assembly_overload_trigger(PlayerbotAI* botAI) { return new IronAssemblyOverloadTrigger(botAI); }
    static Trigger* iron_assembly_rune_of_power_trigger(PlayerbotAI* botAI) { return new IronAssemblyRuneOfPowerTrigger(botAI); }
    static Trigger* kologarn_mark_dps_target_trigger(PlayerbotAI* botAI) { return new KologarnMarkDpsTargetTrigger(botAI); }
    static Trigger* kologarn_fall_from_floor_trigger(PlayerbotAI* botAI) { return new KologarnFallFromFloorTrigger(botAI); }
    static Trigger* kologarn_nature_resistance_trigger(PlayerbotAI* botAI) { return new BossNatureResistanceTrigger(botAI, "kologarn"); }
    static Trigger* kologarn_rubble_slowdown_trigger(PlayerbotAI* botAI) { return new KologarnRubbleSlowdownTrigger(botAI); }
    static Trigger* kologarn_eyebeam_trigger(PlayerbotAI* botAI) { return new KologarnEyebeamTrigger(botAI); }
    static Trigger* kologarn_rti_target_trigger(PlayerbotAI* botAI) { return new KologarnRtiTargetTrigger(botAI); }
    static Trigger* kologarn_crunch_armor_trigger(PlayerbotAI* botAI) { return new KologarnCrunchArmorTrigger(botAI); }
    static Trigger* kologarn_attack_dps_target_trigger(PlayerbotAI* botAI) { return new KologarnAttackDpsTargetTrigger(botAI); }
    static Trigger* auriaya_fall_from_floor_trigger(PlayerbotAI* botAI) { return new AuriayaFallFromFloorTrigger(botAI); }
    static Trigger* hodir_biting_cold(PlayerbotAI* botAI) { return new HodirBitingColdTrigger(botAI); }
    static Trigger* hodir_near_snowpacked_icicle(PlayerbotAI* botAI) { return new HodirNearSnowpackedIcicleTrigger(botAI); }
    static Trigger* hodir_frost_resistance_trigger(PlayerbotAI* botAI) { return new BossFrostResistanceTrigger(botAI, "hodir"); }
    static Trigger* freya_near_nature_bomb(PlayerbotAI* botAI) { return new FreyaNearNatureBombTrigger(botAI); }
    static Trigger* freya_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "freya"); }
    static Trigger* freya_nature_resistance_trigger(PlayerbotAI* botAI) { return new BossNatureResistanceTrigger(botAI, "freya"); }
    static Trigger* freya_mark_dps_target_trigger(PlayerbotAI* botAI) { return new FreyaMarkDpsTargetTrigger(botAI); }
    static Trigger* freya_move_to_healing_spore_trigger(PlayerbotAI* botAI) { return new FreyaMoveToHealingSporeTrigger(botAI); }
    static Trigger* thorim_frost_resistance_trigger(PlayerbotAI* botAI) { return new BossFrostResistanceTrigger(botAI, "thorim"); }
    static Trigger* thorim_nature_resistance_trigger(PlayerbotAI* botAI) { return new BossNatureResistanceTrigger(botAI, "thorim"); }
    static Trigger* thorim_unbalancing_strike_trigger(PlayerbotAI* botAI) { return new ThorimUnbalancingStrikeTrigger(botAI); }
    static Trigger* thorim_mark_dps_target_trigger(PlayerbotAI* botAI) { return new ThorimMarkDpsTargetTrigger(botAI); }
    static Trigger* thorim_arena_positioning_trigger(PlayerbotAI* botAI) { return new ThorimArenaPositioningTrigger(botAI); }
    static Trigger* thorim_gauntlet_positioning_trigger(PlayerbotAI* botAI) { return new ThorimGauntletPositioningTrigger(botAI); }
    static Trigger* thorim_fall_from_floor_trigger(PlayerbotAI* botAI) { return new ThorimFallFromFloorTrigger(botAI); }
    static Trigger* thorim_phase2_positioning_trigger(PlayerbotAI* botAI) { return new ThorimPhase2PositioningTrigger(botAI); }
    static Trigger* mimiron_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "mimiron"); }
    static Trigger* mimiron_shock_blast_trigger(PlayerbotAI* botAI) { return new MimironShockBlastTrigger(botAI); }
    static Trigger* mimiron_phase_1_positioning_trigger(PlayerbotAI* botAI) { return new MimironPhase1PositioningTrigger(botAI); }
    static Trigger* mimiron_p3wx2_laser_barrage_trigger(PlayerbotAI* botAI) { return new MimironP3Wx2LaserBarrageTrigger(botAI); }
    static Trigger* mimiron_rapid_burst_trigger(PlayerbotAI* botAI) { return new MimironRapidBurstTrigger(botAI); }
    static Trigger* mimiron_aerial_command_unit_trigger(PlayerbotAI* botAI) { return new MimironAerialCommandUnitTrigger(botAI); }
    static Trigger* mimiron_rocket_strike_trigger(PlayerbotAI* botAI) { return new MimironRocketStrikeTrigger(botAI); }
    static Trigger* mimiron_phase_4_mark_dps_trigger(PlayerbotAI* botAI) { return new MimironPhase4MarkDpsTrigger(botAI); }
    static Trigger* mimiron_cheat_trigger(PlayerbotAI* botAI) { return new MimironCheatTrigger(botAI); }
    static Trigger* vezax_cheat_trigger(PlayerbotAI* botAI) { return new VezaxCheatTrigger(botAI); }
    static Trigger* vezax_shadow_crash_trigger(PlayerbotAI* botAI) { return new VezaxShadowCrashTrigger(botAI); }
    static Trigger* vezax_shadow_resistance_trigger(PlayerbotAI* botAI) { return new BossShadowResistanceTrigger(botAI, "general vezax"); }
    static Trigger* sara_shadow_resistance_trigger(PlayerbotAI* botAI) { return new BossShadowResistanceTrigger(botAI, "sara"); }
    static Trigger* yogg_saron_shadow_resistance_trigger(PlayerbotAI* botAI) { return new BossShadowResistanceTrigger(botAI, "yogg-saron"); }
    static Trigger* vezax_mark_of_the_faceless_trigger(PlayerbotAI* botAI) { return new VezaxMarkOfTheFacelessTrigger(botAI); }
    static Trigger* yogg_saron_ominous_cloud_cheat_trigger(PlayerbotAI* botAI) { return new YoggSaronOminousCloudCheatTrigger(botAI); }
    static Trigger* yogg_saron_guardian_positioning_trigger(PlayerbotAI* botAI) { return new YoggSaronGuardianPositioningTrigger(botAI); }
    static Trigger* yogg_saron_sanity_trigger(PlayerbotAI* botAI) { return new YoggSaronSanityTrigger(botAI); }
    static Trigger* yogg_saron_death_orb_trigger(PlayerbotAI* botAI) { return new YoggSaronDeathOrbTrigger(botAI); }
    static Trigger* yogg_saron_malady_of_the_mind_trigger(PlayerbotAI* botAI) { return new YoggSaronMaladyOfTheMindTrigger(botAI); }
    static Trigger* yogg_saron_mark_target_trigger(PlayerbotAI* botAI) { return new YoggSaronMarkTargetTrigger(botAI); }
    static Trigger* yogg_saron_brain_link_trigger(PlayerbotAI* botAI) { return new YoggSaronBrainLinkTrigger(botAI); }
    static Trigger* yogg_saron_move_to_enter_portal_trigger(PlayerbotAI* botAI) { return new YoggSaronMoveToEnterPortalTrigger(botAI); }
    static Trigger* yogg_saron_use_portal_trigger(PlayerbotAI* botAI) { return new YoggSaronUsePortalTrigger(botAI); }
    static Trigger* uld_kill_order_mark(PlayerbotAI* botAI) { return new UldKillOrderMarkTrigger(botAI); }
    static Trigger* uld_off_tank_add(PlayerbotAI* botAI) { return new UldOffTankAddTrigger(botAI); }
    static Trigger* uld_ignis_construct(PlayerbotAI* botAI) { return new UldIgnisConstructTrigger(botAI); }
    static Trigger* uld_razorscale_melee(PlayerbotAI* botAI) { return new UldRazorscaleMeleeTrigger(botAI); }
    static Trigger* uld_yogg_ominous_cloud(PlayerbotAI* botAI) { return new UldYoggOminousCloudTrigger(botAI); }
    static Trigger* uld_mimiron_proximity_mine(PlayerbotAI* botAI)
    {
        return new UldMimironProximityMineTrigger(botAI);
    }
    static Trigger* uld_freya_iron_roots(PlayerbotAI* botAI) { return new UldFreyaIronRootsTrigger(botAI); }
    static Trigger* uld_freya_natures_fury(PlayerbotAI* botAI) { return new UldFreyaNaturesFuryTrigger(botAI); }
    static Trigger* uld_freya_sun_beam(PlayerbotAI* botAI) { return new UldFreyaSunBeamTrigger(botAI); }
    static Trigger* uld_hodir_toasty_fire(PlayerbotAI* botAI) { return new UldHodirToastyFireTrigger(botAI); }
    static Trigger* uld_hodir_ice_block(PlayerbotAI* botAI) { return new UldHodirIceBlockTrigger(botAI); }
    static Trigger* uld_hodir_biting_cold(PlayerbotAI* botAI) { return new UldHodirBitingColdTrigger(botAI); }
    static Trigger* uld_thorim_unbalancing_strike(PlayerbotAI* botAI)
    {
        return new UldThorimUnbalancingStrikeTrigger(botAI);
    }
    static Trigger* uld_hodir_frozen_blows(PlayerbotAI* botAI) { return new UldHodirFrozenBlowsTrigger(botAI); }
    static Trigger* uld_kologarn_crunch_armor(PlayerbotAI* botAI)
    {
        return new UldKologarnCrunchArmorTrigger(botAI);
    }
    static Trigger* uld_ignis_melee(PlayerbotAI* botAI) { return new UldIgnisMeleeTrigger(botAI); }
    static Trigger* uld_xt_debuff(PlayerbotAI* botAI) { return new UldXtDebuffTrigger(botAI); }
    static Trigger* uld_xt_bot_add(PlayerbotAI* botAI) { return new UldXtBotAddTrigger(botAI); }
    static Trigger* uld_xt_void_zone(PlayerbotAI* botAI) { return new UldXtVoidZoneTrigger(botAI); }
    static Trigger* uld_thorim_sif_frost_nova(PlayerbotAI* botAI) { return new UldThorimSifFrostNovaTrigger(botAI); }
    static Trigger* uld_thorim_sif_blizzard(PlayerbotAI* botAI) { return new UldThorimSifBlizzardTrigger(botAI); }
    static Trigger* uld_mimiron_flames(PlayerbotAI* botAI) { return new UldMimironFlamesTrigger(botAI); }
    static Trigger* uld_mimiron_frost_bomb(PlayerbotAI* botAI) { return new UldMimironFrostBombTrigger(botAI); }
    static Trigger* uld_vezax_saronite_vapors(PlayerbotAI* botAI)
    {
        return new UldVezaxSaroniteVaporsTrigger(botAI);
    }
    static Trigger* uld_auriaya_stack(PlayerbotAI* botAI) { return new UldAuriayaStackTrigger(botAI); }
    static Trigger* uld_interrupt(PlayerbotAI* botAI) { return new UldInterruptTrigger(botAI); }
    static Trigger* uld_council_tank(PlayerbotAI* botAI) { return new UldCouncilTankTrigger(botAI); }
    static Trigger* uld_algalon_phase_punch(PlayerbotAI* botAI) { return new UldAlgalonPhasePunchTrigger(botAI); }
    static Trigger* uld_algalon_big_bang(PlayerbotAI* botAI) { return new UldAlgalonBigBangTrigger(botAI); }
    static Trigger* uld_algalon_star(PlayerbotAI* botAI) { return new UldAlgalonStarTrigger(botAI); }
    static Trigger* yogg_saron_fall_from_floor_trigger(PlayerbotAI* botAI) { return new YoggSaronFallFromFloorTrigger(botAI); }
    static Trigger* yogg_saron_boss_room_movement_cheat_trigger(PlayerbotAI* botAI) { return new YoggSaronBossRoomMovementCheatTrigger(botAI); }
    static Trigger* yogg_saron_illusion_room_trigger(PlayerbotAI* botAI) { return new YoggSaronIllusionRoomTrigger(botAI); }
    static Trigger* yogg_saron_move_to_exit_portal_trigger(PlayerbotAI* botAI) { return new YoggSaronMoveToExitPortalTrigger(botAI); }
    static Trigger* yogg_saron_lunatic_gaze_trigger(PlayerbotAI* botAI) { return new YoggSaronLunaticGazeTrigger(botAI); }
    static Trigger* yogg_saron_phase_3_positioning_trigger(PlayerbotAI* botAI) { return new YoggSaronPhase3PositioningTrigger(botAI); }
};

#endif
