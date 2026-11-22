/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40ACTIONCONTEXT_H
#define PLAYERBOTS_AQ40ACTIONCONTEXT_H

#include "../RaidResistanceBuff.h"
#include "Aq40Actions.h"
#include "NamedObjectContext.h"

class RaidAq40ActionContext : public NamedObjectContext<Action>
{
public:
    RaidAq40ActionContext()
    {
        creators["raid resistance buff"] = &RaidAq40ActionContext::raid_resistance_buff;
        creators["aq40 twins erase trackers"] = &RaidAq40ActionContext::erase_trackers;
        creators["aq40 twins clear explode"] = &RaidAq40ActionContext::clear_explode;
        creators["aq40 twins clear blizzard"] = &RaidAq40ActionContext::clear_blizzard;
        creators["aq40 twins clear burst"] = &RaidAq40ActionContext::clear_burst;
        creators["aq40 twins focus target"] = &RaidAq40ActionContext::focus_target;
        creators["aq40 twins landing clear"] = &RaidAq40ActionContext::landing_clear;
        creators["aq40 twins victim hold"] = &RaidAq40ActionContext::victim_hold;
        creators["aq40 twins hold station"] = &RaidAq40ActionContext::hold_station;
        creators["aq40 twins hold spot"] = &RaidAq40ActionContext::hold_spot;
        creators["aq40 twins warlock tank"] = &RaidAq40ActionContext::warlock_tank;
        creators["aq40 twins threat hold"] = &RaidAq40ActionContext::twins_threat_hold;
        creators["aq40 twins direct pets"] = &RaidAq40ActionContext::direct_pets;
        creators["aq40 twins stage"] = &RaidAq40ActionContext::twins_stage;
        creators["aq40 twins pull"] = &RaidAq40ActionContext::twins_pull;
        creators["aq40 twins tank heal"] = &RaidAq40ActionContext::twins_tank_heal;
        creators["aq40 cthun erase trackers"] = &RaidAq40ActionContext::cthun_erase_trackers;
        creators["aq40 cthun exit stomach"] = &RaidAq40ActionContext::cthun_exit_stomach;
        creators["aq40 cthun dismount"] = &RaidAq40ActionContext::cthun_dismount;
        creators["aq40 cthun drop to floor"] = &RaidAq40ActionContext::cthun_drop_to_floor;
        creators["aq40 cthun stomach close in"] = &RaidAq40ActionContext::cthun_stomach_close_in;
        creators["aq40 cthun dodge glare"] = &RaidAq40ActionContext::cthun_dodge_glare;
        creators["aq40 cthun clear beam"] = &RaidAq40ActionContext::cthun_clear_beam;
        creators["aq40 cthun run to far side"] = &RaidAq40ActionContext::cthun_run_to_far_side;
        creators["aq40 cthun focus target"] = &RaidAq40ActionContext::cthun_focus_target;
        creators["aq40 cthun form up"] = &RaidAq40ActionContext::cthun_form_up;
        creators["aq40 huhuran erase trackers"] = &RaidAq40ActionContext::huhuran_erase_trackers;
        creators["aq40 huhuran park boss"] = &RaidAq40ActionContext::huhuran_park_boss;
        creators["aq40 huhuran off tank engage"] = &RaidAq40ActionContext::huhuran_off_tank_engage;
        creators["aq40 huhuran hold spot"] = &RaidAq40ActionContext::huhuran_hold_spot;
        creators["aq40 huhuran threat hold"] = &RaidAq40ActionContext::huhuran_threat_hold;
        creators["aq40 huhuran tank heal"] = &RaidAq40ActionContext::huhuran_tank_heal;
        creators["aq40 huhuran mt berserk defensive"] = &RaidAq40ActionContext::huhuran_mt_berserk_defensive;
    }

private:
    static Action* raid_resistance_buff(PlayerbotAI* ai) { return new RaidResistanceBuffAction(ai); }
    static Action* erase_trackers(PlayerbotAI* b) { return new Aq40TwinsEraseTrackersAction(b); }
    static Action* clear_explode(PlayerbotAI* b) { return new Aq40TwinsClearExplodeAction(b); }
    static Action* clear_blizzard(PlayerbotAI* b) { return new Aq40TwinsClearBlizzardAction(b); }
    static Action* clear_burst(PlayerbotAI* b) { return new Aq40TwinsClearBurstAction(b); }
    static Action* focus_target(PlayerbotAI* b) { return new Aq40TwinsFocusTargetAction(b); }
    static Action* landing_clear(PlayerbotAI* b) { return new Aq40TwinsLandingClearAction(b); }
    static Action* victim_hold(PlayerbotAI* b) { return new Aq40TwinsVictimHoldAction(b); }
    static Action* hold_station(PlayerbotAI* b) { return new Aq40TwinsHoldStationAction(b); }
    static Action* hold_spot(PlayerbotAI* b) { return new Aq40TwinsHoldSpotAction(b); }
    static Action* warlock_tank(PlayerbotAI* b) { return new Aq40TwinsWarlockTankAction(b); }
    static Action* twins_threat_hold(PlayerbotAI* b) { return new Aq40TwinsThreatHoldAction(b); }
    static Action* direct_pets(PlayerbotAI* b) { return new Aq40TwinsDirectPetsAction(b); }
    static Action* twins_stage(PlayerbotAI* b) { return new Aq40TwinsStageAction(b); }
    static Action* twins_pull(PlayerbotAI* b) { return new Aq40TwinsPullAction(b); }
    static Action* twins_tank_heal(PlayerbotAI* b) { return new Aq40TwinsTankHealAction(b); }
    static Action* cthun_erase_trackers(PlayerbotAI* b) { return new Aq40CthunEraseTrackersAction(b); }
    static Action* cthun_exit_stomach(PlayerbotAI* b) { return new Aq40CthunExitStomachAction(b); }
    static Action* cthun_dismount(PlayerbotAI* b) { return new Aq40CthunDismountAction(b); }
    static Action* cthun_drop_to_floor(PlayerbotAI* b) { return new Aq40CthunDropToFloorAction(b); }
    static Action* cthun_stomach_close_in(PlayerbotAI* b) { return new Aq40CthunStomachCloseInAction(b); }
    static Action* cthun_dodge_glare(PlayerbotAI* b) { return new Aq40CthunDodgeGlareAction(b); }
    static Action* cthun_clear_beam(PlayerbotAI* b) { return new Aq40CthunClearBeamAction(b); }
    static Action* cthun_run_to_far_side(PlayerbotAI* b) { return new Aq40CthunRunToFarSideAction(b); }
    static Action* cthun_focus_target(PlayerbotAI* b) { return new Aq40CthunFocusTargetAction(b); }
    static Action* cthun_form_up(PlayerbotAI* b) { return new Aq40CthunFormUpAction(b); }
    static Action* huhuran_erase_trackers(PlayerbotAI* b) { return new Aq40HuhuranEraseTrackersAction(b); }
    static Action* huhuran_park_boss(PlayerbotAI* b) { return new Aq40HuhuranParkBossAction(b); }
    static Action* huhuran_off_tank_engage(PlayerbotAI* b) { return new Aq40HuhuranOffTankEngageAction(b); }
    static Action* huhuran_hold_spot(PlayerbotAI* b) { return new Aq40HuhuranHoldSpotAction(b); }
    static Action* huhuran_threat_hold(PlayerbotAI* b) { return new Aq40HuhuranThreatHoldAction(b); }
    static Action* huhuran_tank_heal(PlayerbotAI* b) { return new Aq40HuhuranTankHealAction(b); }
    static Action* huhuran_mt_berserk_defensive(PlayerbotAI* b) { return new Aq40HuhuranMtBerserkDefensiveAction(b); }
};

#endif
