/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Princess Huhuran actions (overlay patch 0016). Spec:
// docs/superpowers/specs/2026-10-20-playerbot-aq40-huhuran-design.md (overlay repo), §4.2-4.3.

#include "Aq40Actions.h"

#include <cmath>

#include "Aq40Helpers.h"
#include "PathGenerator.h"
#include "Playerbots.h"
#include "Timer.h"

using namespace TempleOfAhnQirajHelpers;

namespace
{
    constexpr float HUHURAN_MT_BACK_STEP_MIN = 2.0f;  // path step: at least this far along the route...
    constexpr float HUHURAN_MT_BACK_STEP_MAX = 3.5f;  // ...and clipped to this, so aggro/facing re-check often
    constexpr float HUHURAN_MT_STUCK_MOVE = 1.0f;     // moved less than this...
    constexpr uint32 HUHURAN_MT_STUCK_MS = 3000;      // ...for this long in the Back step = stuck
    constexpr uint32 HUHURAN_MT_BACK_GAP_MS = 1000;   // no Back tick for this long = a new Back episode
    // Every direct move to T is one leg: PathGenerator silently fails past ~296 y (spec §4.2).
    constexpr float HUHURAN_MT_MAX_LEG = 220.0f;
    // "[Aq40Huhuran] ... off-tank spot move refused" throttle, per bot.
    constexpr uint32 HUHURAN_OT_REFUSED_LOG_MS = 5000;

    // The next step toward T along a navmesh route (PathGenerator::CalculatePath), not a straight line - a
    // pillar on the line made every straight step unreachable at Nefarian. Copied from BWL GetNefMtPathStep:
    // the route point HUHURAN_MT_BACK_STEP_MAX along the path (interpolated on its segment, so it stays on
    // the mesh), or the first corner past HUHURAN_MT_BACK_STEP_MIN when that comes first, or the path end.
    // False = no usable route (the caller falls back to a straight-line step).
    bool GetHuhuranMtPathStep(Player* bot, Position const& t, float& stepX, float& stepY, float& stepZ)
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
            if (travelled + seg >= HUHURAN_MT_BACK_STEP_MAX && seg > 0.01f)
            {
                float const f = (HUHURAN_MT_BACK_STEP_MAX - travelled) / seg;
                stepX = prev.x + (p.x - prev.x) * f;
                stepY = prev.y + (p.y - prev.y) * f;
                stepZ = prev.z + (p.z - prev.z) * f;
                return true;
            }
            travelled += seg;
            if (travelled >= HUHURAN_MT_BACK_STEP_MIN || i + 1 == points.size())
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

    // Straight-line fallback: HUHURAN_MT_BACK_STEP_MAX toward T (or T itself when nearer).
    bool GetHuhuranMtStraightStep(Player* bot, Position const& t, float& stepX, float& stepY)
    {
        float const dx = t.GetPositionX() - bot->GetPositionX();
        float const dy = t.GetPositionY() - bot->GetPositionY();
        float const d = std::hypot(dx, dy);
        if (d <= HUHURAN_MT_ARRIVE)
            return false;
        float const f = d <= HUHURAN_MT_BACK_STEP_MAX ? 1.0f : HUHURAN_MT_BACK_STEP_MAX / d;
        stepX = bot->GetPositionX() + dx * f;
        stepY = bot->GetPositionY() + dy * f;
        return true;
    }

    // Walk backwards when the step leads away from her (the Back step already requires aggro and melee).
    bool StepIsBackwards(Player* bot, Unit* huhuran, float stepX, float stepY)
    {
        float const toStepX = stepX - bot->GetPositionX();
        float const toStepY = stepY - bot->GetPositionY();
        float const toBossX = huhuran->GetPositionX() - bot->GetPositionX();
        float const toBossY = huhuran->GetPositionY() - bot->GetPositionY();
        return toStepX * toBossX + toStepY * toBossY < 0.0f;
    }
}  // namespace

bool Aq40HuhuranEraseTrackersAction::Execute(Event /*event*/)
{
    HuhuranEraseTrackers(bot);
    return false;
}

