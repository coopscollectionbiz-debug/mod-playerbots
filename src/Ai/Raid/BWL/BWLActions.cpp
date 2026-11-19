/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BWLActions.h"
#include "BWLHelpers.h"
#include "EncounterHelpers.h"
#include "EquipAction.h"
#include "PathGenerator.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace BlackwingLairHelpers;
using namespace EncounterHelpers;

static constexpr float INCREMENTAL_MOVE_STEP_DISTANCE = 3.0f;

static constexpr float RAZORGORE_CONE_RADIUS = 15.0f;
static constexpr float RAZORGORE_CONE_ARC = M_PI;
static constexpr float RAZORGORE_BACK_FUZZ_ARC = M_PI_4;
static constexpr float RAZORGORE_RANGE_DISTANCE = 15.0f;
static constexpr float RAZORGORE_MELEE_DISTANCE = 3.0f;

static constexpr float VAELASTRASZ_REPULSION_RANGE = 20.0f;
static constexpr float VAELASTRASZ_TAIL_SWEEP_RECOVERY_RANGE = 30.0f;
static constexpr float VAELASTRASZ_RANGED_BIAS_THRESHOLD = 25.0f;
static constexpr float VAELASTRASZ_RANGED_BIAS_FLEE_WEIGHT = 0.7f;
static constexpr float VAELASTRASZ_RANGED_BIAS_BOSS_WEIGHT = 1.0f - VAELASTRASZ_RANGED_BIAS_FLEE_WEIGHT;

// General

bool BwlOnyxiaScaleCloakAuraCheckAction::Execute(Event /*event*/)
{
    bot->AddAura(static_cast<uint32>(BlackwingLairSpells::SPELL_ONYXIA_SCALE_CLOAK), bot);
    return true;
}

bool BwlOnyxiaScaleCloakAuraCheckAction::isUseful()
{
    return !bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_ONYXIA_SCALE_CLOAK));
}

bool BwlTurnOffSuppressionDeviceAction::Execute(Event /*event*/)
{
    GuidVector gos = AI_VALUE(GuidVector, "nearest game objects");
    for (auto i = gos.begin(); i != gos.end(); ++i)
    {
        GameObject* go = botAI->GetGameObject(*i);
        if (IsActiveSuppressionDeviceInRange(go, bot))
        {
            go->SetGoState(GO_STATE_ACTIVE);
        }
    }
    return true;
}

// Razorgore the Untamed

bool BwlRazorgoreAvoidAoeAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorgore the untamed");
    if (!boss)
        return false;

    // The current target should not move.
    // Also prevents non-tanks from rotating the boss if they have aggro.
    if (boss->GetVictim() == bot)
        return false;

    float distance = bot->GetDistance2d(boss);

    // Bot is too close and standing in Razorgore's frontal cone
    if (distance <= RAZORGORE_CONE_RADIUS && boss->HasInArc(RAZORGORE_CONE_ARC, bot))
    {
        // Target position: directly behind the boss with a small random fuzz
        // to prevent all bots stacking on the same spot
        float moveAngle = boss->GetOrientation() + M_PI + frand(-RAZORGORE_BACK_FUZZ_ARC, RAZORGORE_BACK_FUZZ_ARC);
        float razorgoreRadius = PlayerbotAI::IsRanged(bot) ? RAZORGORE_RANGE_DISTANCE : RAZORGORE_MELEE_DISTANCE;

        float targetMoveX = boss->GetPositionX() + razorgoreRadius * cos(moveAngle);
        float targetMoveY = boss->GetPositionY() + razorgoreRadius * sin(moveAngle);

        // Move incrementally toward the target. Allows course correction.
        float dX = targetMoveX - bot->GetPositionX();
        float dY = targetMoveY - bot->GetPositionY();
        float dist = sqrt(dX * dX + dY * dY);
        if (dist == 0.0f)
            dist = 0.1f;

        float moveX = bot->GetPositionX() + (dX / dist) * INCREMENTAL_MOVE_STEP_DISTANCE;
        float moveY = bot->GetPositionY() + (dY / dist) * INCREMENTAL_MOVE_STEP_DISTANCE;

        return MoveTo(boss->GetMapId(), moveX, moveY, bot->GetPositionZ(), false, false,
                      false, false, MovementPriority::MOVEMENT_COMBAT, true);
    }

    // Ranged outside the cone but too close => back off to avoid War Stomp
    if (PlayerbotAI::IsRanged(bot))
    {
        float distToTravel = RAZORGORE_RANGE_DISTANCE - distance;
        if (distToTravel > 0)
            return MoveAway(boss, distToTravel);
    }

    return false;
}

bool BwlRazorgoreMarkBossAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorgore the untamed");
    if (!boss)
        return false;

    if (AreRazorgoreEggsAlive(botAI))
    {
        if (IsRazorgoreOffTank(bot))
        {
            if (MarkTargetWithMoon(bot, boss))
                return true;

            SetRtiTarget(botAI, "moon");

            if (AI_VALUE(Unit*, "current target") != boss)
                return Attack(boss);
        }
    }
    else
    {
        ClearTargetIcon(bot, RtiTargetValue::moonIndex);
    }

    return false;
}

bool BwlRazorgoreMarkBossAction::isUseful()
{
    return PlayerbotAI::IsTank(bot);
}

// Vaelastrasz the Corrupt

bool BwlVaelastraszMoveAwayAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "vaelastrasz the corrupt");

    // Current target with Burning Adrenaline stays put when Vaelastrasz is alive.
    if (boss && boss->IsAlive() && boss->GetVictim() == bot)
        return false;

    float fleeX, fleeY;
    if (!CalculateFleeDirection(boss, fleeX, fleeY))
        return false;

    return MoveAlongFleeDirection(boss, fleeX, fleeY);
}

bool BwlVaelastraszMoveAwayAction::CalculateFleeDirection(Unit const* boss, float& fleeX, float& fleeY) const
{
    fleeX = 0.0f;
    fleeY = 0.0f;
    bool bossDead = !boss || !boss->IsAlive();

    if (Group const* group = bot->GetGroup())
    {
        for (GroupReference const* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* p = gref->GetSource();
            if (!p || p == bot || !p->IsAlive() || p->GetMapId() != bot->GetMapId())
                continue;

            // Boss alive: ignore other Burning Adrenaline bots so they can group together
            if (!bossDead && p->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_BURNING_ADRENALINE)))
                continue;

            // Compute a flee direction via weighted repulsion from other bots.
            // Weighted repulsion: closer = stronger push (linear falloff to 20y)
            float dist = bot->GetDistance2d(p);
            if (dist < VAELASTRASZ_REPULSION_RANGE && dist > 0.1f)
            {
                float weight = 1.0f - dist / VAELASTRASZ_REPULSION_RANGE;
                fleeX += (bot->GetPositionX() - p->GetPositionX()) / dist * weight;
                fleeY += (bot->GetPositionY() - p->GetPositionY()) / dist * weight;
            }
        }
    }

    float fleeLen = sqrt(fleeX * fleeX + fleeY * fleeY);

    // No repulsion
    if (fleeLen < 0.1f)
    {
        // Tail Sweep recovery: move back toward boss
        if (!bossDead && bot->GetDistance2d(boss) > VAELASTRASZ_TAIL_SWEEP_RECOVERY_RANGE)
        {
            float toBossX = boss->GetPositionX() - bot->GetPositionX();
            float toBossY = boss->GetPositionY() - bot->GetPositionY();
            float dist = sqrt(toBossX * toBossX + toBossY * toBossY);
            if (dist > 0.1f)
            {
                fleeX = toBossX / dist;
                fleeY = toBossY / dist;
                return true;
            }
        }
        return false;
    }

    fleeX /= fleeLen;
    fleeY /= fleeLen;

    // Ranged: blend flee direction toward boss when >25y to stay in cast range
    if (!bossDead && PlayerbotAI::IsRanged(bot))
    {
        float bossDist = bot->GetDistance2d(boss);
        if (bossDist > VAELASTRASZ_RANGED_BIAS_THRESHOLD)
        {
            float toBossX = (boss->GetPositionX() - bot->GetPositionX()) / bossDist;
            float toBossY = (boss->GetPositionY() - bot->GetPositionY()) / bossDist;
            if (fleeX * toBossX + fleeY * toBossY < 0.0f)
            {
                fleeX = fleeX * VAELASTRASZ_RANGED_BIAS_FLEE_WEIGHT + toBossX * VAELASTRASZ_RANGED_BIAS_BOSS_WEIGHT;
                fleeY = fleeY * VAELASTRASZ_RANGED_BIAS_FLEE_WEIGHT + toBossY * VAELASTRASZ_RANGED_BIAS_BOSS_WEIGHT;
                float len = sqrt(fleeX * fleeX + fleeY * fleeY);
                fleeX /= len;
                fleeY /= len;
            }
        }
    }

    return true;
}

