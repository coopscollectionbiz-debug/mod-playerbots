/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40TRIGGERCONTEXT_H
#define PLAYERBOTS_AQ40TRIGGERCONTEXT_H

#include "Aq40Triggers.h"
#include "ChatCommandTrigger.h"
#include "NamedObjectContext.h"

class RaidAq40TriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidAq40TriggerContext()
    {
        creators["aq40 twins not in combat"] = &RaidAq40TriggerContext::not_in_combat;
        creators["aq40 twins in explode radius"] = &RaidAq40TriggerContext::in_explode_radius;
        creators["aq40 twins in blizzard"] = &RaidAq40TriggerContext::in_blizzard;
        creators["aq40 twins in burst range"] = &RaidAq40TriggerContext::in_burst_range;
        creators["aq40 twins wrong target"] = &RaidAq40TriggerContext::wrong_target;
        creators["aq40 twins in landing zone"] = &RaidAq40TriggerContext::in_landing_zone;
        creators["aq40 twins wrong victim"] = &RaidAq40TriggerContext::wrong_victim;
        creators["aq40 twins boss off station"] = &RaidAq40TriggerContext::boss_off_station;
        creators["aq40 twins off spot"] = &RaidAq40TriggerContext::off_spot;
        creators["aq40 twins warlock tank due"] = &RaidAq40TriggerContext::warlock_tank_due;
        creators["aq40 twins threat hold"] = &RaidAq40TriggerContext::twins_threat_hold;
        creators["aq40 twins pet on wrong twin"] = &RaidAq40TriggerContext::pet_on_wrong_twin;
        creators["aq40 twins form up"] = &RaidAq40TriggerContext::twins_form_up;
        creators["aq40 twins auto pull"] = &RaidAq40TriggerContext::twins_auto_pull;
        // Spec §5.5: the owner's chat command, named exactly "twins pull" (plain "pull" is a stock command).
        creators["twins pull"] = &RaidAq40TriggerContext::twins_pull;
        creators["aq40 twins tank heal"] = &RaidAq40TriggerContext::twins_tank_heal;
        creators["aq40 cthun not in combat"] = &RaidAq40TriggerContext::cthun_not_in_combat;
        creators["aq40 cthun in stomach"] = &RaidAq40TriggerContext::cthun_in_stomach;
        creators["aq40 cthun mounted in place"] = &RaidAq40TriggerContext::cthun_mounted_in_place;
        creators["aq40 cthun stomach airborne"] = &RaidAq40TriggerContext::cthun_stomach_airborne;
        creators["aq40 cthun stomach far from tentacle"] = &RaidAq40TriggerContext::cthun_stomach_far;
        creators["aq40 cthun in glare path"] = &RaidAq40TriggerContext::cthun_in_glare_path;
        creators["aq40 cthun near beam target"] = &RaidAq40TriggerContext::cthun_near_beam_target;
        creators["aq40 cthun opening puller"] = &RaidAq40TriggerContext::cthun_opening_puller;
        creators["aq40 cthun wrong target"] = &RaidAq40TriggerContext::cthun_wrong_target;
        creators["aq40 cthun not formed"] = &RaidAq40TriggerContext::cthun_not_formed;
        creators["aq40 huhuran not in combat"] = &RaidAq40TriggerContext::huhuran_not_in_combat;
        creators["aq40 huhuran mt has work"] = &RaidAq40TriggerContext::huhuran_mt_has_work;
        creators["aq40 huhuran off tank idle"] = &RaidAq40TriggerContext::huhuran_off_tank_idle;
        creators["aq40 huhuran off spot"] = &RaidAq40TriggerContext::huhuran_off_spot;
        creators["aq40 huhuran threat hold"] = &RaidAq40TriggerContext::huhuran_threat_hold;
        creators["aq40 huhuran tank heal"] = &RaidAq40TriggerContext::huhuran_tank_heal;
        creators["aq40 huhuran mt berserk defensive"] = &RaidAq40TriggerContext::huhuran_mt_berserk_defensive;
    }

