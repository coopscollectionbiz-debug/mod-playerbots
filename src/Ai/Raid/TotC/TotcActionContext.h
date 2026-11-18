/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TOTCACTIONCONTEXT_H
#define PLAYERBOTS_TOTCACTIONCONTEXT_H

#include "NamedObjectContext.h"
#include "TotcActions.h"

class RaidTrialOfTheCrusaderActionContext : public NamedObjectContext<Action>
{
public:
    RaidTrialOfTheCrusaderActionContext()
    {
        creators["totc interrupt"] = &RaidTrialOfTheCrusaderActionContext::interrupt;
        creators["totc taunt gormok"] = &RaidTrialOfTheCrusaderActionContext::taunt_gormok;
        creators["totc kill snobold"] = &RaidTrialOfTheCrusaderActionContext::kill_snobold;
        creators["totc cure paralytic toxin"] = &RaidTrialOfTheCrusaderActionContext::cure_paralytic_toxin;
        creators["totc avoid slime pool"] = &RaidTrialOfTheCrusaderActionContext::avoid_slime_pool;
        creators["totc dodge trample"] = &RaidTrialOfTheCrusaderActionContext::dodge_trample;
        creators["totc spread from raid"] = &RaidTrialOfTheCrusaderActionContext::spread_from_raid;
        creators["totc kill jaraxxus add"] = &RaidTrialOfTheCrusaderActionContext::kill_jaraxxus_add;
        creators["totc mark champion"] = &RaidTrialOfTheCrusaderActionContext::mark_champion;
        creators["totc take essence"] = &RaidTrialOfTheCrusaderActionContext::take_essence;
        creators["totc attack twin"] = &RaidTrialOfTheCrusaderActionContext::attack_twin;
        creators["totc avoid orb"] = &RaidTrialOfTheCrusaderActionContext::avoid_orb;
        creators["totc kite spike"] = &RaidTrialOfTheCrusaderActionContext::kite_spike;
        creators["totc avoid spike"] = &RaidTrialOfTheCrusaderActionContext::avoid_spike;
        creators["totc tank burrower"] = &RaidTrialOfTheCrusaderActionContext::tank_burrower;
        creators["totc kill frost sphere"] = &RaidTrialOfTheCrusaderActionContext::kill_frost_sphere;
        creators["totc tank jormungar"] = &RaidTrialOfTheCrusaderActionContext::tank_jormungar;
        creators["totc attack acidmaw"] = &RaidTrialOfTheCrusaderActionContext::attack_acidmaw;
        creators["totc jormungar melee position"] = &RaidTrialOfTheCrusaderActionContext::jormungar_melee_position;
        creators["totc crowd control champion"] = &RaidTrialOfTheCrusaderActionContext::crowd_control_champion;
    }

private:
    static Action* interrupt(PlayerbotAI* botAI) { return new TotcInterruptAction(botAI); }
    static Action* taunt_gormok(PlayerbotAI* botAI) { return new TotcTauntGormokAction(botAI); }
    static Action* kill_snobold(PlayerbotAI* botAI) { return new TotcKillSnoboldAction(botAI); }
    static Action* cure_paralytic_toxin(PlayerbotAI* botAI) { return new TotcCureParalyticToxinAction(botAI); }
    static Action* avoid_slime_pool(PlayerbotAI* botAI) { return new TotcAvoidSlimePoolAction(botAI); }
    static Action* dodge_trample(PlayerbotAI* botAI) { return new TotcDodgeTrampleAction(botAI); }
    static Action* spread_from_raid(PlayerbotAI* botAI) { return new TotcSpreadFromRaidAction(botAI); }
    static Action* kill_jaraxxus_add(PlayerbotAI* botAI) { return new TotcKillJaraxxusAddAction(botAI); }
    static Action* mark_champion(PlayerbotAI* botAI) { return new TotcMarkChampionAction(botAI); }
    static Action* take_essence(PlayerbotAI* botAI) { return new TotcTakeEssenceAction(botAI); }
    static Action* attack_twin(PlayerbotAI* botAI) { return new TotcAttackTwinAction(botAI); }
    static Action* avoid_orb(PlayerbotAI* botAI) { return new TotcAvoidOrbAction(botAI); }
    static Action* kite_spike(PlayerbotAI* botAI) { return new TotcKiteSpikeAction(botAI); }
    static Action* avoid_spike(PlayerbotAI* botAI) { return new TotcAvoidSpikeAction(botAI); }
    static Action* tank_burrower(PlayerbotAI* botAI) { return new TotcTankBurrowerAction(botAI); }
    static Action* kill_frost_sphere(PlayerbotAI* botAI) { return new TotcKillFrostSphereAction(botAI); }
    static Action* tank_jormungar(PlayerbotAI* botAI) { return new TotcTankJormungarAction(botAI); }
    static Action* attack_acidmaw(PlayerbotAI* botAI) { return new TotcAttackAcidmawAction(botAI); }
    static Action* jormungar_melee_position(PlayerbotAI* botAI) { return new TotcJormungarMeleePositionAction(botAI); }
    static Action* crowd_control_champion(PlayerbotAI* botAI) { return new TotcCrowdControlChampionAction(botAI); }
};

#endif
