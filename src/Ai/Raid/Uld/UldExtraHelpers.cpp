/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldExtraHelpers.h"
#include "AiObjectContext.h"
#include "Group.h"
#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <array>
#include <iterator>
#include <utility>
#include <vector>

namespace UlduarExtraHelpers
{
Unit* GetNearestNpc(PlayerbotAI* botAI, uint32 entry, float maxDistance)
{
    Player* bot = botAI->GetBot();
    Unit* nearest = nullptr;
    float nearestDistance = maxDistance;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
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

bool HasDifficultyAura(Unit const* unit, uint32 baseSpellId, Unit const* caster)
{
    if (unit->HasAura(baseSpellId))
        return true;
    return caster && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(baseSpellId, caster));
}

bool IsCastingDifficultySpell(Unit* caster, uint32 baseSpellId, int32* remainingMs)
{
    if (!caster)
        return false;

    uint32 const difficultyId = sSpellMgr->GetSpellIdForDifficulty(baseSpellId, caster);
    for (CurrentSpellTypes type : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
    {
        Spell* spell = caster->GetCurrentSpell(type);
        if (spell && (spell->m_spellInfo->Id == baseSpellId || spell->m_spellInfo->Id == difficultyId))
        {
            if (remainingMs)
                *remainingMs = spell->GetCastTimeRemaining();
            return true;
        }
    }
    return false;
}

bool IsUlduarHardMode(PlayerbotAI* botAI)
{
    return botAI->HasStrategy("uld hard mode", BotState::BOT_STATE_COMBAT);
}

static uint8 GetKillPriority(PlayerbotAI* botAI, Unit* unit)
{
    switch (unit->GetEntry())
    {
        case NPC_IRON_CONSTRUCT:
            // Only Brittle constructs: a big enough hit shatters them and takes a stack off Ignis.
            return HasDifficultyAura(unit, SPELL_IGNIS_BRITTLE, unit) ? 3 : 0;
        case NPC_HEART_OF_THE_DECONSTRUCTOR:
            // The heart takes double damage that carries over to XT-002; guides stop short of killing it
            // unless they want hard mode.
            return IsUlduarHardMode(botAI) || unit->GetHealthPct() > XT_HEART_STOP_PCT ? 3 : 0;
        case NPC_XT_LIFE_SPARK:
            // Hard mode: guides kill each Life Spark quickly.
            return 3;
        case NPC_SANCTUM_SENTRY:
            return 2;
        case NPC_SARONITE_ANIMUS:
            // Vezax's Saronite Barrier holds while it lives; guides put all DPS on it.
            return 3;
        case NPC_EMERGENCY_FIRE_BOT:
            // Its Deafening Siren silences healers; guides kill these quickly.
            return 3;
        case NPC_STEELBREAKER:
            return IsUlduarHardMode(botAI) ? 4 : 6;
        case NPC_RUNEMASTER_MOLGEIM:
            return 5;
        case NPC_STORMCALLER_BRUNDIR:
            return IsUlduarHardMode(botAI) ? 6 : 4;
        default:
            return 0;
    }
}

Unit* GetKillOrderMarkTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Unit* best = nullptr;
    uint8 bestPriority = 0;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !unit->IsInCombat() || !bot->IsValidAttackTarget(unit))
            continue;

        uint8 const priority = GetKillPriority(botAI, unit);
        if (!priority)
            continue;

        if (priority > bestPriority || (priority == bestPriority && unit->GetHealthPct() < best->GetHealthPct()))
        {
            best = unit;
            bestPriority = priority;
        }
    }

    if (!best)
        return nullptr;

    // Keep a skull that is still in the top tier, so the raid doesn't split as health values cross.
    Unit* skull = botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::skullIndex));
    if (skull && skull->IsAlive() && GetKillPriority(botAI, skull) >= bestPriority)
        return nullptr;

    return best;
}

static bool IsOffTankAdd(uint32 entry)
{
    switch (entry)
    {
        case NPC_XM024_PUMMELLER:
        case NPC_XT_LIFE_SPARK:
        case NPC_SANCTUM_SENTRY:
        case NPC_SARONITE_ANIMUS:
        case NPC_FERAL_DEFENDER:
        case NPC_LIVING_CONSTELLATION:
        case NPC_IRON_CONSTRUCT:
            return true;
        default:
            return false;
    }
}

