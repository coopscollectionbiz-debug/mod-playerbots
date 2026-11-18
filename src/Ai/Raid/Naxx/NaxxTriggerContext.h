/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXTRIGGERCONTEXT_H
#define PLAYERBOTS_NAXXTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "NaxxTriggers.h"

class RaidNaxxTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidNaxxTriggerContext()
    {
        creators["mutating injection melee"] = &RaidNaxxTriggerContext::mutating_injection_melee;
        creators["mutating injection ranged"] = &RaidNaxxTriggerContext::mutating_injection_ranged;
        creators["mutating injection removed"] = &RaidNaxxTriggerContext::mutating_injection_removed;
        creators["grobbulus cloud"] = &RaidNaxxTriggerContext::grobbulus_cloud;
        creators["heigan melee"] = &RaidNaxxTriggerContext::heigan_melee;
        creators["heigan ranged"] = &RaidNaxxTriggerContext::heigan_ranged;

        creators["thaddius phase pet"] = &RaidNaxxTriggerContext::thaddius_phase_pet;
        creators["thaddius phase pet lose aggro"] = &RaidNaxxTriggerContext::thaddius_phase_pet_lose_aggro;
        creators["thaddius phase transition"] = &RaidNaxxTriggerContext::thaddius_phase_transition;
        creators["thaddius phase thaddius"] = &RaidNaxxTriggerContext::thaddius_phase_thaddius;

        creators["razuvious tank"] = &RaidNaxxTriggerContext::razuvious_tank;
        creators["razuvious nontank"] = &RaidNaxxTriggerContext::razuvious_nontank;

        creators["four horsemen attractors"] = &RaidNaxxTriggerContext::four_horsemen_attractors;
        creators["four horsemen except attractors"] = &RaidNaxxTriggerContext::four_horsemen_except_attractors;

        creators["sapphiron ground"] = &RaidNaxxTriggerContext::sapphiron_ground;
        creators["sapphiron flight"] = &RaidNaxxTriggerContext::sapphiron_flight;

        creators["kel'thuzad"] = &RaidNaxxTriggerContext::kelthuzad;

        creators["anub'rekhan"] = &RaidNaxxTriggerContext::anubrekhan;
        creators["faerlina"] = &RaidNaxxTriggerContext::faerlina;
        creators["maexxna"] = &RaidNaxxTriggerContext::maexxna;
        //creators["patchwerk tank"] = &RaidNaxxTriggerContext::patchwerk_tank;
        //creators["patchwerk non-tank"] = &RaidNaxxTriggerContext::patchwerk_non_tank;
        //creators["patchwerk ranged"] = &RaidNaxxTriggerContext::patchwerk_ranged;

        creators["gluth"] = &RaidNaxxTriggerContext::gluth;
        creators["gluth main tank mortal wound"] = &RaidNaxxTriggerContext::gluth_main_tank_mortal_wound;

        creators["loatheb"] = &RaidNaxxTriggerContext::loatheb;
        creators["naxx kill order mark"] = &RaidNaxxTriggerContext::naxx_kill_order_mark;
        creators["naxx faerlina mind control"] = &RaidNaxxTriggerContext::naxx_faerlina_mind_control;
        creators["naxx faerlina embrace"] = &RaidNaxxTriggerContext::naxx_faerlina_embrace;
        creators["naxx faerlina bring worshipper"] = &RaidNaxxTriggerContext::naxx_faerlina_bring_worshipper;
        creators["naxx maexxna web wrap"] = &RaidNaxxTriggerContext::naxx_maexxna_web_wrap;
        creators["naxx noth not tanked"] = &RaidNaxxTriggerContext::naxx_noth_not_tanked;
        creators["naxx plagued champion range"] = &RaidNaxxTriggerContext::naxx_plagued_champion_range;
        creators["naxx gothik side"] = &RaidNaxxTriggerContext::naxx_gothik_side;
        creators["naxx gothik not tanked"] = &RaidNaxxTriggerContext::naxx_gothik_not_tanked;
        creators["naxx patchwerk soaker"] = &RaidNaxxTriggerContext::naxx_patchwerk_soaker;
        creators["naxx patchwerk melee"] = &RaidNaxxTriggerContext::naxx_patchwerk_melee;
        creators["naxx thane meteor stack"] = &RaidNaxxTriggerContext::naxx_thane_meteor_stack;
        creators["naxx detonate mana"] = &RaidNaxxTriggerContext::naxx_detonate_mana;
        creators["naxx chains mark"] = &RaidNaxxTriggerContext::naxx_chains_mark;
    }

