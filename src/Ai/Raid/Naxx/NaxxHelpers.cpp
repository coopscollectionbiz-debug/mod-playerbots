/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxHelpers.h"
#include "AiObjectContext.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <iterator>
#include <vector>

using namespace EncounterHelpers;

namespace NaxxHelpers
{
bool Is25Player(Player const* bot)
{
    return bot->GetMap()->Is25ManRaid();
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

bool IsFaerlinaFrenzied(Unit const* faerlina)
{
    return faerlina &&
           faerlina->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_FAERLINA_FRENZY, faerlina));
}

float GetWidowsEmbraceRadius(Unit const* caster)
{
    // The spell's own area; the fallback is ours, for spell data without one.
    constexpr float FALLBACK_EMBRACE_RADIUS = 10.0f;
    float radius = 0.0f;
    uint32 const embraceId = sSpellMgr->GetSpellIdForDifficulty(SPELL_WIDOWS_EMBRACE, caster);
    if (SpellInfo const* info = sSpellMgr->GetSpellInfo(embraceId))
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            radius = std::max(radius, info->Effects[i].CalcRadius());
    return radius > 0.0f ? radius : FALLBACK_EMBRACE_RADIUS;
}

Player* GetFaerlinaPriest(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* chosen = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsClass(CLASS_PRIEST) || member->GetMapId() != bot->GetMapId() ||
            !GET_PLAYERBOT_AI(member))
            continue;

        if (!chosen || member->GetGUID() < chosen->GetGUID())
            chosen = member;
    }
    return chosen;
}

Unit* GetCharmedWorshipper(Player* bot)
{
    Unit* charm = bot->GetCharm();
    return charm && charm->IsAlive() && charm->GetEntry() == NPC_NAXXRAMAS_WORSHIPPER ? charm : nullptr;
}

static uint8 GetKillPriority(Unit* unit, Unit* faerlina, bool embraceByDeath)
{
    switch (unit->GetEntry())
    {
        case NPC_NAXXRAMAS_WORSHIPPER:
            // In 10-player a dying Worshipper casts Widow's Embrace, which lifts a Frenzy within reach.
            return embraceByDeath && IsFaerlinaFrenzied(faerlina) &&
                           unit->GetExactDist2d(faerlina) < GetWidowsEmbraceRadius(unit)
                       ? 3
                       : 0;
        case NPC_NAXXRAMAS_FOLLOWER:
            return 2;
        case NPC_GRAND_WIDOW_FAERLINA:
            return 1;
        default:
            return 0;
    }
}

Unit* GetKillOrderMarkTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    Unit* faerlina = GetNearestNpc(botAI, NPC_GRAND_WIDOW_FAERLINA, NAXX_ADD_SEARCH_DISTANCE);
    if (!group || !faerlina)
        return nullptr;

    bool const embraceByDeath = !Is25Player(bot);
    Unit* best = nullptr;
    uint8 bestPriority = 0;
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !unit->IsInCombat() || !bot->IsValidAttackTarget(unit))
            continue;

        uint8 const priority = GetKillPriority(unit, faerlina, embraceByDeath);
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
    if (skull && skull->IsAlive() && GetKillPriority(skull, faerlina, embraceByDeath) >= bestPriority)
        return nullptr;

    return best;
}

// Gothik's room and side split, from the core script.
static constexpr float GOTHIK_ROOM_X_MIN = 2618.0f;
static constexpr float GOTHIK_ROOM_X_MAX = 2767.0f;
static constexpr float GOTHIK_ROOM_Y_MIN = -3435.0f;
static constexpr float GOTHIK_ROOM_Y_MAX = -3285.0f;
static constexpr float GOTHIK_GATE_Y = -3360.78f;

bool IsOnGothikDeadSide(Unit const* unit)
{
    return unit->GetPositionY() >= GOTHIK_GATE_Y;
}

bool IsInGothikRoom(Unit const* unit)
{
    return unit->GetPositionX() > GOTHIK_ROOM_X_MIN && unit->GetPositionX() < GOTHIK_ROOM_X_MAX &&
           unit->GetPositionY() > GOTHIK_ROOM_Y_MIN && unit->GetPositionY() < GOTHIK_ROOM_Y_MAX;
}

Position GetGothikSideSpot(bool deadSide)
{
    // The middle of each half of the room; the spots are ours, the halves the core's.
    constexpr float SPOT_X = 2692.0f;
    constexpr float SPOT_GATE_OFFSET = 35.0f;
    return Position(SPOT_X, GOTHIK_GATE_Y + (deadSide ? SPOT_GATE_OFFSET : -SPOT_GATE_OFFSET), 0.0f);
}

bool IsGothikDeadSideMember(Player* bot)
{
    if (bot == GetGroupMainTank(bot))
        return false;
    if (bot == GetGroupAssistTank(bot, 0))
        return true;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Alternate within the bot's role, so each side gets half the healers and half the DPS.
    bool const healer = PlayerbotAI::IsHeal(bot);
    std::vector<ObjectGuid> sameRole;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == GetGroupMainTank(bot) || member == GetGroupAssistTank(bot, 0) ||
            PlayerbotAI::IsHeal(member) != healer)
            continue;
        sameRole.push_back(member->GetGUID());
    }
    std::sort(sameRole.begin(), sameRole.end());

    auto const it = std::find(sameRole.begin(), sameRole.end(), bot->GetGUID());
    return it != sameRole.end() && std::distance(sameRole.begin(), it) % 2 == 1;
}

bool IsGothikGateOpen(Player* bot)
{
    constexpr float GATE_SEARCH_DISTANCE = 150.0f;
    GameObject* gate = bot->FindNearestGameObject(GO_GOTHIK_INNER_GATE, GATE_SEARCH_DISTANCE);
    return !gate || gate->GetGoState() == GO_STATE_ACTIVE;
}

bool IsHatefulStrikeSoaker(Player* bot)
{
    return bot == GetGroupAssistTank(bot, 0) || (Is25Player(bot) && bot == GetGroupAssistTank(bot, 1));
}

float GetManaDetonationRadius()
{
    constexpr float FALLBACK_DETONATION_RADIUS = 10.0f;
    float radius = 0.0f;
    if (SpellInfo const* info = sSpellMgr->GetSpellInfo(SPELL_MANA_DETONATION_DAMAGE))
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            radius = std::max(radius, info->Effects[i].CalcRadius());
    return radius > 0.0f ? radius : FALLBACK_DETONATION_RADIUS;
}

Unit* GetChainsCcTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Unit* moon = botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::moonIndex));
    if (moon && moon->IsAlive() && moon->HasAura(SPELL_CHAINS_OF_KELTHUZAD))
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->HasAura(SPELL_CHAINS_OF_KELTHUZAD))
            return member;
    }
    return nullptr;
}
}
