/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXACTIONCONTEXT_H
#define PLAYERBOTS_NAXXACTIONCONTEXT_H

#include "../RaidResistanceBuff.h"
#include "Action.h"
#include "NamedObjectContext.h"
#include "NaxxActions.h"

class RaidNaxxActionContext : public NamedObjectContext<Action>
{
public:
    RaidNaxxActionContext()
    {
        creators["raid resistance buff"] = &RaidNaxxActionContext::raid_resistance_buff;
        creators["grobbulus go behind the boss"] = &RaidNaxxActionContext::go_behind_the_boss;
        creators["rotate grobbulus"] = &RaidNaxxActionContext::rotate_grobbulus;
        creators["grobbulus move center"] = &RaidNaxxActionContext::grobbulus_move_center;
        creators["grobbulus move away"] = &RaidNaxxActionContext::grobbulus_move_away;
        creators["grobbulus avoid spray"] = &RaidNaxxActionContext::grobbulus_avoid_spray;
        creators["grobbulus avoid cloud"] = &RaidNaxxActionContext::grobbulus_avoid_cloud;
        creators["grobbulus tank face wall"] = &RaidNaxxActionContext::grobbulus_tank_face_wall;

        creators["heigan dance melee"] = &RaidNaxxActionContext::heigan_dance_melee;
        creators["heigan dance ranged"] = &RaidNaxxActionContext::heigan_dance_ranged;
        creators["thaddius attack nearest pet"] = &RaidNaxxActionContext::thaddius_attack_nearest_pet;
        // creators["thaddius melee to place"] = &RaidNaxxActionContext::thaddius_tank_to_place;
        // creators["thaddius ranged to place"] = &RaidNaxxActionContext::thaddius_ranged_to_place;
        creators["thaddius move to platform"] = &RaidNaxxActionContext::thaddius_move_to_platform;
        creators["thaddius move polarity"] = &RaidNaxxActionContext::thaddius_move_polarity;

        creators["razuvious use obedience crystal"] = &RaidNaxxActionContext::razuvious_use_obedience_crystal;
        creators["razuvious target"] = &RaidNaxxActionContext::razuvious_target;
        creators["razuvious keep shout range"] = &RaidNaxxActionContext::razuvious_keep_shout_range;

        creators["four horsemen attract alternatively"] = &RaidNaxxActionContext::four_horsemen_attract_alternatively;
        creators["four horsemen attack in order"] = &RaidNaxxActionContext::four_horsemen_attack_in_order;
        creators["naxx40 horsemen tank"] = &RaidNaxxActionContext::naxx40_horsemen_tank;
        creators["naxx40 horsemen raid"] = &RaidNaxxActionContext::naxx40_horsemen_raid;
        creators["naxx40 horsemen tank heal"] = &RaidNaxxActionContext::naxx40_horsemen_tank_heal;
        creators["naxx40 horsemen threat hold"] = &RaidNaxxActionContext::naxx40_horsemen_threat_hold;
        creators["naxx40 loatheb consumable"] = &RaidNaxxActionContext::naxx40_loatheb_consumable;
        creators["naxx40 loatheb tank cooldown"] = &RaidNaxxActionContext::naxx40_loatheb_tank_cooldown;
        creators["naxx40 loatheb tank heal"] = &RaidNaxxActionContext::naxx40_loatheb_tank_heal;
        creators["naxx40 loatheb group heal"] = &RaidNaxxActionContext::naxx40_loatheb_group_heal;
        creators["naxx40 loatheb spore"] = &RaidNaxxActionContext::naxx40_loatheb_spore;
        creators["naxx40 loatheb healer dps"] = &RaidNaxxActionContext::naxx40_loatheb_healer_dps;

        creators["sapphiron ground position"] = &RaidNaxxActionContext::sapphiron_ground_position;
        creators["sapphiron flight position"] = &RaidNaxxActionContext::sapphiron_flight_position;

        creators["kel'thuzad choose target"] = &RaidNaxxActionContext::kelthuzad_choose_target;
        creators["kel'thuzad position"] = &RaidNaxxActionContext::kelthuzad_position;

        creators["anub'rekhan choose target"] = &RaidNaxxActionContext::anubrekhan_choose_target;
        creators["anub'rekhan position"] = &RaidNaxxActionContext::anubrekhan_position;
        creators["naxx40 anub spread"] = &RaidNaxxActionContext::naxx40_anub_spread;
        creators["naxx40 patchwerk soaker heal"] = &RaidNaxxActionContext::naxx40_patchwerk_soaker_heal;
        creators["naxx40 patchwerk mt heal"] = &RaidNaxxActionContext::naxx40_patchwerk_mt_heal;
        creators["naxx40 patchwerk threat hold"] = &RaidNaxxActionContext::naxx40_patchwerk_threat_hold;
        creators["naxx40 patchwerk mana refill"] = &RaidNaxxActionContext::naxx40_patchwerk_mana_refill;
        creators["naxx40 patchwerk threat boost"] = &RaidNaxxActionContext::naxx40_patchwerk_threat_boost;

        creators["gluth choose target"] = &RaidNaxxActionContext::gluth_choose_target;
        creators["gluth position"] = &RaidNaxxActionContext::gluth_position;
        creators["gluth slowdown"] = &RaidNaxxActionContext::gluth_slowdown;

        //creators["patchwerk ranged position"] = &RaidNaxxActionContext::patchwerk_ranged_position;

        creators["loatheb position"] = &RaidNaxxActionContext::loatheb_position;
        creators["loatheb choose target"] = &RaidNaxxActionContext::loatheb_choose_target;
        creators["naxx kill order mark"] = &RaidNaxxActionContext::naxx_kill_order_mark;
        creators["naxx faerlina mind control"] = &RaidNaxxActionContext::naxx_faerlina_mind_control;
        creators["naxx faerlina embrace"] = &RaidNaxxActionContext::naxx_faerlina_embrace;
        creators["naxx faerlina bring worshipper"] = &RaidNaxxActionContext::naxx_faerlina_bring_worshipper;
        creators["naxx attack web wrap"] = &RaidNaxxActionContext::naxx_attack_web_wrap;
        creators["naxx noth taunt"] = &RaidNaxxActionContext::naxx_noth_taunt;
        creators["naxx move from plagued champion"] = &RaidNaxxActionContext::naxx_move_from_plagued_champion;
        creators["naxx gothik go to side"] = &RaidNaxxActionContext::naxx_gothik_go_to_side;
        creators["naxx gothik taunt"] = &RaidNaxxActionContext::naxx_gothik_taunt;
        creators["naxx patchwerk soak"] = &RaidNaxxActionContext::naxx_patchwerk_soak;
        creators["naxx thane stack"] = &RaidNaxxActionContext::naxx_thane_stack;
        creators["naxx detonate mana spread"] = &RaidNaxxActionContext::naxx_detonate_mana_spread;
        creators["naxx chains mark"] = &RaidNaxxActionContext::naxx_chains_mark;
    }

private:
    static Action* raid_resistance_buff(PlayerbotAI* ai) { return new RaidResistanceBuffAction(ai); }
    static Action* go_behind_the_boss(PlayerbotAI* ai) { return new GrobbulusGoBehindAction(ai); }
    static Action* rotate_grobbulus(PlayerbotAI* ai) { return new GrobbulusRotateAction(ai); }
    static Action* grobbulus_move_center(PlayerbotAI* ai) { return new GrobbulusMoveCenterAction(ai); }
    static Action* grobbulus_move_away(PlayerbotAI* ai) { return new GrobbulusMoveAwayAction(ai); }
    static Action* grobbulus_avoid_spray(PlayerbotAI* ai) { return new GrobbulusAvoidSprayAction(ai); }
    static Action* grobbulus_avoid_cloud(PlayerbotAI* ai) { return new GrobbulusAvoidCloudAction(ai); }
    static Action* grobbulus_tank_face_wall(PlayerbotAI* ai) { return new GrobbulusTankFaceWallAction(ai); }
    static Action* heigan_dance_melee(PlayerbotAI* ai) { return new HeiganDanceAction(ai, false); }
    static Action* heigan_dance_ranged(PlayerbotAI* ai) { return new HeiganDanceAction(ai, true); }
    static Action* thaddius_attack_nearest_pet(PlayerbotAI* ai) { return new ThaddiusAttackNearestPetAction(ai); }
    // static Action* thaddius_tank_to_place(PlayerbotAI* ai) { return new ThaddiusMeleeToPlaceAction(ai); }
    // static Action* thaddius_ranged_to_place(PlayerbotAI* ai) { return new ThaddiusRangedToPlaceAction(ai); }
    static Action* thaddius_move_to_platform(PlayerbotAI* ai) { return new ThaddiusMoveToPlatformAction(ai); }
    static Action* thaddius_move_polarity(PlayerbotAI* ai) { return new ThaddiusMovePolarityAction(ai); }
    static Action* razuvious_target(PlayerbotAI* ai) { return new RazuviousTargetAction(ai); }
    static Action* razuvious_keep_shout_range(PlayerbotAI* ai) { return new RazuviousKeepShoutRangeAction(ai); }
    static Action* razuvious_use_obedience_crystal(PlayerbotAI* ai)
    {
        return new RazuviousUseObedienceCrystalAction(ai);
    }
    static Action* four_horsemen_attract_alternatively(PlayerbotAI* botAI) { return new FourHorsemenAttractAlternativelyAction(botAI); }
    static Action* four_horsemen_attack_in_order(PlayerbotAI* botAI) { return new FourHorsemenAttackInOrderAction(botAI); }
    static Action* naxx40_horsemen_tank(PlayerbotAI* botAI) { return new Naxx40HorsemenTankAction(botAI); }
    static Action* naxx40_horsemen_raid(PlayerbotAI* botAI) { return new Naxx40HorsemenRaidAction(botAI); }
    static Action* naxx40_horsemen_tank_heal(PlayerbotAI* botAI) { return new Naxx40HorsemenTankHealAction(botAI); }
    static Action* naxx40_horsemen_threat_hold(PlayerbotAI* botAI) { return new Naxx40HorsemenThreatHoldAction(botAI); }
    static Action* naxx40_loatheb_consumable(PlayerbotAI* botAI) { return new Naxx40LoathebConsumableAction(botAI); }
    static Action* naxx40_loatheb_tank_cooldown(PlayerbotAI* botAI) { return new Naxx40LoathebTankCooldownAction(botAI); }
    static Action* naxx40_loatheb_tank_heal(PlayerbotAI* botAI) { return new Naxx40LoathebTankHealAction(botAI); }
    static Action* naxx40_loatheb_group_heal(PlayerbotAI* botAI) { return new Naxx40LoathebGroupHealAction(botAI); }
    static Action* naxx40_loatheb_spore(PlayerbotAI* botAI) { return new Naxx40LoathebSporeAction(botAI); }
    static Action* naxx40_loatheb_healer_dps(PlayerbotAI* botAI) { return new Naxx40LoathebHealerDpsAction(botAI); }
    // static Action* sapphiron_ground_main_tank_position(PlayerbotAI* botAI) { return new
    // SapphironGroundMainTankPositionAction(botAI); }
    static Action* sapphiron_ground_position(PlayerbotAI* botAI) { return new SapphironGroundPositionAction(botAI); }
    static Action* sapphiron_flight_position(PlayerbotAI* botAI) { return new SapphironFlightPositionAction(botAI); }
    // static Action* sapphiron_avoid_chill(PlayerbotAI* botAI) { return new SapphironAvoidChillAction(botAI); }
    static Action* kelthuzad_choose_target(PlayerbotAI* botAI) { return new KelthuzadChooseTargetAction(botAI); }
    static Action* kelthuzad_position(PlayerbotAI* botAI) { return new KelthuzadPositionAction(botAI); }
    static Action* anubrekhan_choose_target(PlayerbotAI* botAI) { return new AnubrekhanChooseTargetAction(botAI); }
    static Action* anubrekhan_position(PlayerbotAI* botAI) { return new AnubrekhanPositionAction(botAI); }
    static Action* naxx40_anub_spread(PlayerbotAI* botAI) { return new Naxx40AnubSpreadAction(botAI); }
    static Action* naxx40_patchwerk_soaker_heal(PlayerbotAI* botAI) { return new Naxx40PatchwerkSoakerHealAction(botAI); }
    static Action* naxx40_patchwerk_mt_heal(PlayerbotAI* botAI) { return new Naxx40PatchwerkMtHealAction(botAI); }
    static Action* naxx40_patchwerk_threat_hold(PlayerbotAI* botAI) { return new Naxx40PatchwerkThreatHoldAction(botAI); }
    static Action* naxx40_patchwerk_mana_refill(PlayerbotAI* botAI) { return new Naxx40PatchwerkManaRefillAction(botAI); }
    static Action* naxx40_patchwerk_threat_boost(PlayerbotAI* botAI) { return new Naxx40PatchwerkThreatBoostAction(botAI); }
    static Action* gluth_choose_target(PlayerbotAI* botAI) { return new GluthChooseTargetAction(botAI); }
    static Action* gluth_position(PlayerbotAI* botAI) { return new GluthPositionAction(botAI); }
    static Action* gluth_slowdown(PlayerbotAI* botAI) { return new GluthSlowdownAction(botAI); }
    //static Action* patchwerk_ranged_position(PlayerbotAI* botAI) { return new PatchwerkRangedPositionAction(botAI); }
    static Action* loatheb_position(PlayerbotAI* botAI) { return new LoathebPositionAction(botAI); }
    static Action* loatheb_choose_target(PlayerbotAI* botAI) { return new LoathebChooseTargetAction(botAI); }
    static Action* naxx_kill_order_mark(PlayerbotAI* botAI) { return new NaxxKillOrderMarkAction(botAI); }
    static Action* naxx_faerlina_mind_control(PlayerbotAI* botAI) { return new NaxxFaerlinaMindControlAction(botAI); }
    static Action* naxx_faerlina_embrace(PlayerbotAI* botAI) { return new NaxxFaerlinaEmbraceAction(botAI); }
    static Action* naxx_faerlina_bring_worshipper(PlayerbotAI* botAI)
    {
        return new NaxxFaerlinaBringWorshipperAction(botAI);
    }
    static Action* naxx_attack_web_wrap(PlayerbotAI* botAI) { return new NaxxAttackWebWrapAction(botAI); }
    static Action* naxx_noth_taunt(PlayerbotAI* botAI)
    {
        return new NaxxTauntBossAction(botAI, "naxx noth taunt", "noth the plaguebringer");
    }
    static Action* naxx_move_from_plagued_champion(PlayerbotAI* botAI)
    {
        return new NaxxMoveFromPlaguedChampionAction(botAI);
    }
    static Action* naxx_gothik_go_to_side(PlayerbotAI* botAI) { return new NaxxGothikGoToSideAction(botAI); }
    static Action* naxx_gothik_taunt(PlayerbotAI* botAI)
    {
        return new NaxxTauntBossAction(botAI, "naxx gothik taunt", "gothik the harvester");
    }
    static Action* naxx_patchwerk_soak(PlayerbotAI* botAI) { return new NaxxPatchwerkSoakAction(botAI); }
    static Action* naxx_thane_stack(PlayerbotAI* botAI) { return new NaxxThaneStackAction(botAI); }
    static Action* naxx_detonate_mana_spread(PlayerbotAI* botAI) { return new NaxxDetonateManaSpreadAction(botAI); }
    static Action* naxx_chains_mark(PlayerbotAI* botAI) { return new NaxxChainsMarkAction(botAI); }
};

#endif