bool BwlVaelastraszMoveAwayAction::MoveAlongFleeDirection(Unit const* boss, float fleeX, float fleeY)
{
    bool bossDead = !boss || !boss->IsAlive();
    float baseAngle = atan2(fleeY, fleeX);

    // Pass 1: ranged try LOS positions first
    // Pass 2: everyone, no LOS req
    for (bool requireLos : {true, false})
    {
        // Ranged bots try LOS positions first, then fall back to no-LOS.
        if (requireLos && (!PlayerbotAI::IsRanged(bot) || bossDead))
            continue;

        // Try 5 angles (0°, ±45°, ±90°) around the flee direction to slide along walls.
        static constexpr float angleOffsets[] = {0.0f, M_PI_4, -M_PI_4, M_PI_2, -M_PI_2};
        for (float offset : angleOffsets)
        {
            float angle = baseAngle + offset;
            float targetX = bot->GetPositionX() + INCREMENTAL_MOVE_STEP_DISTANCE * cos(angle);
            float targetY = bot->GetPositionY() + INCREMENTAL_MOVE_STEP_DISTANCE * sin(angle);

            // Reject if LOS to boss would break (only checked in Pass 1 for ranged)
            if (requireLos && boss && !boss->IsWithinLOS(targetX, targetY, bot->GetPositionZ()))
                continue;

            if (MoveTo(bot->GetMapId(), targetX, targetY, bot->GetPositionZ(), false, false,
                       false, false, MovementPriority::MOVEMENT_FORCED, true))
                return true;
        }
    }

    return false;
}

// Broodlord Lashlayer

bool BwlBroodlordRangedMoveAwayAction::Execute(Event /*event*/)
{
    if (Unit* boss = AI_VALUE2(Unit*, "find target", "broodlord lashlayer"))
    {
        float distToTravel = BROODLORD_SAFE_DISTANCE - bot->GetDistance2d(boss);
        if (distToTravel > 0.0f)
            return MoveAway(boss, distToTravel);
    }
    return false;
}

// Firemaw / Ebonroc / Flamegor

bool BwlBlackDrakeAvoidBreathAction::isUseful()
{
    Unit const* boss = GetTarget();
    if (!boss)
        return false;

    // Only reposition while standing in the frontal breath cone.
    // No tail-swipe check: unlike RearFlankAction there is no rear danger zone here.
    return boss->HasInArc(2.0f * minAngle, bot);
}

// Chromaggus

bool BwlUseHourglassSandAction::Execute(Event /*event*/)
{
    return botAI->CastSpell(static_cast<uint32>(BlackwingLairSpells::SPELL_HOURGLASS_SAND), bot);
}

// Nefarian

bool BwlNefarianKillCorruptedTotemAction::Execute(Event /*event*/)
{
    Unit* totem = FindCorruptedTotem(botAI);
    return totem && Attack(totem);
}

// Nefarian phases 2 and 3 (overlay patch 0020, spec 2026-10-07)

namespace
{
    constexpr float NEF_MT_BACK_STEP_MIN = 2.0f;     // path step: at least this far along the route...
    constexpr float NEF_MT_BACK_STEP_MAX = 3.5f;     // ...and clipped to this, so aggro/facing re-check often
    constexpr float NEF_MT_STUCK_MOVE = 1.0f;        // moved less than this...
    constexpr uint32 NEF_MT_STUCK_MS = 3000;         // ...for this long in the Back step = stuck
    constexpr uint32 NEF_MT_BACK_GAP_MS = 1000;      // no Back tick for this long = a new Back episode