Unit* GetUntankedOffTankAdd(PlayerbotAI* botAI)
{
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !unit->IsInCombat() || !IsOffTankAdd(unit->GetEntry()))
            continue;

        // A Brittle construct is stunned and waits for its shatter.
        if (unit->GetEntry() == NPC_IRON_CONSTRUCT && HasDifficultyAura(unit, SPELL_IGNIS_BRITTLE, unit))
            continue;

        Unit* victim = unit->GetVictim();
        Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
        if (!victimPlayer || !PlayerbotAI::IsTank(victimPlayer))
            return unit;
    }
    return nullptr;
}

bool GetIgnisConstructDestination(PlayerbotAI* botAI, Position& destination)
{
    Player* bot = botAI->GetBot();
    bool anyMolten = false;
    bool anyNeedsHeat = false;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_IRON_CONSTRUCT || unit->GetVictim() != bot)
            continue;

        if (HasDifficultyAura(unit, SPELL_IGNIS_MOLTEN, unit))
            anyMolten = true;
        else if (!HasDifficultyAura(unit, SPELL_IGNIS_BRITTLE, unit))
            anyNeedsHeat = true;
    }

    // In the core a Molten construct turns Brittle within 18y of a water trigger.
    if (anyMolten)
    {
        constexpr float WATER_SEARCH_DISTANCE = 80.0f;
        if (Creature* water = bot->FindNearestCreature(NPC_WATER_TRIGGER, WATER_SEARCH_DISTANCE))
        {
            destination = water->GetPosition();
            return true;
        }
        return false;
    }

    // Heat stacks build while the construct stands in Ignis' Scorched Ground.
    if (anyNeedsHeat)
    {
        // Every Scorch leaves one, but the core only lights those 25y or more from the water.
        Unit* nearest = nullptr;
        for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || unit->GetEntry() != NPC_SCORCHED_GROUND ||
                !HasDifficultyAura(unit, SPELL_SCORCHED_GROUND, unit))
                continue;
            if (!nearest || bot->GetExactDist2d(unit) < bot->GetExactDist2d(nearest))
                nearest = unit;
        }
        if (nearest)
        {
            destination = nearest->GetPosition();
            return true;
        }
    }
    return false;
}

static float GetSpellAreaRadius(uint32 spellId)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return 0.0f;

    float radius = 0.0f;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        radius = std::max(radius, info->Effects[i].CalcRadius());
        if (uint32 const triggered = info->Effects[i].TriggerSpell)
            if (SpellInfo const* triggeredInfo = sSpellMgr->GetSpellInfo(triggered))
                for (uint8 j = 0; j < MAX_SPELL_EFFECTS; ++j)
                    radius = std::max(radius, triggeredInfo->Effects[j].CalcRadius());
    }
    return radius;
}

float GetXtDebuffRadius(Player* bot)
{
    // Read from the spell data; the fallback is ours, for spell data without an area.
    constexpr float FALLBACK_XT_DEBUFF_RADIUS = 12.0f;
    constexpr float XT_SEARCH_DISTANCE = 120.0f;
    constexpr uint32 NPC_XT002 = 33293;
    Unit* xt = bot->FindNearestCreature(NPC_XT002, XT_SEARCH_DISTANCE);

    float radius = -1.0f;
    for (uint32 const baseSpellId : { uint32(SPELL_SEARING_LIGHT), uint32(SPELL_GRAVITY_BOMB) })
    {
        if (!HasDifficultyAura(bot, baseSpellId, xt))
            continue;

        uint32 const spellId = xt ? sSpellMgr->GetSpellIdForDifficulty(baseSpellId, xt) : baseSpellId;
        float const spellRadius = GetSpellAreaRadius(spellId);
        radius = std::max(radius, spellRadius > 0.0f ? spellRadius : FALLBACK_XT_DEBUFF_RADIUS);
    }
    return std::max(radius, 0.0f);
}

float GetXtVoidZoneAvoidDistance()
{
    // Read from the spell data; the fallback and margin are ours.
    constexpr float FALLBACK_CONSUMPTION_RADIUS = 6.0f;
    constexpr float VOID_ZONE_MARGIN = 2.0f;
    float const radius = GetSpellAreaRadius(SPELL_XT_CONSUMPTION);
    return (radius > 0.0f ? radius : FALLBACK_CONSUMPTION_RADIUS) + VOID_ZONE_MARGIN;
}

Position GetAuriayaStackSpot(Unit const* auriaya)
{
    // In front of her, inside the Sonic Screech cone, close enough that Feral Rush has no distant target.
    constexpr float STACK_DEPTH = 6.0f;
    Position spot = auriaya->GetPosition();
    spot.RelocatePolarOffset(0.0f, auriaya->GetCombatReach() + STACK_DEPTH);
    return spot;
}

