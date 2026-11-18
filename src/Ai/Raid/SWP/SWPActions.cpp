/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPActions.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "MotionMaster.h"
#include "Playerbots.h"
#include "SWPHelpers.h"
#include <cmath>
#include <limits>
#include <vector>

using namespace EncounterHelpers;
using namespace SunwellPlateauHelpers;

static void UseGameObject(Player* bot, GameObject* go)
{
    WorldPacket use(CMSG_GAMEOBJ_USE);
    use << go->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(use);

    WorldPacket report(CMSG_GAMEOBJ_USE);
    report << go->GetGUID();
    bot->GetSession()->HandleGameobjectReportUse(report);
}

// General

bool SwpKillOrderMarkAction::Execute(Event /*event*/)
{
    return MarkTargetWithSkull(bot, GetKillOrderMarkTarget(botAI));
}

bool SwpCcMarkAction::Execute(Event /*event*/)
{
    return MarkTargetWithMoon(bot, GetCcMarkTarget(botAI));
}

bool SwpAvoidHazardAction::Execute(Event /*event*/)
{
    float radius = 0.0f;
    Unit* hazard = GetNearestHazard(botAI, &radius);
    return hazard && FleePosition(hazard->GetPosition(), radius);
}

bool SwpTauntBossAction::Execute(Event event)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", bossName);
    if (!boss)
        return false;

    if (bot->GetVictim() != boss)
        return Attack(boss);

    Unit* victim = boss->GetVictim();
    if (victim == bot)
        return false;

    Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
    if (!tauntOffTanks && victimPlayer && PlayerbotAI::IsTank(victimPlayer))
        return false;

    return botAI->DoSpecificAction("taunt spell", event, true);
}

static float GetFlankDistance(PlayerbotAI* botAI)
{
    // Ranged stand off at the boss's sides rather than in melee.
    constexpr float RANGED_FLANK_DISTANCE = 15.0f;
    return PlayerbotAI::IsRanged(botAI->GetBot()) ? RANGED_FLANK_DISTANCE : 0.0f;
}

SwpBossFlankAction::SwpBossFlankAction(PlayerbotAI* botAI, std::string const& bossName, float minAngle,
                                       float maxAngle, bool frontOnly)
    : BossRearFlankAction(botAI, bossName, GetFlankDistance(botAI), minAngle, maxAngle), frontOnly(frontOnly)
{
}

bool SwpBossFlankAction::isUseful()
{
    if (!frontOnly)
        return BossRearFlankAction::isUseful();

    Unit const* boss = GetTarget();
    return boss && boss->HasInArc(2.0f * minAngle, bot);
}

bool SwpSpreadAction::Execute(Event /*event*/)
{
    return MoveFromGroup(distance);
}

// Kalecgos

bool SwpKalecgosTakePortalAction::Execute(Event /*event*/)
{
    GameObject* rift = GetSpectralRift(bot);
    if (!rift)
        return false;

    if (bot->GetDistance(rift) > INTERACTION_DISTANCE)
        return MoveTo(rift->GetMapId(), rift->GetPositionX(), rift->GetPositionY(), rift->GetPositionZ(), false,
                      false, false, false, MovementPriority::MOVEMENT_COMBAT, true, false);

    UseGameObject(bot, rift);
    return true;
}

bool SwpKalecgosHealKalecAction::Execute(Event /*event*/)
{
    Unit* kalec = FindCreatureAnyRealm(bot, NPC_KALEC);
    if (!kalec)
        return false;

    // A heal over time first if it isn't up, then the class's big heal; spell names pick the rank.
    char const* hot = nullptr;
    std::vector<char const*> heals;
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            hot = "renew";
            heals = { "greater heal", "heal", "flash heal" };
            break;
        case CLASS_DRUID:
            hot = "rejuvenation";
            heals = { "regrowth", "healing touch" };
            break;
        case CLASS_SHAMAN:
            hot = "riptide";
            heals = { "healing wave", "lesser healing wave" };
            break;
        case CLASS_PALADIN:
            heals = { "holy light", "flash of light" };
            break;
        default:
            return false;
    }

    if (hot && !botAI->HasAura(hot, kalec, false, true) && botAI->CanCastSpell(hot, kalec))
        return botAI->CastSpell(hot, kalec);

    for (char const* heal : heals)
        if (botAI->CanCastSpell(heal, kalec))
            return botAI->CastSpell(heal, kalec);

    return false;
}