    // Fix round 3 (G1): the next step toward T along a navmesh route (PathGenerator::CalculatePath), not a
    // straight line - a pillar on the line made every straight step unreachable. The step is the route
    // point NEF_MT_BACK_STEP_MAX along the path (interpolated on its segment, so it stays on the mesh), or
    // the first corner past NEF_MT_BACK_STEP_MIN when that comes first, or the path end. False = no usable
    // route (caller falls back to the straight-line step).
    bool GetNefMtPathStep(Player* bot, Position const& t, float& stepX, float& stepY, float& stepZ)
    {
        PathGenerator path(bot);
        path.CalculatePath(t.GetPositionX(), t.GetPositionY(), t.GetPositionZ(), false);
        if (!(path.GetPathType() & (PATHFIND_NORMAL | PATHFIND_INCOMPLETE)))
            return false;
        Movement::PointsArray const& points = path.GetPath();
        if (points.size() < 2)
            return false;
        float travelled = 0.0f;
        G3D::Vector3 prev(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
        for (size_t i = 1; i < points.size(); ++i)
        {
            G3D::Vector3 const& p = points[i];
            float const seg = std::hypot(p.x - prev.x, p.y - prev.y);
            if (travelled + seg >= NEF_MT_BACK_STEP_MAX && seg > 0.01f)
            {
                float const f = (NEF_MT_BACK_STEP_MAX - travelled) / seg;
                stepX = prev.x + (p.x - prev.x) * f;
                stepY = prev.y + (p.y - prev.y) * f;
                stepZ = prev.z + (p.z - prev.z) * f;
                return true;
            }
            travelled += seg;
            if (travelled >= NEF_MT_BACK_STEP_MIN || i + 1 == points.size())
            {
                stepX = p.x;
                stepY = p.y;
                stepZ = p.z;
                return true;
            }
            prev = p;
        }
        return false;
    }
}

// Spec §9 #1. Close to melee and LOS first (pathfinding, MOVEMENT_COMBAT - this also runs on the
// non-combat engine, where the MT sits at landing), take Nefarian as the target (Attack also switches
// the engine to combat), and only while he is on the MT and in melee back him to T, then face him
// (Nefarian then faces T, i.e. the empty throne dais). Returns false when idle so the tank rotation runs.
bool BwlNefarianMtStationAction::Execute(Event /*event*/)
{
    NefarianView view;
    if (!GetNefarianView(bot, view) || view.mainTank != bot)
        return false;
    Creature* const nef = view.nefarian;
    Unit* const current = AI_VALUE(Unit*, "current target");
    switch (GetNefMtStep(bot, view, current))
    {
        case NefMtStep::Close:
        {
            bool const los = bot->IsWithinLOSInMap(nef);
            // Attack first where it can succeed: it sets the target and the combat engine, and its
            // facing/stop is overridden by the combat-priority move launched right after it.
            bool const attacked = los && current != nef && Attack(nef);
            bool moved = los && MoveTo(nef, NEF_MT_APPROACH_DIST, MovementPriority::MOVEMENT_COMBAT);
            if (!moved && !bot->isMoving())
                // No LOS, or the straight-line point was refused (duplicate after a short path): path to
                // his own position instead - a different point, always reachable on the floor.
                moved = MoveTo(BWL_MAP_ID, nef->GetPositionX(), nef->GetPositionY(), nef->GetPositionZ(), false,
                               false, false, false, MovementPriority::MOVEMENT_COMBAT);
            // Still out of melee and walking: claim the tick so nothing else cancels the approach.
            return attacked || moved || bot->isMoving();
        }
        case NefMtStep::Attack:
            return Attack(nef);
        case NefMtStep::Back:
        {
            Position const& t = NEF_STATIONS.mt;
            uint32 const now = getMSTime();
            // Stuck fallback: in the Back step for NEF_MT_STUCK_MS without moving NEF_MT_STUCK_MOVE - turn and
            // run to T on a plain pathfinding move (he follows his victim), then resume backing.
            if (!backAnchorMs || getMSTimeDiff(backLastMs, now) > NEF_MT_BACK_GAP_MS ||
                bot->GetExactDist2d(backAnchorX, backAnchorY) >= NEF_MT_STUCK_MOVE)
            {
                backAnchorMs = now ? now : 1;
                backAnchorX = bot->GetPositionX();
                backAnchorY = bot->GetPositionY();
            }
            backLastMs = now;
            if (getMSTimeDiff(backAnchorMs, now) >= NEF_MT_STUCK_MS)
            {
                if (MoveTo(BWL_MAP_ID, t.GetPositionX(), t.GetPositionY(), t.GetPositionZ(), false, false, false,
                           false, MovementPriority::MOVEMENT_COMBAT, true, false))
                {
                    backAnchorMs = 0;   // restart the timer from the next Back tick
                    return true;
                }
            }

            float moveX;
            float moveY;
            float moveZ;
            if (GetNefMtPathStep(bot, t, moveX, moveY, moveZ))
            {
                // Walk backwards when the step leads away from him (the same rule as GetStepToPosition;
                // the Back step already requires aggro and melee range).
                float const toStepX = moveX - bot->GetPositionX();
                float const toStepY = moveY - bot->GetPositionY();
                float const toNefX = nef->GetPositionX() - bot->GetPositionX();
                float const toNefY = nef->GetPositionY() - bot->GetPositionY();
                bool const backwards = toStepX * toNefX + toStepY * toNefY < 0.0f;
                return MoveTo(BWL_MAP_ID, moveX, moveY, moveZ, false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT, true, backwards);
            }
            // No usable route: the old straight-line step.
            bool backwards;
            if (GetStepToPosition(bot, t, BwlNefarianRules::STATION_ARRIVE, nef, moveX, moveY, backwards))
                return MoveTo(BWL_MAP_ID, moveX, moveY, t.GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT, true, backwards);
            return false;
        }
        case NefMtStep::Face:
            bot->SetFacingToObject(nef);
            return false;
        default:
            return false;
    }
}

// Cone escape (dynamic, always) or the authored flank station (once Nefarian is parked) - spec §4.2.
bool BwlNefarianPositionAction::Execute(Event /*event*/)
{
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    Position dest;
    if (!GetNefPositionTarget(bot, botAI, view, dest))
        return false;
    return MoveTo(BWL_MAP_ID, dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

// The ONLY target chooser for non-tanks while Nefarian is landed (the generic ones are vetoed by
// BwlNefarianMultiplier). nullptr = hold fire (spec §4.2/§4.3).
bool BwlNefarianDpsTargetAction::Execute(Event /*event*/)
{
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    Unit* const target = GetNefDpsTarget(bot, botAI, view);
    Unit* const current = AI_VALUE(Unit*, "current target");
    if (target == current)
        return false;
    if (target && Attack(target))
        return true;
    if (target)
        return false;

    LOG_DEBUG("playerbots", "[BWLNefarian] {} holds fire (victim {}, ratio {:.2f}, {} ms since landing)",
              bot->GetName(), view.nefarian->GetVictim() ? view.nefarian->GetVictim()->GetName() : "none",
              NefThreatRatio(bot, view), view.msSinceLanded);
    bot->AttackStop();
    bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    context->GetValue<Unit*>("current target")->Set(nullptr);
    return true;
}

// Off-tanks: infernals (taunted - they are not taunt-immune), then constructs (Hate to Zero: attack
// only), then leftover drakonids (spec §4.3). With no add, Nefarian (spec §9 #2): attacked, never
// taunted (immune), held at 0.90 of the MT's threat so a backup tank is already built when the MT dies.
bool BwlNefarianOtTargetAction::Execute(Event event)
{
    NefarianView view;
    if (!GetNefarianView(bot, view) || GetNefRole(bot, view) != NefRole::OffTank)
        return false;
    Unit* const current = AI_VALUE(Unit*, "current target");
    if (Unit* const add = GetNefOffTankTarget(bot, view))
    {
        if (current != add)
            return Attack(add);
        if (add->GetEntry() == BwlNefarianRules::NPC_CORRUPTED_INFERNAL && add->GetVictim() != bot)
            return botAI->DoSpecificAction("taunt spell", event, true);
        return false;
    }
    if (!OffTankHoldsFireOnNefarian(bot, view))
        return current != view.nefarian && Attack(view.nefarian);
    if (current != view.nefarian && bot->GetVictim() != view.nefarian)
        return false;
    LOG_DEBUG("playerbots", "[BWLNefarian] off-tank {} holds fire (victim {}, ratio {:.2f})", bot->GetName(),
              view.nefarian->GetVictim() ? view.nefarian->GetVictim()->GetName() : "none",
              NefThreatRatio(bot, view));
    bot->AttackStop();
    bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    context->GetValue<Unit*>("current target")->Set(nullptr);
    return true;
}

// The playerbots "co +tremor" tip, applied only while Nefarian is landed (Archimonde precedent). By
// name, so an era-talents clone (Vanilla 931080 / TBC 947070) resolves, not stock 8143 (spec §9 #3).
bool BwlNefarianTremorAction::Execute(Event /*event*/)
{
    if (AI_VALUE2(bool, "has totem", "tremor totem"))
        return false;
    return botAI->CanCastSpell("tremor totem", bot) && botAI->CastSpell("tremor totem", bot);
}

bool BwlNefarianDecurseTankAction::Execute(Event /*event*/)
{
    NefarianView view;
    if (!GetNefarianView(bot, view) || !view.mainTank ||
        !view.mainTank->HasAura(BwlNefarianRules::SPELL_VEIL_OF_SHADOW))
        return false;
    return botAI->CastSpell("remove curse", view.mainTank);
}

bool BwlNefarianDispelPolymorphAction::Execute(Event /*event*/)
{
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    Player* const target = GetNefPolymorphDispelTarget(bot, view);
    if (!target)
        return false;
    return botAI->CastSpell(bot->getClass() == CLASS_PRIEST ? "dispel magic" : "cleanse", target);
}

// Corrupted Healing: direct Holy heals are vetoed by the multiplier; PW:S / Renew go to TANKS ONLY
// (owner, 2026-10-07) so the priest never burns its mana on the raid (spec §4.5).
bool BwlNefarianPriestTankOnlyAction::Execute(Event /*event*/)
{
    NefarianView view;
    if (!GetNefarianView(bot, view) || !bot->HasAura(BwlNefarianRules::SPELL_CALL_PRIEST))
        return false;
    Player* tank = nullptr;
    BwlNefarianRules::TankHeal spell = BwlNefarianRules::TankHeal::None;
    if (!GetNefTankHeal(bot, botAI, view, tank, spell))
        return false;
    return botAI->CastSpell(NefTankHealSpellName(spell), tank);
}

bool BwlNefarianHunterRearmAction::Execute(Event /*event*/)
{
    uint32 const now = getMSTime();
    if (lastTryMs && getMSTimeDiff(lastTryMs, now) < 5000)
        return false;   // one try per 5 s: a refused equip must not spin every tick
    lastTryMs = now ? now : 1;
    uint32 const entry = FindSpareRangedWeapon(bot);
    if (!entry)
        return false;
    EquipAction* equip = dynamic_cast<EquipAction*>(botAI->GetAiObjectContext()->GetAction("equip"));
    if (!equip)
        return false;
    ItemIds ids;
    ids.insert(entry);
    equip->EquipItems(ids);
    LOG_DEBUG("playerbots", "[BWLNefarian] {} re-armed with item {}", bot->GetName(), entry);
    return true;
}

bool BwlNefarianRogueSurviveAction::Execute(Event /*event*/)
{
    if (botAI->CanCastSpell("cloak of shadows", bot))
        return botAI->CastSpell("cloak of shadows", bot);
    if (botAI->CanCastSpell("evasion", bot))
        return botAI->CastSpell("evasion", bot);
    return false;
}

bool BwlNefarianMtSunderAction::Execute(Event /*event*/)
{
    NefarianView view;
    if (!GetNefarianView(bot, view) || view.mainTank != bot)
        return false;
    return botAI->CastSpell("sunder armor", view.nefarian);
}

// Trash

static constexpr float WYRMGUARD_SAFE_DISTANCE = 16.0f;

Unit* BwlDeathTalonWyrmguardTankMoveAwayAction::GetTarget()
{
    // Find a nearby wyrmguard within 16 yards being tanked by someone else
    for (auto const& [guid, ref] : bot->GetThreatMgr().GetThreatenedByMeList())
    {
        Unit* unit = ref->GetOwner();
        if (!unit || !unit->IsAlive() || unit->GetEntry() != static_cast<uint32>(BlackwingLairNPCs::NPC_DEATH_TALON_WYRMGUARD))
            continue;
        if (bot->GetDistance2d(unit) > WYRMGUARD_SAFE_DISTANCE)
            continue;
        Unit* victim = unit->GetVictim();
        if (victim && victim != bot && victim->IsPlayer() && PlayerbotAI::IsTank(victim->ToPlayer()))
            return unit;
    }
    return nullptr;
}

bool BwlDeathTalonWyrmguardTankMoveAwayAction::isUseful()
{
    if (!GetTarget())
        return false;

    // Must be actively tanking a wyrmguard before moving away from another tank's
    for (auto const& [guid, ref] : bot->GetThreatMgr().GetThreatenedByMeList())
    {
        Unit* unit = ref->GetOwner();
        if (unit && unit->IsAlive() && unit->GetEntry() == static_cast<uint32>(BlackwingLairNPCs::NPC_DEATH_TALON_WYRMGUARD) &&
            unit->GetVictim() == bot)
            return true;
    }
    return false;
}

bool BwlDeathTalonWyrmguardTankMoveAwayAction::Execute(Event /*event*/)
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    float distToTravel = WYRMGUARD_SAFE_DISTANCE - bot->GetDistance2d(target);
    if (distToTravel <= 0.0f)
        return false;

    return MoveAway(target, distToTravel);
}

Unit* BwlDeathTalonWyrmguardRangedMoveAwayAction::GetTarget()
{
    // Patch 0020: for the two keyed packs, avoid every living pack guard, not only the ones on this
    // bot's threat list.
    WyrmguardPackView view;
    if (GetActiveWyrmguardPack(bot, view))
    {
        Unit* closest = nullptr;
        float closestDist = WYRMGUARD_SAFE_DISTANCE;
        for (Creature* guard : view.guards)
        {
            if (!guard)
                continue;
            float const dist = bot->GetDistance2d(guard);
            if (dist < closestDist)
            {
                closestDist = dist;
                closest = guard;
            }
        }
        return closest;
    }

    // Find the closest wyrmguard within 16 yards
    Unit* closest = nullptr;
    float closestDist = WYRMGUARD_SAFE_DISTANCE;
    for (auto const& [guid, ref] : bot->GetThreatMgr().GetThreatenedByMeList())
    {
        Unit* unit = ref->GetOwner();
        if (!unit || !unit->IsAlive() || unit->GetEntry() != static_cast<uint32>(BlackwingLairNPCs::NPC_DEATH_TALON_WYRMGUARD))
            continue;
        float dist = bot->GetDistance2d(unit);
        if (dist < closestDist)
        {
            closestDist = dist;
            closest = unit;
        }
    }
    return closest;
}

bool BwlDeathTalonWyrmguardRangedMoveAwayAction::Execute(Event /*event*/)
{
    Unit* target = GetTarget();
    if (!target || target->GetVictim() == bot)
        return false;

    float distToTravel = WYRMGUARD_SAFE_DISTANCE - bot->GetDistance2d(target);
    if (distToTravel <= 0.0f)
        return false;

    return MoveAway(target, distToTravel);
}

// Death Talon Wyrmguard packs (overlay patch 0020)

// Pick up every guard this tank's role holds (taunt when it is on someone else), then back the
// first one to its station facing it. Returns false on the station so the normal rotation runs.
bool BwlWyrmguardTankAction::Execute(Event event)
{
    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;

    int const role = GetTankRole(bot);
    if (role == BwlWyrmguardRules::NONE)
        return false;

    Unit* const currentTarget = AI_VALUE(Unit*, "current target");
    Creature* anchor = nullptr;
    int station = BwlWyrmguardRules::NONE;
    for (int s = 0; s < BwlWyrmguardRules::SLOTS; ++s)
    {
        Creature* guard = view.guards[s];
        if (!guard || view.assign.holderRole[s] != role)
            continue;

        station = view.assign.station[s];
        if (guard->GetVictim() != bot)
        {
            // Never re-Attack the current target: Attack() cancels movement (per-tick freeze).
            if (currentTarget != guard)
                return Attack(guard);
            return botAI->DoSpecificAction("taunt spell", event, true);
        }
        if (!anchor)
            anchor = guard;
    }
    if (!anchor || station == BwlWyrmguardRules::NONE)
        return false;

    if (currentTarget != anchor)
        return Attack(anchor);

    Position const& pos = WYRMGUARD_PACKS[view.packIndex].stations[station];
    float moveX;
    float moveY;
    bool backwards;
    if (!GetStepToPosition(bot, pos, BwlWyrmguardRules::STATION_ARRIVE, anchor, moveX, moveY, backwards))
        return false;

    return MoveTo(BWL_MAP_ID, moveX, moveY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, backwards);
}

// Skull only ever goes on a SETTLED guard: skull is force-added to every bot's attackers
// (AttackersValue.cpp:40-44), so an early mark would pull DPS into a stacked pack.
bool BwlWyrmguardMarkSkullAction::Execute(Event /*event*/)
{
    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;

    int const next = BwlWyrmguardRules::NextSkull(view.state, view.assign, view.skull, view.settledMs);
    if (next == BwlWyrmguardRules::NONE || next == view.skull)
        return false;

    return MarkTargetWithSkull(bot, view.guards[next]);
}

// The ONLY target chooser for non-tanks while a pack is active (the generic ones are vetoed by
// BwlWyrmguardMultiplier). nullptr from the helper means hold fire.
bool BwlWyrmguardDpsTargetAction::Execute(Event /*event*/)
{
    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;

    Unit* const target = GetWyrmguardDpsTarget(bot, botAI, view);
    Unit* const current = AI_VALUE(Unit*, "current target");
    if (target == current)
        return false;
    if (target && Attack(target))
        return true;
    // Attack() failed (LOS / invalid target): never keep hitting an unmarked pack guard (review I2).
    if (target && SlotOfGuard(view, current) == BwlWyrmguardRules::NONE)
        return false;

    bot->AttackStop();
    bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    context->GetValue<Unit*>("current target")->Set(nullptr);
    return true;
}

static constexpr float WYRMGUARD_MIN_STEP_AWAY = 3.0f;

// Held melee keep KEEP_AWAY from every pack guard; released melee from every guard but skull's
// station. A minimum step stops sub-yard jitter moves that MoveTo's duplicate guard refuses.
bool BwlWyrmguardMeleePositionAction::Execute(Event /*event*/)
{
    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;

    int const s = WyrmguardStompThreat(bot, view);
    if (s == BwlWyrmguardRules::NONE)
        return false;

    float const dist = bot->GetExactDist2d(view.guards[s]);
    return MoveAway(view.guards[s], std::max(BwlWyrmguardRules::KEEP_AWAY - dist, WYRMGUARD_MIN_STEP_AWAY));
}

// Spec §4.9: step along the line bot -> pack-1 centroid until EBONROC_KEEPOUT_EXIT from his path.
// The centroid is south-west of the ramp, so that line leads away from the path from anywhere
// near it. MOVEMENT_FORCED so follow cannot drag the bot straight back in.
bool BwlWyrmguardEbonrocKeepOutAction::Execute(Event /*event*/)
{
    if (!EbonrocKeepOutActive(bot))
        return false;

    float const d = EbonrocPathDistance(bot);
    if (d >= BwlWyrmguardRules::EBONROC_KEEPOUT_ENTER)
        return false;

    Position const& c = PACK1_STATION_CENTROID;
    float const toCentroid = bot->GetExactDist2d(c.GetPositionX(), c.GetPositionY());
    float const step = std::min(toCentroid, BwlWyrmguardRules::EBONROC_KEEPOUT_EXIT - d + 3.0f);
    if (toCentroid < 0.1f || step <= 0.0f)
        return false;

    float const t = step / toCentroid;
    float const x = bot->GetPositionX() + (c.GetPositionX() - bot->GetPositionX()) * t;
    float const y = bot->GetPositionY() + (c.GetPositionY() - bot->GetPositionY()) * t;
    return MoveTo(BWL_MAP_ID, x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED);
}

// Spec §4.8a healer positioning: score a fan of ground-snapped candidates around the healer (rings of
// 5-30 y, 16 bearings) with the pure rules and walk to the best one, only when it strictly beats
// staying. Candidates are pre-ranked on their range-only score (an upper bound: LoS only removes
// tanks), so the collision snap and the LoS probes run on the few that can still win.
static constexpr float WYRMGUARD_HEALER_RING_STEP = 5.0f;
static constexpr int WYRMGUARD_HEALER_RINGS = 6;
static constexpr int WYRMGUARD_HEALER_BEARINGS = 16;
static constexpr uint32 WYRMGUARD_HEALER_RETRY_MS = 1000;   // a fruitless sweep is not repeated every tick
static constexpr uint32 WYRMGUARD_HEALER_COMMIT_MS = 4000;  // finish a chosen walk before re-planning

bool BwlWyrmguardHealerPositionAction::Execute(Event /*event*/)
{
    using namespace BwlWyrmguardRules;

    uint32 const now = getMSTime();
    if (noSpotMs && getMSTimeDiff(noSpotMs, now) < WYRMGUARD_HEALER_RETRY_MS)
        return false;
    // Commit to a chosen spot: re-planning every tick mid-walk would re-target the move each tick and
    // jitter. Finish the walk (bounded) unless it has stopped.
    if (movingSinceMs && bot->isMoving() && getMSTimeDiff(movingSinceMs, now) < WYRMGUARD_HEALER_COMMIT_MS)
        return false;
    movingSinceMs = 0;

    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;
    Map* map = bot->GetMap();
    if (!map)
        return false;

    Player* tanks[SLOTS];
    int const n = GetWyrmguardHolderTanks(bot, view, tanks);
    Pt tankPt[SLOTS];
    for (int i = 0; i < n; ++i)
        tankPt[i] = {tanks[i]->GetPositionX(), tanks[i]->GetPositionY()};

    float const bx = bot->GetPositionX();
    float const by = bot->GetPositionY();
    float const bz = bot->GetPositionZ();
    Pt const here{bx, by};

    SpotScore best = ScoreSpot(here, here, tankPt, n, view.state);
    best.tanks = 0;
    for (int i = 0; i < n; ++i)
        if (Dist2d(here, tankPt[i]) <= HEALER_RANGE && bot->IsWithinLOSInMap(tanks[i]))
            ++best.tanks;

    struct Candidate
    {
        Pt p;
        SpotScore bound;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(WYRMGUARD_HEALER_RINGS * WYRMGUARD_HEALER_BEARINGS);
    for (int ring = 1; ring <= WYRMGUARD_HEALER_RINGS; ++ring)
    {
        float const radius = ring * WYRMGUARD_HEALER_RING_STEP;
        for (int b = 0; b < WYRMGUARD_HEALER_BEARINGS; ++b)
        {
            float const angle = b * (2.0f * float(M_PI) / WYRMGUARD_HEALER_BEARINGS);
            Pt const p{bx + radius * std::cos(angle), by + radius * std::sin(angle)};
            SpotScore const bound = ScoreSpot(here, p, tankPt, n, view.state);
            if (BetterSpot(bound, best))
                candidates.push_back({p, bound});
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](Candidate const& a, Candidate const& b) { return BetterSpot(a.bound, b.bound); });

    bool found = false;
    float mx = 0.0f, my = 0.0f, mz = 0.0f;
    for (Candidate const& c : candidates)
    {
        if (!BetterSpot(c.bound, best))
            break;   // sorted by upper bound: nothing after this can win
        float x = c.p.x, y = c.p.y, z = bz;
        if (!map->CheckCollisionAndGetValidCoords(bot, bx, by, bz, x, y, z))
            continue;
        Pt const snapped{x, y};
        SpotScore s = ScoreSpot(here, snapped, tankPt, n, view.state);
        if (!s.walkClear)
            continue;
        s.tanks = 0;
        for (int i = 0; i < n; ++i)
            if (Dist2d(snapped, tankPt[i]) <= HEALER_RANGE && WyrmguardSpotInLos(bot, x, y, z, tanks[i]))
                ++s.tanks;
        if (BetterSpot(s, best))
        {
            best = s;
            mx = x;
            my = y;
            mz = z;
            found = true;
        }
    }

    if (!found)
    {
        noSpotMs = now ? now : 1;
        return false;
    }
    noSpotMs = 0;
    movingSinceMs = now ? now : 1;
    return MoveTo(BWL_MAP_ID, mx, my, mz, false, false, false, true, MovementPriority::MOVEMENT_COMBAT);
}

// BWL-wide DPS threat discipline (overlay patch 0020, spec 2026-10-08)
//
// Stop the running auto-attack / auto-shot but keep the current target: once the ratio falls under
// BWL_THREAT_LIMIT the vetoes lift and the stock rotation and melee restart the attack on it.
bool BwlThreatHoldAction::Execute(Event /*event*/)
{
    bot->AttackStop();
    bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    return true;
}
