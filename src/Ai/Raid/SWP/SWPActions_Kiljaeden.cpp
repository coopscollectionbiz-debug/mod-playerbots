/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPActions.h"

#include <algorithm>
#include <cmath>

#include "GameObject.h"
#include "MotionMaster.h"
#include "Playerbots.h"
#include "SWPHelpers.h"

using namespace SunwellPlateauHelpers;

// Same five-rung ladder as the M'uru escapes, and it exists for the same reason: MoveTo answers an
// unwalkable destination by silently refusing forever rather than by failing loudly, so a single authored
// point can freeze a bot in a kill zone (the Felmyst rev-3 failure). Logging WHICH rung was accepted is
// what separates "the bot is refusing to move" from "every bearing out of here is off-mesh" - two
// failures that look identical from the outside and need opposite fixes.
//
// A macro rather than a helper because MovementAction::MoveTo is protected, so the loop has to live
// inside the member that calls it.
#define KJ_RUN_LADDER(spotFn)                                                                        \
    uint32 accepted = MURU_FLEE_CANDIDATES;                                                          \
    float usedX = bot->GetPositionX();                                                               \
    float usedY = bot->GetPositionY();                                                               \
    for (uint32 attempt = 0; attempt < MURU_FLEE_CANDIDATES; ++attempt)                              \
    {                                                                                                \
        Position spot;                                                                               \
        if (!(spotFn)(bot, spot, attempt))                                                           \
            continue;                                                                                \
        if (MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),    \
                   false, false, false, false, MovementPriority::MOVEMENT_FORCED))                    \
        {                                                                                            \
            accepted = attempt;                                                                      \
            usedX = spot.GetPositionX();                                                             \
            usedY = spot.GetPositionY();                                                             \
            break;                                                                                   \
        }                                                                                            \
    }

bool KjClearArmageddonAction::Execute(Event /*event*/)
{
    // Diagnostics BEFORE the CanMove bail, never after. On Felmyst they sat past that bail, so the bots
    // that mattered - the ones that had lost control and could not act at all - were the only ones that
    // logged nothing.
    Creature* marker = FindKjArmageddonMarker(bot);
    float const distance = marker
                               ? bot->GetExactDist2d(marker->GetPositionX(), marker->GetPositionY())
                               : -1.0f;

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[KJ] {} armageddon CANNOT MOVE at {:.1f}y", bot->GetName(), distance);
        return false;
    }

    // "Already clear" gets its own line, because otherwise it is indistinguishable from "every bearing was
    // refused" - both come out of the ladder as `try 5`. Reading the first kill's trace, 1123 of 1427
    // armageddon lines were `try 5`, which looked like a catastrophic refusal rate until the `moving` flag
    // showed 98% of them had an order IN FLIGHT (MoveTo's duplicate-move guard correctly holding a good
    // order) and only 28 samples were genuinely stuck. M'uru had an explicit hold branch for exactly this
    // and rev 1 of this file did not carry it over.
    Position probe;
    if (!GetKjArmageddonClearSpot(bot, probe, 0))
    {
        LOG_DEBUG("playerbots", "[KJ] {} holding clear of armageddon at {:.1f}y", bot->GetName(), distance);
        return false;
    }

    KJ_RUN_LADDER(GetKjArmageddonClearSpot)

    LOG_DEBUG("playerbots",
              "[KJ] {} clearing armageddon -> ({:.1f},{:.1f}) try {} at {:.1f}y moving {}",
              bot->GetName(), usedX, usedY, accepted, distance, bot->isMoving() ? 1 : 0);

    return false;
}

bool KjQuarantineFireBloomAction::Execute(Event /*event*/)
{
    float const outward = bot->GetExactDist2d(KJ_ANCHOR_X, KJ_ANCHOR_Y);

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[KJ] {} fire bloom CANNOT MOVE at {:.1f}y", bot->GetName(), outward);
        return false;
    }

    Position probe;
    if (!GetKjFireBloomSpot(bot, probe, 0))
    {
        LOG_DEBUG("playerbots", "[KJ] {} holding fire bloom quarantine at {:.1f}y", bot->GetName(),
                  outward);
        return false;
    }

    KJ_RUN_LADDER(GetKjFireBloomSpot)

    // `moving` was missing from this line in rev 1, which left its 667 `try 5` samples unclassifiable -
    // the armageddon line had the flag and its refusals turned out to be 98% benign. Never ship one of
    // these lines without it.
    LOG_DEBUG("playerbots",
              "[KJ] {} quarantining fire bloom -> ({:.1f},{:.1f}) try {} at {:.1f}y moving {}",
              bot->GetName(), usedX, usedY, accepted, outward, bot->isMoving() ? 1 : 0);

    return false;
}

