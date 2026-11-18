/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPHelpers.h"
#include "AiObjectContext.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <vector>

using namespace EncounterHelpers;

namespace SunwellPlateauHelpers
{
// Guide order: Alythess before Sacrolash; at M'uru the Void Sentinel, then the humanoids, Fury
// Mages first; at Kil'jaeden the Sinister Reflections, and the Hands lowest health first.
static uint8 GetKillPriority(uint32 entry)
{
    switch (entry)
    {
        case NPC_VOID_SENTINEL:
        case NPC_SINISTER_REFLECTION:
            return 4;
        case NPC_SHADOWSWORD_FURY_MAGE:
            return 3;
        case NPC_SHADOWSWORD_BERSERKER:
        case NPC_HAND_OF_THE_DECEIVER:
        case NPC_GRAND_WARLOCK_ALYTHESS:
            return 2;
        case NPC_LADY_SACROLASH:
        case NPC_MURU:
        case NPC_KILJAEDEN:
            return 1;
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

        uint8 const priority = GetKillPriority(unit->GetEntry());
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
    if (skull && skull->IsAlive() && GetKillPriority(skull->GetEntry()) >= bestPriority)
        return nullptr;

    return best;
}

Unit* GetCcMarkTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    ObjectGuid const skullGuid = group->GetTargetIcon(RtiTargetValue::skullIndex);
    Unit* moon = botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::moonIndex));
    if (moon && moon->IsAlive() && moon->GetGUID() != skullGuid)
        return nullptr;

    // The healthiest Berserker is the one the skull reaches last.
    Unit* best = nullptr;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetGUID() == skullGuid ||
            unit->GetEntry() != NPC_SHADOWSWORD_BERSERKER)
            continue;

        if (!best || unit->GetHealthPct() > best->GetHealthPct())
            best = unit;
    }
    return best;
}

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

Unit* FindCreatureAnyRealm(Player* bot, uint32 entry)
{
    constexpr float KALECGOS_ARENA_DISTANCE = 200.0f;
    return bot->FindNearestCreature(entry, KALECGOS_ARENA_DISTANCE, true);
}

bool IsInSpectralRealm(Unit const* unit)
{
    return unit && unit->HasAura(SPELL_SPECTRAL_REALM);
}

GameObject* GetSpectralRift(Player* bot)
{
    GameObject* rift = bot->FindNearestGameObject(GO_SPECTRAL_RIFT, KALECGOS_PORTAL_SEARCH_DISTANCE);
    return rift && rift->isSpawned() ? rift : nullptr;
}

Player* GetDragonRider(Player* bot)
{
    if (Player* thirdTank = GetGroupAssistTank(bot, 1))
        return thirdTank;
    return GetGroupAssistTank(bot, 0);
}

GameObject* GetUsableBlueFlightOrb(Player* bot)
{
    constexpr float ORB_SEARCH_DISTANCE = 100.0f;
    GameObject* nearest = nullptr;
    float nearestDistance = ORB_SEARCH_DISTANCE;
    for (uint32 entry : { GO_ORB_OF_THE_BLUE_FLIGHT_1, GO_ORB_OF_THE_BLUE_FLIGHT_2, GO_ORB_OF_THE_BLUE_FLIGHT_3,
                          GO_ORB_OF_THE_BLUE_FLIGHT_4 })
    {
        // Kil'jaeden empowers an orb by clearing its not-selectable flag; using it sets the flag again.
        GameObject* orb = bot->FindNearestGameObject(entry, ORB_SEARCH_DISTANCE);
        if (!orb || orb->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE))
            continue;

        float const distance = bot->GetExactDist2d(orb);
        if (distance < nearestDistance)
        {
            nearest = orb;
            nearestDistance = distance;
        }
    }
    return nearest;
}

Unit* GetPossessedDrake(Player* bot)
{
    Unit* charm = bot->GetCharm();
    return charm && charm->IsAlive() && charm->GetEntry() == NPC_BLUE_DRAKE ? charm : nullptr;
}

