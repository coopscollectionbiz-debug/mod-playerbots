/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPActions.h"

#include <cmath>

#include "Playerbots.h"
#include "SWPHelpers.h"

using namespace SunwellPlateauHelpers;

bool FelmystLeaveFogLaneAction::Execute(Event /*event*/)
{
    Creature* boss = FindFelmyst(bot);
    if (!boss)
        return false;

    Position station;
    if (!GetFelmystFogStation(bot, botAI, boss, station))
        return false;

    float const distance = bot->GetExactDist2d(station.GetPositionX(), station.GetPositionY());

    // DIAGNOSTICS BEFORE THE EARLY RETURNS. They used to sit at the end, past the CanMove() bail, so a
    // charmed bot (IsCharmed => CanMove false) logged nothing at all and every surviving line read
    // "charm 0" - the exact bots that mattered were the invisible ones.
    int32 const lane = GetFelmystActiveLane(boss);
    bool const charmed = bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_FOG_OF_CORRUPTION_CHARM));

    if (distance <= FELMYST_STATION_TOLERANCE)
        return false;

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[Felmyst] {} lane {} CANNOT MOVE (charm {}) dist {:.1f}",
                  bot->GetName(), lane, charmed ? 1 : 0, distance);
        return false;
    }

    // WALK THE CANDIDATE LADDER until MoveTo accepts one. MoveTo returns false outright when
    // SearchForBestPath yields INVALID_HEIGHT, i.e. the destination is off-mesh - and rev 3 authored
    // exactly such an x (1501, taken from her AIR patrol's bounds, which prove nothing about floor).
    // Every order was refused, the bot never moved, and it froze standing on the live fog line. A
    // refuge that turns out to be off-mesh must degrade into "as far out as the floor allows".
    uint32 accepted = FELMYST_REFUGE_CANDIDATES;  // sentinel: nothing worked
    float usedX = station.GetPositionX();
    float usedY = station.GetPositionY();

    for (uint32 attempt = 0; attempt < FELMYST_REFUGE_CANDIDATES; ++attempt)
    {
        Position candidate;
        if (!GetFelmystFogStation(bot, botAI, boss, candidate, attempt))
            continue;

        // Legs, still: the IsDuplicateMove throttle (~5s per identical destination) applies to a 35y
        // hop just as much as it did to rev 1's 130y trek, and a stable quantised waypoint is what
        // stops it re-pathing every tick.
        float const dx = candidate.GetPositionX() - bot->GetPositionX();
        float const dy = candidate.GetPositionY() - bot->GetPositionY();
        float const len = std::sqrt(dx * dx + dy * dy);

        float legX = candidate.GetPositionX();
        float legY = candidate.GetPositionY();
        float legZ = candidate.GetPositionZ();

        if (len > FELMYST_TRAVEL_LEG)
        {
            float const ux = -dx / len;
            float const uy = -dy / len;
            uint32 const k = uint32(std::ceil(len / FELMYST_TRAVEL_LEG)) - 1u;
            legX = candidate.GetPositionX() + ux * (float(k) * FELMYST_TRAVEL_LEG);
            legY = candidate.GetPositionY() + uy * (float(k) * FELMYST_TRAVEL_LEG);
            legZ = bot->GetPositionZ();
            bot->UpdateAllowedPositionZ(legX, legY, legZ);
        }

        // MOVEMENT_FORCED then fall through, exactly as on Brutallus: the priority is what stops
        // generic movement re-issuing a lower-priority order over the top of this hop, and returning
        // false is what keeps heals and add-killing alive while the bot repositions.
        if (MoveTo(bot->GetMapId(), legX, legY, legZ, false, false, false, false,
                   MovementPriority::MOVEMENT_FORCED))
        {
            accepted = attempt;
            usedX = legX;
            usedY = legY;
            break;
        }
    }

    // `try` is the ladder index MoveTo accepted (5 = every candidate refused, which means this bot has
    // no reachable refuge and the authored x values need revisiting). Because MoveTo refuses off-mesh
    // destinations, this field doubles as the walkability probe this room never had: whatever index
    // shows up in a clean run is the x that is actually walkable.
    LOG_DEBUG("playerbots",
              "[Felmyst] {} lane {} -> ({:.1f},{:.1f}) dist {:.1f} try {} moving {} combat {} charm {}",
              bot->GetName(), lane, usedX, usedY, distance, accepted, bot->isMoving() ? 1 : 0,
              bot->IsInCombat() ? 1 : 0, charmed ? 1 : 0);

    return false;
}

bool FelmystFleeEncapsulateAction::Execute(Event /*event*/)
{
    Creature* boss = FindFelmyst(bot);
    if (!boss)
        return false;

    Player* victim = FindFelmystEncapsulateVictim(bot, boss);
    if (!victim || victim == bot)
        return false;

    float const distance = bot->GetExactDist2d(victim->GetPositionX(), victim->GetPositionY());

    if (!botAI->CanMove())
    {
        LOG_DEBUG("playerbots", "[Felmyst] {} encapsulate CANNOT MOVE, {:.1f}y from {}",
                  bot->GetName(), distance, victim->GetName());
        return false;
    }

    // Same off-mesh ladder as the fog refuge: shorten the flee rather than freeze, and never inside
    // the 20.4y blast.
    uint32 accepted = FELMYST_REFUGE_CANDIDATES;
    float usedX = bot->GetPositionX();
    float usedY = bot->GetPositionY();

    for (uint32 attempt = 0; attempt < FELMYST_REFUGE_CANDIDATES; ++attempt)
    {
        Position spot;
        if (!GetFelmystEncapsulateFleeSpot(bot, victim, spot, attempt))
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
              "[Felmyst] {} flee encapsulate from {} dist {:.1f} -> ({:.1f},{:.1f}) try {} moving {}",
              bot->GetName(), victim->GetName(), distance, usedX, usedY, accepted,
              bot->isMoving() ? 1 : 0);

    return false;
}

