/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "VoATriggers.h"
#include "EncounterHelpers.h"
#include "EventMap.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellInfo.h"

bool EmalonMarkBossTrigger::IsActive()
{
    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
    {
        return false;
    }

    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if boss have skull mark
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    int8 skullIndex = 7;  // Skull
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    if (currentSkullTarget == boss->GetGUID())
    {
        return false;
    }

    // Check if there is any overcharged minion
    Unit* overchargedMinion = nullptr;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit)
            continue;

        uint32 entry = unit->GetEntry();
        if (entry == NPC_TEMPEST_MINION && unit->HasAura(AURA_OVERCHARGE))
        {
            overchargedMinion = unit;
            break;
        }
    }
    if (overchargedMinion)
    {
        return false;
    }

    return true;
}

bool EmalonLightingNovaTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Tank dont need to move
    if (botAI->IsTank(bot))
    {
        return false;
    }

    // Check if boss is casting Lightning Nova
    bool isCasting = boss->HasUnitState(UNIT_STATE_CASTING);
    bool isLightingNova = boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_NOVA_10_MAN) ||
                          boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_NOVA_25_MAN);
    return isCasting && isLightingNova;
}

bool EmalonOverchargeTrigger::IsActive()
{
    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
    {
        return false;
    }

    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if there is any overcharged minion
    Unit* overchargedMinion = nullptr;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit)
            continue;

        uint32 entry = unit->GetEntry();
        if (entry == NPC_TEMPEST_MINION && unit->HasAura(AURA_OVERCHARGE))
        {
            overchargedMinion = unit;
            break;
        }
    }
    if (!overchargedMinion)
    {
        return false;
    }

    // Check if minion have skull mark
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    int8 skullIndex = 7;  // Skull
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    if (currentSkullTarget == overchargedMinion->GetGUID())
    {
        return false;
    }

    return true;
}

bool EmalonFallFromFloorTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if bot is on the floor
    return bot->GetPositionZ() < 80.0f;
}

//
// Archavon the Stone Watcher
//
bool ArchavonNotTankedTrigger::IsActive()
{
    // Stomp and Impale wipe the tank's threat, so Archavon turns on whoever is next on his list.
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "archavon the stone watcher");
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    if (!victim)
        return false;

    Player* victimPlayer = victim->ToPlayer();
    if (victimPlayer && PlayerbotAI::IsTank(victimPlayer))
        return false;

    // A tank still held up in the air by Impale can't take him.
    return EncounterHelpers::GetTakeoverTank(bot, victim, [](Player const* tank)
        { return tank->HasUnitState(UNIT_STATE_STUNNED) || tank->IsFalling(); }) == bot;
}

bool ArchavonRockShardsTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "archavon the stone watcher");
    if (!boss)
        return false;

    for (CurrentSpellTypes type : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
    {
        Spell const* spell = boss->GetCurrentSpell(type);
        if (spell && spell->m_spellInfo->Id == SPELL_ARCHAVON_ROCK_SHARDS && spell->m_targets.GetUnitTarget() == bot)
            return EncounterHelpers::GetNearestPlayerInRadius(bot, ARCHAVON_ROCK_SHARDS_SPREAD) != nullptr;
    }
    return false;
}

//
// Toravon the Ice Watcher
//
bool ToravonFrostbiteSwapTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "toravon the ice watcher");
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    if (!victim || victim == bot ||
        EncounterHelpers::GetAuraStackAmount(victim, SPELL_TORAVON_FROSTBITE) < TORAVON_FROSTBITE_SWAP_STACKS)
        return false;

    return EncounterHelpers::GetTakeoverTank(bot, victim, [](Player const* tank)
        {
            return EncounterHelpers::GetAuraStackAmount(tank, SPELL_TORAVON_FROSTBITE) >=
                   TORAVON_FROSTBITE_SWAP_STACKS;
        }) == bot;
}

bool ToravonFrozenOrbTrigger::IsActive()
{
    // Guides send ranged DPS onto the orbs the moment they spawn.
    if (!PlayerbotAI::IsRangedDps(bot) || !AI_VALUE2(Unit*, "find target", "toravon the ice watcher"))
        return false;

    Unit* victim = bot->GetVictim();
    if (victim && victim->GetEntry() == NPC_TORAVON_FROZEN_ORB && victim->IsAlive())
        return false;

    return GetNearestToravonOrb(botAI, botAI->GetRange("spell")) != nullptr;
}

Unit* GetNearestToravonOrb(PlayerbotAI* botAI, float maxDistance)
{
    Player* bot = botAI->GetBot();
    Unit* nearest = nullptr;
    float nearestDistance = maxDistance;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_TORAVON_FROZEN_ORB)
            continue;

        float const distance = bot->GetExactDist2d(unit);
        if (distance < nearestDistance)
        {
            nearest = unit;
            nearestDistance = distance;
        }
    }
    return nearest;
}