static float GetSpellDamageRadius(uint32 spellId)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return 0.0f;

    float radius = 0.0f;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        radius = std::max(radius, info->Effects[i].CalcRadius());
    return radius;
}

static float GetTriggeredSpellRadius(uint32 spellId)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return 0.0f;

    float radius = 0.0f;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (uint32 const triggered = info->Effects[i].TriggerSpell)
            radius = std::max(radius, GetSpellDamageRadius(triggered));
    return radius;
}

float GetHazardRadius(Unit const* hazard, float fallback)
{
    float radius = 0.0f;
    if (hazard->GetEntry() == NPC_ARMAGEDDON_TARGET)
        radius = GetTriggeredSpellRadius(SPELL_ARMAGEDDON_MISSILE);
    else
    {
        for (AuraType type : { SPELL_AURA_PERIODIC_TRIGGER_SPELL, SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE })
            for (AuraEffect const* effect : hazard->GetAuraEffectsByType(type))
                if (uint32 const triggered = effect->GetSpellInfo()->Effects[effect->GetEffIndex()].TriggerSpell)
                    radius = std::max(radius, GetSpellDamageRadius(triggered));
    }
    return radius > 0.0f ? radius : fallback;
}

float GetBurnSpreadRadius(Player const* bot)
{
    constexpr float FALLBACK_BURN_RADIUS = 10.0f;
    float radius = 0.0f;
    for (AuraEffect const* effect : bot->GetAuraEffectsByType(SPELL_AURA_PERIODIC_TRIGGER_SPELL))
        if (effect->GetId() == SPELL_BURN)
        {
            uint32 const spread = effect->GetSpellInfo()->Effects[effect->GetEffIndex()].TriggerSpell;
            radius = std::max(radius, GetSpellDamageRadius(spread));
        }
    return radius > 0.0f ? radius : FALLBACK_BURN_RADIUS;
}

bool IsHazardNpc(uint32 entry)
{
    switch (entry)
    {
        case NPC_DEMONIC_VAPOR_TRAIL:
        case NPC_DARKNESS:
        case NPC_SINGULARITY:
        case NPC_ARMAGEDDON_TARGET:
            return true;
        default:
            return false;
    }
}

// Lane starts from the core script (top, middle, bottom); both sides share each lane's X.
static constexpr std::array<float, 3> FELMYST_LANE_X = { 1493.8f, 1468.3f, 1444.1f };
static constexpr std::array<float, 2> FELMYST_LANE_START_Y = { 515.6f, 702.9f };
// Lanes are 25y apart; anything nearer a lane's line than half that is in its fog.
static constexpr float FELMYST_LANE_HALF_WIDTH = 12.0f;

int8 GetFelmystBreathLane(Unit const* felmyst)
{
    if (!felmyst || !felmyst->IsAlive() || !felmyst->IsFlying())
        return -1;

    // She hovers 5s at a lane's start before the breath, then crosses with Speed Burst.
    constexpr float LANE_START_DISTANCE = 6.0f;
    bool atLaneStart = false;
    for (float const x : FELMYST_LANE_X)
        for (float const y : FELMYST_LANE_START_Y)
            if (felmyst->GetExactDist2d(x, y) < LANE_START_DISTANCE)
                atLaneStart = true;

    if (!atLaneStart && !felmyst->HasAura(SPELL_FELMYST_SPEED_BURST))
        return -1;

    int8 lane = 0;
    for (int8 i = 1; i < int8(FELMYST_LANE_X.size()); ++i)
        if (std::fabs(felmyst->GetPositionX() - FELMYST_LANE_X[i]) <
            std::fabs(felmyst->GetPositionX() - FELMYST_LANE_X[lane]))
            lane = i;
    return lane;
}

float GetFelmystLaneX(int8 lane)
{
    return FELMYST_LANE_X[lane];
}

bool IsInFelmystLane(Player const* bot, int8 lane)
{
    return lane >= 0 && std::fabs(bot->GetPositionX() - FELMYST_LANE_X[lane]) < FELMYST_LANE_HALF_WIDTH;
}

