/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the BWL Death Talon Wyrmguard packs (overlay patch 0020). Deliberately free of
// core and playerbots includes so tests/test_bwl_wyrmguard_rules.cpp builds it standalone.
// Spec: docs/superpowers/specs/2026-10-05-playerbot-bwl-wyrmguard-design.md (overlay repo).

#ifndef PLAYERBOTS_BWLWYRMGUARDRULES_H
#define PLAYERBOTS_BWLWYRMGUARDRULES_H

#include <cmath>
#include <cstdint>
#include <limits>

namespace BwlWyrmguardRules
{
    constexpr int SLOTS = 3;       // Wyrmguards per pack
    constexpr int ROLES = 4;       // 0 main tank, 1 assist tank 0, 2 assist tank 1, 3 assist tank 2 (spare)
    constexpr int SPARE_ROLE = 3;
    constexpr int NONE = -1;

    constexpr float STOMP_RADIUS = 15.0f;         // War Stomp 24375, SpellRadius
    constexpr float KEEP_AWAY = 16.0f;            // stand this far from a Stomp source
    constexpr float SEPARATION = 25.0f;           // Stomp 15 + ~8 melee standing distance + 2 margin
    // Guard this close to its station counts as there. The tank stops up to STATION_ARRIVE short and
    // the guard stands at its melee reach in front of the tank, so the guard can sit well past the
    // station point (dev pull 2026-10-06: >8 y froze the skull). SEPARATION is the safety gate.
    constexpr float SETTLED_RADIUS = 15.0f;
    constexpr float STATION_ARRIVE = 3.0f;        // tank arrival radius at its station
    // A guard is skulled only after this long continuously settled: a taunt only MATCHES top threat,
    // so at the moment of settling the tank's lead is tiny and one DPS crit pulls (spec §4.8a).
    constexpr uint32_t SKULL_LEAD_MS = 8000;
    constexpr uint32_t MELEE_TIMEOUT_MS = 15000;  // after this long the separation relaxes ...
    constexpr float RELAXED_SEPARATION = KEEP_AWAY + 4.0f;  // ... to this: below it no melee spot clears Stomp
    constexpr float REAR_SPOT_DISTANCE = 4.0f;    // where "rear flank" puts a melee bot behind skull
    constexpr float EBONROC_KEEPOUT_ENTER = 32.0f;  // ~23 y aggro (detection 20, level 63 vs 60) + margin
    constexpr float EBONROC_KEEPOUT_EXIT = 36.0f;   // step out to this far from his path
    constexpr float HEALER_STOMP_CLEAR = 18.0f;   // a healer's spot is this far from every living guard
    constexpr float HEALER_RANGE = 36.0f;         // ... and this close to every holder tank (heal range 40)

    struct Pt
    {
        float x;
        float y;
    };

