/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxStrategy.h"

#include <atomic>

#include "Log.h"
#include "NaxxMultipliers.h"

void RaidNaxxStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Raid-only resistance buff near Sapphiron / the Horsemen / Ouro / Huhuran / the Twins (RaidResistanceBuff.h).
    triggers.push_back(new TriggerNode("raid resistance buff", { NextAction("raid resistance buff", ACTION_RAID + 5) }));

    // Build canary (overlay CLAUDE.md local-build rule): proves the running worldserver carries the Naxx40 Four
    // Horsemen, Loatheb, Anub'Rekhan and Patchwerk strategies. InitTriggers runs per bot per engine (map threads), so it
    // logs once per process.
    static std::atomic<bool> horsemenCanary{false};
    if (!horsemenCanary.exchange(true))
    {
        LOG_INFO("playerbots", "[Naxx40Horsemen] Naxx40Horsemen v1 strategy loaded (overlay patch 0022)");
        LOG_INFO("playerbots", "[Naxx40Loatheb] Naxx40Loatheb v7 strategy loaded (overlay patch 0022)");
        LOG_INFO("playerbots", "[Naxx40Anub] Naxx40Anub v2 strategy loaded (overlay patch 0022)");
        LOG_INFO("playerbots", "[Naxx40Patchwerk] Naxx40Patchwerk v3 strategy loaded (overlay patch 0022)");
    }

    // Grobbulus
    triggers.push_back(new TriggerNode("mutating injection melee",
        { NextAction("grobbulus move away", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("mutating injection ranged",
        { NextAction("grobbulus go behind the boss", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("mutating injection removed",
        { NextAction("grobbulus move center", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("grobbulus tank facing",
        { NextAction("grobbulus tank face wall", ACTION_RAID) }
    ));

    triggers.push_back(new TriggerNode("grobbulus poison cloud",
        { NextAction("grobbulus avoid cloud", ACTION_RAID + 4) }
    ));

    triggers.push_back(new TriggerNode("grobbulus spray cone",
        { NextAction("grobbulus avoid spray", ACTION_RAID + 3) }
    ));

    triggers.push_back(new TriggerNode("grobbulus cloud",
        { NextAction("rotate grobbulus", ACTION_RAID + 1) }
    ));

    // Heigan the Unclean
    triggers.push_back(new TriggerNode("heigan melee",
        { NextAction("heigan dance melee", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("heigan ranged",
        { NextAction("heigan dance ranged", ACTION_RAID + 1) }
    ));

    // Kel'Thuzad
    triggers.push_back(
        new TriggerNode("kel'thuzad",
        {
            NextAction("kel'thuzad position", ACTION_RAID + 2),
            NextAction("kel'thuzad choose target", ACTION_RAID + 1)
        })
    );

    // Anub'Rekhan
    triggers.push_back(new TriggerNode("anub'rekhan",
        {
            NextAction("anub'rekhan position", ACTION_RAID + 1),
            NextAction("anub'rekhan choose target", ACTION_RAID)
        }
    ));

    // Naxx40 Anub'Rekhan (overlay patch 0022; spec 2026-10-30-playerbot-naxx40-anubrekhan): non-tanks spread on an arc
    // (melee on two flanks outside the swarm) so one Impale cone catches a few bots, not the raid. Gated on
    // Naxx40Anub::GetView (IP's Naxx40 only); Naxx40AnubMultiplier vetoes upstream's centre clump for slotted bots, the
    // main tank's kite above is untouched.
    triggers.push_back(new TriggerNode("naxx40 anub spread",
        { NextAction("naxx40 anub spread", ACTION_RAID + 1) }
    ));

     // Grand Widow Faerlina
     triggers.push_back(new TriggerNode("faerlina",
        { NextAction("avoid aoe", ACTION_RAID + 1) }
    ));

    // Maexxna
    triggers.push_back(
        new TriggerNode("maexxna",
        {
            NextAction("rear flank", ACTION_RAID + 1),
            NextAction("avoid aoe", ACTION_RAID + 1)
        })
    );

    // Patchwerk
    //triggers.push_back(new TriggerNode("patchwerk tank",
    //    { NextAction("tank face", ACTION_RAID + 2) }
    //));

    //triggers.push_back(new TriggerNode("patchwerk ranged",
    //    { NextAction("patchwerk ranged position", ACTION_RAID + 2) }
    //));

    //triggers.push_back(new TriggerNode("patchwerk non-tank",
    //    { NextAction("rear flank", ACTION_RAID + 1) }
    //));

    // Thaddius
    triggers.push_back(new TriggerNode("thaddius phase pet",
        { NextAction("thaddius attack nearest pet", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase pet lose aggro",
        { NextAction("taunt spell", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase transition",
        { NextAction("thaddius move to platform", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase thaddius",
        { NextAction("thaddius move polarity", ACTION_RAID + 1) }
    ));

    // Instructor Razuvious
    triggers.push_back(new TriggerNode("razuvious tank",
        { NextAction("razuvious use obedience crystal", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("razuvious nontank",
        { NextAction("razuvious target", ACTION_RAID + 1) }
    ));

    // Naxx40: mana users out of Disrupting Shout's 30 yd, ahead of the Mind Control and target actions.
    triggers.push_back(new TriggerNode("razuvious shout range",
        { NextAction("razuvious keep shout range", ACTION_RAID + 3) }
    ));

    // four horsemen
    triggers.push_back(new TriggerNode("four horsemen attractors",
        { NextAction("four horsemen attract alternatively", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("four horsemen except attractors",
        { NextAction("four horsemen attack in order", ACTION_RAID + 1) }
    ));

    // Naxx40 Four Horsemen (overlay patch 0022; spec 2026-10-27). The strategy is added to both engines
    // (PlayerbotAI::ApplyInstanceStrategies), so these also fire out of combat: a resting / ready tank or a DPS on the
    // safe spot may have left combat and still has to hold its spot. The WotLK nodes above are gated off Naxx40 by
    // entry (FourHorsemenBossHelper).
    triggers.push_back(new TriggerNode("naxx40 horsemen tank",
        { NextAction("naxx40 horsemen tank", ACTION_RAID + 1) }
    ));

    // The raid action outranks the tank heal: a healer must re-choose its horseman (and leave on its 3rd stack) even
    // while its tank sits under 90 %. The raid action returns false on the spot, so the heal still runs after it.
    triggers.push_back(new TriggerNode("naxx40 horsemen raid",
        { NextAction("naxx40 horsemen raid", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("naxx40 horsemen tank heal",
        { NextAction("naxx40 horsemen tank heal", ACTION_RAID + 1) }
    ));

    // DPS hard stop (the `bwl threat hold` relevance): below the raid action, which falls through on the spot.
    triggers.push_back(new TriggerNode("naxx40 horsemen threat hold",
        { NextAction("naxx40 horsemen threat hold", ACTION_RAID + 1) }
    ));

    // sapphiron
    triggers.push_back(new TriggerNode("sapphiron ground",
        { NextAction("sapphiron ground position", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("sapphiron flight",
        { NextAction("sapphiron flight position", ACTION_RAID + 1) }
    ));

    // Gluth
    triggers.push_back(
        new TriggerNode("gluth",
        {
            NextAction("gluth choose target", ACTION_RAID + 1),
            NextAction("gluth position", ACTION_RAID + 1),
            NextAction("gluth slowdown", ACTION_RAID)
        })
    );

    triggers.push_back(new TriggerNode("gluth main tank mortal wound",
        { NextAction("taunt spell", ACTION_RAID + 1) }
    ));

    // Loatheb
    triggers.push_back(
        new TriggerNode("loatheb",
        {
            NextAction("loatheb position", ACTION_RAID + 1),
            NextAction("loatheb choose target", ACTION_RAID + 1)
        })
    );

    // Added from the WotLK 10/25-player guides
    triggers.push_back(new TriggerNode("naxx kill order mark", { NextAction("naxx kill order mark", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("naxx faerlina mind control",
        { NextAction("naxx faerlina mind control", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("naxx faerlina embrace",
        { NextAction("naxx faerlina embrace", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("naxx faerlina bring worshipper",
        { NextAction("naxx faerlina bring worshipper", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("naxx maexxna web wrap",
        { NextAction("naxx attack web wrap", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("naxx noth not tanked", { NextAction("naxx noth taunt", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("naxx plagued champion range",
        { NextAction("naxx move from plagued champion", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("naxx gothik side", { NextAction("naxx gothik go to side", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("naxx gothik not tanked", { NextAction("naxx gothik taunt", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("naxx patchwerk soaker",
        { NextAction("naxx patchwerk soak", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("naxx patchwerk melee", { NextAction("rear flank", ACTION_MOVE + 4) }));
    triggers.push_back(new TriggerNode("naxx thane meteor stack", { NextAction("naxx thane stack", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("naxx detonate mana",
        { NextAction("naxx detonate mana spread", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("naxx chains mark", { NextAction("naxx chains mark", ACTION_RAID) }));
    // Naxx40 Loatheb (overlay patch 0022; spec 2026-10-30 §4). Each trigger shares its action's name. Every one is
    // gated on Naxx40Loatheb::GetView (IP's Naxx40 only), so upstream's "loatheb" node above runs unchanged outside it.
    // Consumables and the tank cooldown outrank the rest (a bandage channel is held, a Doom potion is time-bound); the
    // heals outrank the spore kill (DPS only), which outranks healer damage, so a healer only nukes while its heal is
    // not due.
    auto const loathebNode = [&triggers](char const* name, float relevance)
    {
        triggers.push_back(new TriggerNode(name, { NextAction(name, relevance) }));
    };
    loathebNode("naxx40 loatheb consumable", ACTION_RAID + 3);
    loathebNode("naxx40 loatheb tank cooldown", ACTION_RAID + 3);
    loathebNode("naxx40 loatheb tank heal", ACTION_RAID + 2);
    loathebNode("naxx40 loatheb group heal", ACTION_RAID + 2);
    loathebNode("naxx40 loatheb spore", ACTION_RAID + 1);
    loathebNode("naxx40 loatheb healer dps", ACTION_RAID);

    // Naxx40 Patchwerk (overlay patch 0022; spec 2026-11-01). Each trigger shares its action's name; every one is gated
    // on Naxx40Patchwerk::GetView (IP's Naxx40 only). Upstream's Patchwerk nodes above stay commented out. The mana
    // refill outranks the heals so a dry healer is topped up first; the threat hold sits with the heals (one bot never
    // runs both: healers are never capped). The tank threat boost (pull 1) sits with the refill: an instant cheat that
    // goes quiet as soon as the floor is met.
    loathebNode("naxx40 patchwerk soaker heal", ACTION_RAID + 1);
    loathebNode("naxx40 patchwerk mt heal", ACTION_RAID + 1);
    loathebNode("naxx40 patchwerk threat hold", ACTION_RAID + 1);
    loathebNode("naxx40 patchwerk mana refill", ACTION_RAID + 2);
    loathebNode("naxx40 patchwerk threat boost", ACTION_RAID + 2);

}

void RaidNaxxStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new GrobbulusMultiplier(botAI));
    multipliers.push_back(new HeiganDanceMultiplier(botAI));
    multipliers.push_back(new LoathebGenericMultiplier(botAI));
    multipliers.push_back(new Naxx40LoathebMultiplier(botAI));
    multipliers.push_back(new ThaddiusGenericMultiplier(botAI));
    multipliers.push_back(new SapphironGenericMultiplier(botAI));
    multipliers.push_back(new InstructorRazuviousGenericMultiplier(botAI));
    multipliers.push_back(new KelthuzadGenericMultiplier(botAI));
    multipliers.push_back(new AnubrekhanGenericMultiplier(botAI));
    multipliers.push_back(new Naxx40AnubMultiplier(botAI));
    multipliers.push_back(new Naxx40PatchwerkMultiplier(botAI));
    multipliers.push_back(new FourHorsemenGenericMultiplier(botAI));
    multipliers.push_back(new Naxx40HorsemenMultiplier(botAI));
    // multipliers.push_back(new GothikGenericMultiplier(botAI));
    multipliers.push_back(new GluthGenericMultiplier(botAI));
    multipliers.push_back(new NaxxRaidMechanicsMultiplier(botAI));
}