bool IsUsingDarkness(Unit const* kiljaeden, int32* remainingMs)
{
    if (!kiljaeden)
        return false;

    for (CurrentSpellTypes type : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
    {
        Spell* spell = kiljaeden->GetCurrentSpell(type);
        if (spell && spell->m_spellInfo->Id == SPELL_DARKNESS_OF_A_THOUSAND_SOULS)
        {
            if (remainingMs)
                *remainingMs = spell->GetCastTimeRemaining();
            return true;
        }
    }
    return false;
}

Unit* GetNearestHazard(PlayerbotAI* botAI, float* radius)
{
    Player* bot = botAI->GetBot();
    if (!bot->IsInCombat())
        return nullptr;

    // The generic AoE avoidance skips zones wider than its radius cap; these Sunwell ones are.
    constexpr float FALLBACK_HAZARD_RADIUS = 8.0f;
    constexpr float HAZARD_MARGIN = 1.0f;
    Unit* nearest = nullptr;
    float nearestMargin = 0.0f;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !IsHazardNpc(unit->GetEntry()))
            continue;

        float const hazardRadius = GetHazardRadius(unit, FALLBACK_HAZARD_RADIUS) + HAZARD_MARGIN;
        float const margin = hazardRadius - bot->GetExactDist2d(unit);
        if (margin > nearestMargin)
        {
            nearest = unit;
            nearestMargin = margin;
            if (radius)
                *radius = hazardRadius;
        }
    }
    return nearest;
}

enum class PortalRole : uint8
{
    Tank,
    Healer,
    Dps,
};

static PortalRole GetPortalRole(Player* player)
{
    if (PlayerbotAI::IsTank(player))
        return PortalRole::Tank;
    if (PlayerbotAI::IsHeal(player))
        return PortalRole::Healer;
    return PortalRole::Dps;
}

bool IsKalecgosPortalTeamMember(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->IsAlive() || IsInSpectralRealm(bot) || bot->HasAura(SPELL_SPECTRAL_EXHAUSTION) ||
        PlayerbotAI::IsMainTank(bot))
        return false;

    GameObject* rift = GetSpectralRift(bot);
    Group* group = bot->GetGroup();
    if (!rift || !group)
        return false;

    // A tank already inside with time left keeps Sathrovarr; the next tank comes with a later portal.
    constexpr int32 TANK_HANDOVER_MS = 15000;
    // Whoever went in this recently came through this portal, the blasted player included.
    constexpr int32 FRESH_ENTRY_MS = 10000;

    int32 tanksNeeded = 1;
    int32 healersNeeded = KALECGOS_PORTAL_HEALERS;
    int32 dpsNeeded = KALECGOS_PORTAL_DPS;
    std::vector<Player*> candidates;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (Aura const* realm = member->GetAura(SPELL_SPECTRAL_REALM))
        {
            PortalRole const role = GetPortalRole(member);
            if (role == PortalRole::Tank && realm->GetDuration() > TANK_HANDOVER_MS)
                tanksNeeded = 0;

            if (realm->GetMaxDuration() - realm->GetDuration() < FRESH_ENTRY_MS)
            {
                if (role == PortalRole::Healer)
                    --healersNeeded;
                else if (role == PortalRole::Dps)
                    --dpsNeeded;
            }
            continue;
        }

        if (member->HasAura(SPELL_SPECTRAL_EXHAUSTION) || PlayerbotAI::IsMainTank(member) ||
            member->GetExactDist2d(rift) > KALECGOS_PORTAL_SEARCH_DISTANCE)
            continue;

        candidates.push_back(member);
    }

    PortalRole const botRole = GetPortalRole(bot);
    int32 const quota = botRole == PortalRole::Tank ? tanksNeeded :
                        botRole == PortalRole::Healer ? healersNeeded : dpsNeeded;
    if (quota <= 0)
        return false;

    // Nearest first, so every bot works out the same team.
    std::sort(candidates.begin(), candidates.end(), [rift](Player* a, Player* b)
        {
            float const distA = a->GetExactDist2d(rift);
            float const distB = b->GetExactDist2d(rift);
            return distA != distB ? distA < distB : a->GetGUID() < b->GetGUID();
        });

    int32 rank = 0;
    for (Player* candidate : candidates)
    {
        if (GetPortalRole(candidate) != botRole)
            continue;
        if (candidate == bot)
            return rank < quota;
        ++rank;
    }
    return false;
}

