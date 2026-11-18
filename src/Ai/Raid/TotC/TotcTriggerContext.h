/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TOTCTRIGGERCONTEXT_H
#define PLAYERBOTS_TOTCTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "TotcTriggers.h"

class RaidTrialOfTheCrusaderTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidTrialOfTheCrusaderTriggerContext()
    {
        creators["totc interrupt"] = &RaidTrialOfTheCrusaderTriggerContext::interrupt;
        creators["totc gormok impale"] = &RaidTrialOfTheCrusaderTriggerContext::gormok_impale;
        creators["totc snobold vassal"] = &RaidTrialOfTheCrusaderTriggerContext::snobold_vassal;
        creators["totc burning bile"] = &RaidTrialOfTheCrusaderTriggerContext::burning_bile;
        creators["totc slime pool"] = &RaidTrialOfTheCrusaderTriggerContext::slime_pool;
        creators["totc icehowl trample"] = &RaidTrialOfTheCrusaderTriggerContext::icehowl_trample;
        creators["totc legion flame"] = &RaidTrialOfTheCrusaderTriggerContext::legion_flame;
        creators["totc jaraxxus add"] = &RaidTrialOfTheCrusaderTriggerContext::jaraxxus_add;
        creators["totc champion kill order"] = &RaidTrialOfTheCrusaderTriggerContext::champion_kill_order;
        creators["totc twins wrong essence"] = &RaidTrialOfTheCrusaderTriggerContext::twins_wrong_essence;
        creators["totc twins target"] = &RaidTrialOfTheCrusaderTriggerContext::twins_target;
        creators["totc twins opposite orb"] = &RaidTrialOfTheCrusaderTriggerContext::twins_opposite_orb;
        creators["totc pursued by anub'arak"] = &RaidTrialOfTheCrusaderTriggerContext::pursued_by_anubarak;
        creators["totc spike nearby"] = &RaidTrialOfTheCrusaderTriggerContext::spike_nearby;
        creators["totc burrower untanked"] = &RaidTrialOfTheCrusaderTriggerContext::burrower_untanked;
        creators["totc frost sphere"] = &RaidTrialOfTheCrusaderTriggerContext::frost_sphere;
        creators["totc jormungar tank"] = &RaidTrialOfTheCrusaderTriggerContext::jormungar_tank;
        creators["totc jormungar kill order"] = &RaidTrialOfTheCrusaderTriggerContext::jormungar_kill_order;
        creators["totc jormungar melee"] = &RaidTrialOfTheCrusaderTriggerContext::jormungar_melee;
        creators["totc champion crowd control"] = &RaidTrialOfTheCrusaderTriggerContext::champion_crowd_control;
    }

private:
    static Trigger* interrupt(PlayerbotAI* botAI) { return new TotcInterruptTrigger(botAI); }
    static Trigger* gormok_impale(PlayerbotAI* botAI) { return new TotcGormokImpaleTrigger(botAI); }
    static Trigger* snobold_vassal(PlayerbotAI* botAI) { return new TotcSnoboldVassalTrigger(botAI); }
    static Trigger* burning_bile(PlayerbotAI* botAI) { return new TotcBurningBileTrigger(botAI); }
    static Trigger* slime_pool(PlayerbotAI* botAI) { return new TotcSlimePoolTrigger(botAI); }
    static Trigger* icehowl_trample(PlayerbotAI* botAI) { return new TotcIcehowlTrampleTrigger(botAI); }
    static Trigger* legion_flame(PlayerbotAI* botAI) { return new TotcLegionFlameTrigger(botAI); }
    static Trigger* jaraxxus_add(PlayerbotAI* botAI) { return new TotcJaraxxusAddTrigger(botAI); }
    static Trigger* champion_kill_order(PlayerbotAI* botAI) { return new TotcChampionKillOrderTrigger(botAI); }
    static Trigger* twins_wrong_essence(PlayerbotAI* botAI) { return new TotcTwinsWrongEssenceTrigger(botAI); }
    static Trigger* twins_target(PlayerbotAI* botAI) { return new TotcTwinsTargetTrigger(botAI); }
    static Trigger* twins_opposite_orb(PlayerbotAI* botAI) { return new TotcTwinsOppositeOrbTrigger(botAI); }
    static Trigger* pursued_by_anubarak(PlayerbotAI* botAI) { return new TotcPursuedByAnubarakTrigger(botAI); }
    static Trigger* spike_nearby(PlayerbotAI* botAI) { return new TotcSpikeNearbyTrigger(botAI); }
    static Trigger* burrower_untanked(PlayerbotAI* botAI) { return new TotcBurrowerUntankedTrigger(botAI); }
    static Trigger* frost_sphere(PlayerbotAI* botAI) { return new TotcFrostSphereTrigger(botAI); }
    static Trigger* jormungar_tank(PlayerbotAI* botAI) { return new TotcJormungarTankTrigger(botAI); }
    static Trigger* jormungar_kill_order(PlayerbotAI* botAI) { return new TotcJormungarKillOrderTrigger(botAI); }
    static Trigger* jormungar_melee(PlayerbotAI* botAI) { return new TotcJormungarMeleeTrigger(botAI); }
    static Trigger* champion_crowd_control(PlayerbotAI* botAI) { return new TotcChampionCrowdControlTrigger(botAI); }
};

#endif
