/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TotcHelpers.h"
#include "Group.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace TotcHelpers
{

namespace
{
std::mutex twinsStateMutex;
std::unordered_map<uint32, bool> twinsLightPhase;

// The core's essence ids for 10, 25, 10 heroic and 25 heroic.
constexpr std::array<uint32, 4> LIGHT_ESSENCE_IDS = { 65686, 67222, 67223, 67224 };
constexpr std::array<uint32, 4> DARK_ESSENCE_IDS = { 65684, 67176, 67177, 67178 };

constexpr std::array<uint32, 28> FACTION_CHAMPIONS = {
    34461, 34460, 34469, 34467, 34468, 34465, 34471, 34466, 34473, 34472, 34463, 34470, 34474, 34475,
    34458, 34451, 34459, 34448, 34449, 34445, 34456, 34447, 34441, 34454, 34455, 34444, 34450, 34453,
};
}

bool IsTotcEncounter(Player* bot)
{
    return bot->GetMapId() == TOTC_MAP_ID && bot->IsInCombat();
}

Unit* GetNearestNpc(PlayerbotAI* botAI, uint32 entry, float maxDistance, bool attackableOnly)
{
    Player* bot = botAI->GetBot();
    Unit* nearest = nullptr;
    float nearestDistance = maxDistance;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
            continue;

        if (attackableOnly && !bot->IsValidAttackTarget(unit))
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

Unit* GetUntankedNpc(PlayerbotAI* botAI, uint32 entry)
{
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
            continue;

        Unit* victim = unit->GetVictim();
        Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
        if (!victimPlayer || !PlayerbotAI::IsTank(victimPlayer))
            return unit;
    }
    return nullptr;
}

bool IsCastingDifficultySpell(Unit* caster, uint32 baseSpellId)
{
    if (!caster)
        return false;

    uint32 const difficultyId = sSpellMgr->GetSpellIdForDifficulty(baseSpellId, caster);
    for (CurrentSpellTypes const type : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
    {
        Spell* spell = caster->GetCurrentSpell(type);
        if (spell && (spell->m_spellInfo->Id == baseSpellId || spell->m_spellInfo->Id == difficultyId))
            return true;
    }
    return false;
}

bool HasDifficultyAura(Unit const* unit, uint32 baseSpellId)
{
    return unit && (unit->HasAura(baseSpellId) ||
                    unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(baseSpellId, unit)));
}

uint32 GetDifficultyAuraStacks(Unit const* unit, uint32 baseSpellId)
{
    if (!unit)
        return 0;

    Aura const* aura = unit->GetAura(baseSpellId);
    if (!aura)
        aura = unit->GetAura(sSpellMgr->GetSpellIdForDifficulty(baseSpellId, unit));
    return aura ? aura->GetStackAmount() : 0;
}

float GetSpellAreaRadius(uint32 spellId, float fallback)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return fallback;

    float radius = 0.0f;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        radius = std::max(radius, info->Effects[i].CalcRadius());
    return radius > 0.0f ? radius : fallback;
}

Unit* GetTotcInterruptTarget(PlayerbotAI* botAI)
{
    Unit* jaraxxus = GetNearestNpc(botAI, NPC_JARAXXUS, TOTC_SEARCH_DISTANCE);
    if (IsCastingDifficultySpell(jaraxxus, SPELL_FEL_FIREBALL))
        return jaraxxus;

    if (Unit* pactCaster = GetTwinCastingPact(botAI))
        return pactCaster;

    Unit* burrower = GetNearestNpc(botAI, NPC_NERUBIAN_BURROWER, TOTC_SEARCH_DISTANCE);
    return IsCastingDifficultySpell(burrower, SPELL_SHADOW_STRIKE) ? burrower : nullptr;
}

// The Beasts of Northrend

Player* GetParalyzedGroupMember(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* nearest = nullptr;
    float nearestDistance = TOTC_SEARCH_DISTANCE;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != bot->GetMapId() ||
            !HasDifficultyAura(member, SPELL_PARALYTIC_TOXIN))
        {
            continue;
        }

        float const distance = bot->GetExactDist2d(member);
        if (distance < nearestDistance)
        {
            nearest = member;
            nearestDistance = distance;
        }
    }
    return nearest;
}

