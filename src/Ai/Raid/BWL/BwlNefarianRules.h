/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for BWL Nefarian phases 2 and 3 (overlay patch 0020). Deliberately free of core and
// playerbots includes so tests/test_bwl_nefarian_rules.cpp builds it standalone.
// Spec: docs/superpowers/specs/2026-10-07-playerbot-bwl-nefarian-design.md (overlay repo).

#ifndef PLAYERBOTS_BWLNEFARIANRULES_H
#define PLAYERBOTS_BWLNEFARIANRULES_H

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace BwlNefarianRules
{
    constexpr int NONE = -1;
    constexpr float PI_F = 3.14159265358979f;
    constexpr float DEG = PI_F / 180.0f;

    // Creatures (spec §2)
    constexpr uint32_t NPC_NEFARIAN = 11583;
    constexpr uint32_t NPC_BONE_CONSTRUCT = 14605;
    constexpr uint32_t NPC_CORRUPTED_INFERNAL = 14668;

    // Class calls (spec §2.4) and related auras
    constexpr uint32_t SPELL_CALL_WARRIOR = 23397;   // Berserk: forced Berserker Stance, +30% taken
    constexpr uint32_t SPELL_CALL_DRUID = 23398;     // forced Cat Form
    constexpr uint32_t SPELL_CALL_PRIEST = 23401;    // Corrupted Healing
    constexpr uint32_t SPELL_CALL_MAGE = 23410;      // Wild Magic
    constexpr uint32_t SPELL_CALL_ROGUE = 23414;     // Paralyze (rooted in front)
    constexpr uint32_t SPELL_CALL_PALADIN = 23418;   // Siphon Blessing
    constexpr uint32_t SPELL_CALL_SHAMAN = 23425;    // Corrupted Totems
    constexpr uint32_t SPELL_CALL_WARLOCK = 23427;   // Summon Infernals
    constexpr uint32_t SPELL_IMPROVED_BOP = 23415;   // on Nefarian: physical immunity
    constexpr uint32_t SPELL_WILD_POLYMORPH = 23603;
    constexpr uint32_t SPELL_VEIL_OF_SHADOW = 22687;
    constexpr uint32_t SPELL_WEAKENED_SOUL = 6788;
    constexpr uint32_t SPELL_TREMOR_TOTEM = 8143;    // stock id, reference only: cast by NAME (spec §9 #3)

    // Geometry (spec §2.2, §4.2). Both cones are spell_cone 82 = +-41 degrees (Position::HasInArc halves).
    constexpr float CONE_HALF_DEG = 41.0f;
    constexpr float FRONT_RANGE = 45.0f;            // Shadow Flame 22539
    constexpr float BACK_RANGE = 30.0f;             // Tail Lash 23364 (SPELL_ATTR0_CU_CONE_BACK)
    constexpr float FRONT_AVOID = 50.0f;
    constexpr float BACK_AVOID = 35.0f;
    constexpr float AVOID_MARGIN_DEG = 10.0f;       // escape this far before the cone edge
    constexpr float STATION_MARGIN_DEG = 20.0f;     // authored stations keep this clear of both edges
    constexpr float MELEE_ESCAPE_RADIUS = 6.0f;
    constexpr float RANGED_ESCAPE_RADIUS = 14.0f;
    constexpr float PARKED_RADIUS = 12.0f;          // Nefarian within this of P = parked (after an MT swap he
                                                    // sits ~10 y off P: his reach covers the MT at T)
    constexpr float STATION_ARRIVE = 3.0f;
    constexpr float P3_HP_PCT = 20.0f;              // ScheduleHealthCheckEvent(20) raises the drakonids

    // Threat discipline (spec §4.2, shared with 0020's float ratio)
    constexpr uint32_t MT_HOLD_FAILOPEN_MS = 20000;
    constexpr float THREAT_LIMIT = 0.80f;
    constexpr float THREAT_STOP = 0.90f;

    constexpr float RENEW_BELOW_PCT = 95.0f;        // tank-only Renew is not cast on a full tank

    // Threat-cap denominators on Nefarian (spec §9 #2). The MT is never capped. An off-tank is capped
    // against the MT; everyone else against the backup tank (highest-threat living non-MT tank) when it
    // has threat, else against the MT - so the order stays MT > backup > DPS. 0 = no cap (no tank threat).
    enum class ThreatRole
    {
        MainTank,
        OffTank,
        Other
    };

    inline float ThreatRatio(ThreatRole role, float own, float mtThreat, float backupThreat)
    {
        if (role == ThreatRole::MainTank)
            return 0.0f;
        float denom = mtThreat;
        if (role == ThreatRole::Other && backupThreat > 0.0f)
            denom = backupThreat;
        if (denom <= 0.0f)
            return 0.0f;
        return own / denom;
    }

    struct Pt
    {
        float x;
        float y;
    };

    inline float Dist(Pt a, Pt b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    inline float NormalizeAngle(float a)
    {
        while (a > PI_F)
            a -= 2.0f * PI_F;
        while (a < -PI_F)
            a += 2.0f * PI_F;
        return a;
    }

    // |angle| in degrees, [0, 180], between `facing` and the direction origin -> p.
    inline float RelBearingDeg(Pt origin, float facing, Pt p)
    {
        float const a = std::atan2(p.y - origin.y, p.x - origin.x);
        return std::fabs(NormalizeAngle(a - facing)) / DEG;
    }

    // +1 = p is left of the facing (counter-clockwise), -1 = right.
    inline int SideOf(Pt origin, float facing, Pt p)
    {
        float const a = std::atan2(p.y - origin.y, p.x - origin.x);
        return NormalizeAngle(a - facing) >= 0.0f ? 1 : -1;
    }

    inline bool InFrontCone(Pt nef, float facing, Pt p, float range, float extraDeg)
    {
        return Dist(nef, p) <= range && RelBearingDeg(nef, facing, p) <= CONE_HALF_DEG + extraDeg;
    }

    inline bool InBackCone(Pt nef, float facing, Pt p, float range, float extraDeg)
    {
        return Dist(nef, p) <= range && RelBearingDeg(nef, facing, p) >= 180.0f - (CONE_HALF_DEG + extraDeg);
    }

    // The trigger for the dynamic escape (spec §4.2): either cone, padded by range and angle.
    inline bool InDanger(Pt nef, float facing, Pt p)
    {
        return InFrontCone(nef, facing, p, FRONT_AVOID, AVOID_MARGIN_DEG) ||
               InBackCone(nef, facing, p, BACK_AVOID, AVOID_MARGIN_DEG);
    }

    // An authored station is used only while it is out of both (padded) cones for Nefarian's LIVE
    // position and facing: off P a flank station can fall inside a cone, and a bot would then alternate
    // station <-> escape point.
    inline bool StationUsable(Pt nef, float facing, Pt station)
    {
        return !InDanger(nef, facing, station);
    }

    // True when p's bearing lies in [41 + margin, 139 - margin] degrees off the facing, on either side.
    inline bool InSafeArc(Pt nef, float facing, Pt p, float marginDeg)
    {
        float const b = RelBearingDeg(nef, facing, p);
        return b >= CONE_HALF_DEG + marginDeg - 0.01f && b <= 180.0f - CONE_HALF_DEG - marginDeg + 0.01f;
    }

    // A fixed point straight off Nefarian's side: stable (derived from his position, not the bot's).
    inline Pt EscapePoint(Pt nef, float facing, int side, float radius)
    {
        float const a = facing + static_cast<float>(side) * PI_F / 2.0f;
        return Pt{nef.x + radius * std::cos(a), nef.y + radius * std::sin(a)};
    }

    // "Come in" while Nefarian is not parked (fix round 3, G2): a bot farther than its role radius +
    // APPROACH_SLACK from him, or without LOS, walks in and stops at the role radius.
    constexpr float APPROACH_RADIUS_MELEE = 5.0f;    // melee and off-tanks
    constexpr float APPROACH_RADIUS_RANGED = 15.0f;
    constexpr float APPROACH_RADIUS_HEALER = 12.0f;
    constexpr float APPROACH_SLACK = 8.0f;

    inline bool NeedsApproach(float distToNef, float radius, bool los)
    {
        return !los || distToNef > radius + APPROACH_SLACK;
    }

    // The point on the line Nefarian -> bot at `radius` from him; when that point is inside a (padded)
    // cone, the side escape point at that radius instead (`side`: the bot's own side, see SideOf, so it
    // never walks across his front or tail).
    inline Pt ApproachPoint(Pt nef, float facing, Pt bot, float radius, int side)
    {
        float const d = Dist(nef, bot);
        if (d < 0.01f)
            return EscapePoint(nef, facing, side, radius);
        Pt const p{nef.x + (bot.x - nef.x) * radius / d, nef.y + (bot.y - nef.y) * radius / d};
        if (InDanger(nef, facing, p))
            return EscapePoint(nef, facing, side, radius);
        return p;
    }

    inline int FlankSide(int ordinal)
    {
        return ordinal % 2 == 0 ? 1 : -1;
    }

    struct Slot
    {
        int side;    // +1 / -1, see SideOf
        int index;   // into that side's station array; NONE when perSide == 0
    };

    // North/south alternate by ordinal, then wrap through that side's stations.
    inline Slot SlotFor(int ordinal, int perSide)
    {
        return Slot{FlankSide(ordinal), perSide > 0 ? (ordinal / 2) % perSide : NONE};
    }

    inline bool Parked(Pt nef, Pt park)
    {
        return Dist(nef, park) <= PARKED_RADIUS;
    }

    enum class Phase
    {
        None,
        P2,   // landed
        P3    // <= 20% or any Bone Construct alive
    };

    inline Phase GetPhase(bool landed, float hpPct, bool constructAlive)
    {
        if (!landed)
            return Phase::None;
        if (hpPct <= P3_HP_PCT || constructAlive)
            return Phase::P3;
        return Phase::P2;
    }

    enum class Kind
    {
        Totem,
        Infernal,
        Drakonid,
        Construct,
        Nefarian,
        Other
    };

    inline Kind KindOfEntry(uint32_t entry)
    {
        switch (entry)
        {
            case 14662: case 14663: case 14664: case 14666:   // Fire Nova / Stoneskin / Healing / Windfury
                return Kind::Totem;
            case NPC_CORRUPTED_INFERNAL:
                return Kind::Infernal;
            case 14261: case 14262: case 14263: case 14264: case 14265: case 14302:
                return Kind::Drakonid;
            case NPC_BONE_CONSTRUCT:
                return Kind::Construct;
            case NPC_NEFARIAN:
                return Kind::Nefarian;
            default:
                return Kind::Other;
        }
    }

    struct Cand
    {
        Kind kind;
        float dist;
    };

    // Kill priority (spec §4.3); lower first, NONE = never a DPS target in this state.
    inline int Rank(Phase phase, bool siphonPhysical, Kind kind)
    {
        switch (kind)
        {
            case Kind::Totem:
                return 0;
            case Kind::Infernal:
                return 1;
            case Kind::Drakonid:
                return siphonPhysical ? 2 : NONE;
            case Kind::Construct:
                return phase == Phase::P3 ? 3 : NONE;
            case Kind::Nefarian:
                return 4;
            default:
                return NONE;
        }
    }

    // Index of the best candidate: best rank, then nearest. NONE when nothing qualifies.
    inline int PickTarget(Phase phase, bool siphonPhysical, Cand const* c, int n)
    {
        int best = NONE;
        for (int i = 0; i < n; ++i)
        {
            int const r = Rank(phase, siphonPhysical, c[i].kind);
            if (r == NONE)
                continue;
            if (best == NONE)
            {
                best = i;
                continue;
            }
            int const br = Rank(phase, siphonPhysical, c[best].kind);
            if (r < br || (r == br && c[i].dist < c[best].dist))
                best = i;
        }
        return best;
    }

    // Off-tanks: infernals, then constructs, then leftover drakonids. Never Nefarian, never a totem.
    inline int PickOtTarget(Cand const* c, int n)
    {
        auto rank = [](Kind k)
        {
            return k == Kind::Infernal ? 0 : k == Kind::Construct ? 1 : k == Kind::Drakonid ? 2 : NONE;
        };
        int best = NONE;
        for (int i = 0; i < n; ++i)
        {
            int const r = rank(c[i].kind);
            if (r == NONE)
                continue;
            if (best == NONE || r < rank(c[best].kind) || (r == rank(c[best].kind) && c[i].dist < c[best].dist))
                best = i;
        }
        return best;
    }

    // Non-tanks hold fire on Nefarian until the MT is his victim (fail-open after 20 s), and at/above
    // THREAT_STOP of the top living tank's threat (spec §4.2).
    inline bool HoldFireOnNefarian(bool mtIsVictim, uint32_t msSinceLanded, float ownRatio)
    {
        if (!mtIsVictim && msSinceLanded < MT_HOLD_FAILOPEN_MS)
            return true;
        return ownRatio >= THREAT_STOP;
    }

    struct TankInfo
    {
        bool eligible;       // alive, in 40 y, and the MT or an off-tank currently holding an add
        bool mainTank;
        bool weakenedSoul;
        bool myRenew;        // this priest's Renew is on it
        float hpPct;
    };

    enum class TankHeal
    {
        None,
        Shield,
        Renew
    };

    struct TankHealPick
    {
        int index;
        TankHeal spell;
    };

    // Corrupted Healing (spec §4.5): PW:S, else Renew, on TANKS ONLY - MT first, then off-tanks by HP.
    inline TankHealPick PickTankHeal(TankInfo const* t, int n)
    {
        int order[64];
        int count = 0;
        for (int i = 0; i < n && count < 64; ++i)
            if (t[i].eligible && t[i].mainTank)
                order[count++] = i;
        int const firstOther = count;
        for (int i = 0; i < n && count < 64; ++i)
            if (t[i].eligible && !t[i].mainTank)
                order[count++] = i;
        for (int a = firstOther; a < count; ++a)          // off-tanks lowest HP first
            for (int b = a + 1; b < count; ++b)
                if (t[order[b]].hpPct < t[order[a]].hpPct)
                {
                    int const tmp = order[a];
                    order[a] = order[b];
                    order[b] = tmp;
                }
        for (int k = 0; k < count; ++k)
        {
            TankInfo const& ti = t[order[k]];
            if (!ti.weakenedSoul)
                return TankHealPick{order[k], TankHeal::Shield};
            if (!ti.myRenew && ti.hpPct < RENEW_BELOW_PCT)
                return TankHealPick{order[k], TankHeal::Renew};
        }
        return TankHealPick{NONE, TankHeal::None};
    }

    struct DispelInfo
    {
        bool mainTank;
        bool healer;
        float hpPct;
    };

    // Wild Polymorph dispel order (spec §4.5): MT, then healers, then everyone else; ties lowest HP.
    inline int PickDispel(DispelInfo const* d, int n)
    {
        auto rank = [](DispelInfo const& x) { return x.mainTank ? 0 : x.healer ? 1 : 2; };
        int best = NONE;
        for (int i = 0; i < n; ++i)
            if (best == NONE || rank(d[i]) < rank(d[best]) ||
                (rank(d[i]) == rank(d[best]) && d[i].hpPct < d[best].hpPct))
                best = i;
        return best;
    }

    // Spell-name veto lists, matched against CastSpellAction::getSpell() so self/party/macro variants
    // are all caught (spec §4.5/§4.6).
    inline bool InList(std::string_view s, std::initializer_list<std::string_view> list)
    {
        for (std::string_view v : list)
            if (s == v)
                return true;
        return false;
    }

    // Holy, direct SPELL_EFFECT_HEAL, no periodic-heal aura (SpellEffects.cpp EffectHeal). Renew and
    // Power Word: Shield are deliberately absent.
    inline bool IsCorruptedHealingSpell(std::string_view s)
    {
        return InList(s, {"lesser heal", "heal", "greater heal", "flash heal", "binding heal", "circle of healing",
                          "prayer of healing", "prayer of mending", "penance", "divine hymn", "desperate prayer"});
    }

    inline bool IsCatAbility(std::string_view s)
    {
        return InList(s, {"claw", "rake", "shred", "mangle (cat)", "rip", "ferocious bite", "tiger's fury",
                          "savage roar"});
    }

    inline bool IsStanceSpell(std::string_view s)
    {
        return InList(s, {"battle stance", "defensive stance"});
    }

    inline bool IsHunterShot(std::string_view s)
    {
        return InList(s, {"auto shot", "arcane shot", "aimed shot", "chimera shot", "steady shot", "kill shot",
                          "multi-shot", "concussive shot", "explosive shot", "silencing shot", "volley",
                          "serpent sting", "viper sting", "scorpid sting", "wyvern sting", "scatter shot",
                          "tranquilizing shot", "distracting shot"});
    }

    inline bool IsNonTremorEarthTotem(std::string_view s)
    {
        return InList(s, {"strength of earth totem", "stoneskin totem", "earthbind totem", "stoneclaw totem"});
    }
}

#endif
