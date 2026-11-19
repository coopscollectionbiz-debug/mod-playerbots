/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPActions.h"

#include <algorithm>
#include <cmath>

#include "Playerbots.h"
#include "SWPHelpers.h"

using namespace SunwellPlateauHelpers;

namespace
{
    // Quantised legs, same contract as the Twins version. MoveTo refuses a repeat of the SAME
    // destination for ~5s (IsDuplicateMove), so an action re-requesting one unchanging point every tick
    // gets at most one accepted order per 5s. Quantising back from the target keeps the waypoint stable
    // across ticks while still advancing as the bot does.
    void MoveInLegs(Player* bot, Position const& target, float& legX, float& legY, float& legZ)
    {
        legX = target.GetPositionX();
        legY = target.GetPositionY();
        legZ = target.GetPositionZ();

        float const dx = legX - bot->GetPositionX();
        float const dy = legY - bot->GetPositionY();
        float const len = std::sqrt(dx * dx + dy * dy);

        if (len > MURU_TRAVEL_LEG)
        {
            float const ux = -dx / len;
            float const uy = -dy / len;
            uint32 const k = uint32(std::ceil(len / MURU_TRAVEL_LEG)) - 1u;
            legX = target.GetPositionX() + ux * (float(k) * MURU_TRAVEL_LEG);
            legY = target.GetPositionY() + uy * (float(k) * MURU_TRAVEL_LEG);
            legZ = bot->GetPositionZ();
            bot->UpdateAllowedPositionZ(legX, legY, legZ);
        }
    }

    // All four M'uru escapes have the same shape - "get away from this thing, and if the ground that way
    // is off-mesh, try another bearing". This resolves one rung of that ladder; the MoveTo itself stays
    // inside the action because MovementAction::MoveTo is protected.
    //
    // The ladder exists because MoveTo answers an unwalkable destination by silently refusing forever
    // rather than failing loudly - the Felmyst rev-3 failure, where a bot stood in the kill zone
    // re-requesting an authored point it could never reach.
    using SpotFn = bool (*)(Player*, Position&, uint32);

    bool LegForAttempt(Player* bot, SpotFn spotFn, uint32 attempt,
                       float& legX, float& legY, float& legZ)
    {
        Position spot;
        if (!spotFn(bot, spot, attempt))
            return false;

        MoveInLegs(bot, spot, legX, legY, legZ);
        return true;
    }
}  // namespace

// Every escape below runs the same five-rung ladder and logs which rung was accepted, because "try 5"
// in the trace is the difference between "the bot is refusing to move" and "every bearing out of here
// is off-mesh" - two failures that look identical from the outside and need opposite fixes.
#define MURU_RUN_LADDER(spotFn)                                                                       \
    uint32 accepted = MURU_FLEE_CANDIDATES;                                                           \
    float usedX = bot->GetPositionX();                                                                \
    float usedY = bot->GetPositionY();                                                                \
    for (uint32 attempt = 0; attempt < MURU_FLEE_CANDIDATES; ++attempt)                               \
    {                                                                                                 \
        float legX, legY, legZ;                                                                       \
        if (!LegForAttempt(bot, &(spotFn), attempt, legX, legY, legZ))                                 \
            continue;                                                                                 \
        if (MoveTo(bot->GetMapId(), legX, legY, legZ, false, false, false, false,                      \
                   MovementPriority::MOVEMENT_FORCED))                                                \
        {                                                                                             \
            accepted = attempt;                                                                       \
            usedX = legX;                                                                             \
            usedY = legY;                                                                             \
            break;                                                                                    \
        }                                                                                             \
    }

bool MuruClearDarknessAction::Execute(Event /*event*/)
{
    // Diagnostics BEFORE the CanMove bail, not after. On Felmyst the diagnostics sat past that bail, so
    // the bots that mattered - the ones that had lost control and could not act at all - were the only
    // ones that logged nothing.
    bool const standing = bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_MURU_DARKNESS));

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[Muru] {} darkness CANNOT MOVE (in zone {})",
                  bot->GetName(), standing ? 1 : 0);
        return false;
    }

    // Distance is logged because it is what separates the two cases the predicate covers, and rev 2's line
    // could not tell them apart: inside the circle means "still escaping", outside means "holding so
    // generic movement does not walk me back into a boss standing in the middle of it".
    Position center;
    float const distance = GetMuruDarknessCenter(bot, center)
                               ? bot->GetExactDist2d(center.GetPositionX(), center.GetPositionY())
                               : -1.0f;

    Position probe;
    if (!GetMuruDarknessClearSpot(bot, probe, 0))
    {
        LOG_DEBUG("playerbots", "[Muru] {} holding out of darkness at {:.1f}y (in zone {})",
                  bot->GetName(), distance, standing ? 1 : 0);
        return false;
    }

    MURU_RUN_LADDER(GetMuruDarknessClearSpot)

    LOG_DEBUG("playerbots",
              "[Muru] {} clearing darkness -> ({:.1f},{:.1f}) try {} at {:.1f}y in zone {} moving {}",
              bot->GetName(), usedX, usedY, accepted, distance, standing ? 1 : 0,
              bot->isMoving() ? 1 : 0);

    return false;
}

