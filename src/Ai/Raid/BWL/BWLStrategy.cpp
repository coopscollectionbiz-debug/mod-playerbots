/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BWLStrategy.h"
#include "BWLHelpers.h"
#include "BWLMultipliers.h"
#include "Log.h"
#include "Playerbots.h"

#include <mutex>

void RaidBwlStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    static std::once_flag s_wyrmguardCanary;
    std::call_once(s_wyrmguardCanary, []
    {
        LOG_INFO("playerbots", "[BWLWyrmguard] pack strategy loaded (patch 0020)");
    });
    static std::once_flag s_nefarianCanary;
    std::call_once(s_nefarianCanary, []
    {
        LOG_INFO("playerbots", "[BWLNefarian] strategy loaded (patch 0020)");
    });

    triggers.push_back(new TriggerNode("often", {
        NextAction("bwl check onyxia scale cloak", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl suppression device", {
        NextAction("bwl turn off suppression device", ACTION_RAID) }));

    triggers.push_back(new TriggerNode("bwl razorgore fire resistance", {
        NextAction("bwl razorgore fire resistance", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl razorgore not mind controlled", {
        NextAction("bwl razorgore avoid aoe", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl razorgore not mind controlled", {
        NextAction("bwl razorgore mark boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("bwl vaelastrasz fire resistance", {
        NextAction("bwl vaelastrasz fire resistance", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl vaelastrasz positioning", {
        NextAction("rear flank", ACTION_MOVE + 4) }));
    triggers.push_back(new TriggerNode("bwl vaelastrasz burning adrenaline", {
        NextAction("bwl vaelastrasz move away", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode("bwl broodlord fire resistance", {
        NextAction("bwl broodlord fire resistance", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl broodlord ranged too close", {
        NextAction("bwl broodlord ranged move away", ACTION_RAID) }));

    triggers.push_back(new TriggerNode("bwl firemaw fire resistance", {
        NextAction("bwl firemaw fire resistance", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl firemaw not victim", {
        NextAction("bwl firemaw avoid breath", ACTION_MOVE + 4) }));
    triggers.push_back(new TriggerNode("bwl ebonroc not victim", {
        NextAction("bwl ebonroc avoid breath", ACTION_MOVE + 4) }));
    triggers.push_back(new TriggerNode("bwl flamegor fire resistance", {
        NextAction("bwl flamegor fire resistance", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl flamegor not victim", {
        NextAction("bwl flamegor avoid breath", ACTION_MOVE + 4) }));

    triggers.push_back(new TriggerNode("bwl chromaggus fire resistance", {
        NextAction("bwl chromaggus fire resistance", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl affliction bronze", {
        NextAction("bwl use hourglass sand", ACTION_RAID) }));

    triggers.push_back(new TriggerNode("bwl nefarian positioning", {
        NextAction("bwl nefarian rear flank", ACTION_MOVE + 4) }));
    triggers.push_back(new TriggerNode("bwl nefarian wild magic", {
        NextAction("ice block", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl nefarian corrupted totem", {
        NextAction("bwl nefarian kill corrupted totem", ACTION_RAID + 1) }));

    // Nefarian phases 2 and 3 (overlay patch 0020, spec 2026-10-07 §4.7)
    triggers.push_back(new TriggerNode("bwl nefarian mt station", {
        NextAction("bwl nefarian mt station", ACTION_RAID + 3) }));
    triggers.push_back(new TriggerNode("bwl nefarian position", {
        NextAction("bwl nefarian position", ACTION_RAID + 3) }));
    triggers.push_back(new TriggerNode("bwl nefarian dps target", {
        NextAction("bwl nefarian dps target", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("bwl nefarian ot target", {
        NextAction("bwl nefarian ot target", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("bwl nefarian dispel polymorph", {
        NextAction("bwl nefarian dispel polymorph", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("bwl nefarian priest tank only", {
        NextAction("bwl nefarian priest tank only", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("bwl nefarian decurse tank", {
        NextAction("bwl nefarian decurse tank", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("bwl nefarian tremor", {
        NextAction("bwl nefarian tremor", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl nefarian hunter rearm", {
        NextAction("bwl nefarian hunter rearm", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("bwl nefarian rogue survive", {
        NextAction("bwl nefarian rogue survive", ACTION_RAID + 1) }));
    // Dev attempt 1 fixes (spec §9)
    // Just above the stock tank rotation (Revenge ACTION_HIGH + 2, Heroic Strike ACTION_HIGH).
    triggers.push_back(new TriggerNode("bwl nefarian mt sunder", {
        NextAction("bwl nefarian mt sunder", ACTION_HIGH + 3) }));

    triggers.push_back(new TriggerNode("bwl death talon wyrmguard tank", {
        NextAction("bwl death talon wyrmguard tank move away", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("bwl death talon wyrmguard ranged", {
        NextAction("bwl death talon wyrmguard ranged move away", ACTION_RAID) }));

    // BWL-wide DPS threat discipline (spec 2026-10-08): the hard stop. The multiplier's vetoes cannot
    // stop an auto-attack or auto-shot that is already running.
    triggers.push_back(new TriggerNode("bwl threat hold", {
        NextAction("bwl threat hold", ACTION_RAID + 1) }));

    // Death Talon Wyrmguard packs (overlay patch 0020)
    triggers.push_back(new TriggerNode("bwl wyrmguard tank", {
        NextAction("bwl wyrmguard tank", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("bwl wyrmguard mark skull", {
        NextAction("bwl wyrmguard mark skull", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("bwl wyrmguard dps target", {
        NextAction("bwl wyrmguard dps target", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("bwl wyrmguard melee position", {
        NextAction("bwl wyrmguard melee position", ACTION_RAID + 3) }));
    triggers.push_back(new TriggerNode("bwl wyrmguard melee behind", {
        NextAction("rear flank", ACTION_MOVE + 4) }));
    triggers.push_back(new TriggerNode("bwl wyrmguard ebonroc keep out", {
        NextAction("bwl wyrmguard ebonroc keep out", ACTION_RAID + 4) }));
    // Spec 4.8a: above light/medium heals - a healer out of range or LoS of a tank cannot help him -
    // and below critical heals. It fires only while the healer's spot is not good.
    triggers.push_back(new TriggerNode("bwl wyrmguard healer position", {
        NextAction("bwl wyrmguard healer position", ACTION_MEDIUM_HEAL + 1) }));
}

void RaidBwlStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new RazorgoreTankMultiplier(botAI));
    multipliers.push_back(new VaelastraszTankMultiplier(botAI));
    multipliers.push_back(new VaelastraszBurningAdrenalineMultiplier(botAI));
    multipliers.push_back(new NefarianCorruptedHealingMultiplier(botAI));
    multipliers.push_back(new NefarianCorruptedTotemMultiplier(botAI));
    multipliers.push_back(new BwlWyrmguardMultiplier(botAI));
    multipliers.push_back(new BwlNefarianMultiplier(botAI));
    multipliers.push_back(new BwlThreatDisciplineMultiplier(botAI));
}

void RaidBwlStrategy::AppendTargetExclusions(GuidSet& exclusions, TargetValueExclusionType type)
{
    AiObjectContext* context = botAI->GetAiObjectContext();
    if (type == TargetValueExclusionType::Dps || type == TargetValueExclusionType::Attacker)
    {
        // Nefarian is not a target while Bone Constructs are in combat, so DPS clear them first. His tank stays on him.
        Player* bot = botAI->GetBot();
        if (!PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsHeal(bot) &&
            BlackwingLairHelpers::HasBoneConstructAttacker(botAI))
        {
            if (Unit* nefarian = AI_VALUE2(Unit*, "find target", "nefarian"))
                exclusions.insert(nefarian->GetGUID());
        }
        return;
    }

    if (type != TargetValueExclusionType::Tank)
        return;

    if (Unit* broodlord = AI_VALUE2(Unit*, "find target", "broodlord lashlayer"))
    {
        Unit* bossVictim = broodlord->GetVictim();
        Player* bossVictimPlayer = bossVictim ? bossVictim->ToPlayer() : nullptr;

        Unit* bot = botAI->GetBot();
        if (!bossVictimPlayer || !PlayerbotAI::IsTank(bossVictimPlayer) || bossVictim == bot)
        {
            // Only the tank actually holding the boss keeps it as its target at all times.
            // All other tanks may switch to non-whelp targets (other branch).
            // This should not happen, if the elites were killed before the encounter, but just in case...
            // If the boss targets a non-tank, all tanks should ignore the adds and attack the boss only.
            for (ObjectGuid const guid : AI_VALUE(GuidVector, "attackers"))
            {
                Unit* unit = botAI->GetUnit(guid);
                if (unit != broodlord)
                    exclusions.insert(guid);
            }
        }
        else
        {
            // Prevents the tanks from changing targets to a whelp during the encounter.
            // Tanks should build threat on the boss. DPS handles whelps on their own.
            // Change target if it isn't a whelp (boss or respawned elite NPCs).
            for (ObjectGuid const guid : AI_VALUE(GuidVector, "attackers"))
            {
                Unit* unit = botAI->GetUnit(guid);
                if (BlackwingLairHelpers::IsCorruptedWhelp(unit))
                    exclusions.insert(guid);
            }
        }
    }

    if (Unit* nefarian = AI_VALUE2(Unit*, "find target", "nefarian"))
    {
        // The tank holding Nefarian ignores everything else, e.g. the Bone Constructs he raises at 20%.
        // While no tank holds him (tank died), the main tank, or the first living assist tank, takes him.
        Player* bot = botAI->GetBot();
        Unit* bossVictim = nefarian->GetVictim();
        Player* bossVictimPlayer = bossVictim ? bossVictim->ToPlayer() : nullptr;
        bool holdsBoss = bossVictim == bot;
        if (!holdsBoss && (!bossVictimPlayer || !PlayerbotAI::IsTank(bossVictimPlayer)))
        {
            Unit* mainTank = botAI->GetUnit(PlayerbotAI::GetMainTankGuid(bot->GetGroup()));
            holdsBoss = mainTank && mainTank->IsAlive() ? PlayerbotAI::IsMainTank(bot)
                                                        : PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
        }

        if (holdsBoss)
        {
            for (ObjectGuid const guid : AI_VALUE(GuidVector, "attackers"))
            {
                if (guid != nefarian->GetGUID())
                    exclusions.insert(guid);
            }
        }
        // While a tank holds him, the other tanks stay on the constructs. Loose on the raid, any tank may take him.
        else if (bossVictimPlayer && PlayerbotAI::IsTank(bossVictimPlayer) &&
                 BlackwingLairHelpers::HasBoneConstructAttacker(botAI))
            exclusions.insert(nefarian->GetGUID());
    }
}
