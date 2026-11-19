/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Naxx40 Loatheb (Individual Progression's level-60 Naxxramas). Deliberately
// free of core and playerbots includes so tests/test_naxx40_loatheb_rules.cpp (overlay repo) builds
// it standalone:
//   g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/nx40lb tests/test_naxx40_loatheb_rules.cpp && /tmp/nx40lb
// Spec: docs/superpowers/specs/2026-10-30-playerbot-naxx40-loatheb-design.md (overlay repo).

#ifndef PLAYERBOTS_NAXX40LOATHEBRULES_H
#define PLAYERBOTS_NAXX40LOATHEBRULES_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace Naxx40LoathebRules
{
    constexpr uint32_t LOATHEB_ENTRY = 351020;
    constexpr uint32_t SPORE_ENTRY = 351066;
    constexpr uint32_t BOSS_STATE_ID = 5;
    constexpr uint32_t DOOM_SPELL = 29204;

    // Corrupted Mind primer / lockout spells, indexed by healer class family (priest-druid / paladin / shaman).
    constexpr uint32_t CM_PRIMER[3] = {29194, 29196, 29198};
    constexpr uint32_t CM_LOCKOUT[3] = {29195, 29197, 29199};
    constexpr uint32_t HEAL_LOCKOUT_MS = 20000;

    constexpr uint32_t POTION_ITEM = 13459;  // Greater Shadow Protection Potion
    constexpr uint32_t SHADOW_ABSORB_SPELL = 17548;
    constexpr uint32_t HEALTHSTONE_ITEMS[3] = {9421, 19012, 19013};
    constexpr uint32_t BANDAGE_ITEM = 14530;
    constexpr uint32_t RECENTLY_BANDAGED_SPELL = 11196;

    constexpr float POISON_SHOCK_RADIUS = 5.0f;
    constexpr float BANDAGE_CLEAR = 8.0f;
    constexpr float FUNGAL_RADIUS = 10.0f;
    // Spec §4.9 item 3 fallback: a spore alive SPORE_STRAY_MS that never reached the ranged stack is attacked by a DPS
    // bot within this of it (2-D, centre to centre).
    constexpr float SPORE_ENGAGE = 8.0f;
    // Spec §4.9 item 3: a spore is attacked once it is within this of RANGE_SPOT (2-D), so every ranged bot held there
    // is inside its FUNGAL_RADIUS when it dies.
    constexpr float SPORE_STACK_RADIUS = 4.0f;
    constexpr uint32_t SPORE_STRAY_MS = 20000;
    constexpr float SPORE_TANK_EXCLUDE = 12.0f;
    constexpr float SPORE_CHOOSER_VETO = 1.5f;  // upstream's spore chooser picks a spore within 1 y (2D surface)

    // Spec §4.8 item 2: the tank rotation is exactly this many bot healers (one heal every ~3.3 s).
    constexpr size_t ROTATION_SIZE = 6;
    constexpr uint32_t ROTATION_MIN_SPACING_MS = 2000;
    constexpr float HEALER_DPS_MANA_PCT = 30.0f;
    constexpr float MANA_REFILL_BELOW_PCT = 25.0f;
    constexpr uint32_t STOP_BEFORE_MS = 2000;
    constexpr uint32_t AFTER_DOOM_WINDOW_MS = 8000;
    constexpr float HEALTHSTONE_BELOW_PCT = 40.0f;
    constexpr float BANDAGE_BELOW_PCT = 80.0f;
    // Spec §4.9 item 2 (was 30): below this any free healer may heal the main tank, and the rotation slot that is up
    // casts without waiting out the spacing.
    constexpr float EMERGENCY_TANK_PCT = 50.0f;
    // Spec §4.9 item 1: below this the first raid heal of an after-Doom window goes to the main tank.
    constexpr float MT_TOPUP_BELOW_PCT = 60.0f;
    constexpr float TANK_FULL_PCT = 95.0f;
    constexpr uint32_t JUST_HEALED_MS = 3000;
    constexpr float SHIELD_WALL_PCT = 40.0f;
    constexpr float LAST_STAND_PCT = 30.0f;
    constexpr float HEAL_REACH = 36.0f;
    // Prayer of Healing's party radius around the caster: every player rank (596, 996, 10960, 10961, 25316, 25308,
    // 48072) has EffectRadiusIndex 10 = 30 y in the server's 3.3.5 Spell.dbc / SpellRadius.dbc.
    constexpr float PRAYER_RADIUS = 30.0f;
    // Upstream's Loatheb ranged spot (LoathebBossHelper::rangePos, NaxxBossHelper.h), inside the room: bandage
    // step-outs head toward it (spec §4.7 item 2), and spores are killed once they reach it (spec §4.9 item 3).
    constexpr float RANGE_SPOT_X = 2896.96f;
    constexpr float RANGE_SPOT_Y = -3980.61f;
    // Spec §3: Inevitable Doom comes every 15 s instead of 30 s once this many have resolved.
    constexpr uint8_t DOOM_FAST_FROM = 6;
    constexpr uint32_t DIAG_INTERVAL_MS = 5000;
    constexpr uint32_t REFRESH_INTERVAL_MS = 500;

    // ---- Healer assignment (spec §4.2) ----

    enum class HealerClass : uint8_t
    {
        Paladin,
        Druid,
        Priest,
        Shaman
    };

    struct HealerMember
    {
        uint64_t guid;
        HealerClass cls;
        uint8_t subgroup;
        bool human;
    };

    struct HealerPlan
    {
        std::vector<uint64_t> rotation;
        // The raid healers (spec §4.7 item 3; the field keeps its pre-pull-1 "group healer" name): every bot healer
        // outside the rotation. They heal the raid right after each Doom (ChooseRaidHeal).
        std::vector<uint64_t> group;
    };

    // Spec §4.7 item 3 / §4.8 item 2. Humans are never assigned. The rotation is exactly the first ROTATION_SIZE bot
    // healers ordered paladins, druids, priests, shamans (member order within a class), or every bot healer if fewer;
    // every other bot healer is a raid healer. subgroup is not used.
    inline HealerPlan AssignHealers(std::vector<HealerMember> members)
    {
        HealerPlan plan;
        for (HealerClass cls : {HealerClass::Paladin, HealerClass::Druid, HealerClass::Priest, HealerClass::Shaman})
        {
            for (HealerMember const& m : members)
            {
                if (m.human || m.cls != cls)
                    continue;
                if (plan.rotation.size() < ROTATION_SIZE)
                    plan.rotation.push_back(m.guid);
                else
                    plan.group.push_back(m.guid);
            }
        }
        return plan;
    }

    // ---- Rotation (spec §4.2) ----

    inline uint32_t RotationSpacingMs(size_t rotationSize)
    {
        if (rotationSize == 0)
            return HEAL_LOCKOUT_MS;
        return std::max<uint32_t>(HEAL_LOCKOUT_MS / static_cast<uint32_t>(rotationSize), ROTATION_MIN_SPACING_MS);
    }

    // Below EMERGENCY_TANK_PCT the slot that is up skips the spacing, but never casts within ROTATION_MIN_SPACING_MS of
    // the last tank heal: without that floor every rotation healer would start its heal within about a second and the
    // whole rotation would then sit locked out for ~20 s.
    inline bool RotationCastNow(uint32_t sinceLastTankHealMs, uint32_t spacingMs, float tankPct)
    {
        if (tankPct < EMERGENCY_TANK_PCT && sinceLastTankHealMs >= ROTATION_MIN_SPACING_MS)
            return true;
        if (sinceLastTankHealMs >= spacingMs && tankPct < TANK_FULL_PCT)
            return true;
        return sinceLastTankHealMs >= 2 * spacingMs;
    }

    // The slot that may cast; SIZE_MAX = nobody. Within the spacing the next slot is up whatever its readiness (the
    // caller casts only if that bot is ready and RotationCastNow holds); once the spacing has passed it is the first
    // READY slot after lastCaster, cyclic, lastCaster itself last, so a dead/locked healer costs at most one spacing.
    inline size_t RotationUp(size_t lastCaster, std::vector<bool> const& ready, uint32_t sinceLastTankHealMs,
                             uint32_t spacingMs)
    {
        size_t const n = ready.size();
        if (n == 0)
            return SIZE_MAX;
        if (sinceLastTankHealMs < spacingMs)
            return (lastCaster + 1) % n;
        for (size_t i = 1; i <= n; ++i)
        {
            size_t const idx = (lastCaster + i) % n;
            if (ready[idx])
                return idx;
        }
        return SIZE_MAX;
    }

    // Milliseconds since a healer's own last heal, from its Corrupted Mind lockout aura's remaining time;
    // UINT32_MAX when it has no lockout aura (remaining < 0).
    inline uint32_t MsSinceOwnHeal(int32_t lockoutRemainingMs)
    {
        if (lockoutRemainingMs < 0)
            return UINT32_MAX;
        uint32_t const remaining = static_cast<uint32_t>(lockoutRemainingMs);
        if (remaining >= HEAL_LOCKOUT_MS)
            return 0;
        return HEAL_LOCKOUT_MS - remaining;
    }

    // castTimeMs = the damage spell's cast time (0 for the overall test, before a spell is picked): the AI pauses for
    // the whole cast, so it must end STOP_BEFORE_MS before the heal is due.
    inline bool HealerMayDps(float manaPct, uint32_t msUntilDue, uint32_t castTimeMs = 0)
    {
        return manaPct > HEALER_DPS_MANA_PCT && uint64_t(msUntilDue) > uint64_t(castTimeMs) + STOP_BEFORE_MS;
    }

    // Spec §3: Inevitable Doom every 30 s, every 15 s once the sixth has resolved (doomCount >= DOOM_FAST_FROM).
    inline uint32_t DoomIntervalMs(uint8_t doomCount)
    {
        return doomCount >= DOOM_FAST_FROM ? 15000 : 30000;
    }

    // Spec §4.7 item 5: raid healers stop damage spells once Doom is on its 15 s cycle, to keep mana for healing.
    inline bool RaidHealerMayDps(uint8_t doomCount)
    {
        return doomCount < DOOM_FAST_FROM;
    }

    // ms until the next Doom resolves: the ticking Doom's remaining time; between Dooms lastResolve + DoomIntervalMs
    // (0 once overdue); UINT32_MAX before the first resolve with none ticking.
    inline uint32_t MsToNextDoomResolve(uint32_t msToTickingResolve, uint32_t msSinceDoomResolved, uint8_t doomCount)
    {
        if (msToTickingResolve != UINT32_MAX)
            return msToTickingResolve;
        if (msSinceDoomResolved == UINT32_MAX)
            return UINT32_MAX;
        uint32_t const interval = DoomIntervalMs(doomCount);
        return msSinceDoomResolved >= interval ? 0 : interval - msSinceDoomResolved;
    }

    // ---- Raid healers (spec §4.7 item 4) ----

    enum class RaidHeal : uint8_t
    {
        None,
        Prayer,  // Prayer of Healing on the priest's own party
        Chain,   // Chain Heal on the most injured raid member in reach
        Big      // the class big heal on the most injured raid member in reach
    };

    // Spec §4.9 item 1: the first raid heal of an after-Doom window tops up the main tank (Loatheb's victim, any
    // player) when he is below MT_TOPUP_BELOW_PCT, in reach and sight of the raid healer, and no raid healer has
    // claimed the top-up for this Doom yet. Otherwise he stays out of the raid heals (the rotation covers him).
    inline bool MainTankTopUp(uint32_t msSinceDoomResolved, float tankPct, bool tankInReach, bool topUpClaimed)
    {
        return msSinceDoomResolved <= AFTER_DOOM_WINDOW_MS && tankPct < MT_TOPUP_BELOW_PCT && tankInReach &&
               !topUpClaimed;
    }

    // Right after a Doom (AFTER_DOOM_WINDOW_MS) only. topUp (MainTankTopUp held): the heal goes on the main tank, a
    // shaman's Chain Heal, anyone else's big heal, ahead of Prayer. Otherwise a priest prays when its own party has
    // >= 2 injured in reach and no other priest has claimed that party for this Doom; otherwise (and for paladins and
    // druids) the big heal goes on the most injured raid member in reach (hasRaidTarget). A shaman always chain heals
    // that member.
    inline RaidHeal ChooseRaidHeal(HealerClass cls, uint32_t msSinceDoomResolved, uint8_t injuredInParty,
                                   bool partyClaimed, bool hasRaidTarget, bool topUp = false)
    {
        if (msSinceDoomResolved > AFTER_DOOM_WINDOW_MS)
            return RaidHeal::None;
        if (topUp)
            return cls == HealerClass::Shaman ? RaidHeal::Chain : RaidHeal::Big;
        if (cls == HealerClass::Priest && injuredInParty >= 2 && !partyClaimed)
            return RaidHeal::Prayer;
        if (!hasRaidTarget)
            return RaidHeal::None;
        return cls == HealerClass::Shaman ? RaidHeal::Chain : RaidHeal::Big;
    }

    // ---- Consumables (spec §4.6) ----

    enum class Consumable : uint8_t
    {
        None,
        Potion,
        Healthstone,
        Bandage,
        StepOut
    };

    struct ConsumableInput
    {
        bool hasDoom;
        bool potionReady;
        bool hasShadowAbsorb;
        uint32_t msSinceDoomResolved;
        float healthPct;
        bool healthstoneClaimed;
        bool healthstoneReady;
        bool recentlyBandaged;
        bool bandageReady;
        bool isBossVictim;
        bool isTank;  // spec §4.7 item 1: tanks never bandage (nor step out); potions and healthstones still apply
        // Spec §4.8 item 1: a rotation healer never leaves its spot, so it never bandages (nor steps out) either;
        // potions and healthstones still apply.
        bool isRotationHealer;
        bool isHealer;
        uint32_t msSinceOwnHeal;
        float distToBoss;
    };

    inline Consumable ChooseConsumable(ConsumableInput const& in)
    {
        // The consumable would proc a heal's worth of lockout: a healer past its just-healed window keeps its heal.
        if (in.isHealer && in.msSinceOwnHeal > JUST_HEALED_MS)
            return Consumable::None;
        if (in.hasDoom && in.potionReady && !in.hasShadowAbsorb)
            return Consumable::Potion;
        bool const afterDoom = in.msSinceDoomResolved <= AFTER_DOOM_WINDOW_MS;
        if (afterDoom && in.healthPct < HEALTHSTONE_BELOW_PCT && !in.healthstoneClaimed && in.healthstoneReady)
            return Consumable::Healthstone;
        if (afterDoom && in.healthPct < BANDAGE_BELOW_PCT && in.bandageReady && !in.recentlyBandaged &&
            !in.isBossVictim && !in.isTank && !in.isRotationHealer)
            return in.distToBoss < BANDAGE_CLEAR ? Consumable::StepOut : Consumable::Bandage;
        return Consumable::None;
    }

    struct Point2
    {
        float x;
        float y;
    };

    // Spec §4.7 item 2: where a bandager steps out of Poison Shock. reach (BANDAGE_CLEAR + 1 y plus both units' sizes,
    // as the caller measures) from Loatheb along the line from him to the ranged spot, or the spot itself when he is
    // closer to it than that. A fixed spot, so an off-tank never walks past the tank toward the wall.
    inline Point2 StepOutPoint(float bossX, float bossY, float reach)
    {
        float const dx = RANGE_SPOT_X - bossX;
        float const dy = RANGE_SPOT_Y - bossY;
        float const d = std::sqrt(dx * dx + dy * dy);
        if (d <= reach)
            return Point2{RANGE_SPOT_X, RANGE_SPOT_Y};
        return Point2{bossX + dx / d * reach, bossY + dy / d * reach};
    }

    // ---- Spores (spec §4.9 item 3, superseding §4.8 item 3's engage rule) ----

    // Spec §4.7 item 6: measured to Loatheb's current victim only (any unit), not to every living tank. A spore this
    // close to his victim is never attacked, so the tank never takes Fungal Creep's threat wipe.
    inline bool SporeNearVictim(float sporeToBossVictim)
    {
        return sporeToBossVictim < SPORE_TANK_EXCLUDE;
    }

    // Spec §4.9 item 3: spores walk to their target, so nobody runs to one; they die at the ranged stack. A DPS bot
    // (the caller excludes tanks and healers) newly attacks a live spore once the spore is within SPORE_STACK_RADIUS
    // of RANGE_SPOT (sporeToStack), or, as the fallback for a spore that never gets there, once it has been alive
    // SPORE_STRAY_MS (sporeAgeMs) and is within SPORE_ENGAGE of the bot. Never one within SPORE_TANK_EXCLUDE of
    // Loatheb's victim. engaged = the bot is already on this spore: it also keeps it while the spore is within
    // FUNGAL_RADIUS of the bot (hysteresis, so a spore drifting out of the stack zone does not flip the bot's target;
    // keeping is a superset of engaging, so a bot walking in to a stacked spore never drops it on the way).
    inline bool SporeEngages(float sporeToBot, float sporeToStack, uint32_t sporeAgeMs, float sporeToBossVictim,
                             bool engaged)
    {
        if (SporeNearVictim(sporeToBossVictim))
            return false;
        if (engaged && sporeToBot <= FUNGAL_RADIUS)
            return true;
        if (sporeToStack <= SPORE_STACK_RADIUS)
            return true;
        return sporeAgeMs >= SPORE_STRAY_MS && sporeToBot <= SPORE_ENGAGE;
    }

    // 2-D distance from (x, y) to RANGE_SPOT, for SporeEngages' sporeToStack.
    inline float DistToRangeSpot(float x, float y)
    {
        float const dx = x - RANGE_SPOT_X;
        float const dy = y - RANGE_SPOT_Y;
        return std::sqrt(dx * dx + dy * dy);
    }

    // ---- Tank cooldowns ----

    enum class TankCd : uint8_t
    {
        None,
        ShieldWall,
        LastStand
    };

    inline TankCd ChooseTankCd(float hpPct, bool shieldWallReady, bool lastStandReady)
    {
        if (hpPct < LAST_STAND_PCT && lastStandReady)
            return TankCd::LastStand;
        if (hpPct < SHIELD_WALL_PCT && shieldWallReady)
            return TankCd::ShieldWall;
        return TankCd::None;
    }
}

#endif