#undef KJ_RUN_LADDER

bool KjStackForDarknessAction::Execute(Event /*event*/)
{
    uint32 remaining = 0;
    KjDarknessWindow(bot, remaining);

    Position spot;
    if (!GetKjDarknessStackSpot(bot, spot))
    {
        // Seated. Returning false with the predicate still true is what makes this a HOLD: the movement
        // veto keeps standing so generic behaviour cannot walk the bot back out of the shield, but nothing
        // issues a move, so it parks instead of shuffling for the whole eight seconds.
        LOG_DEBUG("playerbots", "[KJ] {} seated for darkness, {}ms left", bot->GetName(), remaining);
        return false;
    }

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[KJ] {} darkness stack CANNOT MOVE, {}ms left", bot->GetName(),
                  remaining);
        return false;
    }

    bool const moved = MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(),
                              spot.GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_FORCED);

    // The remaining duration is in the line because it is the only number that decides whether this
    // design works: 47,499 lands the instant the aura expires, so a bot still walking at 0ms is a dead
    // bot and the trace has to show which it was.
    LOG_DEBUG("playerbots", "[KJ] {} stacking for darkness -> ({:.1f},{:.1f}) at {:.1f}y, {}ms left, moved {}",
              bot->GetName(), spot.GetPositionX(), spot.GetPositionY(),
              bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()), remaining,
              moved ? 1 : 0);

    return false;
}

bool KjHoldAnchorAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    Position anchor;
    GetKjAnchor(bot, anchor);

    bool const moved = MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(),
                              anchor.GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT);

    LOG_DEBUG("playerbots", "[KJ] {} holding anchor at {:.1f}y moved {}", bot->GetName(),
              bot->GetExactDist2d(anchor.GetPositionX(), anchor.GetPositionY()), moved ? 1 : 0);

    return false;
}

bool KjHoldStationAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    Position station;
    if (!GetKjStation(bot, botAI, station))
        return false;

    bool const moved = MoveTo(bot->GetMapId(), station.GetPositionX(), station.GetPositionY(),
                              station.GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT);

    LOG_DEBUG("playerbots", "[KJ] {} to station ({:.1f},{:.1f}) at {:.1f}y moved {}", bot->GetName(),
              station.GetPositionX(), station.GetPositionY(),
              bot->GetExactDist2d(station.GetPositionX(), station.GetPositionY()), moved ? 1 : 0);

    return false;
}

bool KjClaimOrbAction::Execute(Event /*event*/)
{
    GameObject* orb = FindKjEmpoweredOrb(bot);
    if (!orb)
        return false;

    float const distance = bot->GetExactDist2d(orb->GetPositionX(), orb->GetPositionY());

    if (distance <= KJ_ORB_USE_RANGE)
    {
        // GameObject::Use is the established way a bot clicks a goober in this module (the TOC lance rack,
        // the Magtheridon cubes, the BT spines all do exactly this). The orb's own spell, 45833, then
        // casts 45836 + 45839 on the clicker via spell_kiljaeden_power_of_the_blue_flight.
        orb->Use(bot);

        LOG_DEBUG("playerbots", "[KJ] {} clicked orb {} at {:.1f}y, drake {}", bot->GetName(),
                  orb->GetEntry(), distance, GetKjDrake(bot) ? 1 : 0);
        return true;
    }

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[KJ] {} orb walk CANNOT MOVE at {:.1f}y", bot->GetName(), distance);
        return false;
    }

    // The orb is a FIXED world point, so a single MoveTo is stable across ticks all on its own and
    // IsDuplicateMove holds the order until it completes. The short rungs are only the usual off-mesh
    // insurance: each is a fraction of the way along the same bearing, so any accepted one is progress.
    static float const fractions[KJ_ORB_WALK_CANDIDATES] = { 1.0f, 0.66f, 0.33f };

    uint32 accepted = KJ_ORB_WALK_CANDIDATES;
    for (uint32 attempt = 0; attempt < KJ_ORB_WALK_CANDIDATES; ++attempt)
    {
        float const f = fractions[attempt];
        float x = bot->GetPositionX() + (orb->GetPositionX() - bot->GetPositionX()) * f;
        float y = bot->GetPositionY() + (orb->GetPositionY() - bot->GetPositionY()) * f;
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        if (MoveTo(bot->GetMapId(), x, y, z, false, false, false, false,
                   MovementPriority::MOVEMENT_FORCED))
        {
            accepted = attempt;
            break;
        }
    }

    LOG_DEBUG("playerbots", "[KJ] {} walking to orb {} at {:.1f}y try {} moving {}", bot->GetName(),
              orb->GetEntry(), distance, accepted, bot->isMoving() ? 1 : 0);

    return true;
}