// Brutallus

bool SwpBrutallusSoakAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "brutallus");
    if (!boss)
        return false;

    Position const spot = GetBrutallusSoakSpot(boss);
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool SwpBrutallusBurnSpreadAction::Execute(Event /*event*/)
{
    constexpr float BURN_SPREAD_MARGIN = 2.0f;
    return MoveFromGroup(GetBurnSpreadRadius(bot) + BURN_SPREAD_MARGIN);
}

// Felmyst

bool SwpFelmystDodgeLaneAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "felmyst");
    if (!boss)
        boss = GetNearestNpc(botAI, NPC_FELMYST, SWP_ADD_SEARCH_DISTANCE);

    int8 const lane = GetFelmystBreathLane(boss);
    if (lane < 0)
        return false;

    // Step sideways into the nearest other lane, keeping the same spot along the stage.
    float bestX = bot->GetPositionX();
    float bestDistance = std::numeric_limits<float>::max();
    for (int8 other = 0; other < 3; ++other)
    {
        if (other == lane)
            continue;

        float const distance = std::fabs(bot->GetPositionX() - GetFelmystLaneX(other));
        if (distance < bestDistance)
        {
            bestX = GetFelmystLaneX(other);
            bestDistance = distance;
        }
    }

    float const z = bot->GetMapWaterOrGroundLevel(bestX, bot->GetPositionY(), bot->GetPositionZ());
    return MoveTo(bot->GetMapId(), bestX, bot->GetPositionY(), z, false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

// M'uru

bool SwpVoidSentinelTankAction::Execute(Event event)
{
    Unit* sentinel = GetNearestNpc(botAI, NPC_VOID_SENTINEL, SWP_ADD_SEARCH_DISTANCE);
    if (!sentinel)
        return false;

    if (bot->GetVictim() != sentinel)
        return Attack(sentinel);

    return sentinel->GetVictim() != bot && botAI->DoSpecificAction("taunt spell", event, true);
}

bool SwpDispelDarkFiendAction::Execute(Event /*event*/)
{
    Unit* fiend = GetNearestNpc(botAI, NPC_DARK_FIEND, botAI->GetRange("spell"));
    if (!fiend)
        return false;

    char const* spell = bot->IsClass(CLASS_PRIEST) ? "dispel magic" : bot->IsClass(CLASS_SHAMAN) ? "purge"
                                                                                                   : "spellsteal";
    return botAI->CanCastSpell(spell, fiend) && botAI->CastSpell(spell, fiend);
}

// Kil'jaeden

bool SwpHandTankAction::Execute(Event event)
{
    Unit* hand = GetAssignedHand(botAI);
    if (!hand)
        return false;

    if (bot->GetVictim() != hand)
        return Attack(hand);

    if (hand->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", event, true);

    // Drag it away from the other Hands so the raid can stand clear of every Shadow Bolt Volley.
    Unit* nearestOther = nullptr;
    for (ObjectGuid const guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit == hand || !unit->IsAlive() || unit->GetEntry() != NPC_HAND_OF_THE_DECEIVER)
            continue;
        if (!nearestOther || hand->GetExactDist2d(unit) < hand->GetExactDist2d(nearestOther))
            nearestOther = unit;
    }

    if (!nearestOther)
        return false;

    float const distToTravel = HAND_SEPARATION_DISTANCE - hand->GetExactDist2d(nearestOther);
    return distToTravel > 0.0f && MoveAway(nearestOther, distToTravel);
}

bool SwpHandVolleyMoveAwayAction::Execute(Event /*event*/)
{
    Unit* hand = GetNearestNpc(botAI, NPC_HAND_OF_THE_DECEIVER, HAND_VOLLEY_DISTANCE);
    if (!hand)
        return false;

    return MoveAway(hand, HAND_VOLLEY_DISTANCE - bot->GetExactDist2d(hand));
}

bool SwpAttackRangedAddAction::Execute(Event /*event*/)
{
    Unit* add = GetRangedAddTarget(botAI);
    return add && Attack(add);
}

bool SwpUseDragonOrbAction::Execute(Event /*event*/)
{
    GameObject* orb = GetUsableBlueFlightOrb(bot);
    if (!orb)
        return false;

    if (bot->GetDistance(orb) > INTERACTION_DISTANCE)
        return MoveTo(orb->GetMapId(), orb->GetPositionX(), orb->GetPositionY(), orb->GetPositionZ(), false, false,
                      false, false, MovementPriority::MOVEMENT_COMBAT, true, false);

    UseGameObject(bot, orb);
    return true;
}

static bool GetRaidCentre(Player* bot, Position& centre)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    constexpr float RAID_CENTRE_RANGE = 60.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint32 count = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != bot->GetMapId() ||
            bot->GetExactDist2d(member) > RAID_CENTRE_RANGE)
            continue;

        x += member->GetPositionX();
        y += member->GetPositionY();
        z += member->GetPositionZ();
        ++count;
    }

    if (!count)
        return false;

    centre.Relocate(x / count, y / count, z / count);
    return true;
}