private:
    static Trigger* mutating_injection_melee(PlayerbotAI* botAI) { return new MutatingInjectionMeleeTrigger(botAI); }
    static Trigger* mutating_injection_ranged(PlayerbotAI* botAI) { return new MutatingInjectionRangedTrigger(botAI); }
    static Trigger* mutating_injection_removed(PlayerbotAI* botAI) { return new MutatingInjectionRemovedTrigger(botAI); }
    static Trigger* grobbulus_cloud(PlayerbotAI* botAI) { return new GrobbulusCloudTrigger(botAI); }
    static Trigger* heigan_melee(PlayerbotAI* botAI) { return new HeiganMeleeTrigger(botAI); }
    static Trigger* heigan_ranged(PlayerbotAI* botAI) { return new HeiganRangedTrigger(botAI); }

    static Trigger* thaddius_phase_pet(PlayerbotAI* botAI) { return new ThaddiusPhasePetTrigger(botAI); }
    static Trigger* thaddius_phase_pet_lose_aggro(PlayerbotAI* botAI) { return new ThaddiusPhasePetLoseAggroTrigger(botAI); }
    static Trigger* thaddius_phase_transition(PlayerbotAI* botAI) { return new ThaddiusPhaseTransitionTrigger(botAI); }
    static Trigger* thaddius_phase_thaddius(PlayerbotAI* botAI) { return new ThaddiusPhaseThaddiusTrigger(botAI); }
    static Trigger* razuvious_tank(PlayerbotAI* botAI) { return new RazuviousTankTrigger(botAI); }
    static Trigger* razuvious_nontank(PlayerbotAI* botAI) { return new RazuviousNontankTrigger(botAI); }

    static Trigger* four_horsemen_attractors(PlayerbotAI* botAI) { return new FourHorsemenAttractorsTrigger(botAI); }
    static Trigger* four_horsemen_except_attractors(PlayerbotAI* botAI) { return new FourHorsemenExceptAttractorsTrigger(botAI); }

    static Trigger* sapphiron_ground(PlayerbotAI* botAI) { return new SapphironGroundTrigger(botAI); }
    static Trigger* sapphiron_flight(PlayerbotAI* botAI) { return new SapphironFlightTrigger(botAI); }
    static Trigger* kelthuzad(PlayerbotAI* botAI) { return new KelthuzadTrigger(botAI); }
    static Trigger* anubrekhan(PlayerbotAI* botAI) { return new AnubrekhanTrigger(botAI); }
    static Trigger* faerlina(PlayerbotAI* botAI) { return new FaerlinaTrigger(botAI); }
    static Trigger* maexxna(PlayerbotAI* botAI) { return new MaexxnaTrigger(botAI); }
    //static Trigger* patchwerk_tank(PlayerbotAI* botAI) { return new PatchwerkTankTrigger(botAI); }
    //static Trigger* patchwerk_non_tank(PlayerbotAI* botAI) { return new PatchwerkNonTankTrigger(botAI); }
    //static Trigger* patchwerk_ranged(PlayerbotAI* botAI) { return new PatchwerkRangedTrigger(botAI); }
    static Trigger* gluth(PlayerbotAI* botAI) { return new GluthTrigger(botAI); }
    static Trigger* gluth_main_tank_mortal_wound(PlayerbotAI* botAI) { return new GluthMainTankMortalWoundTrigger(botAI); }
    static Trigger* loatheb(PlayerbotAI* botAI) { return new LoathebTrigger(botAI); }
    static Trigger* naxx_kill_order_mark(PlayerbotAI* botAI) { return new NaxxKillOrderMarkTrigger(botAI); }
    static Trigger* naxx_faerlina_mind_control(PlayerbotAI* botAI) { return new NaxxFaerlinaMindControlTrigger(botAI); }
    static Trigger* naxx_faerlina_embrace(PlayerbotAI* botAI) { return new NaxxFaerlinaEmbraceTrigger(botAI); }
    static Trigger* naxx_faerlina_bring_worshipper(PlayerbotAI* botAI)
    {
        return new NaxxFaerlinaBringWorshipperTrigger(botAI);
    }
    static Trigger* naxx_maexxna_web_wrap(PlayerbotAI* botAI) { return new NaxxMaexxnaWebWrapTrigger(botAI); }
    static Trigger* naxx_noth_not_tanked(PlayerbotAI* botAI) { return new NaxxNothNotTankedTrigger(botAI); }
    static Trigger* naxx_plagued_champion_range(PlayerbotAI* botAI)
    {
        return new NaxxPlaguedChampionRangeTrigger(botAI);
    }
    static Trigger* naxx_gothik_side(PlayerbotAI* botAI) { return new NaxxGothikSideTrigger(botAI); }
    static Trigger* naxx_gothik_not_tanked(PlayerbotAI* botAI) { return new NaxxGothikNotTankedTrigger(botAI); }
    static Trigger* naxx_patchwerk_soaker(PlayerbotAI* botAI) { return new NaxxPatchwerkSoakerTrigger(botAI); }
    static Trigger* naxx_patchwerk_melee(PlayerbotAI* botAI) { return new NaxxPatchwerkMeleeTrigger(botAI); }
    static Trigger* naxx_thane_meteor_stack(PlayerbotAI* botAI) { return new NaxxThaneMeteorStackTrigger(botAI); }
    static Trigger* naxx_detonate_mana(PlayerbotAI* botAI) { return new NaxxDetonateManaTrigger(botAI); }
    static Trigger* naxx_chains_mark(PlayerbotAI* botAI) { return new NaxxChainsMarkTrigger(botAI); }
};

#endif
