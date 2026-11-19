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
#include "ThreatManager.h"

using namespace SunwellPlateauHelpers;

namespace
{
    // Quantised legs, shared by every twins movement. MoveTo refuses a repeat of the SAME destination
    // for ~5s (IsDuplicateMove), so an action that re-requests one unchanging point every tick gets at
    // most one accepted order per 5s - measured on Felmyst as 1-4y of progress per 5s. Quantising back
    // from the target keeps the waypoint stable across ticks while still advancing as the bot does.
    void MoveInLegs(Player* bot, Position const& target, float& legX, float& legY, float& legZ)
    {
        legX = target.GetPositionX();
        legY = target.GetPositionY();
        legZ = target.GetPositionZ();

        float const dx = legX - bot->GetPositionX();
        float const dy = legY - bot->GetPositionY();
        float const len = std::sqrt(dx * dx + dy * dy);

        if (len > TWINS_TRAVEL_LEG)
        {
            float const ux = -dx / len;
            float const uy = -dy / len;
            uint32 const k = uint32(std::ceil(len / TWINS_TRAVEL_LEG)) - 1u;
            legX = target.GetPositionX() + ux * (float(k) * TWINS_TRAVEL_LEG);
            legY = target.GetPositionY() + uy * (float(k) * TWINS_TRAVEL_LEG);
            legZ = bot->GetPositionZ();
            bot->UpdateAllowedPositionZ(legX, legY, legZ);
        }
    }
}  // namespace

bool TwinsFleeConflagrationAction::Execute(Event /*event*/)
{
    // Diagnostics before every early return, including the CanMove bail. On Felmyst the diagnostics
    // originally sat past that bail, so the bots that mattered - the ones that had lost control and
    // could not act - were the only ones that logged nothing at all.
    bool const confused = bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_CONFLAGRATION));

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[Twins] {} conflagration CANNOT MOVE (landed {})",
                  bot->GetName(), confused ? 1 : 0);
        return false;
    }

    // Ladder shortens the run rather than freezing when the ground out there is off-mesh - the Felmyst
    // rev-3 failure, where an authored destination turned out to be unwalkable and MoveTo silently
    // refused every order while the bot stood in the kill zone.
    uint32 accepted = TWINS_FLEE_CANDIDATES;
    float usedX = bot->GetPositionX();
    float usedY = bot->GetPositionY();

    for (uint32 attempt = 0; attempt < TWINS_FLEE_CANDIDATES; ++attempt)
    {
        Position spot;
        if (!GetTwinsConflagrationFleeSpot(bot, spot, attempt))
            continue;

        float legX, legY, legZ;
        MoveInLegs(bot, spot, legX, legY, legZ);

        if (MoveTo(bot->GetMapId(), legX, legY, legZ, false, false, false, false,
                   MovementPriority::MOVEMENT_FORCED))
        {
            accepted = attempt;
            usedX = legX;
            usedY = legY;
            break;
        }
    }

    LOG_DEBUG("playerbots", "[Twins] {} fleeing conflagration -> ({:.1f},{:.1f}) try {} moving {}",
              bot->GetName(), usedX, usedY, accepted, bot->isMoving() ? 1 : 0);

    return false;
}

bool TwinsClearConflagrationAction::Execute(Event /*event*/)
{
    Player* bomb = FindTwinsConflagrationBomb(bot);
    if (!bomb || bomb == bot)
        return false;

    float const distance = bot->GetExactDist2d(bomb->GetPositionX(), bomb->GetPositionY());

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[Twins] {} clear conflagration CANNOT MOVE, {:.1f}y from {}",
                  bot->GetName(), distance, bomb->GetName());
        return false;
    }

    uint32 accepted = TWINS_FLEE_CANDIDATES;
    float usedX = bot->GetPositionX();
    float usedY = bot->GetPositionY();

    for (uint32 attempt = 0; attempt < TWINS_FLEE_CANDIDATES; ++attempt)
    {
        Position spot;
        if (!GetTwinsConflagrationClearSpot(bot, bomb, spot, attempt))
            continue;

        if (MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                   false, false, false, false, MovementPriority::MOVEMENT_FORCED))
        {
            accepted = attempt;
            usedX = spot.GetPositionX();
            usedY = spot.GetPositionY();
            break;
        }
    }

    LOG_DEBUG("playerbots",
              "[Twins] {} clearing conflagration on {} dist {:.1f} -> ({:.1f},{:.1f}) try {} moving {}",
              bot->GetName(), bomb->GetName(), distance, usedX, usedY, accepted,
              bot->isMoving() ? 1 : 0);

    return false;
}