bool GetTrampleDodgePosition(PlayerbotAI* botAI, Position& destination)
{
    Player* bot = botAI->GetBot();
    Unit* icehowl = GetNearestNpc(botAI, NPC_ICEHOWL, TOTC_SEARCH_DISTANCE);
    Unit* stalker = GetNearestNpc(botAI, NPC_FURIOUS_CHARGE_STALKER, TOTC_SEARCH_DISTANCE);
    if (!icehowl || !stalker)
        return false;

    // Icehowl charges through the glared spot and on into the arena wall.
    float const dirX = stalker->GetPositionX() - icehowl->GetPositionX();
    float const dirY = stalker->GetPositionY() - icehowl->GetPositionY();
    float const length = std::sqrt(dirX * dirX + dirY * dirY);
    if (length < 0.1f)
        return false;

    float const unitX = dirX / length;
    float const unitY = dirY / length;
    float const toBotX = bot->GetPositionX() - icehowl->GetPositionX();
    float const toBotY = bot->GetPositionY() - icehowl->GetPositionY();
    float const along = toBotX * unitX + toBotY * unitY;
    float const across = toBotX * -unitY + toBotY * unitX;
    if (along < 0.0f || std::abs(across) >= TRAMPLE_DODGE_WIDTH)
        return false;

    // Step out to the side the bot already leans towards.
    constexpr float DODGE_MARGIN = 2.0f;
    float const side = across >= 0.0f ? 1.0f : -1.0f;
    float const step = TRAMPLE_DODGE_WIDTH + DODGE_MARGIN - std::abs(across);
    destination.Relocate(bot->GetPositionX() + -unitY * side * step, bot->GetPositionY() + unitX * side * step,
                         bot->GetPositionZ());
    return true;
}

bool IsJormungarMobile(Unit const* worm)
{
    return worm &&
           (worm->GetDisplayId() == MODEL_ACIDMAW_MOBILE || worm->GetDisplayId() == MODEL_DREADSCALE_MOBILE);
}

Unit* GetAssignedJormungar(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (PlayerbotAI::IsMainTank(bot))
        return GetNearestNpc(botAI, NPC_ACIDMAW, TOTC_SEARCH_DISTANCE);
    if (PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
        return GetNearestNpc(botAI, NPC_DREADSCALE, TOTC_SEARCH_DISTANCE);
    return nullptr;
}

bool GetJormungarMeleePosition(PlayerbotAI* botAI, Unit* worm, Position& destination)
{
    Player* bot = botAI->GetBot();
    if (!worm)
        return false;

    // Behind the mobile worm, at its side for the stationary one; the tolerance is ours.
    constexpr float PI = static_cast<float>(M_PI);
    constexpr float ARC_TOLERANCE = PI / 4.0f;
    bool const mobile = IsJormungarMobile(worm);
    float const wanted = mobile ? PI : PI / 2.0f;
    float const relative = Position::NormalizeOrientation(worm->GetAngle(bot) - worm->GetOrientation());
    float const offset = std::min(std::abs(relative - wanted), std::abs(relative - (2.0f * PI - wanted)));
    if (offset <= ARC_TOLERANCE)
        return false;

    float const side = relative <= PI ? 1.0f : -1.0f;
    float const angle = worm->GetOrientation() + (mobile ? wanted : side * wanted);
    float const distance = worm->GetCombatReach() + 1.0f;
    destination.Relocate(worm->GetPositionX() + distance * std::cos(angle),
                         worm->GetPositionY() + distance * std::sin(angle), bot->GetPositionZ());
    return true;
}

// Lord Jaraxxus

Unit* GetJaraxxusAddToKill(PlayerbotAI* botAI)
{
    for (uint32 const entry : { uint32(NPC_NETHER_PORTAL), uint32(NPC_INFERNAL_VOLCANO), uint32(NPC_MISTRESS_OF_PAIN) })
        if (Unit* add = GetNearestNpc(botAI, entry, TOTC_SEARCH_DISTANCE, true))
            return add;
    return nullptr;
}

// The Faction Champions

bool IsFactionChampionHealer(uint32 entry)
{
    switch (entry)
    {
        case NPC_ALLIANCE_DRUID_RESTORATION:
        case NPC_ALLIANCE_PALADIN_HOLY:
        case NPC_ALLIANCE_PRIEST_DISCIPLINE:
        case NPC_ALLIANCE_SHAMAN_RESTORATION:
        case NPC_HORDE_DRUID_RESTORATION:
        case NPC_HORDE_PALADIN_HOLY:
        case NPC_HORDE_PRIEST_DISCIPLINE:
        case NPC_HORDE_SHAMAN_RESTORATION:
            return true;
        default:
            return false;
    }
}

Unit* GetFactionChampionKillTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Unit* best = nullptr;
    bool bestIsHealer = false;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !bot->IsValidAttackTarget(unit) ||
            std::find(FACTION_CHAMPIONS.begin(), FACTION_CHAMPIONS.end(), unit->GetEntry()) == FACTION_CHAMPIONS.end())
        {
            continue;
        }

        bool const isHealer = IsFactionChampionHealer(unit->GetEntry());
        if (!best || (isHealer && !bestIsHealer) ||
            (isHealer == bestIsHealer && unit->GetHealthPct() < best->GetHealthPct()))
        {
            best = unit;
            bestIsHealer = isHealer;
        }
    }
    return best;
}

char const* GetChampionCrowdControlSpell(Player* bot)
{
    if (!PlayerbotAI::IsDps(bot))
        return nullptr;

    switch (bot->getClass())
    {
        case CLASS_MAGE:
            return "polymorph";
        case CLASS_WARLOCK:
            return "fear";
        case CLASS_DRUID:
            return "cyclone";
        case CLASS_SHAMAN:
            return "hex";
        case CLASS_PALADIN:
            return "repentance";
        default:
            return nullptr;
    }
}

