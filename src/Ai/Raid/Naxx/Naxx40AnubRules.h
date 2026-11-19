/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Naxx40 Anub'Rekhan raid spread (Individual Progression's level-60 Naxxramas). Deliberately free
// of core and playerbots includes so tests/test_naxx40_anub_rules.cpp (overlay repo) builds it standalone:
//   g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/nx40anub tests/test_naxx40_anub_rules.cpp && /tmp/nx40anub
// Spec: docs/superpowers/specs/2026-10-30-playerbot-naxx40-anubrekhan-design.md (overlay repo).
//
// Geometry: every spot is a function of Anub'Rekhan's position, the attempt's latched fallback bearing and the bot's
// latched slot only, never of the bot's own position, so a spot never creeps while the bot walks to it.

#ifndef PLAYERBOTS_NAXX40ANUBRULES_H
#define PLAYERBOTS_NAXX40ANUBRULES_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Naxx40AnubRules
{
    constexpr uint32_t ANUB_ENTRY = 351009;
    constexpr uint32_t BOSS_STATE_ID = 6;  // BOSS_ANUB in naxxramas.h
    constexpr uint32_t SPELL_IMPALE = 28783;
    constexpr uint32_t SPELL_LOCUST_SWARM = 28785;

    // Locust Swarm pulses 28786 at this radius around Anub'Rekhan (creature caster: centre to centre, no reach added).
    constexpr float SWARM_RADIUS = 25.0f;
    // Impale: a 30° cone (spell_cone 28783), 45 y long, aimed at a random player.
    constexpr float IMPALE_HALF_ANGLE_DEG = 15.0f;
    constexpr float IMPALE_LENGTH = 45.0f;

    // Upstream's kite circle centre (AnubrekhanPositionAction), the room centre. Arc spots stay within ROOM_MAX_RADIUS
    // of it (spec §5), clear of the walls.
    constexpr float ROOM_X = 3272.49f;
    constexpr float ROOM_Y = -3476.27f;
    constexpr float ROOM_MAX_RADIUS = 42.0f;

    // The arc: ARC_HALF_SPAN_DEG either side of the arc axis (Anub'Rekhan -> room centre), two rows alternating by
    // slot.
    // The inner row keeps 5 y over the swarm (a kited boss closes up to ARC_TOLERANCE * sin 60° on the leading edge
    // before a bot re-moves); the outer row puts 4 y between rows, so neighbours stand ~3.5-4.5 y apart even with 38
    // bots on the arc. Anub'Rekhan's combat reach is 10 y, so both rows are well inside spell range (28.5 y surface)
    // and an outer-row healer still reaches the far melee flank (~40.6 y centre to centre, under 38.5 y surface).
    constexpr float ARC_HALF_SPAN_DEG = 60.0f;
    constexpr float ARC_INNER = 30.0f;
    constexpr float ARC_OUTER = 34.0f;

    // Arc axis: toward the room centre once Anub'Rekhan is AXIS_CENTRE_FULL off it, the attempt's latched fallback
    // bearing within AXIS_CENTRE_NONE of it (the centre bearing is undefined there), blended in between.
    constexpr float AXIS_CENTRE_NONE = 8.0f;
    constexpr float AXIS_CENTRE_FULL = 20.0f;

    // Melee flanks: MELEE_FLANK_DEG either side of the arc axis, MELEE_DIST from his centre (melee range is ~12.8 y
    // centre to centre against his 10 y reach). 100° keeps every flank 25° outside the widest arc Impale cone
    // (60° + 15°) and the reverse; 7 y keeps the flank ~46.7 y from the room centre while he is on the kite circle.
    constexpr float MELEE_FLANK_DEG = 100.0f;
    constexpr float MELEE_DIST = 7.0f;

    // Re-move tolerances: a bot walks to its spot only when it is farther than this from it (a per-tick re-move would
    // freeze casting, and MoveTo refuses near-duplicate orders).
    constexpr float ARC_TOLERANCE = 2.5f;
    constexpr float MELEE_TOLERANCE = 2.0f;

    constexpr uint32_t REFRESH_INTERVAL_MS = 500;
    constexpr uint32_t DIAG_INTERVAL_MS = 5000;

    constexpr float PI_F = 3.14159265358979f;
    constexpr float DegToRad(float d) { return d * PI_F / 180.0f; }

    struct Point2
    {
        float x;
        float y;
    };

    inline float Dist2d(Point2 a, Point2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

    // a wrapped into (-pi, pi].
    inline float NormAngle(float a)
    {
        a = std::fmod(a, 2.0f * PI_F);
        if (a <= -PI_F)
            a += 2.0f * PI_F;
        else if (a > PI_F)
            a -= 2.0f * PI_F;
        return a;
    }

    // The signed shortest turn from `from` to `to`, in (-pi, pi].
    inline float AngleDiff(float to, float from) { return NormAngle(to - from); }

    inline Point2 Polar(Point2 o, float bearing, float r)
    {
        return Point2{o.x + r * std::cos(bearing), o.y + r * std::sin(bearing)};
    }

    inline Point2 RoomCentre() { return Point2{ROOM_X, ROOM_Y}; }

    // The bearing from the boss to the room centre; 0 when he stands on it.
    inline float CentreBearing(Point2 boss)
    {
        float const dx = ROOM_X - boss.x;
        float const dy = ROOM_Y - boss.y;
        return (dx == 0.0f && dy == 0.0f) ? 0.0f : std::atan2(dy, dx);
    }

    // The arc axis (spec §3: the line from him to the room centre). Within AXIS_CENTRE_NONE of the centre that line is
    // undefined, so the attempt's latched fallback bearing is used, blended linearly (shortest turn) up to
    // AXIS_CENTRE_FULL. Continuous in the boss position, so the arc never jumps as he moves.
    inline float ArcAxis(Point2 boss, float fallbackBearing)
    {
        float const d = Dist2d(boss, RoomCentre());
        if (d >= AXIS_CENTRE_FULL)
            return CentreBearing(boss);
        if (d <= AXIS_CENTRE_NONE)
            return NormAngle(fallbackBearing);
        float const t = (d - AXIS_CENTRE_NONE) / (AXIS_CENTRE_FULL - AXIS_CENTRE_NONE);
        return NormAngle(fallbackBearing + t * AngleDiff(CentreBearing(boss), fallbackBearing));
    }

    // The attempt's fallback bearing, latched once at engage: from the boss toward where the raid stands (its
    // centroid), which is open floor by construction; toward the room centre when the raid is on top of him.
    inline float FallbackBearing(Point2 boss, Point2 raidCentroid)
    {
        if (Dist2d(boss, raidCentroid) < 3.0f)
            return CentreBearing(boss);
        return std::atan2(raidCentroid.y - boss.y, raidCentroid.x - boss.x);
    }

    inline float ArcRadius(uint32_t slot) { return (slot % 2) ? ARC_OUTER : ARC_INNER; }

    // Arc slot `slot` of `count`: evenly spread over ±ARC_HALF_SPAN_DEG around the axis (slot 0 at -span), rows
    // alternating by slot. A spot beyond ROOM_MAX_RADIUS of the room centre is turned toward the centre bearing in
    // quarter steps (keeping its distance to the boss): at the centre bearing it is |d - r| from the centre, inside the
    // room for any boss position the kite allows. Never moved closer to the boss, so it stays outside the swarm.
    inline Point2 ArcSpot(Point2 boss, float axis, uint32_t slot, uint32_t count)
    {
        float const frac = count <= 1 ? 0.5f : float(slot) / float(count - 1);
        float const bearing = axis + (2.0f * frac - 1.0f) * DegToRad(ARC_HALF_SPAN_DEG);
        float const r = ArcRadius(slot);
        Point2 p = Polar(boss, bearing, r);
        if (Dist2d(p, RoomCentre()) <= ROOM_MAX_RADIUS)
            return p;
        float const turn = AngleDiff(CentreBearing(boss), bearing);
        for (int step = 1; step <= 4; ++step)
        {
            p = Polar(boss, bearing + turn * (float(step) / 4.0f), r);
            if (Dist2d(p, RoomCentre()) <= ROOM_MAX_RADIUS)
                break;
        }
        return p;
    }

    // A melee flank spot: MELEE_FLANK_DEG to the left (+) or right (-) of the arc axis, MELEE_DIST from his centre.
    inline Point2 MeleeSpot(Point2 boss, float axis, bool left)
    {
        float const off = DegToRad(MELEE_FLANK_DEG);
        return Polar(boss, axis + (left ? off : -off), MELEE_DIST);
    }

    // Impale's cone from the boss toward `target`: p is within IMPALE_LENGTH of the boss and within
    // IMPALE_HALF_ANGLE_DEG of the boss->target line. For the test and the diag only.
    inline bool InImpaleCone(Point2 boss, Point2 target, Point2 p)
    {
        float const d = Dist2d(boss, p);
        if (d > IMPALE_LENGTH)
            return false;
        if (d < 0.01f)
            return true;
        float const aim = std::atan2(target.y - boss.y, target.x - boss.x);
        float const at = std::atan2(p.y - boss.y, p.x - boss.x);
        return std::fabs(AngleDiff(at, aim)) <= DegToRad(IMPALE_HALF_ANGLE_DEG);
    }

    // A non-tank bot in the raid at engage (humans, self bots and tanks are never in the plan).
    struct Member
    {
        uint64_t guid;
        uint8_t subgroup;
        bool melee;  // !PlayerbotAI::IsRanged (healers and hunters are ranged)
    };

    struct Slot
    {
        uint64_t guid;
        uint16_t arcSlot;  // every member has one: melee use it while the swarm is up
        bool melee;
        bool left;  // melee only: the flank outside the swarm
    };

    struct Plan
    {
        std::vector<Slot> slots;  // arc slot order
        uint16_t arcCount = 0;
        uint16_t meleeLeft = 0;
        uint16_t meleeRight = 0;
    };

    // Spec §3 / §5: one latched slot per bot for the attempt. Members are ordered by (raid subgroup, guid), so the
    // result does not depend on member iteration order, and a subgroup's bots sit next to each other on the arc (stock
    // bot heals cover the healer's own subgroup, so its healer stays in reach of them). Every member gets an arc slot;
    // melee also alternate left / right flanks in that order.
    inline Plan AssignSlots(std::vector<Member> members)
    {
        std::sort(members.begin(), members.end(), [](Member const& a, Member const& b)
                  { return a.subgroup != b.subgroup ? a.subgroup < b.subgroup : a.guid < b.guid; });
        members.erase(std::unique(members.begin(), members.end(), [](Member const& a, Member const& b)
                                  { return a.guid == b.guid; }),
                      members.end());
        Plan plan;
        plan.slots.reserve(members.size());
        uint16_t meleeIndex = 0;
        for (Member const& m : members)
        {
            Slot s{m.guid, uint16_t(plan.slots.size()), m.melee, false};
            if (m.melee)
            {
                s.left = (meleeIndex++ % 2) == 0;
                ++(s.left ? plan.meleeLeft : plan.meleeRight);
            }
            plan.slots.push_back(s);
        }
        plan.arcCount = uint16_t(plan.slots.size());
        return plan;
    }

    inline Slot const* FindSlot(Plan const& plan, uint64_t guid)
    {
        for (Slot const& s : plan.slots)
            if (s.guid == guid)
                return &s;
        return nullptr;
    }

    // Where the slot stands now: melee on their flank, or on the arc while the swarm is up (they cannot fight inside
    // it); everyone else on the arc.
    inline Point2 SpotFor(Slot const& s, Plan const& plan, Point2 boss, float fallbackBearing, bool swarm)
    {
        float const axis = ArcAxis(boss, fallbackBearing);
        if (s.melee && !swarm)
            return MeleeSpot(boss, axis, s.left);
        return ArcSpot(boss, axis, s.arcSlot, plan.arcCount);
    }

    inline float ToleranceFor(Slot const& s, bool swarm)
    {
        return s.melee && !swarm ? MELEE_TOLERANCE : ARC_TOLERANCE;
    }
}

#endif