private:
    static Trigger* not_in_combat(PlayerbotAI* b) { return new Aq40TwinsNotInCombatTrigger(b); }
    static Trigger* in_explode_radius(PlayerbotAI* b) { return new Aq40TwinsInExplodeRadiusTrigger(b); }
    static Trigger* in_blizzard(PlayerbotAI* b) { return new Aq40TwinsInBlizzardTrigger(b); }
    static Trigger* in_burst_range(PlayerbotAI* b) { return new Aq40TwinsInBurstRangeTrigger(b); }
    static Trigger* wrong_target(PlayerbotAI* b) { return new Aq40TwinsWrongTargetTrigger(b); }
    static Trigger* in_landing_zone(PlayerbotAI* b) { return new Aq40TwinsInLandingZoneTrigger(b); }
    static Trigger* wrong_victim(PlayerbotAI* b) { return new Aq40TwinsWrongVictimTrigger(b); }
    static Trigger* boss_off_station(PlayerbotAI* b) { return new Aq40TwinsBossOffStationTrigger(b); }
    static Trigger* off_spot(PlayerbotAI* b) { return new Aq40TwinsOffSpotTrigger(b); }
    static Trigger* warlock_tank_due(PlayerbotAI* b) { return new Aq40TwinsWarlockTankDueTrigger(b); }
    static Trigger* twins_threat_hold(PlayerbotAI* b) { return new Aq40TwinsThreatHoldTrigger(b); }
    static Trigger* pet_on_wrong_twin(PlayerbotAI* b) { return new Aq40TwinsPetOnWrongTwinTrigger(b); }
    static Trigger* twins_form_up(PlayerbotAI* b) { return new Aq40TwinsFormUpTrigger(b); }
    static Trigger* twins_pull(PlayerbotAI* b) { return new ChatCommandTrigger(b, "twins pull"); }
    static Trigger* twins_auto_pull(PlayerbotAI* b) { return new Aq40TwinsAutoPullTrigger(b); }
    static Trigger* twins_tank_heal(PlayerbotAI* b) { return new Aq40TwinsTankHealTrigger(b); }
    static Trigger* cthun_not_in_combat(PlayerbotAI* b) { return new Aq40CthunNotInCombatTrigger(b); }
    static Trigger* cthun_in_stomach(PlayerbotAI* b) { return new Aq40CthunInStomachTrigger(b); }
    static Trigger* cthun_mounted_in_place(PlayerbotAI* b) { return new Aq40CthunMountedInPlaceTrigger(b); }
    static Trigger* cthun_stomach_airborne(PlayerbotAI* b) { return new Aq40CthunStomachAirborneTrigger(b); }
    static Trigger* cthun_stomach_far(PlayerbotAI* b) { return new Aq40CthunStomachFarFromTentacleTrigger(b); }
    static Trigger* cthun_in_glare_path(PlayerbotAI* b) { return new Aq40CthunInGlarePathTrigger(b); }
    static Trigger* cthun_near_beam_target(PlayerbotAI* b) { return new Aq40CthunNearBeamTargetTrigger(b); }
    static Trigger* cthun_opening_puller(PlayerbotAI* b) { return new Aq40CthunOpeningPullerTrigger(b); }
    static Trigger* cthun_wrong_target(PlayerbotAI* b) { return new Aq40CthunWrongTargetTrigger(b); }
    static Trigger* cthun_not_formed(PlayerbotAI* b) { return new Aq40CthunNotFormedTrigger(b); }
    static Trigger* huhuran_not_in_combat(PlayerbotAI* b) { return new Aq40HuhuranNotInCombatTrigger(b); }
    static Trigger* huhuran_mt_has_work(PlayerbotAI* b) { return new Aq40HuhuranMtHasWorkTrigger(b); }
    static Trigger* huhuran_off_tank_idle(PlayerbotAI* b) { return new Aq40HuhuranOffTankIdleTrigger(b); }
    static Trigger* huhuran_off_spot(PlayerbotAI* b) { return new Aq40HuhuranOffSpotTrigger(b); }
    static Trigger* huhuran_threat_hold(PlayerbotAI* b) { return new Aq40HuhuranThreatHoldTrigger(b); }
    static Trigger* huhuran_tank_heal(PlayerbotAI* b) { return new Aq40HuhuranTankHealTrigger(b); }
    static Trigger* huhuran_mt_berserk_defensive(PlayerbotAI* b) { return new Aq40HuhuranMtBerserkDefensiveTrigger(b); }
};

#endif