Unit* GetInterruptTarget(PlayerbotAI* botAI)
{
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (IsCastingDifficultySpell(unit, SPELL_SENTINEL_BLAST) ||
            IsCastingDifficultySpell(unit, SPELL_BRUNDIR_CHAIN_LIGHTNING) ||
            IsCastingDifficultySpell(unit, SPELL_BRUNDIR_LIGHTNING_WHIRL) ||
            IsCastingDifficultySpell(unit, SPELL_VEZAX_SEARING_FLAMES) ||
            IsCastingDifficultySpell(unit, SPELL_GUARDIAN_DARK_VOLLEY))
            return unit;
    }
    return nullptr;
}

Unit* GetAssignedCouncilMember(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsTank(bot))
        return nullptr;

    std::array<Player*, 3> const tanks = { EncounterHelpers::GetGroupMainTank(bot),
                                           EncounterHelpers::GetGroupAssistTank(bot, 0),
                                           EncounterHelpers::GetGroupAssistTank(bot, 1) };
    auto const it = std::find(tanks.begin(), tanks.end(), bot);
    if (it == tanks.end())
        return nullptr;

    constexpr float COUNCIL_SEARCH_DISTANCE = 100.0f;
    std::vector<Unit*> living;
    for (uint32 const entry : { uint32(NPC_STEELBREAKER), uint32(NPC_RUNEMASTER_MOLGEIM),
                                uint32(NPC_STORMCALLER_BRUNDIR) })
        if (Creature* member = bot->FindNearestCreature(entry, COUNCIL_SEARCH_DISTANCE))
            if (member->IsInCombat())
                living.push_back(member);

    if (living.empty())
        return nullptr;

    // A tank whose member has died moves on to the next living one.
    size_t const index = std::distance(tanks.begin(), it);
    return living[std::min(index, living.size() - 1)];
}

uint32 GetCrunchArmorStacks(Unit const* unit)
{
    return std::max(EncounterHelpers::GetAuraStackAmount(unit, SPELL_CRUNCH_ARMOR_10),
                    EncounterHelpers::GetAuraStackAmount(unit, SPELL_CRUNCH_ARMOR_25));
}

Unit* GetHodirIceBlock(PlayerbotAI* botAI)
{
    float const range = botAI->GetRange("spell");
    if (Unit* player = GetNearestNpc(botAI, NPC_FLASH_FREEZE_PLAYER, range))
        return player;
    return GetNearestNpc(botAI, NPC_FLASH_FREEZE_HELPER, range);
}

Unit* GetFreyaIronRoots(PlayerbotAI* botAI)
{
    float const range = botAI->GetRange("spell");
    Unit* roots = GetNearestNpc(botAI, NPC_IRON_ROOTS, range);
    Unit* strengthened = GetNearestNpc(botAI, NPC_STRENGTHENED_IRON_ROOTS, range);
    if (!roots || !strengthened)
        return roots ? roots : strengthened;

    Player* bot = botAI->GetBot();
    return bot->GetExactDist2d(roots) <= bot->GetExactDist2d(strengthened) ? roots : strengthened;
}

float GetNaturesFuryRadius(Player* bot)
{
    // Read from the spell data; the fallback is ours, for spell data without an area.
    constexpr float FALLBACK_NATURES_FURY_RADIUS = 10.0f;
    if (!HasDifficultyAura(bot, SPELL_NATURES_FURY, bot))
        return 0.0f;

    float const radius = GetSpellAreaRadius(sSpellMgr->GetSpellIdForDifficulty(SPELL_NATURES_FURY, bot));
    return radius > 0.0f ? radius : FALLBACK_NATURES_FURY_RADIUS;
}

Unit* GetFreyaSunBeamInRange(PlayerbotAI* botAI, float& safeDistance)
{
    // Read from the spell data; the fallback and margin are ours.
    constexpr float FALLBACK_SUN_BEAM_RADIUS = 5.0f;
    constexpr float SUN_BEAM_MARGIN = 2.0f;
    Player* bot = botAI->GetBot();
    float const radius = GetSpellAreaRadius(sSpellMgr->GetSpellIdForDifficulty(SPELL_UNSTABLE_ENERGY_FREYA, bot));
    safeDistance = (radius > 0.0f ? radius : FALLBACK_SUN_BEAM_RADIUS) + SUN_BEAM_MARGIN;

    for (uint32 const entry : { uint32(NPC_FREYA_SUN_BEAM), uint32(NPC_BRIGHTLEAF_SUN_BEAM) })
        if (Unit* beam = GetNearestNpc(botAI, entry, safeDistance))
            return beam;
    return nullptr;
}