    inline float Dist2d(Pt a, Pt b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    struct Assignment
    {
        int holderRole[SLOTS];  // tank role holding each slot's guard; NONE for a dead guard
        int station[SLOTS];     // station the guard is held at; station 0 != slot means folded
    };

    struct PackState
    {
        Pt guard[SLOTS];        // live guard positions (unused when !alive)
        bool alive[SLOTS];
        bool onHolder[SLOTS];   // the guard's victim is its holder
        Pt station[SLOTS];      // authored station positions, indexed by station
    };

    // tankAlive[r]: a living tank fills role r. guardAlive[s]: slot s's guard is alive.
    // Own tank first, then the spare (at most one slot), else fold onto the main tank's station.
    inline Assignment Assign(bool const tankAlive[ROLES], bool const guardAlive[SLOTS])
    {
        Assignment a{};
        bool spareUsed = false;
        for (int s = 0; s < SLOTS; ++s)
        {
            a.station[s] = s;
            if (!guardAlive[s])
            {
                a.holderRole[s] = NONE;
                continue;
            }
            if (tankAlive[s])
            {
                a.holderRole[s] = s;
                continue;
            }
            if (tankAlive[SPARE_ROLE] && !spareUsed)
            {
                a.holderRole[s] = SPARE_ROLE;
                spareUsed = true;
                continue;
            }
            a.holderRole[s] = 0;
            a.station[s] = 0;
        }
        return a;
    }

    inline bool IsFolded(Assignment const& a, int s)
    {
        return a.holderRole[s] != NONE && a.station[s] != s;
    }

    // Ownership follows aggro (spec 4.2). victimRole[s]: the role (0..ROLES-1) of the present,
    // living role tank slot s's guard is attacking, else NONE. In slot order, a living guard whose
    // victim is a role tank other than its holder is adopted by that tank - unless the tank already
    // holds another living slot in `a` as it stands (earlier adoptions included). The station is
    // unchanged. At the pull the main tank holds slot 0, so the other two it pulled stay with
    // their assigned off-tanks.
    inline void AdoptByVictim(Assignment& a, PackState const& p, int const victimRole[SLOTS])
    {
        for (int s = 0; s < SLOTS; ++s)
        {
            int const r = victimRole[s];
            if (!p.alive[s] || r == NONE || r == a.holderRole[s])
                continue;
            bool holdsOther = false;
            for (int t = 0; t < SLOTS; ++t)
                if (t != s && p.alive[t] && a.holderRole[t] == r)
                    holdsOther = true;
            if (!holdsOther)
                a.holderRole[s] = r;
        }
    }

    // True when no guard other than slot s is alive (s itself may be alive or not).
    inline bool OthersDead(PackState const& p, int s)
    {
        for (int t = 0; t < SLOTS; ++t)
            if (t != s && p.alive[t])
                return false;
        return true;
    }

    // On its holder, on its station, and SEPARATION clear of every living guard held elsewhere.
    // The last living guard of the pack is settled wherever it is and whoever holds it (spec 4.5).
    inline bool IsSettled(PackState const& p, Assignment const& a, int s)
    {
        if (OthersDead(p, s))
            return p.alive[s];
        if (!p.alive[s] || !p.onHolder[s])
            return false;
        if (Dist2d(p.guard[s], p.station[a.station[s]]) > SETTLED_RADIUS)
            return false;
        for (int t = 0; t < SLOTS; ++t)
        {
            if (t == s || !p.alive[t] || a.station[t] == a.station[s])
                continue;
            if (Dist2d(p.guard[s], p.guard[t]) < SEPARATION)
                return false;
        }
        return true;
    }

    // Settled AND settled for SKULL_LEAD_MS (settledMs = how long slot s has been continuously
    // settled). The last living guard is exempt from the lead, as it is from IsSettled's checks.
    inline bool SkullReady(PackState const& p, Assignment const& a, int s, uint32_t const settledMs[SLOTS])
    {
        if (!IsSettled(p, a, s))
            return false;
        return OthersDead(p, s) || settledMs[s] >= SKULL_LEAD_MS;
    }

    // currentSkull: the slot carrying skull now, or NONE. A skull on a living guard is never moved
    // (it may be the player's own mark). Otherwise: folded extras first, then slots 0, 1, 2 -
    // the first one settled for SKULL_LEAD_MS (spec §4.8a). NONE means mark nothing yet.
    inline int NextSkull(PackState const& p, Assignment const& a, int currentSkull, uint32_t const settledMs[SLOTS])
    {
        if (currentSkull != NONE && p.alive[currentSkull])
            return currentSkull;
        for (int s = 0; s < SLOTS; ++s)
            if (IsFolded(a, s) && SkullReady(p, a, s, settledMs))
                return s;
        for (int s = 0; s < SLOTS; ++s)
            if (SkullReady(p, a, s, settledMs))
                return s;
        return NONE;
    }

    // Melee wait until skull is SEPARATION clear of every living guard held elsewhere. From
    // MELEE_TIMEOUT_MS after the pack became active that relaxes to RELAXED_SEPARATION - never
    // below it, where releasing would only ping-pong melee between the away rule and reach-melee.
    inline bool MeleeHeld(PackState const& p, Assignment const& a, int skull, uint32_t msSinceActive)
    {
        if (skull == NONE || !p.alive[skull])
            return true;
        float const separation = msSinceActive >= MELEE_TIMEOUT_MS ? RELAXED_SEPARATION : SEPARATION;
        for (int t = 0; t < SLOTS; ++t)
        {
            if (t == skull || !p.alive[t] || a.station[t] == a.station[skull])
                continue;
            if (Dist2d(p.guard[skull], p.guard[t]) < separation)
                return true;
        }
        return false;
    }

    // The nearest living guard within KEEP_AWAY of `bot` that the bot must step away from.
    // Guards held at `exemptStation` are ignored: a released melee bot passes skull's station, a
    // held one passes NONE and avoids every guard.
    inline int StompThreat(PackState const& p, Assignment const& a, Pt bot, int exemptStation)
    {
        int best = NONE;
        float bestDist = KEEP_AWAY;
        for (int s = 0; s < SLOTS; ++s)
        {
            if (!p.alive[s])
                continue;
            if (exemptStation != NONE && a.station[s] == exemptStation)
                continue;
            float const d = Dist2d(bot, p.guard[s]);
            if (d < bestDist)
            {
                bestDist = d;
                best = s;
            }
        }
        return best;
    }

    // The point REAR_SPOT_DISTANCE directly behind a unit at `pos` facing `orientation`.
    inline Pt RearPoint(Pt pos, float orientation)
    {
        return Pt{pos.x - REAR_SPOT_DISTANCE * std::cos(orientation),
                  pos.y - REAR_SPOT_DISTANCE * std::sin(orientation)};
    }

    // True when `rear` (the spot behind skull) is KEEP_AWAY clear of every living guard not held
    // on skull's station - otherwise "rear flank" would trade the bot back and forth with the
    // away rule.
    inline bool RearSpotClear(PackState const& p, Assignment const& a, int skull, Pt rear)
    {
        if (skull == NONE || !p.alive[skull])
            return false;
        return StompThreat(p, a, rear, a.station[skull]) == NONE;
    }

    // 2D distance from `pt` to the segment a-b.
    inline float DistToSegment(Pt pt, Pt a, Pt b)
    {
        float const dx = b.x - a.x;
        float const dy = b.y - a.y;
        float const len2 = dx * dx + dy * dy;
        float t = len2 > 0.0f ? ((pt.x - a.x) * dx + (pt.y - a.y) * dy) / len2 : 0.0f;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return Dist2d(pt, Pt{a.x + t * dx, a.y + t * dy});
    }

    // 2D distance from `pt` to the polyline path[0..n) (segment distance, not vertex distance).
    inline float DistToPath(Pt pt, Pt const* path, int n)
    {
        if (n <= 0)
            return std::numeric_limits<float>::max();
        float best = Dist2d(pt, path[0]);
        for (int i = 0; i + 1 < n; ++i)
        {
            float const d = DistToSegment(pt, path[i], path[i + 1]);
            if (d < best)
                best = d;
        }
        return best;
    }

    // ---- Healer positioning (spec §4.8a "Healer positioning") ----

    // How many of tanks[0..nTanks) are within HEALER_RANGE (2D) of `spot`. Range only: the runtime
    // adds line of sight.
    inline int TanksReached(Pt spot, Pt const* tanks, int nTanks)
    {
        int n = 0;
        for (int i = 0; i < nTanks; ++i)
            if (Dist2d(spot, tanks[i]) <= HEALER_RANGE)
                ++n;
        return n;
    }

    // `spot` is at least HEALER_STOMP_CLEAR from every living guard.
    inline bool StompClear(Pt spot, PackState const& p)
    {
        for (int s = 0; s < SLOTS; ++s)
            if (p.alive[s] && Dist2d(spot, p.guard[s]) < HEALER_STOMP_CLEAR)
                return false;
        return true;
    }

    // The straight walk from -> to never passes within KEEP_AWAY of a living guard. A walk that
    // STARTS inside KEEP_AWAY of a guard (upstream's ranged keep-away has not pushed the bot out
    // yet) only has to never get closer to it than its start - otherwise no walk could ever leave.
    inline bool WalkClear(Pt from, Pt to, PackState const& p)
    {
        for (int s = 0; s < SLOTS; ++s)
        {
            if (!p.alive[s])
                continue;
            float const start = Dist2d(from, p.guard[s]);
            float const limit = start < KEEP_AWAY ? start - 0.001f : KEEP_AWAY;
            if (DistToSegment(p.guard[s], from, to) < limit)
                return false;
        }
        return true;
    }

    struct SpotScore
    {
        bool walkClear;   // the walk there is clear (always true for the current spot)
        bool stompClear;
        int tanks;        // holder tanks reached (the runtime replaces this with range AND LoS)
        float move;       // 2D walk length; 0 for the current spot
    };

    inline SpotScore ScoreSpot(Pt from, Pt spot, Pt const* tanks, int nTanks, PackState const& p)
    {
        return SpotScore{WalkClear(from, spot, p), StompClear(spot, p), TanksReached(spot, tanks, nTanks),
                         Dist2d(from, spot)};
    }

    // a strictly better than b: walk clear, then stomp clear, then more tanks, then a shorter move.
    // The current spot (move 0, walk clear) therefore only loses to a strict improvement.
    inline bool BetterSpot(SpotScore const& a, SpotScore const& b)
    {
        if (a.walkClear != b.walkClear)
            return a.walkClear;
        if (a.stompClear != b.stompClear)
            return a.stompClear;
        if (a.tanks != b.tanks)
            return a.tanks > b.tanks;
        return a.move < b.move;
    }

    // The best of cands[0..nCands) for a healer standing at `from`, or NONE when staying is at least
    // as good as every candidate (range-only tank counts; see the runtime for LoS).
    inline int BestSpot(Pt from, Pt const* cands, int nCands, Pt const* tanks, int nTanks, PackState const& p)
    {
        SpotScore best = ScoreSpot(from, from, tanks, nTanks, p);
        int bestIdx = NONE;
        for (int i = 0; i < nCands; ++i)
        {
            SpotScore const s = ScoreSpot(from, cands[i], tanks, nTanks, p);
            if (BetterSpot(s, best))
            {
                best = s;
                bestIdx = i;
            }
        }
        return bestIdx;
    }
}

#endif