bool IsBrutallusSoaker(Player* bot, Unit* brutallus)
{
    if (!brutallus || PlayerbotAI::IsTank(bot) || PlayerbotAI::IsMelee(bot) || bot->HasAura(SPELL_BURN))
        return false;

    Unit* victim = brutallus->GetVictim();
    Player* tank = victim ? victim->ToPlayer() : nullptr;
    Group* group = bot->GetGroup();
    return tank && tank != bot && group && group->SameSubGroup(bot, tank);
}

Position GetBrutallusSoakSpot(Unit* brutallus)
{
    // Behind the tank, inside the Meteor Slash cone.
    constexpr float SOAK_DEPTH = 6.0f;
    Position spot = brutallus->GetPosition();
    spot.RelocatePolarOffset(0.0f, brutallus->GetCombatReach() + SOAK_DEPTH);
    return spot;
}

bool IsInBrutallusSoakSpot(Player* bot, Unit* brutallus)
{
    constexpr float SOAK_ARC = static_cast<float>(M_PI) / 3.0f;
    constexpr float SOAK_MAX_DEPTH = 12.0f;
    return brutallus->HasInArc(SOAK_ARC, bot) &&
           bot->GetExactDist2d(brutallus) < brutallus->GetCombatReach() + SOAK_MAX_DEPTH;
}

bool IsDemonicVaporTarget(Unit* vapor, Player* bot)
{
    TempSummon const* summon = vapor->ToTempSummon();
    return summon && summon->GetSummonerGUID() == bot->GetGUID();
}

Player* GetAlythessTank(Player* bot)
{
    if (Player* thirdTank = GetGroupAssistTank(bot, 1))
        return thirdTank;
    return GetGroupAssistTank(bot, 0);
}

bool IsSacrolashTank(Player* bot)
{
    if (bot == GetAlythessTank(bot))
        return false;
    return bot == GetGroupMainTank(bot) || bot == GetGroupAssistTank(bot, 0);
}

static std::vector<Unit*> GetHands(PlayerbotAI* botAI)
{
    std::vector<Unit*> hands;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == NPC_HAND_OF_THE_DECEIVER)
            hands.push_back(unit);
    }
    std::sort(hands.begin(), hands.end(), [](Unit* a, Unit* b) { return a->GetGUID() < b->GetGUID(); });
    return hands;
}

Unit* GetAssignedHand(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsTank(bot))
        return nullptr;

    std::array<Player*, 3> const tanks = { GetGroupMainTank(bot), GetGroupAssistTank(bot, 0),
                                           GetGroupAssistTank(bot, 1) };
    auto const it = std::find(tanks.begin(), tanks.end(), bot);
    if (it == tanks.end())
        return nullptr;

    std::vector<Unit*> const hands = GetHands(botAI);
    size_t const index = std::distance(tanks.begin(), it);
    return index < hands.size() ? hands[index] : nullptr;
}

bool IsTooCloseToOtherHand(PlayerbotAI* botAI, Unit* hand)
{
    for (Unit* other : GetHands(botAI))
        if (other != hand && other->GetExactDist2d(hand) < HAND_SEPARATION_DISTANCE)
            return true;
    return false;
}

Unit* GetRangedAddTarget(PlayerbotAI* botAI)
{
    float const range = botAI->GetRange("spell");
    if (Unit* fiend = GetNearestNpc(botAI, NPC_VOLATILE_FELFIRE_FIEND, range))
        return fiend;
    return GetNearestNpc(botAI, NPC_SHIELD_ORB, range);
}

Unit* GetShieldDrake(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_BLUE_DRAKE)
            continue;

        Player* rider = ObjectAccessor::GetPlayer(*bot, unit->GetCharmerGUID());
        if (rider && group->IsMember(rider->GetGUID()))
            return unit;
    }
    return nullptr;
}
}