bool TwinsFocusSacrolashAction::Execute(Event /*event*/)
{
    Creature* sacrolash = FindSacrolash(bot);
    if (!sacrolash || !sacrolash->IsAlive())
        return false;

    LOG_DEBUG("playerbots", "[Twins] {} switching to sacrolash (empower would refund alythess)",
              bot->GetName());

    // Returning Attack()'s result is what makes this work: a bot already on her falls straight through
    // to its normal rotation. Claiming the tick every tick would select the target and never damage it.
    return Attack(sacrolash);
}

bool TwinsTankAlythessAction::Execute(Event /*event*/)
{
    Creature* alythess = FindAlythess(bot);
    if (!alythess || !alythess->IsAlive())
        return false;

    LOG_DEBUG("playerbots", "[Twins] {} off tank picking up alythess", bot->GetName());

    return Attack(alythess);
}

bool TwinsBlazeFootworkAction::Execute(Event /*event*/)
{
    Position station;
    bool seeking = false;
    if (!GetTwinsBlazeStation(bot, station, seeking))
        return false;

    uint32 const darkStacks =
        GetTwinsTouchStacks(bot, static_cast<uint32>(SunwellSpells::SPELL_DARK_TOUCHED));
    float const distance = bot->GetExactDist2d(station.GetPositionX(), station.GetPositionY());

    if (!botAI->CanMove())
        return false;

    bool const accepted =
        MoveTo(bot->GetMapId(), station.GetPositionX(), station.GetPositionY(),
               station.GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_FORCED);

    // BOTH stack counts, not just dark. Logging only `dark` here is what hid the first kill's real failure:
    // the bot dying was drowning in FLAME, and this line - the one it emitted hundreds of times - could not
    // show it. When a diagnostic covers one half of a symmetric mechanic, it will be the other half that
    // breaks.
    LOG_DEBUG("playerbots", "[Twins] {} blaze footwork seek {} dark {} flame {} dist {:.1f} ok {}",
              bot->GetName(), seeking ? 1 : 0, darkStacks,
              GetTwinsTouchStacks(bot, static_cast<uint32>(SunwellSpells::SPELL_FLAME_TOUCHED)),
              distance, accepted ? 1 : 0);

    return false;
}

bool TwinsStackOnAlythessAction::Execute(Event /*event*/)
{
    Creature* alythess = FindAlythess(bot);
    if (!alythess)
        return false;

    if (!botAI->CanMove())
        return false;

    // Straight at her. Everyone converging on one point is the POINT - Shadow Nova lands on a random
    // raider with a 10y radius, so the clump has to be tight enough for one hit to cover the rest, and her
    // own position is the only anchor every bot agrees on without shared state. Bots cannot occupy her
    // exact spot, so they ring up around her a few yards out, which is the clump we want.
    Position spot;
    spot.Relocate(alythess->GetPositionX(), alythess->GetPositionY(), alythess->GetPositionZ());

    float legX, legY, legZ;
    MoveInLegs(bot, spot, legX, legY, legZ);

    bool const accepted = MoveTo(bot->GetMapId(), legX, legY, legZ, false, false, false, false,
                                 MovementPriority::MOVEMENT_FORCED);

    LOG_DEBUG("playerbots", "[Twins] {} stacking on alythess, {:.1f}y out, flame {} ok {}",
              bot->GetName(),
              bot->GetExactDist2d(alythess->GetPositionX(), alythess->GetPositionY()),
              GetTwinsTouchStacks(bot, static_cast<uint32>(SunwellSpells::SPELL_FLAME_TOUCHED)),
              accepted ? 1 : 0);

    return false;
}

bool TwinsSeekShadowBladesAction::Execute(Event /*event*/)
{
    // Log the bail reasons. The first version returned silently from both of these, so a run in which the
    // action executed 24 times produced ZERO log lines and there was no way to tell "no spot" from "could
    // not move" - the single most useful thing the log could have said.
    Position spot;
    if (!GetTwinsShadowSeekSpot(bot, spot))
    {
        LOG_DEBUG("playerbots", "[Twins] {} shadow seek: NO SPOT (sacrolash gone?)", bot->GetName());
        return false;
    }

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[Twins] {} shadow seek: CANNOT MOVE, flame {}", bot->GetName(),
                  GetTwinsTouchStacks(bot, static_cast<uint32>(SunwellSpells::SPELL_FLAME_TOUCHED)));
        return false;
    }

    float legX, legY, legZ;
    MoveInLegs(bot, spot, legX, legY, legZ);

    bool const accepted = MoveTo(bot->GetMapId(), legX, legY, legZ, false, false, false, false,
                                 MovementPriority::MOVEMENT_FORCED);

    LOG_DEBUG("playerbots", "[Twins] {} seeking shadow blades, flame {} -> ({:.1f},{:.1f}) ok {}",
              bot->GetName(),
              GetTwinsTouchStacks(bot, static_cast<uint32>(SunwellSpells::SPELL_FLAME_TOUCHED)),
              legX, legY, accepted ? 1 : 0);

    return false;
}

