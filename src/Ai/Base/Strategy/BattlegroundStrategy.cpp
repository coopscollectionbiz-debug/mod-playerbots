/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BattlegroundStrategy.h"
#include "Playerbots.h"

// mod-battleground-bots (overlay patch 0019): BG-aware targeting for every open-world BG strategy.
// The per-BG strategies live in the COMBAT engine (AiFactory.cpp), so these fire mid-fight.
// Relevance sits just above "dps assist" (50, DpsAssistStrategy.cpp) so the priority target wins
// the retarget while any ACTION_RAID/EMERGENCY node still preempts it.
// Healer follow is 5.0: above the BG movement nodes (ACTION_BG = 1..2) and below every heal
// (ACTION_LIGHT_HEAL = 10), so a healer finishes healing before it walks — a bot cannot cast a
// cast-time spell while moving, so ranking the follow above the heals would silently mute it.
static void AddBgDirectorNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("bg priority target near", { NextAction("attack bg priority target", 51.0f) }));
    triggers.push_back(new TriggerNode("bg ally needs peel", { NextAction("bg peel for ally", 52.0f) }));
    triggers.push_back(new TriggerNode("bg healer squad far", { NextAction("bg healer follow squad", 5.0f) }));
}

void BGStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("often", { NextAction("bg join", relevance)}));
    triggers.push_back(new TriggerNode("bg invite active", { NextAction("bg status check", relevance)}));
    triggers.push_back(new TriggerNode("timer", { NextAction("bg strategy check", relevance)}));
}

BGStrategy::BGStrategy(PlayerbotAI* botAI) : PassThroughStrategy(botAI) {}

void BattlegroundStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("bg waiting", { NextAction("bg move to start", ACTION_BG)}));
    triggers.push_back(new TriggerNode("bg active", { NextAction("bg move to objective", ACTION_BG)}));
    triggers.push_back(new TriggerNode("often", { NextAction("bg check objective", ACTION_BG + 1)}));
    triggers.push_back(new TriggerNode("dead", { NextAction("bg reset objective force", ACTION_EMERGENCY)}));
}

void WarsongStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("bg active", { NextAction("bg check flag", ACTION_EMERGENCY )}));
    triggers.push_back(new TriggerNode("enemy flagcarrier near", { NextAction("attack enemy flag carrier", ACTION_RAID + 1.0f)}));
    triggers.push_back(new TriggerNode("team flagcarrier near", { NextAction("bg protect fc", ACTION_RAID)}));
    triggers.push_back(new TriggerNode("often", { NextAction("bg use buff", ACTION_BG)}));
    triggers.push_back(new TriggerNode("low health", { NextAction("bg use buff", ACTION_MOVE)}));
    triggers.push_back(new TriggerNode("low mana", { NextAction("bg use buff", ACTION_MOVE)}));
    triggers.push_back(new TriggerNode("player has flag", { NextAction("bg move to objective", ACTION_EMERGENCY)}));
    triggers.push_back(new TriggerNode("timer bg", { NextAction("bg reset objective force", ACTION_EMERGENCY)}));
    AddBgDirectorNodes(triggers);
}

void AlteracStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("timer bg", { NextAction("bg reset objective force", ACTION_EMERGENCY)}));
    AddBgDirectorNodes(triggers);
}

void ArathiStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("bg active", { NextAction("bg check flag", ACTION_EMERGENCY)}));
    triggers.push_back(new TriggerNode("often", { NextAction("bg use buff", ACTION_BG)}));
    triggers.push_back(new TriggerNode("low health", { NextAction("bg use buff", ACTION_MOVE)}));
    triggers.push_back(new TriggerNode("low mana", { NextAction("bg use buff", ACTION_MOVE)}));
    AddBgDirectorNodes(triggers);
}

void EyeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("bg active", { NextAction("bg check flag", ACTION_EMERGENCY)}));
    triggers.push_back(new TriggerNode("often", { NextAction("bg use buff", ACTION_BG)}));
    triggers.push_back(new TriggerNode("low health", { NextAction("bg use buff", ACTION_MOVE)}));
    triggers.push_back(new TriggerNode("low mana", { NextAction("bg use buff", ACTION_MOVE)}));
    triggers.push_back(new TriggerNode("enemy flagcarrier near", { NextAction("attack enemy flag carrier", ACTION_RAID)}));
    triggers.push_back(new TriggerNode("player has flag",{ NextAction("bg move to objective", ACTION_EMERGENCY)}));
    AddBgDirectorNodes(triggers);
}

//TODO: Do Priorities
void IsleStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("bg active", { NextAction("bg check flag", ACTION_MOVE)}));
    triggers.push_back(new TriggerNode("timer", { NextAction("enter vehicle", ACTION_MOVE + 8.0f)}));
    triggers.push_back(new TriggerNode("random", { NextAction("leave vehicle", ACTION_MOVE + 7.0f)}));
    triggers.push_back(new TriggerNode("in vehicle", { NextAction("hurl boulder", ACTION_MOVE + 9.0f)}));
    triggers.push_back(new TriggerNode("in vehicle", { NextAction("fire cannon", ACTION_MOVE + 9.0f)}));
    triggers.push_back(new TriggerNode("in vehicle", { NextAction("napalm", ACTION_MOVE + 9.0f)}));
    triggers.push_back(new TriggerNode("enemy is close", { NextAction("steam blast", ACTION_MOVE + 9.0f)}));
    triggers.push_back(new TriggerNode("in vehicle", { NextAction("ram", ACTION_MOVE + 9.0f)}));
    triggers.push_back(new TriggerNode("enemy is close", { NextAction("ram", ACTION_MOVE + 9.1f)}));
    triggers.push_back(new TriggerNode("enemy out of melee", { NextAction("steam rush", ACTION_MOVE + 9.2f)}));
    triggers.push_back(new TriggerNode("in vehicle", { NextAction("incendiary rocket", ACTION_MOVE + 9.0f)}));
    triggers.push_back(new TriggerNode("in vehicle", { NextAction("rocket blast", ACTION_MOVE + 9.0f)}));
    // this is bugged: it doesn't work, and stops glaive throw working (which is needed to take down gate)
    // triggers.push_back(new TriggerNode("in vehicle", { NextAction("blade salvo", ACTION_MOVE + 9.0f)}));
    triggers.push_back(new TriggerNode("in vehicle", { NextAction("glaive throw", ACTION_MOVE + 9.0f)}));
    AddBgDirectorNodes(triggers);
}

void ArenaStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("no possible targets", { NextAction("arena tactics", ACTION_BG)}));
    // Periodic cadence for the patch-0005 coordination flow: "no possible targets" alone goes
    // quiet once the bot has a victim, so mid-fight focus swaps need this "often" re-run
    // (relevance mirrors BattlegroundStrategy's "bg check objective" ACTION_BG + 1).
    triggers.push_back(new TriggerNode("often", { NextAction("arena tactics", ACTION_BG + 1)}));
    // Trinket-out-of-CC (Task 11): isUseful gates on InArena + CC'd + medallion equipped;
    // ACTION_EMERGENCY + 1 so a CC break preempts everything else the tick it fires. "timer"
    // (1/s), NOT "often": the action's ReactMs probabilistic-hold math assumes ~1s evaluation
    // cadence, and "often" is a sparse RandomTrigger (~10s expected) that would leave even a
    // full-sharp bot sitting in CC.
    // No healer-dispel node: the base contexts ship no generic dispel/cure trigger+action pair
    // (class AIs dispel via their own "cure" strategies) — documented skip, don't invent one.
    // No defensives node either: class AIs already carry low-HP defensives (Task 10/11 scope).
    triggers.push_back(new TriggerNode("timer", { NextAction("arena pvp trinket", ACTION_EMERGENCY + 1)}));
}