bool KjDriveDrakeAction::Execute(Event /*event*/)
{
    Creature* drake = GetKjDrake(bot);
    if (!drake)
        return false;

    // Refresh first, because everything below is pointless on a possession that is about to lapse. The
    // pilot's body cannot walk back to an orb while it is possessing (UNIT_FLAG_DISABLE_MOVE is set on the
    // CHARMER by Unit::SetCharmedBy), so dropping the aura is the only way to start a re-claim - and
    // KjShouldRefreshPossession refuses to do it inside a Darkness window.
    if (KjShouldRefreshPossession(bot))
    {
        LOG_DEBUG("playerbots", "[KJ] {} dropping possession to refresh", bot->GetName());
        bot->RemoveAurasDueToSpell(static_cast<uint32>(SunwellSpells::SPELL_KJ_VENGEANCE_BLUE));
        return true;
    }

    uint32 const shield = static_cast<uint32>(SunwellSpells::SPELL_KJ_SHIELD_OF_THE_BLUE);
    uint32 const revitalize = static_cast<uint32>(SunwellSpells::SPELL_KJ_BREATH_REVITALIZE);
    uint32 const haste = static_cast<uint32>(SunwellSpells::SPELL_KJ_BREATH_HASTE);

    Position anchor;
    GetKjAnchor(bot, anchor);
    float const drift = drake->GetExactDist2d(anchor.GetPositionX(), anchor.GetPositionY());

    // 1. THE SHIELD, ahead of everything including repositioning. It is a persistent area aura at the
    //    drake's own position with a 12y radius and a 5s life against an 8s aura, so firing at
    //    KJ_SHIELD_LEAD_MS remaining covers the expiry with a second of margin on both sides.
    //
    //    Cooldowns are added BY HAND because a charmed creature observes none of its own - not even the
    //    GCD (the hard-won note on TeronGorefiendControlAndDestroyShadowyConstructsAction). Without this
    //    the drake would re-cast every tick and the 20s cooldown would never exist.
    uint32 remaining = 0;
    if (KjDarknessWindow(bot, remaining) && remaining <= KJ_SHIELD_LEAD_MS &&
        !drake->HasSpellCooldown(shield))
    {
        drake->CastSpell(drake, shield, true);
        drake->AddSpellCooldown(shield, 0, KJ_SHIELD_COOLDOWN_MS);

        // THE COVERAGE COUNT IS THE POINT OF THIS LINE. A bot left outside the 12y bubble takes the full
        // 47,499 and nothing else in the trace would say so - the stack action logs "seated" and this one
        // logs "SHIELD". "covered 22/24" is the only evidence that the error budget on
        // KJ_STACK_RING_OUTER + KJ_STACK_TOLERANCE + KJ_DRAKE_PARK_TOLERANCE actually held in game.
        uint32 total = 0;
        uint32 const covered = CountKjShieldCoverage(bot, drake, total);

        LOG_DEBUG("playerbots", "[KJ] {} drake SHIELD at {}ms left, drift {:.1f}y, covered {}/{}",
                  bot->GetName(), remaining, drift, covered, total);
        return true;
    }

    // A SECOND COVERAGE SAMPLE, AT THE MOMENT THAT ACTUALLY MATTERS. 45657 fires when the aura EXPIRES, so
    // coverage at cast time (~4000ms remaining) is not the number that decides who lives - it is merely the
    // number that was easy to log. The first kill read 13/22, 18/22, 18/21 at cast and looked alarming;
    // total held at 22 -> 22 -> 21 across the fight, so the late arrivals plainly were covered by the time
    // the damage landed. This line is what turns that inference into evidence.
    if (remaining > 0 && remaining <= KJ_COVERAGE_SAMPLE_MS)
    {
        uint32 total = 0;
        uint32 const covered = CountKjShieldCoverage(bot, drake, total);

        LOG_DEBUG("playerbots", "[KJ] {} darkness LANDING in {}ms, covered {}/{} shielded {}",
                  bot->GetName(), remaining, covered, total,
                  drake->HasSpellCooldown(shield) ? 1 : 0);
    }

    // 2. Park on the anchor, which is where the main tank holds the boss and therefore where the melee
    //    stack and the Darkness stack both are. Only re-issued while the drake is standing still: once a
    //    MovePoint is in flight, re-ordering it every tick would restart the spline forever.
    if (drift > KJ_DRAKE_PARK_TOLERANCE)
    {
        if (!drake->isMoving())
        {
            drake->GetMotionMaster()->Clear();
            drake->GetMotionMaster()->MovePoint(0, anchor.GetPositionX(), anchor.GetPositionY(),
                                                anchor.GetPositionZ());

            LOG_DEBUG("playerbots", "[KJ] {} drake -> anchor, drift {:.1f}y", bot->GetName(), drift);
        }

        return true;
    }

    // 3. Parked, so spend the two breaths. They are the drake's whole contribution BETWEEN Darknesses and
    //    they are on 10s cooldowns against a 45s Darkness cycle, so there is time for roughly four casts
    //    per cycle - none of which may be allowed to delay the shield, which is why they sit below it.
    //
    //    Revitalize before Haste: at level 70 this fight is a survival check, and 449 health plus 449 mana
    //    every 2s for 10s is worth more than 24% haste on whoever happens to be in the cone.
    struct BreathPlan
    {
        uint32 spell;
        bool healing;
        char const* label;
    };
    BreathPlan const breaths[2] = { { revitalize, true, "REVITALIZE" }, { haste, false, "HASTE" } };

    for (BreathPlan const& plan : breaths)
    {
        if (drake->HasSpellCooldown(plan.spell))
            continue;

        // AIM IT. Both are TARGET_UNIT_CONE_ALLY at 13y, so a cone left pointing at open floor hits
        // nobody at all. Revitalize goes at the lowest-health raider in range, Haste at the melee stack.
        float bearing = 0.0f;
        bool aimed = false;
        if (GetKjBreathBearing(bot, drake, plan.healing, bearing))
        {
            float delta = bearing - drake->GetOrientation();
            while (delta > float(M_PI))
                delta -= 2.0f * float(M_PI);
            while (delta < -float(M_PI))
                delta += 2.0f * float(M_PI);

            if (std::fabs(delta) > KJ_BREATH_FACING_EPSILON)
            {
                // SetFacingTo FIRST so its spline starts from the real old bearing and the client sees the
                // turn, then SetOrientation so the server-side HasInArc test is correct THIS tick - the
                // facing spline does not update m_orientation until it resolves, so without the second call
                // the cast below would be tested against the old facing. Only re-spline while stopped.
                if (!drake->isMoving())
                    drake->SetFacingTo(bearing);
                drake->SetOrientation(bearing);
                aimed = true;
            }
        }

        drake->CastSpell(drake, plan.spell, true);
        drake->AddSpellCooldown(plan.spell, 0, KJ_BREATH_COOLDOWN_MS);

        LOG_DEBUG("playerbots", "[KJ] {} drake breath {} bearing {:.2f} refaced {}", bot->GetName(),
                  plan.label, bearing, aimed ? 1 : 0);
        return true;
    }

    // Nothing to do, and STILL true. The pilot's body is pacified and silenced by 45839, so every action
    // below this one is a no-op; holding the tick keeps the queue from thrashing on them.
    return true;
}

bool KjFocusTargetAction::Execute(Event /*event*/)
{
    Unit* wanted = GetKjFocusTarget(bot, botAI);
    if (!wanted)
        return false;

    // hp to ONE decimal, with the bucket alongside it. Rev 3 of the M'uru work logged "{:.0f}%" and that
    // is what hid a violent target flip as a steady state: two adds a fraction of a percent apart both
    // printed the same number for hundreds of lines.
    uint32 const entry = wanted->ToCreature() ? wanted->ToCreature()->GetEntry() : 0;
    LOG_DEBUG("playerbots", "[KJ] {} retargeting {}->{} (entry {}) hp {:.1f}% bucket {} melee {}",
              bot->GetName(), bot->GetVictim() ? bot->GetVictim()->GetName() : "none",
              wanted->GetName(), entry, wanted->GetHealthPct(),
              uint32(wanted->GetHealthPct() / MURU_FOCUS_HEALTH_BUCKET), botAI->IsMelee(bot) ? 1 : 0);

    return Attack(wanted);
}
