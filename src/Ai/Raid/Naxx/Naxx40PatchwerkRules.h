/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Naxx40 Patchwerk soaker strategy (Individual Progression's level-60 Naxxramas). Deliberately free
// of core and playerbots includes so tests/test_naxx40_patchwerk_rules.cpp (overlay repo) builds it standalone:
//   g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/nx40pw tests/test_naxx40_patchwerk_rules.cpp && /tmp/nx40pw
// Spec: docs/superpowers/specs/2026-11-01-playerbot-naxx40-patchwerk-design.md (overlay repo).
//
// Roles are derived each refresh: the latched tank set (and its priority, now only a tiebreak) is fixed per attempt, but
// soakers are the two non-MT tanks with the highest LIVE threat (pull 1: a max-health-latched soaker that was never
// struck gained no rage and sat 4th, starving every cap keyed to it). Threat caps always reference a LIVING unit; a
// reference with threat 0 blocks (the opener: DPS wait for the soakers). Under the `raid` cheat a starved soaker or
// spare is lifted to a threat floor (ThreatBoostFor).

#ifndef PLAYERBOTS_NAXX40PATCHWERKRULES_H
#define PLAYERBOTS_NAXX40PATCHWERKRULES_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Naxx40PatchwerkRules
{
    constexpr uint32_t PATCHWERK_ENTRY = 351028;
    constexpr uint32_t BOSS_STATE_ID = 0;  // BOSS_PATCHWERK in naxxramas.h
    constexpr uint32_t SPELL_HATEFUL_STRIKE = 41926;

    constexpr uint32_t ENRAGE_MS = 420000;
    // Non-tanks hold fire until the roles latch, failing open this long after the gate first opens.
    constexpr uint32_t OPENER_FAIL_OPEN_MS = 20000;
    constexpr uint32_t REFRESH_INTERVAL_MS = 500;
    constexpr uint32_t DIAG_INTERVAL_MS = 5000;

    // Pull 2: the MT takes only 100-300 dps, so two of its healers moved to the soakers (8/2).
    constexpr uint32_t SOAKER_HEALERS_MAX = 8;
    constexpr uint32_t MT_HEALERS_MAX = 2;
    constexpr uint32_t MANA_REFILL_BELOW_PCT = 25;
    // A health drop of at least this much between two refreshes counts as a strike-sized hit in the diag.
    constexpr uint32_t STRIKE_DROP_MIN = 4000;

    // Threat caps: veto at `limit` with THREAT_HEADROOM absolute headroom, hard stop of autos at `stop` (no headroom).
    constexpr float THREAT_HEADROOM = 1500.0f;
    constexpr float SOAKER_LIMIT = 1.00f;
    constexpr float SOAKER_STOP = 1.05f;
    constexpr float DPS_LIMIT = 0.80f;
    constexpr float DPS_STOP = 0.90f;

    constexpr float HEAL_RANGE = 35.0f;

    // Last-moment cancel of a soaker healer's pre-cast (pull 2, spec §3.3): mana is only spent at cast completion, so
    // a heal whose target is still at PRECAST_CANCEL_PCT health or more with PRECAST_CANCEL_MS or less to go is
    // interrupted for free instead of landing as overheal.
    constexpr int32_t PRECAST_CANCEL_MS = 300;
    constexpr float PRECAST_CANCEL_PCT = 95.0f;

    inline bool ShouldCancelPrecast(int32_t remainingMs, float targetHealthPct)
    {
        return remainingMs <= PRECAST_CANCEL_MS && targetHealthPct >= PRECAST_CANCEL_PCT;
    }

    // Tank threat floor under the `raid` cheat (pull 1, spec §3.1): a soaker is lifted to SOAKER_FLOOR x the MT's
    // threat, a spare to SPARE_FLOOR x the lower soaker's, whenever the deficit is at least BOOST_MIN_DEFICIT.
    constexpr float SOAKER_FLOOR = 0.80f;
    constexpr float SPARE_FLOOR = 0.90f;
    constexpr float BOOST_MIN_DEFICIT = 1000.0f;

    // One unit on Patchwerk's threat list, in the boss's sorted order (highest first).
    struct ThreatEntry
    {
        uint64_t guid;
        float threat;
        bool inMelee;
    };

    struct StrikePair
    {
        uint64_t a = 0;
        uint64_t b = 0;
    };

    // Hateful Strike's targets as the boss script picks them: the units in melee range, in threat order, skipping the
    // top one (the main tank): the second and third. With fewer, the pair shrinks (b, then a, become 0).
    inline StrikePair StrikeCandidates(std::vector<ThreatEntry> const& sorted)
    {
        std::vector<uint64_t> melee;
        for (ThreatEntry const& e : sorted)
            if (e.inMelee)
                melee.push_back(e.guid);
        StrikePair p;
        if (melee.size() == 1)
            p.a = melee[0];
        else if (melee.size() == 2)
            p.a = melee[1];
        else if (melee.size() >= 3)
        {
            p.a = melee[1];
            p.b = melee[2];
        }
        return p;
    }

    struct TankInfo
    {
        uint64_t guid;
        bool alive;
        float threat;
        uint32_t maxHealth;
        bool inMelee;
    };

    // The latched priority: living tanks only, max health descending, guid ascending on a tie.
    inline std::vector<uint64_t> TankPriority(std::vector<TankInfo> tanks)
    {
        tanks.erase(std::remove_if(tanks.begin(), tanks.end(), [](TankInfo const& t) { return !t.alive; }),
                    tanks.end());
        std::sort(tanks.begin(), tanks.end(),
                  [](TankInfo const& a, TankInfo const& b)
                  { return a.maxHealth != b.maxHealth ? a.maxHealth > b.maxHealth : a.guid < b.guid; });
        std::vector<uint64_t> out;
        out.reserve(tanks.size());
        for (TankInfo const& t : tanks)
            out.push_back(t.guid);
        return out;
    }

    // The live entry of a living latched tank, else null.
    inline TankInfo const* FindLiving(std::vector<TankInfo> const& live, uint64_t guid)
    {
        if (guid == 0)
            return nullptr;
        for (TankInfo const& t : live)
            if (t.guid == guid)
                return t.alive ? &t : nullptr;
        return nullptr;
    }

    struct Roles
    {
        uint64_t mt = 0;
        uint64_t soaker[2] = {0, 0};
        std::vector<uint64_t> spares;
    };

    // MT = Patchwerk's victim if it is a living latched tank, else the living latched tank highest on `live` threat
    // (first in priority order on a tie). Soakers = the two living latched tanks other than the MT with the highest live
    // threat; spares = the rest, threat descending. Ties keep `priority` order (the latch order is only a tiebreak).
    inline Roles DeriveRoles(std::vector<uint64_t> const& priority, std::vector<TankInfo> const& live, uint64_t victim)
    {
        Roles r;
        bool victimIsTank = false;
        for (uint64_t g : priority)
            if (g == victim && FindLiving(live, g))
                victimIsTank = true;
        if (victimIsTank)
            r.mt = victim;
        else
        {
            float best = 0.0f;
            for (uint64_t g : priority)
                if (TankInfo const* t = FindLiving(live, g))
                    if (r.mt == 0 || t->threat > best)
                    {
                        r.mt = g;
                        best = t->threat;
                    }
        }
        std::vector<TankInfo const*> rest;
        for (uint64_t g : priority)
            if (g != r.mt)
                if (TankInfo const* t = FindLiving(live, g))
                    rest.push_back(t);
        std::stable_sort(rest.begin(), rest.end(),
                         [](TankInfo const* a, TankInfo const* b) { return a->threat > b->threat; });
        for (size_t i = 0; i < rest.size(); ++i)
        {
            if (i < 2)
                r.soaker[i] = rest[i]->guid;
            else
                r.spares.push_back(rest[i]->guid);
        }
        return r;
    }

    enum class CapRole : uint8_t
    {
        None,
        Soaker,
        Spare,
        MeleeDps,
        RangedDps
    };

    struct Cap
    {
        bool capped = false;
        float refThreat = 0;
        float limit = 0;
        float stop = 0;
    };

    // The living soaker with the lower threat (self excluded), or null when neither is alive.
    inline TankInfo const* LowerSoaker(uint64_t self, Roles const& roles, std::vector<TankInfo> const& live)
    {
        TankInfo const* best = nullptr;
        for (uint64_t g : roles.soaker)
        {
            if (g == self)
                continue;
            if (TankInfo const* t = FindLiving(live, g))
                if (!best || t->threat < best->threat)
                    best = t;
        }
        return best;
    }

    // The threat to add to `self` under the `raid` cheat: a living soaker is lifted to SOAKER_FLOOR x the MT's threat, a
    // living spare to SPARE_FLOOR x the lower soaker's (self excluded). Returns floor - own when that deficit is at least
    // BOOST_MIN_DEFICIT, else 0; 0 for the MT, a non-tank, a dead tank or a missing reference.
    inline float ThreatBoostFor(uint64_t self, Roles const& roles, std::vector<TankInfo> const& live)
    {
        TankInfo const* me = FindLiving(live, self);
        if (!me || self == roles.mt)
            return 0.0f;
        TankInfo const* ref = nullptr;
        float factor = 0.0f;
        if (self == roles.soaker[0] || self == roles.soaker[1])
        {
            ref = FindLiving(live, roles.mt);
            factor = SOAKER_FLOOR;
        }
        else if (std::find(roles.spares.begin(), roles.spares.end(), self) != roles.spares.end())
        {
            ref = LowerSoaker(self, roles, live);
            factor = SPARE_FLOOR;
        }
        if (!ref)
            return 0.0f;
        float const deficit = factor * ref->threat - me->threat;
        return deficit >= BOOST_MIN_DEFICIT ? deficit : 0.0f;
    }

    // Which unit a role's threat is capped under, per the ruling table. Never references `self` or a dead unit; a
    // role with no living reference is uncapped.
    inline Cap CapFor(CapRole role, uint64_t self, Roles const& roles, std::vector<TankInfo> const& live)
    {
        Cap cap;
        if (role == CapRole::None)
            return cap;

        TankInfo const* mt = roles.mt != self ? FindLiving(live, roles.mt) : nullptr;
        TankInfo const* ref = nullptr;
        float limit = DPS_LIMIT;
        float stop = DPS_STOP;
        switch (role)
        {
            case CapRole::Soaker:
                ref = mt;
                limit = SOAKER_LIMIT;
                stop = SOAKER_STOP;
                break;
            case CapRole::Spare:
            case CapRole::RangedDps:
                ref = LowerSoaker(self, roles, live);
                break;
            case CapRole::MeleeDps:
                for (uint64_t g : roles.spares)
                {
                    if (g == self)
                        continue;
                    TankInfo const* t = FindLiving(live, g);
                    if (t && t->inMelee && t->threat > 0.0f)
                    {
                        ref = t;
                        break;
                    }
                }
                if (!ref)
                    ref = LowerSoaker(self, roles, live);
                break;
            case CapRole::None:
                break;
        }
        if (!ref && role != CapRole::Soaker)
            ref = mt;
        if (!ref)
            return cap;
        cap.capped = true;
        cap.refThreat = ref->threat;
        cap.limit = limit;
        cap.stop = stop;
        return cap;
    }

    // Veto a threat-adding action: the bot's threat plus absolute headroom would reach limit * reference.
    inline bool ThreatVetoed(float own, Cap const& cap)
    {
        return cap.capped && (cap.refThreat <= 0.0f || own + THREAT_HEADROOM >= cap.limit * cap.refThreat);
    }

    // Hard-stop running autos: the plain ratio, no headroom.
    inline bool ThreatStopped(float own, Cap const& cap)
    {
        return cap.capped && (cap.refThreat <= 0.0f || own >= cap.stop * cap.refThreat);
    }

    struct HealerSplit
    {
        std::vector<uint64_t> soaker;
        std::vector<uint64_t> mt;
    };

    // Sorted by guid; the first min(8, ceil(0.6 n)) heal the soakers, the next min(2, rest) the MT, the rest are stock.
    inline HealerSplit SplitHealers(std::vector<uint64_t> healers)
    {
        std::sort(healers.begin(), healers.end());
        healers.erase(std::unique(healers.begin(), healers.end()), healers.end());
        size_t const n = healers.size();
        size_t const soaker = std::min<size_t>(SOAKER_HEALERS_MAX, (3 * n + 4) / 5);
        size_t const mt = std::min<size_t>(MT_HEALERS_MAX, n - soaker);
        HealerSplit s;
        s.soaker.assign(healers.begin(), healers.begin() + soaker);
        s.mt.assign(healers.begin() + soaker, healers.begin() + soaker + mt);
        return s;
    }

    // The strike-pair member a soaker healer should heal: fewer in-flight claims, then lower health %, then `a`.
    // 0 when both are 0; a lone member is picked.
    inline uint64_t PickSoakerTarget(uint64_t a, float aPct, uint32_t aClaims, uint64_t b, float bPct,
                                     uint32_t bClaims)
    {
        if (a == 0)
            return b;
        if (b == 0)
            return a;
        if (aClaims != bClaims)
            return aClaims < bClaims ? a : b;
        return bPct < aPct ? b : a;
    }

    struct DamageSample
    {
        uint32_t dmg = 0;
        uint32_t hits = 0;
    };

    // One refresh's health change of a latched tank: positive drops add to dmg (a lower bound: a heal inside the same
    // sample hides part of a hit); a drop of STRIKE_DROP_MIN or more is a strike-sized hit.
    inline void AddSample(DamageSample& s, uint32_t prevHp, uint32_t curHp)
    {
        if (curHp >= prevHp)
            return;
        uint32_t const drop = prevHp - curHp;
        s.dmg += drop;
        if (drop >= STRIKE_DROP_MIN)
            ++s.hits;
    }
}

#endif