bool MuruDispelDarkFiendAction::Execute(Event /*event*/)
{
    char const* spell = SwpOffensiveDispelSpell(bot);
    if (!spell)
        return false;

    Creature* fiend = FindMuruDarkFiend(bot, botAI);
    if (!fiend)
    {
        LOG_DEBUG("playerbots", "[Muru] {} fiend dispel: NO ASSIGNED FIEND", bot->GetName());
        return false;
    }

    bool const cast = botAI->CastSpell(spell, fiend);

    // The distance is in the line because it is the number that decides whether this design works at
    // all: a fiend closes at 2.8 yd/s from a 13y spawn ring, so a failed cast at 25y still leaves
    // seconds of slack, while one at 5y means a raid-wide 25,000 is about to land.
    LOG_DEBUG("playerbots", "[Muru] {} dispelling fiend {} with '{}' at {:.1f}y cast {}",
              bot->GetName(), fiend->GetGUID().ToString(), spell,
              bot->GetExactDist2d(fiend->GetPositionX(), fiend->GetPositionY()), cast ? 1 : 0);

    return cast;
}

bool MuruClearVoidZoneAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[Muru] {} void zone CANNOT MOVE", bot->GetName());
        return false;
    }

    MURU_RUN_LADDER(GetMuruVoidZoneClearSpot)

    LOG_DEBUG("playerbots", "[Muru] {} clearing void zone -> ({:.1f},{:.1f}) try {} moving {}",
              bot->GetName(), usedX, usedY, accepted, bot->isMoving() ? 1 : 0);

    return false;
}

bool MuruFleeSingularityAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[Muru] {} singularity CANNOT MOVE", bot->GetName());
        return false;
    }

    MURU_RUN_LADDER(GetMuruSingularityFleeSpot)

    LOG_DEBUG("playerbots", "[Muru] {} fleeing singularity -> ({:.1f},{:.1f}) try {} moving {}",
              bot->GetName(), usedX, usedY, accepted, bot->isMoving() ? 1 : 0);

    return false;
}

bool MuruLeaveShadowPulseAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[Muru] {} shadow pulse CANNOT MOVE", bot->GetName());
        return false;
    }

    MURU_RUN_LADDER(GetMuruShadowPulseSpot)

    LOG_DEBUG("playerbots", "[Muru] {} leaving shadow pulse -> ({:.1f},{:.1f}) try {} moving {}",
              bot->GetName(), usedX, usedY, accepted, bot->isMoving() ? 1 : 0);

    return false;
}

#undef MURU_RUN_LADDER

bool MuruTankAddAction::Execute(Event /*event*/)
{
    Creature* add = FindMuruUntankedAdd(bot, botAI);
    if (!add)
        return false;

    Unit* victim = add->GetVictim();

    // Taunt only when something else already holds it. On a fresh spawn there is nothing to pull off and
    // Attack alone builds the threat, so spending the taunt cooldown there wastes it for the next loose
    // add - and with six elites plus a Sentinel per minute, there is always a next one.
    bool taunted = false;
    if (victim && victim != bot)
        taunted = CastClassTaunt(bot, botAI, add);

    LOG_DEBUG("playerbots", "[Muru] {} picking up add {} (entry {}) held by {} taunt {}",
              bot->GetName(), add->GetGUID().ToString(), add->GetEntry(),
              victim ? victim->GetName() : "nobody", taunted ? 1 : 0);

    // Returning Attack()'s result is what makes this fall through: a tank already on the add drops
    // straight into its normal rotation instead of re-selecting the same target every tick.
    return Attack(add);
}

bool MuruFocusTargetAction::Execute(Event /*event*/)
{
    Unit* wanted = GetMuruFocusTarget(bot, botAI);
    if (!wanted)
        return false;

    // Health percent and the resolved score are in the line because they are what the focus rule is
    // decided on. Logging only the entry is what made the first run's starvation invisible until the
    // retarget destinations were counted by hand: "everyone is on a Fury Mage" and "no Berserker has
    // ever been touched" are the same log until you can see the losing candidates' scores too.
    // hp is logged to ONE DECIMAL and the bucket alongside it, because rev 3's "{:.0f}%" is what hid this
    // bug: two Fury Mages a fraction of a percent apart both printed "67%", so hundreds of lines of target
    // flipping read as a bot calmly re-selecting the same add.
    uint32 const entry = wanted->ToCreature() ? wanted->ToCreature()->GetEntry() : 0;
    LOG_DEBUG("playerbots",
              "[Muru] {} retargeting {}->{} (entry {}) hp {:.1f}% bucket {} melee {}",
              bot->GetName(), bot->GetVictim() ? bot->GetVictim()->GetName() : "none",
              wanted->GetName(), entry, wanted->GetHealthPct(),
              uint32(wanted->GetHealthPct() / MURU_FOCUS_HEALTH_BUCKET), botAI->IsMelee(bot) ? 1 : 0);

    return Attack(wanted);
}