static uint32 CountMembersNear(Player* bot, Unit* centre, float radius, uint32 lackingAura)
{
    uint32 count = 0;
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member != bot && member->IsAlive() && centre->GetExactDist2d(member) < radius &&
                (!lackingAura || !member->HasAura(lackingAura)))
                ++count;
        }
    return count;
}

bool SwpRideDragonAction::Execute(Event /*event*/)
{
    Unit* drake = GetPossessedDrake(bot);
    if (!drake)
        return false;

    // Shield of the Blue goes up as Darkness lands; nothing else may hold up its cooldown.
    Unit* boss = bot->FindNearestCreature(NPC_KILJAEDEN, SWP_ADD_SEARCH_DISTANCE);
    int32 remaining = 0;
    if (IsUsingDarkness(boss, &remaining))
    {
        if (remaining <= DRAKE_SHIELD_CAST_LEAD_MS && !drake->HasSpellCooldown(SPELL_DRAKE_SHIELD_OF_THE_BLUE))
        {
            drake->CastSpell(drake, SPELL_DRAKE_SHIELD_OF_THE_BLUE, false);
            return true;
        }
        return false;
    }

    // Hover over the raid so the shield and breaths reach as many as possible.
    Position centre;
    constexpr float CENTRE_TOLERANCE = 4.0f;
    if (GetRaidCentre(bot, centre) && drake->GetExactDist2d(centre) > CENTRE_TOLERANCE)
    {
        if (!drake->isMoving())
            drake->GetMotionMaster()->MovePoint(0, centre);
        return true;
    }

    // Breath: Haste lifts the Flame Dart slow and splash; Revitalize heals and restores mana.
    constexpr float BREATH_RADIUS = 13.0f;
    constexpr uint32 MIN_BREATH_TARGETS = 3;
    if (!drake->HasSpellCooldown(SPELL_DRAKE_BREATH_HASTE) &&
        CountMembersNear(bot, drake, BREATH_RADIUS, SPELL_DRAKE_BREATH_HASTE) >= MIN_BREATH_TARGETS)
    {
        drake->CastSpell(drake, SPELL_DRAKE_BREATH_HASTE, false);
        return true;
    }

    if (!drake->HasSpellCooldown(SPELL_DRAKE_BREATH_REVITALIZE) &&
        CountMembersNear(bot, drake, BREATH_RADIUS, 0) >= MIN_BREATH_TARGETS)
    {
        drake->CastSpell(drake, SPELL_DRAKE_BREATH_REVITALIZE, false);
        return true;
    }

    return false;
}

bool SwpStackOnDragonAction::Execute(Event /*event*/)
{
    Unit* drake = GetShieldDrake(botAI);
    if (!drake)
        return false;

    return MoveTo(bot->GetMapId(), drake->GetPositionX(), drake->GetPositionY(), bot->GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_FORCED, true, false);
}