// Spec §4.2, mirroring BWL BwlNefarianMtStationAction. Close to melee and LOS first (pathfinding,
// MOVEMENT_COMBAT), take her as the target (Attack also switches the engine to combat), and only while she
// is on the MT, in melee and not parked, back her toward T. Rev 7 (spec §12.2 #2): once she is parked the MT's
// generic movers are vetoed, and with no step left this turns it in place to face her (only while stopped).
// Returns false when idle so the tank rotation runs.
bool Aq40HuhuranParkBossAction::Execute(Event /*event*/)
{
    HuhuranView view;
    if (!GetHuhuranView(bot, view) || view.mainTank != bot)
        return false;
    Creature* const huhuran = view.huhuran;
    Unit* const current = AI_VALUE(Unit*, "current target");
    switch (GetHuhuranMtStep(bot, view, current))
    {
        case HuhuranMtStep::Close:
        {
            bool const los = bot->IsWithinLOSInMap(huhuran);
            // Attack first where it can succeed: it sets the target and the combat engine, and its
            // facing/stop is overridden by the combat-priority move launched right after it.
            bool const attacked = los && current != huhuran && Attack(huhuran);
            bool moved = los && MoveTo(huhuran, HUHURAN_MT_APPROACH_DIST, MovementPriority::MOVEMENT_COMBAT);
            if (!moved && !bot->isMoving())
                // No LOS, or the straight-line point was refused (duplicate after a short path): path to her
                // own position instead - a different point, always reachable on the floor.
                moved = MoveTo(AQ40_MAP_ID, huhuran->GetPositionX(), huhuran->GetPositionY(),
                               huhuran->GetPositionZ(), false, false, false, false,
                               MovementPriority::MOVEMENT_COMBAT);
            // Still out of melee and walking: claim the tick so nothing else cancels the approach.
            return attacked || moved || bot->isMoving();
        }
        case HuhuranMtStep::Attack:
            return Attack(huhuran);
        case HuhuranMtStep::Back:
        {
            Position const t = GetHuhuranTankStation(bot, view);  // latched by GetHuhuranMtStep
            uint32 const now = getMSTime();
            // Stuck fallback: in the Back step for HUHURAN_MT_STUCK_MS without moving HUHURAN_MT_STUCK_MOVE -
            // turn and run to T on a plain pathfinding move (she follows her victim), then resume backing.
            if (!backAnchorMs || getMSTimeDiff(backLastMs, now) > HUHURAN_MT_BACK_GAP_MS ||
                bot->GetExactDist2d(backAnchorX, backAnchorY) >= HUHURAN_MT_STUCK_MOVE)
            {
                backAnchorMs = now ? now : 1;
                backAnchorX = bot->GetPositionX();
                backAnchorY = bot->GetPositionY();
            }
            backLastMs = now;
            if (getMSTimeDiff(backAnchorMs, now) >= HUHURAN_MT_STUCK_MS &&
                bot->GetExactDist2d(t.GetPositionX(), t.GetPositionY()) <= HUHURAN_MT_MAX_LEG)
            {
                if (MoveTo(AQ40_MAP_ID, t.GetPositionX(), t.GetPositionY(), t.GetPositionZ(), false, false,
                           false, false, MovementPriority::MOVEMENT_COMBAT, true, false))
                {
                    backAnchorMs = 0;  // restart the timer from the next Back tick
                    return true;
                }
            }

            float moveX;
            float moveY;
            float moveZ;
            if (GetHuhuranMtPathStep(bot, t, moveX, moveY, moveZ))
                return MoveTo(AQ40_MAP_ID, moveX, moveY, moveZ, false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT, true, StepIsBackwards(bot, huhuran, moveX, moveY));
            // No usable route: a straight-line step.
            if (GetHuhuranMtStraightStep(bot, t, moveX, moveY))
                return MoveTo(AQ40_MAP_ID, moveX, moveY, t.GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT, true, StepIsBackwards(bot, huhuran, moveX, moveY));
            return false;
        }
        default:
            // Rev 7 (spec §12.2 #2): parked, the MT holds position (its generic movers are vetoed) and turns in
            // place to face her; false while it moves, so the turn never claims every tick.
            if (HuhuranMtShouldFace(bot, view))
            {
                bot->SetFacingToObject(huhuran);
                return true;
            }
            return false;
    }
}

