/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for AQ40 Twin Emperors v2 (overlay patch 0016). Deliberately free of core and
// playerbots includes so tests/test_aq40_twins_rules.cpp builds it standalone.
// Spec: docs/superpowers/specs/2026-10-22-playerbot-aq40-twin-emperors-v2-design.md (overlay repo).

#ifndef PLAYERBOTS_AQ40TWINSRULES_H
#define PLAYERBOTS_AQ40TWINSRULES_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace Aq40TwinsRules
{
    // Heal Brother fires when the twins are within this of each other (spec §4).
    constexpr float HEAL_BROTHER_GATE = 69.0f;
    // The two authored stations sit this far apart (spec §4; Revision 2 §11.1: as far as floor allows, min 105 y).
    constexpr float STATION_SEP_MIN = 105.0f;
    constexpr float STATION_SEP_MAX = 120.0f;
    // Vek'nilash Arcane Burst trigger range (spec §5.3).
    constexpr float ARCANE_BURST_TRIGGER = 11.0f;
    // Vek'nilash melee reach (spec §5.3).
    constexpr float VEKNILASH_MELEE_REACH = 7.3f;
    // Waiting spots from the station: warlock tank, then melee tank (spec §5.3).
    constexpr float WARLOCK_WAIT_DIST = 10.0f;
    constexpr float WARRIOR_WAIT_DIST = 14.0f;
    // Clear radius on the Vek'nilash side (spec §5.3).
    constexpr float CLEAR_RADIUS_VEKNILASH_SIDE = 13.0f;
    // Clear radius on the Vek'lor side (spec §5.3). It was also the caster keep-out until Revision 3 §12.2 removed
    // that: the magic arc spots track Vek'lor's anchor, which keeps casters at 22-26 y from him.
    constexpr float CLEAR_RADIUS_VEKLOR_SIDE = 17.0f;
    // Healer spots lie this far from their station (spec §5.3; Revision 2 §11.4: inner spots out to 26 y).
    constexpr float HEALER_SPOT_MIN = 20.0f;
    constexpr float HEALER_SPOT_MAX = 26.0f;
    // Every healer spot stays within this of the station, the casting spot and both waiting spots (spec §5.3).
    constexpr float HEAL_REACH = 36.0f;
    // Revision 6 §15.1: a side tank heal targets a designated tank of the healer's side below this health %.
    constexpr float TANK_HEAL_PCT = 90.0f;
    // Warlock casting spot lies this far from its station (spec §5.3).
    constexpr float CAST_SPOT_MIN = 20.0f;
    constexpr float CAST_SPOT_MAX = 26.0f;
    // A wrong victim of Vek'lor stays within the max of him and at least the min away (spec §5.4, §11.2).
    constexpr float WRONG_VICTIM_MIN = 12.0f;
    constexpr float WRONG_VICTIM_MAX = 40.0f;
    // Mutated bugs within this of the bot pull its target switch (spec §5.5).
    constexpr float BUG_SWITCH_RADIUS = 40.0f;
    // Threat discipline (spec §5.6): veto at own + headroom >= ratio x the active tank's threat, hard stop at the
    // hard ratio. Ruling R13: ratio-only (headroom 0). With 1000 headroom a warlock tank at 798 threat vetoed every
    // DPS on Vek'lor until it passed 1250; a ranged bot needs 130% of the victim's threat to pull aggro anyway.
    constexpr float THREAT_RATIO = 0.80f;
    constexpr float THREAT_HARD_RATIO = 0.90f;
    constexpr float THREAT_HEADROOM = 0.0f;
    // Hold-fire fails open this long after the last landing window or engage (spec §5.6). Revision 5 §14.1: 8 s, and
    // it applies to Vek'nilash only (Dps never hold fire on Vek'lor: he stays put while his victim is within 45 y).
    constexpr uint32_t HOLD_FIRE_FAILOPEN_MS = 8000;
    // Vek'lor counts as on a side once within this of its station (spec §5.2).
    constexpr float VEKLOR_SIDE_RADIUS = 30.0f;
    // [Aq40Twins] diag cadence (spec §6).
    constexpr uint32_t DIAG_INTERVAL_MS = 5000;

    // Revision 3 §12.2, boss anchors: a side's anchor re-latches to its twin once the twin has stayed within
    // ANCHOR_SETTLE_RADIUS of one candidate point for ANCHOR_SETTLE_MS and that point is more than ANCHOR_RELATCH_MIN
    // from the current anchor. Every per-side spot is anchor + (authored spot - authored station).
    constexpr float ANCHOR_SETTLE_RADIUS = 3.0f;
    constexpr uint32_t ANCHOR_SETTLE_MS = 3000;
    constexpr float ANCHOR_RELATCH_MIN = 6.0f;
    // Revision 3 §12.2, the separation guard: the active melee tank drags Vek'nilash only while the twins are closer
    // than this, and then toward the AUTHORED station farther from Vek'lor (kite him away from the other boss).
    constexpr float SEPARATION_GUARD = 100.0f;
    // Revision 3 §12.1, the melee pre-swap move: this long after the last landing window OPENED (or engage, before
    // the first teleport at 30 s), melee Dps and spare melee tanks leave Vek'nilash for a point PRE_SWAP_DIST from
    // the other side's anchor, toward their own side (teleports come every 30-40 s). Revision 5 §14.2: 28 s (2 s
    // before the earliest teleport, plus Vek'lor's 2.3 s passive, gives ~30 y of clearance).
    constexpr uint32_t PRE_SWAP_MS = 28000;
    constexpr float PRE_SWAP_DIST = 20.0f;
    // Revision 4 §13.2, healer step-in: once one of the healer's side's designated tanks needs healing and has been
    // more than STEP_IN_TRIGGER from the healer's spot for STEP_IN_MS, the healer's spot becomes the point
    // STEP_IN_DIST from that tank toward the spot, until the tank is back within STEP_IN_END of the spot or dead
    // (ruling R25: not at full health, which is transient on a tank under steady damage).
    constexpr float STEP_IN_TRIGGER = 36.0f;
    constexpr float STEP_IN_END = 34.0f;
    constexpr float STEP_IN_DIST = 32.0f;
    constexpr uint32_t STEP_IN_MS = 2000;
    // Revision 11 §20.2, the LOS step-in: a side tank alive, below 100 %, within STEP_IN_TRIGGER of the healer's spot
    // but out of its line of sight for STEP_IN_LOS_MS (its own clock) moves the healer's spot to the point
    // STEP_IN_LOS_DIST from that tank toward the spot, until LOS is back with the tank within STEP_IN_END, or it dies.
    constexpr uint32_t STEP_IN_LOS_MS = 1500;
    constexpr float STEP_IN_LOS_DIST = 20.0f;
    // Final-review I1, the active warlock in spell range of an off-station Vek'lor but out of his line of sight: its
    // spot is the first point with LOS on the straight line from it to him. Candidates sit on a fixed grid of radii
    // from him, WARLOCK_LOS_MIN + k * WARLOCK_LOS_STEP down the bearing to the bot (stable while it walks that line),
    // scanned from the bot inward; none qualifies: the point WARLOCK_LOS_FALLBACK from him on that bearing.
    constexpr float WARLOCK_LOS_MIN = 17.0f;
    constexpr float WARLOCK_LOS_STEP = 2.0f;
    constexpr float WARLOCK_LOS_FALLBACK = 20.0f;
    // Revision 4 §13.3, the Vek'lor berth: the strategy's own moves route round a BERTH_RADIUS circle around him via
    // waypoints BERTH_DETOUR from him. BERTH_ARC_BAND: within this beyond BERTH_DETOUR a bot counts as on the detour
    // circle and steps round it in fixed BERTH_ARC_STEP sectors, at least BERTH_ARC_LEAD ahead of itself.
    constexpr float BERTH_RADIUS = 14.0f;
    constexpr float BERTH_DETOUR = 16.0f;
    constexpr float BERTH_ARC_BAND = 1.0f;
    constexpr float BERTH_ARC_STEP = 3.14159265f / 6.0f;  // 30 degrees
    constexpr float BERTH_ARC_LEAD = 3.14159265f / 12.0f; // 15 degrees
    // Revision 4 §13.4: a healer bot below this mana is refilled once per attempt (only under the `raid` cheat).
    constexpr float MANA_REFILL_BELOW_PCT = 25.0f;
    // Revision 7 §16.1, the universal burst escape: any bot within BURST_ESCAPE_TRIGGER (2-D) of a living Vek'lor that
    // is not REACT_PASSIVE steps out to BURST_ESCAPE_TO from him. Native combat movement (melee chasing a bug or its
    // target, reach) still entered the 11 y Arcane Burst range that the §13.3 berth only keeps the strategy's own moves
    // out of.
    constexpr float BURST_ESCAPE_TRIGGER = 12.5f;
    constexpr float BURST_ESCAPE_TO = 16.0f;
    // Revision 7 §16.2: a physical melee (non-hunter) bot does not switch to a mutated bug within this of a living
    // Vek'lor. Ranged still take it.
    constexpr float MELEE_BUG_VEKLOR_SKIP = 14.0f;
    // Revision 8 §17.1, the waiting pickups' basis: each side keeps a basis point for the twin on it, and the two
    // waiting pickup spots are measured from it. It re-latches to the twin's position only once the twin is more than
    // PICKUP_BASIS_RELATCH from it, so the destinations hold while native tanking shuffles the boss.
    constexpr float PICKUP_BASIS_RELATCH = 4.0f;

    // Form-up (spec §5.5, ruling R11): side-A bots and magic DPS stage this far from station A along the A->B
    // crossing line, inside warlock A's 28.5 y spell range. The twins' aggro radius against level-60 players is
    // ~23 y (detection_range 20, levelDiff 60 - 63 = -3 -> 20 - (-3); CanStartAttack measures centre to centre),
    // so S_A, ~28.4 y from Vek'lor's spawn, clears it by ~5 y.
    constexpr float STAGE_A_DIST = 28.0f;
    // Form-up needs the master within this of the midpoint of the two spawns (the Twins room).
    constexpr float ROOM_RADIUS = 100.0f;
    // Every assigned bot within this of its staging spot (2-D) counts as formed up.
    constexpr float STAGE_ARRIVE = 3.0f;

    // Spec §5.3 ordering, encoded so a retune that breaks it fails to compile.
    static_assert(WARLOCK_WAIT_DIST > VEKNILASH_MELEE_REACH, "waiting warlock must be outside Vek'nilash melee reach");
    static_assert(WARLOCK_WAIT_DIST + 3.0f <= CLEAR_RADIUS_VEKNILASH_SIDE,
                  "waiting warlock needs a 3 y margin inside the Vek'nilash-side clear radius");
    static_assert(WARRIOR_WAIT_DIST >= CLEAR_RADIUS_VEKNILASH_SIDE,
                  "waiting warrior must stand at or beyond the Vek'nilash-side clear radius");
    static_assert(WARRIOR_WAIT_DIST > ARCANE_BURST_TRIGGER, "waiting warrior must be outside the Arcane Burst trigger");
    static_assert(BURST_ESCAPE_TRIGGER > ARCANE_BURST_TRIGGER, "the burst escape must fire before Arcane Burst range");
    static_assert(BURST_ESCAPE_TO > BURST_ESCAPE_TRIGGER, "the burst escape point must end its own trigger");
    static_assert(WARRIOR_WAIT_DIST + 3.0f <= CLEAR_RADIUS_VEKLOR_SIDE,
                  "waiting warrior needs a 3 y margin inside the Vek'lor-side clear radius");
    static_assert(CLEAR_RADIUS_VEKLOR_SIDE <= HEALER_SPOT_MIN,
                  "healer spots must be outside the Vek'lor-side clear radius");
    static_assert(HEALER_SPOT_MIN <= HEALER_SPOT_MAX && HEALER_SPOT_MAX <= HEAL_REACH,
                  "healer spots must lie inside the heal reach of their station");
    static_assert(CLEAR_RADIUS_VEKLOR_SIDE <= CAST_SPOT_MIN,
                  "casting spot must be outside the Vek'lor-side clear radius");
    static_assert(CAST_SPOT_MAX <= 28.5f, "casting spot must stay inside the caster range budget");
    static_assert(SEPARATION_GUARD > HEAL_BROTHER_GATE && SEPARATION_GUARD <= STATION_SEP_MIN,
                  "the separation guard sits between the Heal Brother gate and the authored separation");
    static_assert(PRE_SWAP_DIST > CLEAR_RADIUS_VEKLOR_SIDE && PRE_SWAP_DIST > ARCANE_BURST_TRIGGER,
                  "the pre-swap point must be outside Vek'nilash's landing clear and Vek'lor's Arcane Burst");
    static_assert(STEP_IN_DIST < STEP_IN_END && STEP_IN_END < STEP_IN_TRIGGER && STEP_IN_TRIGGER <= HEAL_REACH,
                  "step-in: point < end < trigger <= heal reach");
    static_assert(STEP_IN_LOS_DIST < STEP_IN_END, "the LOS step-in point must lie inside the step-in end distance");
    static_assert(WARLOCK_LOS_MIN > BURST_ESCAPE_TO && WARLOCK_LOS_FALLBACK >= WARLOCK_LOS_MIN,
                  "the warlock's LOS points must stay outside the burst escape (the Revision 9 push never moves them)");
    static_assert(BERTH_RADIUS > ARCANE_BURST_TRIGGER && BERTH_DETOUR > BERTH_RADIUS,
                  "the berth must clear Arcane Burst, and its waypoints the berth");
    // A 45 degree chord of the detour circle (an arc step's widest) stays outside the berth: 16 cos 22.5 = 14.8.
    static_assert(BERTH_ARC_STEP + BERTH_ARC_LEAD <= 3.14159265f / 4.0f + 1e-4f, "arc steps span at most 45 degrees");
    // Ruling R3: the station separation keeps a 20 y margin over the Heal Brother gate.
    static_assert(STATION_SEP_MIN >= HEAL_BROTHER_GATE + 20.0f, "station separation needs >= 20 y over the gate");

    struct Pt2
    {
        float x, y;
    };

    // The point `dist` along the segment from -> to (past `to` if dist exceeds its length); `from` when they coincide.
    inline Pt2 PointAlong(Pt2 from, Pt2 to, float dist)
    {
        float const dx = to.x - from.x;
        float const dy = to.y - from.y;
        float const len = std::hypot(dx, dy);
        if (len <= 0.0f)
            return from;
        return Pt2{from.x + dx / len * dist, from.y + dy / len * dist};
    }

    // Revision 9 §18.2: `p` pushed radially out to `r` from `centre` when it is closer than r (on the bearing
    // centre -> p, or along +x when p is the centre); otherwise `p` unchanged.
    inline Pt2 PushOutFrom(Pt2 centre, Pt2 p, float r)
    {
        float const dx = p.x - centre.x;
        float const dy = p.y - centre.y;
        float const d = std::hypot(dx, dy);
        if (d >= r)
            return p;
        if (d <= 0.0f)
            return Pt2{centre.x + r, centre.y};
        return Pt2{centre.x + dx / d * r, centre.y + dy / d * r};
    }

    // Fix round 1 (R22): `p` pulled back along centre -> p to at most `r` from `centre` (same bearing); unchanged
    // when already within r. Anchors are clamped this way to their authored station, or they ratchet outward: after
    // each swap Vek'nilash settles on the pickup tank's OUTER waiting spot, which the next anchor then moves out again.
    inline Pt2 ClampToRadius(Pt2 centre, Pt2 p, float r)
    {
        float const dx = p.x - centre.x;
        float const dy = p.y - centre.y;
        float const len = std::hypot(dx, dy);
        if (len <= r || len <= 0.0f)
            return p;
        return Pt2{centre.x + dx / len * r, centre.y + dy / len * r};
    }

    enum class Side : uint8_t { A = 0, B = 1, None = 2 };
    enum class Role : uint8_t { None, MeleeTank, WarlockTank, Healer, Dps };

    inline Side Other(Side s)
    {
        return s == Side::A ? Side::B : (s == Side::B ? Side::A : Side::None);
    }

    // cls uses WoW class ids: 1 warrior, 2 paladin, 3 hunter, 4 rogue, 5 priest, 6 DK, 7 shaman, 8 mage,
    // 9 warlock, 11 druid.
    struct Member
    {
        uint32_t guid;
        uint8_t cls;
        bool isBot;
        bool isTank;
        bool isHeal;
        bool isMelee;
    };

    // side is set for designated tanks, spares and healers; None for DPS and humans. A spare is a melee tank or
    // warlock beyond the first two (Role::Dps). fillIn marks a non-warlock standing in for a missing warlock tank.
    struct Assignment
    {
        uint32_t guid;
        Role role;
        Side side;
        bool spare;
        bool fillIn;
    };

    // Physical-damage dealers: Vek'lor's shadow/fire/arcane immunity makes them useless on him (spec §5.4).
    inline bool IsPhysical(uint8_t cls, bool isMelee)
    {
        switch (cls)
        {
            case 1:  // warrior
            case 2:  // paladin
            case 3:  // hunter
            case 4:  // rogue
            case 6:  // death knight
                return true;
            case 7:   // shaman
            case 11:  // druid
                return isMelee;
            default:
                return false;
        }
    }

    // Assigns tanks, warlock tanks and healers to sides (spec §5.1). Humans get Role::None.
    inline std::vector<Assignment> AssignTwins(std::vector<Member> const& members)
    {
        std::vector<Assignment> out;
        out.reserve(members.size());
        for (Member const& m : members)
            out.push_back(Assignment{m.guid, m.isBot ? Role::Dps : Role::None, Side::None, false, false});

        auto classRank = [](uint8_t cls) -> int
        {
            return cls == 1 ? 0 : (cls == 2 ? 1 : (cls == 11 ? 2 : 3));
        };

        // 1. Melee tanks: warrior, paladin, druid, other; then guid. #0 -> A, #1 -> B, the rest alternate as spares.
        std::vector<size_t> tanks;
        for (size_t i = 0; i < members.size(); ++i)
            if (members[i].isBot && members[i].isTank)
                tanks.push_back(i);
        std::sort(tanks.begin(), tanks.end(),
                  [&](size_t a, size_t b)
                  {
                      int const ra = classRank(members[a].cls);
                      int const rb = classRank(members[b].cls);
                      return ra != rb ? ra < rb : members[a].guid < members[b].guid;
                  });
        for (size_t k = 0; k < tanks.size(); ++k)
        {
            Assignment& a = out[tanks[k]];
            a.side = (k % 2 == 0) ? Side::A : Side::B;
            if (k < 2)
                a.role = Role::MeleeTank;
            else
                a.spare = true;  // stays Role::Dps
        }

        // 2. Warlock tanks: non-tank, non-heal warlocks by guid; the rest are sideless spares.
        std::vector<size_t> locks;
        for (size_t i = 0; i < members.size(); ++i)
            if (members[i].isBot && members[i].cls == 9 && !members[i].isTank && !members[i].isHeal)
                locks.push_back(i);
        std::sort(locks.begin(), locks.end(), [&](size_t a, size_t b) { return members[a].guid < members[b].guid; });
        bool taken[2] = {false, false};
        for (size_t k = 0; k < locks.size(); ++k)
        {
            Assignment& a = out[locks[k]];
            if (k < 2)
            {
                a.role = Role::WarlockTank;
                a.side = k == 0 ? Side::A : Side::B;
                taken[k] = true;
            }
            else
                a.spare = true;  // stays Role::Dps, no side
        }

        // Fill free warlock slots from mage, then non-healing priest, then any non-physical non-healer.
        for (int slot = 0; slot < 2; ++slot)
        {
            if (taken[slot])
                continue;
            size_t best = members.size();
            int bestTier = 4;
            for (size_t i = 0; i < members.size(); ++i)
            {
                Member const& m = members[i];
                if (!m.isBot || m.isTank || m.isHeal || out[i].role != Role::Dps || out[i].spare)
                    continue;
                int const tier = m.cls == 8 ? 1 : (m.cls == 5 ? 2 : (!IsPhysical(m.cls, m.isMelee) ? 3 : 4));
                if (tier < bestTier || (tier == bestTier && tier < 4 && m.guid < members[best].guid))
                {
                    best = i;
                    bestTier = tier;
                }
            }
            if (best == members.size())
                continue;
            out[best].role = Role::WarlockTank;
            out[best].side = slot == 0 ? Side::A : Side::B;
            out[best].fillIn = true;
            taken[slot] = true;
        }

        // 3. Healers by guid, alternating A, B.
        std::vector<size_t> healers;
        for (size_t i = 0; i < members.size(); ++i)
            if (members[i].isBot && members[i].isHeal && out[i].role == Role::Dps && !out[i].spare)
                healers.push_back(i);
        std::sort(healers.begin(), healers.end(),
                  [&](size_t a, size_t b) { return members[a].guid < members[b].guid; });
        for (size_t k = 0; k < healers.size(); ++k)
        {
            out[healers[k]].role = Role::Healer;
            out[healers[k]].side = (k % 2 == 0) ? Side::A : Side::B;
        }

        // 4. Everyone else stays Dps with Side::None.
        return out;
    }

    // Replacement for a dead designated tank (spec §5.1): a living spare of the same kind, same side first, else
    // the other side (warlock spares carry no side, so they sort after same-side ones), lowest guid first. 0 if none.
    inline uint32_t PickReplacement(Role role, Side side,
                                    std::vector<std::pair<Assignment, Member>> const& living)
    {
        if (role != Role::MeleeTank && role != Role::WarlockTank)
            return 0;
        uint32_t best = 0;
        int bestTier = 2;
        for (auto const& [a, m] : living)
        {
            if (!a.spare)
                continue;
            bool const kind = role == Role::MeleeTank ? m.isTank : (m.cls == 9 && !m.isTank);
            if (!kind)
                continue;
            int const tier = a.side == side ? 0 : 1;
            if (tier < bestTier || (tier == bestTier && a.guid < best))
            {
                best = a.guid;
                bestTier = tier;
            }
        }
        return best;
    }

    // Vek'lor's side from geometry with hysteresis (spec §5.2): on a side only within VEKLOR_SIDE_RADIUS of its
    // station and nearer it than the other; otherwise keep the previous side.
    inline Side UpdateVeklorSide(Side prev, float distToA, float distToB)
    {
        if (distToA <= VEKLOR_SIDE_RADIUS && distToA < distToB)
            return Side::A;
        if (distToB <= VEKLOR_SIDE_RADIUS && distToB < distToA)
            return Side::B;
        return prev;
    }

    // Ruling R15: Vek'lor's side at the landing window's rising edge. A Twin Teleport always swaps the twins, so the
    // side flips even when he lands more than VEKLOR_SIDE_RADIUS off both stations and geometry (UpdateVeklorSide,
    // `geometric`) kept the old one. Later refreshes keep the geometric override.
    inline Side SideAfterTeleport(Side prev, Side geometric)
    {
        return geometric != prev ? geometric : Other(prev);
    }

    // Threat discipline (spec §5.6). No tank threat means no cap.
    inline bool ThreatVetoed(float own, float activeTankThreat)
    {
        return activeTankThreat > 0.0f && own + THREAT_HEADROOM >= THREAT_RATIO * activeTankThreat;
    }

    inline bool ThreatHardStop(float own, float activeTankThreat)
    {
        return activeTankThreat > 0.0f && own >= THREAT_HARD_RATIO * activeTankThreat;
    }

    // Revision 2 §11.2, the no-drag rule: a bot that is a twin's current victim but not that twin's designated
    // ACTIVE tank is a "wrong victim", and every movement but a hazard dodge and the victim hold is vetoed for it. A
    // designated tank of the other kind (a warlock tank, active or waiting, either side, for Vek'nilash; a melee
    // tank for Vek'lor) is never a wrong victim: it keeps doing its own job (dev pull 4 walked warlock A, Vek'nilash's
    // first target, toward warrior B, and Vek'lor followed it 55 y inward).
    inline bool IsWrongVictim(bool isThatTwinsVictim, bool isThatTwinsActiveTank, bool isOtherKindDesignatedTank)
    {
        return isThatTwinsVictim && !isThatTwinsActiveTank && !isOtherKindDesignatedTank;
    }

    // Revision 3 §12.2: re-latch a side's anchor to its twin's settled candidate point.
    inline bool AnchorShouldRelatch(float distFromCandidate, uint32_t msStill, float candToAnchor)
    {
        return distFromCandidate <= ANCHOR_SETTLE_RADIUS && msStill >= ANCHOR_SETTLE_MS &&
               candToAnchor > ANCHOR_RELATCH_MIN;
    }

    // Revision 3 §12.1: the melee pre-swap move is due (ms since the last landing window started, or since engage).
    inline bool PreSwapDue(uint32_t msSinceLastWindowStart)
    {
        return msSinceLastWindowStart >= PRE_SWAP_MS;
    }

    // Revision 4 §13.2 + ruling R25, the step-in clock. Before the step-in (`stepping` false) it runs only while the
    // tank is alive, below 100 % and more than STEP_IN_TRIGGER from the healer's spot, so the start needs both for
    // STEP_IN_MS. Once stepping in it runs while the tank is alive and beyond STEP_IN_END: health no longer ends it,
    // or tank damage would cycle the healer out and back between heals.
    inline bool HealerStepInClockRuns(bool stepping, bool tankAlive, float tankHpPct, float dist)
    {
        if (stepping)
            return tankAlive && dist > STEP_IN_END;
        return tankAlive && tankHpPct < 100.0f && dist > STEP_IN_TRIGGER;
    }

    // Revision 4 §13.2: the healer steps in once the clock above has run msOut >= STEP_IN_MS; it ends at STEP_IN_END
    // or death. The clock starting only beyond STEP_IN_TRIGGER is the 36/34 y hysteresis.
    inline bool HealerStepInDue(bool tankAlive, float dist, uint32_t msOut)
    {
        return tankAlive && dist > STEP_IN_END && msOut >= STEP_IN_MS;
    }

    // Revision 11 §20.2: the LOS step-in is due once the start condition (tank alive, below 100 %, within
    // STEP_IN_TRIGGER of the healer's spot, not in LOS) has held for msNoLos >= STEP_IN_LOS_MS. The distance step-in
    // owns everything beyond STEP_IN_TRIGGER.
    inline bool HealerLosStepInDue(bool tankAlive, float hpPct, float dist, bool los, uint32_t msNoLos)
    {
        return tankAlive && hpPct < 100.0f && dist <= STEP_IN_TRIGGER && !los && msNoLos >= STEP_IN_LOS_MS;
    }

    // Revision 11 §20.2: a running LOS step-in ends once LOS is back with the tank within STEP_IN_END of the spot, or
    // the tank is dead. Health no longer ends it (ruling R25, as for the distance step-in).
    inline bool HealerLosStepInEnds(bool tankAlive, float dist, bool los)
    {
        return !tankAlive || (los && dist <= STEP_IN_END);
    }

    // Revision 11 §20.2 + final-review M1: how far from the tank, toward the healer's spot (`d` away), its step-in
    // point lies. The LOS step-in can start with the tank inside STEP_IN_LOS_DIST of the spot, where a fixed 20 y
    // would put the point past the spot (stepping the healer the wrong way), so it takes at most half of d. The
    // distance step-in (d > STEP_IN_END > STEP_IN_DIST) keeps its fixed STEP_IN_DIST.
    inline float HealerStepInPointDist(bool los, float d)
    {
        return los ? std::min(STEP_IN_LOS_DIST, 0.5f * d) : STEP_IN_DIST;
    }

    // Final-review I1: the largest WARLOCK_LOS_MIN + k * WARLOCK_LOS_STEP (k >= 0) strictly below botDist - arrive,
    // where the warlock's LOS scan starts (so any chosen point is a real walk, never the spot it already stands on).
    // Below WARLOCK_LOS_MIN when there is none.
    inline float WarlockLosTopRadius(float botDist, float arrive)
    {
        float const limit = botDist - arrive;
        if (limit <= WARLOCK_LOS_MIN)
            return 0.0f;
        float const k = std::ceil((limit - WARLOCK_LOS_MIN) / WARLOCK_LOS_STEP) - 1.0f;
        return WARLOCK_LOS_MIN + std::max(k, 0.0f) * WARLOCK_LOS_STEP;
    }

    // Revision 11 §20.1: whoever Vek'nilash attacks that is not his active melee tank (a §11.2 wrong victim) kites him
    // toward the drag station while the twins are closer than SEPARATION_GUARD, or when no active melee tank is alive
    // to walk him to; otherwise it walks him to that tank.
    inline bool VictimKites(float separation, bool activeMeleeTankAlive)
    {
        return separation < SEPARATION_GUARD || !activeMeleeTankAlive;
    }

    // The closest the segment a -> b comes to c.
    inline float SegmentDistance(Pt2 a, Pt2 b, Pt2 c)
    {
        float const dx = b.x - a.x;
        float const dy = b.y - a.y;
        float const len2 = dx * dx + dy * dy;
        float t = len2 > 0.0f ? ((c.x - a.x) * dx + (c.y - a.y) * dy) / len2 : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        return std::hypot(a.x + dx * t - c.x, a.y + dy * t - c.y);
    }

    // Revision 4 §13.3, the Vek'lor berth: the next waypoint from `from` toward `to` round an `r` circle at `centre`.
    //   * `to` inside r: `to` (that destination is deliberately near him; detouring would oscillate against it).
    //   * `from` inside r: radially out to `detour` (straight out, so the destination holds while it walks).
    //   * the segment clears r: `to`.
    //   * else round the circle on from's side of the centre -> to line (the side the segment passes; a straight-
    //     through tie goes counter-clockwise), via `detour` waypoints that each depend only on the centre, `to` and
    //     that side, so they hold while the bot walks (MoveTo's duplicate guard):
    //       - the tangent point from `to` (the side nearer `to`) once the leg there clears r;
    //       - else, from beyond the detour circle, the tangent point from `from`: every point on that tangent line
    //         has the same one, and the leg itself never comes closer than `detour`;
    //       - else (on the detour circle) the next fixed BERTH_ARC_STEP sector boundary at least BERTH_ARC_LEAD
    //         ahead: chords of at most 45 degrees, >= 14.8 y from him at r 14 / detour 16.
    inline Pt2 BerthWaypoint(Pt2 from, Pt2 to, Pt2 centre, float r = BERTH_RADIUS, float detour = BERTH_DETOUR)
    {
        float const tx = to.x - centre.x;
        float const ty = to.y - centre.y;
        float const dt = std::hypot(tx, ty);
        if (dt < r)
            return to;
        float const fx = from.x - centre.x;
        float const fy = from.y - centre.y;
        float const df = std::hypot(fx, fy);
        if (df < r)
        {
            if (df <= 0.0f)  // on the centre itself: out toward `to`
                return Pt2{centre.x + tx / dt * detour, centre.y + ty / dt * detour};
            return Pt2{centre.x + fx / df * detour, centre.y + fy / df * detour};
        }
        if (SegmentDistance(from, to, centre) >= r)
            return to;

        // s = +1: from is counter-clockwise of to, so the short way round is clockwise (decreasing bearing).
        float const s = tx * fy - ty * fx >= 0.0f ? 1.0f : -1.0f;
        auto onCircle = [&](float bearing)
        { return Pt2{centre.x + std::cos(bearing) * detour, centre.y + std::sin(bearing) * detour}; };

        float const bt = std::atan2(ty, tx);
        Pt2 const tangentTo = onCircle(bt + s * (dt > detour ? std::acos(detour / dt) : 0.0f));
        if (SegmentDistance(from, tangentTo, centre) >= r)
            return tangentTo;

        float const bf = std::atan2(fy, fx);
        if (df > detour + BERTH_ARC_BAND)
            return onCircle(bf - s * std::acos(detour / df));

        float const sector = s > 0.0f ? std::floor((bf - BERTH_ARC_LEAD) / BERTH_ARC_STEP)
                                       : std::ceil((bf + BERTH_ARC_LEAD) / BERTH_ARC_STEP);
        return onCircle(sector * BERTH_ARC_STEP);
    }

    // Non-tanks hold fire on a twin until it is on its active tank, failing open after the window (spec §5.6). The
    // caller applies it to Vek'nilash only (Revision 5 §14.1).
    inline bool HoldFireOnTwin(bool victimIsActiveTank, uint32_t msSinceWindowOrEngage)
    {
        return !victimIsActiveTank && msSinceWindowOrEngage < HOLD_FIRE_FAILOPEN_MS;
    }

    // Revision 7 §16.1: the bot is inside the burst escape trigger of a living, non-passive Vek'lor.
    inline bool InBurstRange(float dist2d, bool veklorAlive, bool veklorPassive)
    {
        return veklorAlive && !veklorPassive && dist2d < BURST_ESCAPE_TRIGGER;
    }

    // Revision 7 §16.2: a physical melee non-hunter (`physicalMelee`) skips a mutated bug within MELEE_BUG_VEKLOR_SKIP
    // (2-D) of a living Vek'lor.
    inline bool MeleeSkipsBug(bool physicalMelee, bool veklorAlive, float bugToVeklor2d)
    {
        return physicalMelee && veklorAlive && bugToVeklor2d < MELEE_BUG_VEKLOR_SKIP;
    }

    // Revision 8 §17.1: re-latch a side's pickup basis to its twin (`valid` false at the latch and at every landing
    // window's rising edge, so the first refresh after either always latches).
    inline bool BasisShouldRelatch(bool valid, float distTwinToBasis)
    {
        return !valid || distTwinToBasis > PICKUP_BASIS_RELATCH;
    }

    // Revision 10 §19.1: inside the landing window the Vek'nilash pickup (the waiting melee tank of the side Vek'lor
    // left) engages at once: it targets him, owns no spot and closes to melee with native reach. Damage during his
    // 2 s REACT_PASSIVE window adds threat, and being in melee makes it the nearest unit for his +2000 roll.
    inline bool PickupEngagesInWindow(bool window, bool isVeknilashPickup)
    {
        return window && isVeknilashPickup;
    }

    // Revision 10 §19.3: inside the landing window a latched healer makes no move but an exempt dodge (the Explode,
    // Blizzard and landing-clear moves). Hold spot, step-in and heal reach are vetoed; casting is unaffected.
    inline bool HealerFrozenInWindow(bool window, bool isHealer, bool isExemptDodge)
    {
        return window && isHealer && !isExemptDodge;
    }
}  // namespace Aq40TwinsRules

#endif