Unit* GetSifInNovaRange(PlayerbotAI* botAI, float& safeDistance)
{
    // Read from the spell data; the fallback and margin are ours.
    constexpr float FALLBACK_FROST_NOVA_RADIUS = 10.0f;
    constexpr float FROST_NOVA_MARGIN = 3.0f;
    float const radius = GetSpellAreaRadius(SPELL_SIF_FROST_NOVA);
    safeDistance = (radius > 0.0f ? radius : FALLBACK_FROST_NOVA_RADIUS) + FROST_NOVA_MARGIN;
    return GetNearestNpc(botAI, NPC_SIF, safeDistance);
}

Unit* GetSifBlizzardInRange(PlayerbotAI* botAI, float& safeDistance)
{
    // Read from the spell data; the fallback and margin are ours.
    constexpr float FALLBACK_BLIZZARD_RADIUS = 8.0f;
    constexpr float BLIZZARD_MARGIN = 2.0f;
    float const radius = GetSpellAreaRadius(SPELL_SIF_BLIZZARD);
    safeDistance = (radius > 0.0f ? radius : FALLBACK_BLIZZARD_RADIUS) + BLIZZARD_MARGIN;

    // Other Thorim Controllers share the entry; only the one carrying the Blizzard matters.
    Unit* controller = GetNearestNpc(botAI, NPC_SIF_BLIZZARD, safeDistance);
    return controller && HasDifficultyAura(controller, SPELL_SIF_BLIZZARD, controller) ? controller : nullptr;
}

Unit* GetMimironFlamesInRange(PlayerbotAI* botAI, float& safeDistance)
{
    // Read from the spell data; the fallback and margin are ours.
    constexpr float FALLBACK_FLAMES_RADIUS = 4.0f;
    constexpr float FLAMES_MARGIN = 1.5f;
    float const radius = GetSpellAreaRadius(SPELL_MIMIRON_FLAMES);
    safeDistance = (radius > 0.0f ? radius : FALLBACK_FLAMES_RADIUS) + FLAMES_MARGIN;
    return GetNearestNpc(botAI, NPC_MIMIRON_FLAMES, safeDistance);
}

Unit* GetMimironFrostBombInRange(PlayerbotAI* botAI, float& safeDistance)
{
    // Read from the spell data; the fallback and margin are ours.
    constexpr float FALLBACK_FROST_BOMB_RADIUS = 10.0f;
    constexpr float FROST_BOMB_MARGIN = 2.0f;
    float const radius = std::max(GetSpellAreaRadius(SPELL_FROST_BOMB_EXPLOSION_10),
                                  GetSpellAreaRadius(SPELL_FROST_BOMB_EXPLOSION_25));
    safeDistance = (radius > 0.0f ? radius : FALLBACK_FROST_BOMB_RADIUS) + FROST_BOMB_MARGIN;
    return GetNearestNpc(botAI, NPC_MIMIRON_FROST_BOMB, safeDistance);
}

Unit* GetLeviathanTowerHazard(PlayerbotAI* botAI, Unit const* vehicle, float& safeDistance)
{
    // Read from the spell data; the fallback is ours, and the margin covers the vehicle's own size.
    constexpr float FALLBACK_TOWER_STRIKE_RADIUS = 10.0f;
    constexpr float VEHICLE_MARGIN = 5.0f;
    constexpr std::array<std::pair<uint32, uint32>, 3> hazards = { {
        { NPC_HODIRS_FURY_TARGET, SPELL_HODIRS_FURY },
        { NPC_THORIMS_HAMMER_TARGET, SPELL_THORIMS_HAMMER },
        { NPC_MIMIRONS_INFERNO_TARGET, SPELL_MIMIRONS_INFERNO_DAMAGE },
    } };

    Unit* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        for (auto const& [entry, spellId] : hazards)
        {
            if (unit->GetEntry() != entry)
                continue;

            float const radius = GetSpellAreaRadius(spellId);
            float const reach = (radius > 0.0f ? radius : FALLBACK_TOWER_STRIKE_RADIUS) + VEHICLE_MARGIN;
            float const distance = vehicle->GetExactDist2d(unit);
            if (distance < reach && (!nearest || distance < nearestDistance))
            {
                nearest = unit;
                nearestDistance = distance;
                safeDistance = reach;
            }
        }
    }
    return nearest;
}
}
