/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Naxx40 Four Horsemen (Individual Progression's level-60 Naxxramas). Deliberately
// free of core and playerbots includes so tests/test_naxx40_horsemen_rules.cpp (overlay repo) builds
// it standalone:
//   g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/nx40hm tests/test_naxx40_horsemen_rules.cpp && /tmp/nx40hm
// Spec: docs/superpowers/specs/2026-10-27-playerbot-naxx40-four-horsemen-design.md (overlay repo).

#ifndef PLAYERBOTS_NAXX40HORSEMENRULES_H
#define PLAYERBOTS_NAXX40HORSEMENRULES_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Naxx40HorsemenRules
{
    enum class Horse : uint8_t
    {
        Korthazz = 0,
        Mograine = 1,
        Blaumeux = 2,
        Zeliek = 3,
        None = 4
    };

    // Indexed by Horse.
    constexpr uint32_t ENTRY[4] = {351039, 351037, 351040, 351038};
    constexpr uint32_t SPIRIT_ENTRY[4] = {16778, 16775, 16776, 16777};
    constexpr uint32_t MARK_SPELL[4] = {28832, 28834, 28833, 28835};
    constexpr uint32_t VOID_ZONE_ENTRY = 16697;
    constexpr uint32_t BOSS_STATE_ID = 12;
    constexpr float MARK_RADIUS = 55.0f;
    constexpr uint8_t LEAVE_AT_STACKS = 3;
    // Spec §4.3f: a healer bot below this mana % gets its one refill under the `raid` cheat (the AQ40 Twins pattern).
    constexpr float MANA_REFILL_BELOW_PCT = 25.0f;
    constexpr uint8_t READY_AT_STACKS = 2;
    constexpr float MELEE_BEHIND = 3.0f;     // melee spot distance behind the boss
    // Holy Wrath (28883) is DmgClass MAGIC, so its chain jumps 10 y (Spell.cpp jump search, no spell_jump_distance
    // override): 10 y plus a 2 y reach margin. Melee in reach of Zeliek are always inside it, so melee DPS never
    // work him (WithoutZeliek); ranged / healer spots keep >= this + 1 from his tank spot (stations tool).
    constexpr float ZELIEK_TANK_GAP = 12.0f;
    constexpr float VOID_CLEAR = 5.0f;
    constexpr float THREAT_HOLD = 0.90f;     // DPS threat / active tank threat
    // Absolute headroom on the pre-cast veto (ThreatBlocked): a ratio check before a cast does not bound the cast's own
    // threat (the BWL lesson, BWL_THREAT_HEADROOM). The hard stop (ThreatStop) uses the plain ratio.
    constexpr float THREAT_HEADROOM = 500.0f;
    constexpr uint32_t DIAG_INTERVAL_MS = 5000;
    constexpr float TANK_HEAL_PCT = 90.0f;   // the Aq40 twins tank-heal threshold
    constexpr float HEAL_REACH = 36.0f;      // 2-D; all four fast heals reach 40 y
    // A horseman within this of his tank spot is parked (Blaumeux's tank points are up to 11.2 y off hers; healers
    // measure her from her current point).
    constexpr float PARK_RANGE = 10.0f;
    constexpr float ESCORT_RANGE = 25.0f;    // healers escorting an unparked horseman's tank close to within this
    constexpr float MARK_MARGIN = 2.0f;      // MarkSafe: a bot this far past MARK_RADIUS counts as inside it

    struct Pt
    {
        float x, y;
    };

    constexpr uint8_t Idx(Horse h) { return static_cast<uint8_t>(h); }

    inline Horse HorseOfEntry(uint32_t entry)
    {
        for (uint8_t i = 0; i < 4; ++i)
            if (ENTRY[i] == entry)
                return static_cast<Horse>(i);
        return Horse::None;
    }

    // ---- Assignment (spec §4) ----

    enum class Role : uint8_t
    {
        None,
        Tank,
        Healer,
        Dps
    };

    struct Member
    {
        uint64_t guid;
        Role role;
        bool melee;
    };

    struct Assignment
    {
        uint64_t active[4]{}, resting[4]{};  // tank pairs by Horse; 0 = empty
        std::vector<uint64_t> spareTanks;    // tanks beyond 8
        std::vector<uint64_t> healers[5];    // [0..3] start on Horse(i), [4] floaters
        std::vector<uint64_t> dpsHalf[2];
    };

    // Insertion sort (lists are <= 40 long).
    template <typename Less>
    inline void SortMembers(std::vector<Member>& v, Less less)
    {
        for (size_t i = 1; i < v.size(); ++i)
        {
            Member const cur = v[i];
            size_t j = i;
            while (j > 0 && less(cur, v[j - 1]))
            {
                v[j] = v[j - 1];
                --j;
            }
            v[j] = cur;
        }
    }

    inline Assignment Assign(std::vector<Member> members)
    {
        std::vector<Member> tanks, healers, dps;
        for (Member const& m : members)
        {
            if (m.role == Role::Tank)
                tanks.push_back(m);
            else if (m.role == Role::Healer)
                healers.push_back(m);
            else if (m.role == Role::Dps)
                dps.push_back(m);
        }
        auto byGuid = [](Member const& a, Member const& b) { return a.guid < b.guid; };
        SortMembers(tanks, byGuid);
        SortMembers(healers, byGuid);
        SortMembers(dps, [](Member const& a, Member const& b)
                    { return a.melee != b.melee ? a.melee : a.guid < b.guid; });

        Assignment out;
        for (size_t i = 0; i < tanks.size(); ++i)
        {
            if (i < 4)
                out.active[i] = tanks[i].guid;
            else if (i < 8)
                out.resting[i - 4] = tanks[i].guid;
            else
                out.spareTanks.push_back(tanks[i].guid);
        }
        for (size_t j = 0; j < healers.size(); ++j)
            out.healers[j < 8 ? j % 4 : 4].push_back(healers[j].guid);
        for (size_t k = 0; k < dps.size(); ++k)
            out.dpsHalf[k % 2].push_back(dps[k].guid);
        return out;
    }

    // ---- Rotation (spec §4.2, §4.3) ----

    using Order = std::array<Horse, 4>;  // None-padded
    constexpr Order DPS_ORDER[2] = {{Horse::Korthazz, Horse::Mograine, Horse::Blaumeux, Horse::Zeliek},
                                    {Horse::Mograine, Horse::Korthazz, Horse::Zeliek, Horse::Blaumeux}};

    // Melee DPS never work Zeliek (Holy Wrath's 10 y chain reaches every melee in his reach): his slot becomes None,
    // the rest stay in place.
    inline Order WithoutZeliek(Order o)
    {
        for (Horse& h : o)
            if (h == Horse::Zeliek)
                h = Horse::None;
        return o;
    }

    // group 0..3 = the healer pairs that start on Horse(group); group 4 = the floater (follows DPS half 0).
    inline Order HealerOrder(uint8_t group, std::array<bool, 4> const& alive)
    {
        bool const frontGone = !alive[Idx(Horse::Korthazz)] && !alive[Idx(Horse::Mograine)];
        switch (group)
        {
            case 0:
                return frontGone ? Order{Horse::Blaumeux, Horse::Zeliek, Horse::None, Horse::None}
                                 : Order{Horse::Korthazz, Horse::Mograine, Horse::None, Horse::None};
            case 1:
                return frontGone ? Order{Horse::Zeliek, Horse::Blaumeux, Horse::None, Horse::None}
                                 : Order{Horse::Mograine, Horse::Korthazz, Horse::None, Horse::None};
            case 2:
                return Order{Horse::Blaumeux, Horse::Zeliek, Horse::None, Horse::None};
            case 3:
                return Order{Horse::Zeliek, Horse::Blaumeux, Horse::None, Horse::None};
            case 4:
                return DPS_ORDER[0];
            default:
                return Order{Horse::None, Horse::None, Horse::None, Horse::None};
        }
    }

    // Which horseman to be on for the coming turn. None = the safe spot.
    // frontGate true (DPS and the group-4 floater healer, which follows DPS_ORDER[0]): a back horseman
    // (Blaumeux/Zeliek) is eligible only once Korth'azz or Mograine is dead, so nobody joins the phase-2 pair
    // early. frontGate false (healer groups 0-3 only): the pairs on Blaumeux/Zeliek work from the pull, so
    // eligibility is just alive + in order.
    // leaveAt: leave `current` at this many of its marks (HealerLeaveAt staggers a healer group's first turn).
    // uncapped (spec §4.2f): a horseman with no stack cap. A member on him stays, and one whose order holds him
    // joins him whatever his stacks. None = every horseman is capped.
    inline Horse Choose(Order const& order, Horse current, std::array<uint8_t, 4> const& stacks,
                        std::array<bool, 4> const& alive, bool frontGate, uint8_t leaveAt = LEAVE_AT_STACKS,
                        Horse uncapped = Horse::None)
    {
        bool const frontDead = !alive[Idx(Horse::Korthazz)] || !alive[Idx(Horse::Mograine)];
        auto inOrder = [&order](Horse h)
        {
            for (Horse o : order)
                if (o == h)
                    return true;
            return false;
        };
        auto eligible = [&](Horse h)
        {
            if (h == Horse::None || !alive[Idx(h)] || !inOrder(h))
                return false;
            bool const front = h == Horse::Korthazz || h == Horse::Mograine;
            return !frontGate || front || frontDead;
        };

        if (eligible(current) && (current == uncapped || stacks[Idx(current)] < leaveAt))
            return current;
        for (Horse h : order)
            if (h != current && eligible(h) && (h == uncapped || stacks[Idx(h)] == 0))
                return h;
        return Horse::None;
    }

    // Spec §4.2f (pull 11): the surviving front horseman once the other is dead, else None. With doable marks the
    // action passes him to Choose as `uncapped`, so the whole front stays on him (the K <-> M trade has nowhere left
    // to shed his stacks).
    inline Horse LastFront(std::array<bool, 4> const& alive)
    {
        bool const k = alive[Idx(Horse::Korthazz)];
        bool const m = alive[Idx(Horse::Mograine)];
        if (k == m)
            return Horse::None;
        return k ? Horse::Korthazz : Horse::Mograine;
    }

    // Spec §4.3d (pull 5): a healer group's two healers arrive together, so they took their marks in step and crossed
    // together on the 3rd, the same mark the tanks swap on. Seat 0 leaves its first horseman on mark 1 and seat 1 on
    // mark 2; full 3-mark turns after that keep them one mark apart, so one healer of a corner crosses at a time and
    // never on a tank-swap mark.
    inline uint8_t HealerLeaveAt(uint8_t seat, bool firstTurn)
    {
        if (!firstTurn || seat > 1)
            return LEAVE_AT_STACKS;
        return seat + 1;
    }

    // Spec §4.2d (pull 7): both DPS halves left Korth'azz and Mograine on the same (3rd) mark, so his tank stood
    // alone through the ~10 s crossing and an unsplit Meteor killed him. Each half's melee (and, apart, its ranged)
    // are seated alternately: seat 0 leaves its first horseman on mark 2, seat 1 on mark 3, and full 3-mark turns
    // after that keep them one mark apart, so every corner keeps half its melee. Seat 0 never crosses on a tank-swap
    // mark; seat 1 crosses with the tanks, as every DPS did before.
    inline uint8_t DpsLeaveAt(uint8_t seat, bool firstTurn)
    {
        return firstTurn && seat == 0 ? LEAVE_AT_STACKS - 1 : LEAVE_AT_STACKS;
    }

    // A horseman is settled when he is dead, or on his active tank and that tank is not the one about to hand him over
    // (fewer than LEAVE_AT_STACKS marks). Unsettled = mid-swap (the 3-stack tank still has him) or loose.
    inline bool HorseSettled(bool alive, bool hasActiveTank, bool victimIsActive, uint8_t activeStacks)
    {
        return !alive || (hasActiveTank && victimIsActive && activeStacks < LEAVE_AT_STACKS);
    }

    // Spec §4.2a (pull 1): everyone on a horseman takes his marks in step with his tank, so DPS halves and healer groups
    // reach 3 stacks on the same mark as the tanks. Switching then sent the whole raid across the room mid-swap and left
    // the 3-stack tanks unhealed until they died. A member leaving `current` for `next` (Choose's pick) waits until both
    // are settled; a 4th mark is accepted, after that it goes (to the safe spot while `next` is still unsettled).
    // From the safe spot, or with no change, nothing is held.
    inline Horse HoldSwitch(Horse current, Horse next, std::array<uint8_t, 4> const& stacks,
                            std::array<bool, 4> const& settled)
    {
        if (current == next || current == Horse::None)
            return next;
        bool const nextSettled = next == Horse::None || settled[Idx(next)];
        if (settled[Idx(current)] && nextSettled)
            return next;
        if (stacks[Idx(current)] <= LEAVE_AT_STACKS)
            return current;
        return nextSettled ? next : Horse::None;
    }

    // Spec §4.3a (pull 2): a healer group's two healers reach 3 stacks on the same mark, so both left their corner at
    // once and the corner's fresh tank went unhealed for the whole ~70 y crossing (both new front tanks died). A
    // healer leaving a live horseman with a living active tank stays while no other healer working him stands within
    // heal reach of that tank (anotherCovers); a 4th mark is accepted, after that it goes. The first healer of a pair
    // leaves, the second waits for the one arriving from the other corner, and the pair is out of step from then on.
    inline bool HealerMustStay(bool leavingAlive, bool tankAlive, bool anotherCovers, uint8_t myStacks)
    {
        return leavingAlive && tankAlive && !anotherCovers && myStacks <= LEAVE_AT_STACKS;
    }

    // Spec §4.3b (pull 3): while a horseman is not yet parked on his tank spot (the opening drag, or a loose horseman
    // being walked back) his healers escort the tank instead of waiting on the healer spot. In pull 3 every healer
    // ran straight to its spot for the whole opening, never cast, and Mograine's tank died before he was parked.
    inline bool HealerEscorts(float horseToTankSpot) { return horseToTankSpot > PARK_RANGE; }

    // Spec §4.3c (pull 3): a healer may stop where it stands (to cast, or to escort) unless that puts it in the circle
    // of another live horseman whose next mark would be its 4th or later (`dist` = bot to each horseman, 2-D).
    inline bool MarkSafe(Horse h, std::array<float, 4> const& dist, std::array<bool, 4> const& alive,
                         std::array<uint8_t, 4> const& stacks)
    {
        for (uint8_t i = 0; i < 4; ++i)
            if (i != Idx(h) && alive[i] && dist[i] <= MARK_RADIUS + MARK_MARGIN && stacks[i] >= LEAVE_AT_STACKS)
                return false;
        return true;
    }

    // Spec §4.3c: a healer travelling to its spot stops to heal its horseman's active tank when the tank is under
    // TANK_HEAL_PCT and in reach (and line of sight, which the caller checks) and stopping here is MarkSafe. Bot heals
    // refuse any cast-time spell while moving, so a healer that keeps running heals nobody.
    inline bool HealerStopsToHeal(bool tankAlive, float tankHealthPct, float distToTank, bool markSafe)
    {
        return tankAlive && tankHealthPct < TANK_HEAL_PCT && distToTank <= HEAL_REACH && markSafe;
    }

    // ---- Tanks (spec §4.1) ----

    enum class TankDuty : uint8_t
    {
        Hold,
        Ready,
        Relieve,
        Leave,
        Rest
    };

    inline TankDuty TankDutyFor(bool iAmActive, uint8_t myStacks, bool iAmVictim, bool partnerPresent,
                                uint8_t partnerStacks)
    {
        if (iAmActive)
            return (!iAmVictim && myStacks >= LEAVE_AT_STACKS && partnerPresent) ? TankDuty::Leave
                                                                                  : TankDuty::Hold;
        // Still carrying this horseman's marks (they fade 25 s after the last one): never step back in (pull 1 sent a
        // 4-stack resting tank in when the active died).
        if (myStacks >= LEAVE_AT_STACKS)
            return TankDuty::Rest;
        if (!partnerPresent || partnerStacks >= LEAVE_AT_STACKS)
            return TankDuty::Relieve;
        if (partnerStacks >= READY_AT_STACKS)
            return TankDuty::Ready;
        return TankDuty::Rest;
    }

    // Every mark halves the threat of each unit it hits (IP boss_four_horsemen_40 SpellHitTarget), and a resting tank
    // stands outside it, so its old threat outlasts the active tank's and the horseman walks back to it. That is a
    // handover only for a resting tank that may take him; one still carrying LEAVE_AT_STACKS of his marks never does
    // (the active tank taunts him back instead; pull 4 handed Mograine to a 4-stack tank, and the 5th mark killed it).
    inline bool TakesOver(uint8_t restingStacks) { return restingStacks < LEAVE_AT_STACKS; }

    // The relief taunts once it stands on the tank spot beside the active tank, so the horseman turns in place. A
    // taunt from range pulled him toward the room middle, and his marks then reached the safe spot where the resting
    // tanks wait. Off his spot (more than PARK_RANGE) he is moving anyway, and the relief taunts from range.
    inline bool ReliefTauntsNow(float botToSpot, float horseToSpot, float arrive)
    {
        return botToSpot <= arrive || horseToSpot > PARK_RANGE;
    }

    // Replace a live horseman's dead (or empty) active tank: his resting tank when it carries fewer than
    // LEAVE_AT_STACKS of his marks (restingStacks), else a free tank, else nobody until those marks fade. Then fill
    // empty resting slots from freeTanks in Horse order. The caller pushes a dead horseman's tanks onto freeTanks.
    inline void RefillPairs(uint64_t active[4], uint64_t resting[4], std::vector<uint64_t>& freeTanks,
                            std::array<bool, 4> const& alive, std::array<bool, 4> const& activeAlive,
                            std::array<uint8_t, 4> const& restingStacks)
    {
        for (uint8_t h = 0; h < 4; ++h)
        {
            if (!alive[h] || activeAlive[h])
                continue;
            if (resting[h] && restingStacks[h] < LEAVE_AT_STACKS)
            {
                active[h] = resting[h];
                resting[h] = 0;
            }
            else if (!freeTanks.empty())
            {
                active[h] = freeTanks.front();
                freeTanks.erase(freeTanks.begin());
            }
            else
                active[h] = 0;
        }
        for (uint8_t h = 0; h < 4; ++h)
        {
            if (!alive[h] || resting[h] != 0 || freeTanks.empty())
                continue;
            resting[h] = freeTanks.front();
            freeTanks.erase(freeTanks.begin());
        }
    }

    // ---- Positions (spec §4.2) ----

    inline float Dist(Pt a, Pt b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    // Melee stand behind the boss, on the far side from its tank. Zeliek: also >= ZELIEK_TANK_GAP from
    // the tank (Holy Wrath chains 10 y; unused by melee DPS since WithoutZeliek, kept as the rule).
    inline Pt MeleeSpot(Horse h, Pt boss, Pt tank)
    {
        float dx = boss.x - tank.x;
        float dy = boss.y - tank.y;
        float const len = std::hypot(dx, dy);
        if (len < 0.001f)
        {
            dx = 1.0f;  // tank on the boss: any bearing will do
            dy = 0.0f;
        }
        else
        {
            dx /= len;
            dy /= len;
        }
        float d = MELEE_BEHIND;
        if (h == Horse::Zeliek && len + d < ZELIEK_TANK_GAP)  // tank, boss and spot are collinear
            d = ZELIEK_TANK_GAP - len;
        return Pt{boss.x + dx * d, boss.y + dy * d};
    }

    // Rotate a spot around the boss (30 degree steps, alternating sides) until no Void Zone is within
    // VOID_CLEAR of it. No clear rotation: the original spot.
    inline Pt AvoidVoid(Pt spot, Pt boss, std::vector<Pt> const& voids)
    {
        auto clear = [&voids](Pt p)
        {
            for (Pt const& v : voids)
                if (Dist(p, v) < VOID_CLEAR)
                    return false;
            return true;
        };
        if (clear(spot))
            return spot;
        float const rx = spot.x - boss.x;
        float const ry = spot.y - boss.y;
        constexpr float DEG = 3.14159265358979f / 180.0f;
        for (int step = 1; step <= 6; ++step)
        {
            for (int sign = 1; sign >= -1; sign -= 2)
            {
                if (step == 6 && sign < 0)
                    break;  // +180 and -180 are the same spot
                float const a = static_cast<float>(sign * step * 30) * DEG;
                float const c = std::cos(a);
                float const s = std::sin(a);
                Pt const p{boss.x + rx * c - ry * s, boss.y + rx * s + ry * c};
                if (clear(p))
                    return p;
            }
        }
        return spot;
    }

    // Blaumeux's tank point (spec §4.2c): stay on `cur` while it is clear, else step to the clear point nearest the
    // tank. A point is clear with no Void Zone within VOID_CLEAR of it; the current point also needs none within
    // VOID_CLEAR of the tank while he stands on it (she casts the zone on him). None clear: the point farthest from
    // its nearest zone. Pull 6: three blocked points of a 6 y circle left the tank standing in a zone until he died
    // (a zone lasts 90 s and she casts one every 15 s, so up to six are alive).
    inline size_t PickBlauPoint(std::vector<Pt> const& points, size_t cur, Pt tank, bool onCur,
                                std::vector<Pt> const& zones)
    {
        auto nearestZone = [&zones](Pt p)
        {
            float d = 1e9f;
            for (Pt const& z : zones)
                d = std::min(d, Dist(p, z));
            return d;
        };
        if (points.empty())
            return 0;
        if (cur >= points.size())
            cur = 0;
        auto blocked = [&](size_t i)
        {
            return nearestZone(points[i]) < VOID_CLEAR || (i == cur && onCur && nearestZone(tank) < VOID_CLEAR);
        };
        if (!blocked(cur))
            return cur;
        size_t best = points.size();
        for (size_t i = 0; i < points.size(); ++i)
            if (!blocked(i) && (best == points.size() || Dist(tank, points[i]) < Dist(tank, points[best])))
                best = i;
        if (best != points.size())
            return best;
        best = 0;
        for (size_t i = 1; i < points.size(); ++i)
            if (nearestZone(points[i]) > nearestZone(points[best]))
                best = i;
        return best;
    }

    // ---- Threat (spec §4.2) ----

    // Pre-cast veto (multiplier): no new threat action once own threat plus THREAT_HEADROOM passes THREAT_HOLD of the
    // active tank's. No tank threat (0) = no cap.
    inline bool ThreatBlocked(float myThreat, float activeTankThreat)
    {
        return activeTankThreat > 0.0f && myThreat + THREAT_HEADROOM > THREAT_HOLD * activeTankThreat;
    }

    // Hard stop ("naxx40 horsemen threat hold"): a running auto-attack / auto-shot is stopped once own threat passes
    // THREAT_HOLD of the active tank's on the plain ratio, no headroom.
    inline bool ThreatStop(float myThreat, float activeTankThreat)
    {
        return activeTankThreat > 0.0f && myThreat > THREAT_HOLD * activeTankThreat;
    }
}

#endif