// Off-tanks build backup threat from melee (spec §4.2): attacked, never taunted (she is immune). Rev 2 (spec
// §7.2 #3): first walk to the melee spot beside the MT (her position + combat reach + 1.5 y along her -> MT,
// +-20 deg per off-tank rank), then take her as the target. Moving comes first because Attack() cancels
// movement. The Attack is skipped at the threat cap (§7.2 #2): this action is exempt from the threat
// multiplier (an "aq40 huhuran" action), so it gates itself.
bool Aq40HuhuranOffTankEngageAction::Execute(Event /*event*/)
{
    HuhuranView view;
    if (!GetHuhuranView(bot, view) || view.mainTank == bot || HuhuranOpenerHold(view))
        return false;

    Position spot;
    bool const hasSpot = GetHuhuranOffTankSpot(bot, view, spot);
    float const spotDist = hasSpot ? bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) : 0.0f;
    if (hasSpot && spotDist > Aq40HuhuranRules::OFFTANK_SPOT_TOLERANCE && botAI->CanMove())
    {
        if (MoveTo(AQ40_MAP_ID, spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                   false, MovementPriority::MOVEMENT_COMBAT, true))
            return true;
        // MoveTo's already-heading-there guard: holding course, claim the tick.
        if (bot->isMoving())
            return true;
        uint32 const now = getMSTime();
        if (!refusedLogMs || getMSTimeDiff(refusedLogMs, now) >= HUHURAN_OT_REFUSED_LOG_MS)
        {
            refusedLogMs = now ? now : 1;
            LOG_DEBUG("playerbots", "[Aq40Huhuran] {} off-tank spot move refused, dist {:.1f}", bot->GetName(),
                      bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()));
        }
    }

    // Rev 6 (spec §11.2): at the spot, turn to face her - in place, no movement. Stock "tank face" is vetoed
    // here by the movement multiplier, and without it the off-tank never swung (no rage, no threat). Only while
    // standing still (rev 7): a moving bot's turn may not take, and must not claim the tick forever.
    bool faced = false;
    if (hasSpot && spotDist <= Aq40HuhuranRules::OFFTANK_SPOT_TOLERANCE && bot->IsStopped() &&
        !bot->HasInArc(HUHURAN_FACE_ARC, view.huhuran))
    {
        bot->SetFacingToObject(view.huhuran);
        faced = true;
    }

    if (AI_VALUE(Unit*, "current target") == view.huhuran)
        return faced;
    if (HuhuranThreatVetoed(bot, view))
        return faced;
    return Attack(view.huhuran) || faced;
}

// Ring spot (spec §4.3). The trigger is false within SLOT_TOLERANCE, so this only runs off the spot.
bool Aq40HuhuranHoldSpotAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    HuhuranView view;
    if (!GetHuhuranView(bot, view))
        return false;
    Position spot;
    if (!GetHuhuranSpot(bot, botAI, view, spot))
        return false;

    bool const moved = MoveTo(AQ40_MAP_ID, spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false,
                              false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    if (moved)
        return true;
    // A false while walking is MoveTo's already-heading-there guard: the bot is holding its course, so claim
    // the tick (a false would fall through to lower actions). Standing still = MoveTo refused the spot.
    if (bot->isMoving())
        return true;
    LOG_DEBUG("playerbots", "[Aq40Huhuran] {} hold spot move refused, dist {:.1f}", bot->GetName(),
              bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()));
    return false;
}

// Rev 2 (spec §7.2 #2), BwlThreatHoldAction: stop the swing / auto-repeat, keep the target.
bool Aq40HuhuranThreatHoldAction::Execute(Event /*event*/)
{
    bot->AttackStop();
    bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    return true;
}

// Rev 2 (spec §7.2 #4): re-resolved at execution (the trigger's pick may be a tick old). False = stock healing.
bool Aq40HuhuranTankHealAction::Execute(Event /*event*/)
{
    HuhuranView view;
    if (!GetHuhuranView(bot, view))
        return false;
    Player* tank = nullptr;
    char const* spell = nullptr;
    if (!GetHuhuranTankHeal(bot, botAI, view, tank, spell))
        return false;
    return botAI->CastSpell(spell, tank);
}

// Rev 3 (spec §8.2 #2): cast it by name on self; the once-flag (MT guid + spell) is set only when it went off.
bool Aq40HuhuranMtBerserkDefensiveAction::Execute(Event /*event*/)
{
    HuhuranView view;
    if (!GetHuhuranView(bot, view))
        return false;
    char const* spell = nullptr;
    HuhuranMtDefensive const which = GetHuhuranMtDefensive(bot, botAI, view, spell);
    if (which == HuhuranMtDefensive::None || !botAI->CastSpell(spell, bot))
        return false;
    MarkHuhuranMtDefensiveUsed(bot, which);
    LOG_DEBUG("playerbots", "[Aq40Huhuran] {} mt berserk defensive: {} at {:.0f}%", bot->GetName(), spell,
              bot->GetHealthPct());
    return true;
}