bool FelmystRegroupAction::Execute(Event /*event*/)
{
    Creature* boss = FindFelmyst(bot);
    if (!boss)
        return false;

    Position centroid;
    if (!GetFelmystRaidCentroid(bot, centroid))
        return false;

    if (!botAI->CanMove())
        return false;

    float const dx = centroid.GetPositionX() - bot->GetPositionX();
    float const dy = centroid.GetPositionY() - bot->GetPositionY();
    float const len = std::sqrt(dx * dx + dy * dy);

    // Legs again, for the same reason as the fog hop: MoveTo refuses a repeat of an identical
    // destination for ~5s, and the centroid barely moves, so a single fixed target would be throttled
    // to one accepted order per 5s. Quantising back from the target keeps the waypoint stable across
    // ticks while still changing when the bot actually advances.
    float legX = centroid.GetPositionX();
    float legY = centroid.GetPositionY();
    float legZ = centroid.GetPositionZ();

    if (len > FELMYST_TRAVEL_LEG)
    {
        float const ux = -dx / len;
        float const uy = -dy / len;
        uint32 const k = uint32(std::ceil(len / FELMYST_TRAVEL_LEG)) - 1u;
        legX = centroid.GetPositionX() + ux * (float(k) * FELMYST_TRAVEL_LEG);
        legY = centroid.GetPositionY() + uy * (float(k) * FELMYST_TRAVEL_LEG);
        legZ = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(legX, legY, legZ);
    }

    bool const accepted = MoveTo(bot->GetMapId(), legX, legY, legZ, false, false, false, false,
                                 MovementPriority::MOVEMENT_FORCED);

    LOG_DEBUG("playerbots", "[Felmyst] {} regrouping, {:.1f}y to raid -> ({:.1f},{:.1f}) ok {}",
              bot->GetName(), len, legX, legY, accepted ? 1 : 0);

    return false;
}

bool FelmystAttackCharmedRaiderAction::Execute(Event /*event*/)
{
    Player* charmed = FindFelmystCharmedRaider(bot);
    if (!charmed)
        return false;

    LOG_DEBUG("playerbots", "[Felmyst] {} attacking charmed raider {} at {:.1f}y",
              bot->GetName(), charmed->GetName(),
              bot->GetExactDist2d(charmed->GetPositionX(), charmed->GetPositionY()));

    // Only switches the target. Returning Attack()'s result means a bot already on this target falls
    // through to its normal rotation, which is what actually kills them - claiming the tick here every
    // tick would leave the charmed bot targeted but never damaged.
    return Attack(charmed);
}

bool FelmystMassDispelGasNovaAction::Execute(Event /*event*/)
{
    // Centred on the caster: Mass Dispel takes a ground target (Targets flag TARGET_FLAG_DEST_LOCATION,
    // which PlayerbotAI::CastSpell's location overload handles), and since Gas Nova hits the entire
    // room the bot's own position is always in the middle of a cluster of afflicted allies.
    uint32 const afflicted = CountFelmystGasNovaAfflicted(bot);

    bool const cast = botAI->CastSpell(static_cast<uint32>(SunwellSpells::SPELL_MASS_DISPEL),
                                       bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    LOG_DEBUG("playerbots", "[Felmyst] {} mass dispel gas nova ({} afflicted within {:.0f}y) cast {}",
              bot->GetName(), afflicted, FELMYST_MASS_DISPEL_RADIUS, cast ? 1 : 0);

    return cast;
}

bool FelmystKiteVaporAction::Execute(Event /*event*/)
{
    Creature* vapor = FindFelmystVaporChasingMe(bot);
    if (!vapor)
        return false;

    if (!botAI->CanMove())
        return false;

    // Run RADIALLY OUTWARD from the raid's centre of mass, out to FELMYST_VAPOR_KITE_RANGE. Not to a
    // fixed corner: that stranded the carriers ~145y away, and since the movement multiplier removes
    // ReachTargetAction for the whole flight phase, nothing walked them back once the vapor despawned -
    // they sat at the wall and died. Radial keeps the trip short enough to reverse and separates the two
    // carriers for free, since they leave on different bearings.
    Position centroid;
    if (!GetFelmystRaidCentroid(bot, centroid))
        return false;

    float dx = bot->GetPositionX() - centroid.GetPositionX();
    float dy = bot->GetPositionY() - centroid.GetPositionY();
    float len = std::sqrt(dx * dx + dy * dy);

    if (len < 0.5f)
    {
        // Standing on the raid's centre: pick a deterministic bearing from the GUID rather than a
        // degenerate one, so two carriers in the same spot still leave in different directions.
        float const bearing = float(bot->GetGUID().GetCounter() % 16u) * 2.0f * float(M_PI) / 16.0f;
        dx = std::cos(bearing);
        dy = std::sin(bearing);
        len = 1.0f;
    }

    float x = centroid.GetPositionX() + dx / len * FELMYST_VAPOR_KITE_RANGE;
    float y = centroid.GetPositionY() + dy / len * FELMYST_VAPOR_KITE_RANGE;
    x = std::max(FELMYST_ROOM_X_MIN, std::min(FELMYST_ROOM_X_MAX, x));

    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);

    LOG_DEBUG("playerbots", "[Felmyst] {} kiting vapor outward, {:.1f}y to ({:.1f},{:.1f})",
              bot->GetName(), bot->GetExactDist2d(x, y), x, y);

    MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_FORCED);

    return false;
}
