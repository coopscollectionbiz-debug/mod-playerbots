/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldExtraActions.h"
#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UldExtraHelpers.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace EncounterHelpers;
using namespace UlduarExtraHelpers;

bool UldKillOrderMarkAction::Execute(Event /*event*/)
{
    return MarkTargetWithSkull(bot, GetKillOrderMarkTarget(botAI));
}

bool UldTankOffTankAddAction::Execute(Event event)
{
    Unit* add = GetUntankedOffTankAdd(botAI);
    if (!add)
        return false;

    if (bot->GetVictim() != add)
        return Attack(add);

    return botAI->DoSpecificAction("taunt spell", event, true);
}

bool UldIgnisDragConstructAction::Execute(Event /*event*/)
{
    Position destination;
    if (!GetIgnisConstructDestination(botAI, destination))
        return false;

    return MoveTo(bot->GetMapId(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(),
                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool UldXtSpreadAction::Execute(Event /*event*/)
{
    constexpr float SPREAD_MARGIN = 3.0f;
    return MoveFromGroup(GetXtDebuffRadius(bot) + SPREAD_MARGIN);
}

bool UldXtAttackBotAddAction::Execute(Event /*event*/)
{
    float const range = botAI->GetRange("spell");
    Unit* add = GetNearestNpc(botAI, NPC_XE321_BOOMBOT, range);
    if (!add)
        add = GetNearestNpc(botAI, NPC_XS013_SCRAPBOT, range);
    return add && Attack(add);
}

bool UldAuriayaStackAction::Execute(Event /*event*/)
{
    Unit* auriaya = AI_VALUE2(Unit*, "find target", "auriaya");
    if (!auriaya)
        return false;

    Position const spot = GetAuriayaStackSpot(auriaya);
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool UldInterruptAction::Execute(Event /*event*/)
{
    static constexpr std::array<char const*, 7> INTERRUPT_SPELLS = {
        "kick", "pummel", "shield bash", "wind shear", "counterspell", "mind freeze", "silencing shot"};

    Unit* caster = GetInterruptTarget(botAI);
    if (!caster)
        return false;

    for (char const* name : INTERRUPT_SPELLS)
    {
        uint32 const spellId = AI_VALUE2(uint32, "spell id", name);
        SpellInfo const* spellInfo = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!spellInfo || !bot->IsWithinCombatRange(caster, spellInfo->GetMaxRange(false)))
            continue;

        if (botAI->CanCastSpell(name, caster) && botAI->CastSpell(name, caster))
            return true;
    }
    return false;
}

bool UldAlgalonTauntAction::Execute(Event event)
{
    Unit* algalon = AI_VALUE2(Unit*, "find target", "algalon the observer");
    if (!algalon)
        return false;

    if (bot->GetVictim() != algalon)
        return Attack(algalon);

    return algalon->GetVictim() != bot && botAI->DoSpecificAction("taunt spell", event, true);
}

bool UldAlgalonEnterBlackHoleAction::Execute(Event /*event*/)
{
    Unit* hole = GetNearestNpc(botAI, NPC_BLACK_HOLE, ULDUAR_EXTRA_SEARCH_DISTANCE);
    if (!hole)
        return false;

    return MoveTo(hole->GetMapId(), hole->GetPositionX(), hole->GetPositionY(), hole->GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool UldAlgalonAttackStarAction::Execute(Event /*event*/)
{
    // Lowest health first, so the stars die one after another rather than together.
    Unit* star = nullptr;
    float const range = botAI->GetRange("spell");
    for (ObjectGuid const guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_COLLAPSING_STAR || bot->GetExactDist2d(unit) > range)
            continue;

        if (!star || unit->GetHealthPct() < star->GetHealthPct())
            star = unit;
    }
    return star && Attack(star);
}

bool UldCouncilTankAction::Execute(Event event)
{
    Unit* member = GetAssignedCouncilMember(botAI);
    if (!member)
        return false;

    if (bot->GetVictim() != member)
        return Attack(member);

    return member->GetVictim() != bot && botAI->DoSpecificAction("taunt spell", event, true);
}

bool UldTauntBossAction::Execute(Event event)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", bossName);
    if (!boss)
        return false;

    if (bot->GetVictim() != boss)
        return Attack(boss);

    return boss->GetVictim() != bot && botAI->DoSpecificAction("taunt spell", event, true);
}

bool UldThorimAvoidSifFrostNovaAction::Execute(Event /*event*/)
{
    float safeDistance = 0.0f;
    Unit* sif = GetSifInNovaRange(botAI, safeDistance);
    return sif && MoveAway(sif, safeDistance);
}

bool UldThorimAvoidSifBlizzardAction::Execute(Event /*event*/)
{
    float safeDistance = 0.0f;
    Unit* blizzard = GetSifBlizzardInRange(botAI, safeDistance);
    return blizzard && MoveAway(blizzard, safeDistance);
}

bool UldMimironAvoidFlamesAction::Execute(Event /*event*/)
{
    // The fire spreads in patches, so pick a spot clear of them rather than just backing off.
    float safeDistance = 0.0f;
    Unit* flames = GetMimironFlamesInRange(botAI, safeDistance);
    return flames && FleePosition(flames->GetPosition(), safeDistance);
}

bool UldMimironAvoidFrostBombAction::Execute(Event /*event*/)
{
    float safeDistance = 0.0f;
    Unit* bomb = GetMimironFrostBombInRange(botAI, safeDistance);
    return bomb && MoveAway(bomb, safeDistance);
}

bool UldVezaxKillVaporsAction::Execute(Event /*event*/)
{
    Unit* vapors = GetNearestNpc(botAI, NPC_SARONITE_VAPORS, botAI->GetRange("spell"));
    return vapors && Attack(vapors);
}

bool UldHodirToastyFireAction::Execute(Event /*event*/)
{
    Unit* fire = GetNearestNpc(botAI, NPC_TOASTY_FIRE, ULDUAR_EXTRA_SEARCH_DISTANCE);
    if (!fire)
        return false;

    return MoveTo(fire->GetMapId(), fire->GetPositionX(), fire->GetPositionY(), fire->GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool UldHodirShedBitingColdAction::Execute(Event /*event*/)
{
    Unit* hodir = AI_VALUE2(Unit*, "find target", "hodir");
    if (!hodir || bot->isMoving())
        return false;

    float const distance = std::max(bot->GetExactDist2d(hodir), 1.0f);
    clockwise = !clockwise;
    float const angle = hodir->GetAngle(bot) + (clockwise ? -1.0f : 1.0f) * BITING_COLD_STEP_DISTANCE / distance;
    float const x = hodir->GetPositionX() + distance * std::cos(angle);
    float const y = hodir->GetPositionY() + distance * std::sin(angle);
    return MoveTo(bot->GetMapId(), x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool UldHodirBreakIceAction::Execute(Event /*event*/)
{
    Unit* ice = GetHodirIceBlock(botAI);
    return ice && Attack(ice);
}

bool UldFreyaBreakRootsAction::Execute(Event /*event*/)
{
    Unit* roots = GetFreyaIronRoots(botAI);
    return roots && Attack(roots);
}

bool UldFreyaNaturesFuryAction::Execute(Event /*event*/)
{
    constexpr float SPREAD_MARGIN = 3.0f;
    float const radius = GetNaturesFuryRadius(bot);
    return radius > 0.0f && MoveFromGroup(radius + SPREAD_MARGIN);
}

bool UldFreyaAvoidSunBeamAction::Execute(Event /*event*/)
{
    float safeDistance = 0.0f;
    Unit* beam = GetFreyaSunBeamInRange(botAI, safeDistance);
    return beam && MoveAway(beam, safeDistance);
}

bool UldXtAvoidVoidZoneAction::Execute(Event /*event*/)
{
    float const distance = GetXtVoidZoneAvoidDistance();
    Unit* zone = GetNearestNpc(botAI, NPC_XT_VOID_ZONE, distance);
    return zone && MoveAway(zone, distance);
}

bool UldMimironAvoidMineAction::Execute(Event /*event*/)
{
    Unit* mine = GetNearestNpc(botAI, NPC_MIMIRON_PROXIMITY_MINE, MIMIRON_MINE_AVOID_DISTANCE);
    return mine && MoveAway(mine, MIMIRON_MINE_AVOID_DISTANCE);
}

bool UldYoggAvoidCloudAction::Execute(Event /*event*/)
{
    Unit* cloud = GetNearestNpc(botAI, NPC_YOGG_OMINOUS_CLOUD, YOGG_CLOUD_AVOID_DISTANCE);
    return cloud && MoveAway(cloud, YOGG_CLOUD_AVOID_DISTANCE);
}
