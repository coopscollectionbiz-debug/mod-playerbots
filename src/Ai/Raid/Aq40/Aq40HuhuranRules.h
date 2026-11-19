/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for AQ40 Princess Huhuran (overlay patch 0016). Deliberately free of core and
// playerbots includes so tests/test_aq40_huhuran_rules.cpp builds it standalone.
// Spec: docs/superpowers/specs/2026-10-20-playerbot-aq40-huhuran-design.md (overlay repo).

#ifndef PLAYERBOTS_AQ40HUHURANRULES_H
#define PLAYERBOTS_AQ40HUHURANRULES_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace Aq40HuhuranRules
{
    struct Pt
    {
        float x;
        float y;
    };

    constexpr float PI_F = 3.14159265358979f;

    // Ring radii, both centred on the park spot P (spec §4.3).
    constexpr float RANGED_RADIUS = 17.0f;   // inner ring: ranged DPS bots
    constexpr float HEALER_RADIUS = 32.0f;   // outer ring: healer bots
    // Noxious Poison splash reach (spec §4.3 constraint 1).
    constexpr float SPLASH_REACH = 16.5f;

    // Parking (spec §4.2, rev 2 §7.2 #3). Hysteresis: parked at <= 6 y, re-park once she is beyond 8 y.
    constexpr float PARKED_RADIUS = 6.0f;
    constexpr float REPARK_RADIUS = 8.0f;
    // A bot counts as on its spot within this distance (spec §4.3).
    constexpr float SLOT_TOLERANCE = 1.5f;

    // Opener hold fails open after 45 s (rev 2, spec §7.2 #1).
    constexpr uint32_t OPENER_FAILOPEN_MS = 45000;

    // Rev 3 (spec §8.2 #2): a warrior MT's Last Stand goes off once below this HP % during Berserk.
    constexpr float LAST_STAND_HP_PCT = 50.0f;
    // Rev 3 (spec §8.2 #4): the one-time healer mana refill only within this long of the first Berserk tick seen
    // in the instance (the Nefarian landing-refill window), so a healer revived later is not refilled.
    constexpr uint32_t MANA_REFILL_WINDOW_MS = 5000;

    // spell_huhuran_poison_bolt truncates to the 15 nearest players (spec §1 / §4.6).
    constexpr uint32_t BOLT_TARGETS = 15;
    // [Aq40Huhuran] diag cadence (spec §4.6).
    constexpr uint32_t DIAG_INTERVAL_MS = 5000;

    // Spot `rank` of `count` evenly spaced on the ring (spec §4.3). Requires rank < count.
    inline Pt RingSpot(Pt centre, float radius, uint32_t count, uint32_t rank, float refBearing)
    {
        float const a = refBearing + 2.0f * PI_F * (static_cast<float>(rank) + 0.5f) / static_cast<float>(count);
        return Pt{centre.x + radius * std::cos(a), centre.y + radius * std::sin(a)};
    }

    // How many of `others` lie within `reach` of `victim` (Noxious Poison splash, spec §4.3).
    inline uint32_t PointsWithin(Pt victim, std::vector<Pt> const& others, float reach)
    {
        uint32_t n = 0;
        for (Pt const& p : others)
        {
            float const dx = p.x - victim.x;
            float const dy = p.y - victim.y;
            if (std::sqrt(dx * dx + dy * dy) <= reach)
                ++n;
        }
        return n;
    }

    // Non-tank bots hold fire until she has been parked once this attempt, failing open (rev 2, spec §7.2 #1).
    // The first pull showed why "until her victim is a tank" was not enough: a backpedalling MT builds almost
    // no threat, so full DPS out-threatened it mid-drag and she never reached P.
    inline bool HoldFireOnHuhuran(bool everParked, uint32_t msSinceEngage)
    {
        return !everParked && msSinceEngage < OPENER_FAILOPEN_MS;
    }

    // Threat discipline after the hold (rev 2, spec §7.2 #2), mirroring BWL's BwlThreatBlocked /
    // BWL_THREAT_* (copied, not called). The denominator comes from ThreatDenominator below.
    constexpr float THREAT_LIMIT = 0.80f;       // DPS: veto threat-generating actions at/above this (with headroom)
    constexpr float THREAT_STOP = 0.90f;        // DPS: hard-stop a running auto-attack/auto-shot at/above this
    // Off-tanks (rev 3, spec §8.2 #3): up to the MT's threat, so they out-threaten uncapped heal threat. She only
    // switches victim above 110 % in melee, so 1.05 still never takes her off a living MT.
    constexpr float OFFTANK_THREAT_LIMIT = 1.00f;
    constexpr float OFFTANK_THREAT_STOP = 1.05f;
    // A ratio checked before a cast does not bound that cast's own threat: ~one big level-60 crit.
    constexpr float THREAT_HEADROOM = 1500.0f;

    // Threat order MT > backup > DPS (fix round 1, R11; mirrors BwlNefarianRules::ThreatRatio's role split).
    // The MT is never capped. An off-tank is capped against the MT. Everyone else (DPS) is capped against the
    // top living off-tank (highest threat of a living role tank other than the MT) when it has threat on her,
    // else against the MT - so when the MT dies she turns to an off-tank, not a DPS. 0 = no cap.
    enum class ThreatRole
    {
        MainTank,
        OffTank,
        Other
    };

    // Rev 5 §10.3 "DPS unload in Berserk": once she is berserk, DPS (Other) are capped against the MT's threat
    // with NO headroom (ratios below). By Berserk the MT holds ~90 s of threat, so in practice DPS are uncapped;
    // she switches only above 110 % (melee) / 130 % (range), so she is never pulled off the MT. Off-tanks and
    // healers are unchanged.
    // Fix round 1 (R14): in Berserk the order stays MT > off-tank > DPS, all against the MT with no headroom:
    // off-tanks 1.05 / 1.08, DPS 1.00 / 1.03 - every one under her 110 % melee switch.
    constexpr float BERSERK_DPS_THREAT_LIMIT = 1.00f;
    constexpr float BERSERK_DPS_THREAT_STOP = 1.03f;
    constexpr float BERSERK_OFFTANK_THREAT_LIMIT = 1.05f;
    constexpr float BERSERK_OFFTANK_THREAT_STOP = 1.08f;

    inline float ThreatDenominator(ThreatRole role, float mtThreat, float backupThreat, bool berserk = false)
    {
        if (role == ThreatRole::MainTank)
            return 0.0f;
        if (role == ThreatRole::Other && backupThreat > 0.0f && !berserk)
            return backupThreat;
        return mtThreat > 0.0f ? mtThreat : 0.0f;
    }

    // Per-role ratios: off-tanks 1.00 / 1.05 of the MT (rev 3), everyone else 0.80 / 0.90 of the top off-tank.
    inline float ThreatLimit(ThreatRole role, bool berserk = false)
    {
        if (role == ThreatRole::OffTank)
            return berserk ? BERSERK_OFFTANK_THREAT_LIMIT : OFFTANK_THREAT_LIMIT;
        return (role == ThreatRole::Other && berserk) ? BERSERK_DPS_THREAT_LIMIT : THREAT_LIMIT;
    }
    inline float ThreatStop(ThreatRole role, bool berserk = false)
    {
        if (role == ThreatRole::OffTank)
            return berserk ? BERSERK_OFFTANK_THREAT_STOP : OFFTANK_THREAT_STOP;
        return (role == ThreatRole::Other && berserk) ? BERSERK_DPS_THREAT_STOP : THREAT_STOP;
    }
    // The veto's absolute headroom: none for berserk off-tanks and DPS (§10.3, R14), THREAT_HEADROOM otherwise.
    inline float ThreatHeadroom(ThreatRole role, bool berserk = false)
    {
        return ((role == ThreatRole::Other || role == ThreatRole::OffTank) && berserk) ? 0.0f : THREAT_HEADROOM;
    }

    // The veto gate: own + THREAT_HEADROOM >= limit(role) x denom. No cap when the denominator is 0 (and the MT,
    // whose denominator is always 0, is never capped).
    inline bool ThreatVetoed(ThreatRole role, float own, float denom, bool berserk = false)
    {
        return denom > 0.0f && own + ThreatHeadroom(role, berserk) >= ThreatLimit(role, berserk) * denom;
    }

    // The hard stop, on the plain ratio (no headroom). No cap when the denominator is 0.
    inline bool ThreatHardStop(ThreatRole role, float own, float denom, bool berserk = false)
    {
        return denom > 0.0f && own >= ThreatStop(role, berserk) * denom;
    }

    // Off-tanks stack on the MT's side (rev 2, spec §7.2 #3): her position + (her combat reach + 1.5 y) along
    // her -> MT, rotated +20, -20, +40, -40 ... deg by off-tank rank. When the MT dies she turns to a body
    // standing where the MT stood, so she barely moves off P.
    constexpr float OFFTANK_REACH_EXTRA = 1.5f;
    constexpr float OFFTANK_BEARING_STEP = 20.0f * PI_F / 180.0f;
    // An off-tank counts as on its spot within this distance.
    constexpr float OFFTANK_SPOT_TOLERANCE = 2.0f;

    inline float OffTankBearingOffset(uint32_t rank)
    {
        float const magnitude = static_cast<float>(rank / 2 + 1) * OFFTANK_BEARING_STEP;
        return (rank % 2 == 0) ? magnitude : -magnitude;
    }

    inline Pt OffTankSpot(Pt boss, Pt mt, float bossCombatReach, uint32_t rank)
    {
        float const dx = mt.x - boss.x;
        float const dy = mt.y - boss.y;
        // The MT on top of her has no bearing: use 0 rather than atan2(0, 0)'s platform-defined sign games.
        float const base = (dx * dx + dy * dy) > 1e-6f ? std::atan2(dy, dx) : 0.0f;
        float const a = base + OffTankBearingOffset(rank);
        float const r = bossCombatReach + OFFTANK_REACH_EXTRA;
        return Pt{boss.x + r * std::cos(a), boss.y + r * std::sin(a)};
    }

    // Healer tank priority in Berserk (rev 2, spec §7.2 #4): ceil(n / 2) of the n living healer bots are tank
    // healers - the ones NEAREST the MT (fix round 1, R12). The first ceil(n/2) by ring rank was a contiguous
    // arc that could sit on the far side of the ring, beyond 40 y of the MT.
    inline uint32_t TankHealerCount(uint32_t healers) { return (healers + 1) / 2; }
    inline bool IsTankHealer(uint32_t rank, uint32_t healers) { return rank < TankHealerCount(healers); }

    struct HealerDist
    {
        float distToMt;  // 2-D
        uint64_t guid;   // tie-break
    };
    // `self` is a tank healer: among the TankHealerCount(n) nearest the MT, ties by GUID. False when absent.
    inline bool IsNearestTankHealer(std::vector<HealerDist> healers, uint64_t self)
    {
        std::sort(healers.begin(), healers.end(), [](HealerDist const& a, HealerDist const& b)
                  { return a.distToMt != b.distToMt ? a.distToMt < b.distToMt : a.guid < b.guid; });
        uint32_t const n = static_cast<uint32_t>(healers.size());
        for (uint32_t i = 0; i < n; ++i)
            if (healers[i].guid == self)
                return IsTankHealer(i, n);
        return false;
    }

    // A tank healer heals the MT when it is under TANK_HEAL_PCT, else the lowest-HP% role tank under it.
    constexpr float TANK_HEAL_PCT = 90.0f;
    struct TankHp
    {
        bool isMt;
        float hpPct;
    };
    // Index into `tanks` of the tank to heal, or -1 for none.
    inline int PickTankHealTarget(std::vector<TankHp> const& tanks)
    {
        for (size_t i = 0; i < tanks.size(); ++i)
            if (tanks[i].isMt && tanks[i].hpPct < TANK_HEAL_PCT)
                return static_cast<int>(i);
        int best = -1;
        for (size_t i = 0; i < tanks.size(); ++i)
            if (tanks[i].hpPct < TANK_HEAL_PCT && (best < 0 || tanks[i].hpPct < tanks[static_cast<size_t>(best)].hpPct))
                best = static_cast<int>(i);
        return best;
    }

    // Ring spots latch at engage (rev 4, spec §9.2): true from the first view of the attempt, wherever she is.
    // Supersedes the 8 y / 30 s-within-20 y rules (§4.3, R8): pull 3's ranged and healers followed the dragging
    // MT in a clump and one Noxious Poison hit 35 (8 healers). The MT now drags her into already-formed rings;
    // healers are out of the MT's heal range for part of the drag (~15-20 s), an accepted cost. Kept as a rule
    // (rather than a bare `true` at the call site) so the latch point stays documented and tested.
    inline bool SpotsLatch(bool /*alreadyLatched*/) { return true; }

    // Parked hysteresis (spec §4.2): stay parked to REPARK_RADIUS, become parked at PARKED_RADIUS.
    inline bool UpdateParked(bool wasParked, float bossDistToPark)
    {
        return wasParked ? bossDistToPark <= REPARK_RADIUS : bossDistToPark <= PARKED_RADIUS;
    }

    // Healers among the `n` nearest bodies to her: the predicted Poison Bolt set (spec §4.6).
    inline uint32_t HealersAmongNearest(std::vector<std::pair<float, bool>> distIsHealer, uint32_t n)
    {
        std::sort(distIsHealer.begin(), distIsHealer.end(),
                  [](std::pair<float, bool> const& a, std::pair<float, bool> const& b) { return a.first < b.first; });
        uint32_t healers = 0;
        for (size_t i = 0; i < distIsHealer.size() && i < n; ++i)
            if (distIsHealer[i].second)
                ++healers;
        return healers;
    }
}

#endif