Unit* GetAssignedChampionToControl(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group || !GetChampionCrowdControlSpell(bot))
        return nullptr;

    // This bot's place among the crowd-control bots.
    size_t index = 0;
    bool found = false;
    for (GroupReference* ref = group->GetFirstMember(); ref && !found; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !GET_PLAYERBOT_AI(member) || !GetChampionCrowdControlSpell(member))
            continue;
        if (member == bot)
            found = true;
        else
            ++index;
    }
    if (!found)
        return nullptr;

    // The champions other than the skull, in a fixed order.
    Unit* skull = botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::skullIndex));
    std::vector<Unit*> champions;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit != skull && unit->IsAlive() &&
            std::find(FACTION_CHAMPIONS.begin(), FACTION_CHAMPIONS.end(), unit->GetEntry()) != FACTION_CHAMPIONS.end())
        {
            champions.push_back(unit);
        }
    }
    std::sort(champions.begin(), champions.end(),
              [](Unit const* a, Unit const* b) { return a->GetGUID() < b->GetGUID(); });

    // The healers die first, so the dps champions are the ones to hold.
    std::stable_partition(champions.begin(), champions.end(),
                          [](Unit const* unit) { return !IsFactionChampionHealer(unit->GetEntry()); });
    return index < champions.size() ? champions[index] : nullptr;
}

// The Twin Val'kyr

bool IsTwinsLightPhase(uint32 instanceId)
{
    std::lock_guard lock(twinsStateMutex);
    auto const it = twinsLightPhase.find(instanceId);
    return it != twinsLightPhase.end() && it->second;
}

void SetTwinsLightPhase(uint32 instanceId, bool light)
{
    std::lock_guard lock(twinsStateMutex);
    twinsLightPhase[instanceId] = light;
}

bool WantsLightEssence(PlayerbotAI* botAI)
{
    uint32 const instanceId = botAI->GetBot()->GetInstanceId();
    Unit* fjola = GetNearestNpc(botAI, NPC_FJOLA_LIGHTBANE, TOTC_SEARCH_DISTANCE);
    Unit* eydis = GetNearestNpc(botAI, NPC_EYDIS_DARKBANE, TOTC_SEARCH_DISTANCE);
    if (IsCastingDifficultySpell(fjola, SPELL_LIGHT_VORTEX))
        SetTwinsLightPhase(instanceId, true);
    else if (IsCastingDifficultySpell(eydis, SPELL_DARK_VORTEX))
        SetTwinsLightPhase(instanceId, false);
    return IsTwinsLightPhase(instanceId);
}

bool HasEssence(Player* bot, bool light)
{
    for (uint32 const spellId : light ? LIGHT_ESSENCE_IDS : DARK_ESSENCE_IDS)
        if (bot->HasAura(spellId))
            return true;
    return false;
}

Unit* GetTwinCastingPact(PlayerbotAI* botAI)
{
    Unit* fjola = GetNearestNpc(botAI, NPC_FJOLA_LIGHTBANE, TOTC_SEARCH_DISTANCE);
    if (IsCastingDifficultySpell(fjola, SPELL_LIGHT_TWIN_PACT))
        return fjola;

    Unit* eydis = GetNearestNpc(botAI, NPC_EYDIS_DARKBANE, TOTC_SEARCH_DISTANCE);
    return IsCastingDifficultySpell(eydis, SPELL_DARK_TWIN_PACT) ? eydis : nullptr;
}

Unit* GetTwinToAttack(PlayerbotAI* botAI)
{
    if (Unit* pactCaster = GetTwinCastingPact(botAI))
        return pactCaster;

    // Guides hit the twin of the other color than their essence.
    Player* bot = botAI->GetBot();
    bool const light = HasEssence(bot, true) ||
                       (!HasEssence(bot, false) && IsTwinsLightPhase(bot->GetInstanceId()));
    return GetNearestNpc(botAI, light ? NPC_EYDIS_DARKBANE : NPC_FJOLA_LIGHTBANE, TOTC_SEARCH_DISTANCE);
}

Unit* GetOppositeOrbInRange(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (HasEssence(bot, true))
        return GetNearestNpc(botAI, NPC_CONCENTRATED_DARKNESS, TWINS_ORB_AVOID_DISTANCE);
    if (HasEssence(bot, false))
        return GetNearestNpc(botAI, NPC_CONCENTRATED_LIGHT, TWINS_ORB_AVOID_DISTANCE);
    return nullptr;
}

// Anub'arak

bool IsAnubarakSubmerged(Unit* anubarak)
{
    return anubarak && HasDifficultyAura(anubarak, SPELL_SUBMERGE);
}

Unit* GetNearestPermafrost(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Unit* nearest = nullptr;
    float nearestDistance = TOTC_SEARCH_DISTANCE;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit->GetEntry() != NPC_FROST_SPHERE || !HasDifficultyAura(unit, SPELL_PERMAFROST))
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
}