bool TwinsReliefTauntSacrolashAction::Execute(Event /*event*/)
{
    Creature* sacrolash = FindSacrolash(bot);
    if (!sacrolash || !sacrolash->IsAlive())
        return false;

    // CUT BEFORE TAUNTING, and cut at all - this is the Brutallus handoff, and the reasoning transfers
    // exactly. Spell::EffectTaunt calls ThreatManager::MatchUnitThreatToHighestThreat, which only sets
    // the taunter EQUAL to the current top, and ThreatManager refuses to change victim until somebody
    // breaks 110% of it. So the 3s taunt aura holds her, expires, threat is merely tied, and she snaps
    // straight back to the confused tank - which is the whole thing we are trying to prevent. Measured on
    // Brutallus: 67 taunts, zero swaps.
    //
    // Cutting AFTER the taunt would be pointless for the same reason: the match would simply raise us to
    // whatever the holder still has.
    Unit* const previous = sacrolash->GetVictim();
    bool acted = false;

    if (previous && previous != bot)
    {
        // Only ever demote another TANK. The trigger already guarantees this, but the guard stays here
        // too - stripping a caster who grabbed her by accident would just hand her to the next caster.
        Player* previousTank = previous->ToPlayer();
        if (previousTank && botAI->IsTank(previousTank))
        {
            ThreatManager& threatMgr = sacrolash->GetThreatMgr();
            float const mine = threatMgr.GetThreat(bot);
            float const theirs = threatMgr.GetThreat(previousTank);
            float const target = mine / 1.1f * 0.9f;

            // Require a real threat base of our own before cutting, so a relief tank who has not been
            // swinging cannot strip the holder down to nothing and hand her to a clothie.
            if (mine > 0.0f && theirs > target && mine >= theirs * 0.5f)
            {
                threatMgr.ScaleThreat(previousTank, std::max(target / theirs, 0.05f));
                acted = true;
            }
        }
    }

    if (previous != bot && CastClassTaunt(bot, botAI, sacrolash))
        acted = true;

    // Unit::Attack returns false when already melee-attacking, which is a tank's normal state - so it
    // must not be this action's whole return value, or a good taunt logs as FAILED and the engine falls
    // straight past it.
    if (Attack(sacrolash))
        acted = true;

    LOG_DEBUG("playerbots", "[Twins] {} relief taunt on sacrolash (co-tank confounded) acted {}",
              bot->GetName(), acted ? 1 : 0);

    return acted;
}

bool TwinsPullSacrolashBackAction::Execute(Event /*event*/)
{
    Creature* sacrolash = FindSacrolash(bot);
    if (!sacrolash)
        return false;

    Position spot;
    if (!GetTwinsLeashSpot(bot, spot))
        return false;

    if (!botAI->CanMove())
        return false;

    Position const& home = sacrolash->GetHomePosition();
    float const drift = sacrolash->GetExactDist2d(home.GetPositionX(), home.GetPositionY());

    // She chases her victim, so walking her holder home walks HER home. Legs, because the destination is
    // a fixed world point and MoveTo refuses an identical repeat for ~5s.
    float legX, legY, legZ;
    MoveInLegs(bot, spot, legX, legY, legZ);

    bool const accepted = MoveTo(bot->GetMapId(), legX, legY, legZ, false, false, false, false,
                                 MovementPriority::MOVEMENT_FORCED);

    LOG_DEBUG("playerbots", "[Twins] {} walking sacrolash home, drift {:.1f}y -> ({:.1f},{:.1f}) ok {}",
              bot->GetName(), drift, legX, legY, accepted ? 1 : 0);

    return false;
}

bool TwinsDispelPyrogenicsAction::Execute(Event /*event*/)
{
    char const* spell = TwinsOffensiveDispelSpell(bot);
    if (!spell)
        return false;

    Creature* alythess = FindAlythess(bot);
    if (!alythess)
        return false;

    bool const cast = botAI->CastSpell(spell, alythess);

    LOG_DEBUG("playerbots", "[Twins] {} dispelling pyrogenics with '{}' cast {}",
              bot->GetName(), spell, cast ? 1 : 0);

    return cast;
}
