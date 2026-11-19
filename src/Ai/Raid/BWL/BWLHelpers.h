/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BWLHELPERS_H
#define PLAYERBOTS_BWLHELPERS_H

#include "Player.h"
#include "PlayerbotAI.h"
#include "BwlWyrmguardRules.h"
#include "BwlNefarianRules.h"

class Creature;
class Unit;

namespace BlackwingLairHelpers
{
    enum class BlackwingLairSpells : uint32
    {
        // General
        SPELL_ONYXIA_SCALE_CLOAK = 22683,

        // Razorgore the Untamed
        SPELL_MINDCONTROL = 19832,

        // Vaelastrasz the Corrupt
        SPELL_BURNING_ADRENALINE = 18173,

        // Chromaggus
        SPELL_BROOD_AFFLICTION_BRONZE = 23170,
        SPELL_HOURGLASS_SAND = 23645,

        // Nefarian
        SPELL_WILD_MAGIC = 23410,
        SPELL_CORRUPTED_HEALING = 23401,

        // Priest talent and glyph
        SPELL_EMPOWERED_RENEW_R1 = 63534,
        SPELL_EMPOWERED_RENEW_R2 = 63542,
        SPELL_EMPOWERED_RENEW_R3 = 63543,
        SPELL_GLYPH_OF_POWER_WORD_SHIELD = 55672
    };

    enum class BlackwingLairGameObjects : uint32
    {
        // General
        GO_SUPPRESSION_DEVICE = 179784,

        // Razorgore the Untamed
        GO_BLACK_DRAGON_EGG = 177807
    };

    enum class BlackwingLairNPCs : uint32
    {
        // Broodlord Lashlayer
        NPC_CORRUPTED_RED_WHELP = 14022,
        NPC_CORRUPTED_GREEN_WHELP = 14023,
        NPC_CORRUPTED_BLUE_WHELP = 14024,
        NPC_CORRUPTED_BRONZE_WHELP = 14025,

        // Nefarian
        NPC_CORRUPTED_FIRE_NOVA_TOTEM = 14662,
        NPC_CORRUPTED_STONESKIN_TOTEM = 14663,
        NPC_CORRUPTED_HEALING_TOTEM = 14664,
        NPC_CORRUPTED_WINDFURY_TOTEM = 14666,

        // Nefarian phase 3
        NPC_BONE_CONSTRUCT = 14605,

        // Trash
        NPC_DEATH_TALON_WYRMGUARD = 12460
    };

    constexpr float BROODLORD_SAFE_DISTANCE = 18.0f;

    bool IsActiveSuppressionDeviceInRange(GameObject const* go, Player const* bot);
    bool AreRazorgoreEggsAlive(PlayerbotAI* botAI);
    bool IsRazorgoreOffTank(Player* bot);
    bool IsNonBABotNearPosition(Player const* bot, Position const& position, float distance);
    bool IsCorruptedWhelp(Unit const* unit);
    bool IsCorruptedTotemEntry(uint32 entry);
    bool IsCorruptedTotem(Unit const* unit);
    Unit* FindCorruptedTotem(PlayerbotAI* botAI);
    bool IsBoneConstruct(Unit const* unit);
    bool HasBoneConstructAttacker(PlayerbotAI* botAI);

    // ---- Death Talon Wyrmguard packs (overlay patch 0020) ----
    constexpr uint32 BWL_MAP_ID = 469;
    constexpr int WYRMGUARD_PACK_COUNT = 2;

    struct WyrmguardPack
    {
        uint32 spawnIds[BwlWyrmguardRules::SLOTS];         // slot order; slot 0 = formation leader
        Position center;                                   // mean of the three spawns
        Position stations[BwlWyrmguardRules::SLOTS];       // authored, probe-verified (spec §4.3a)
    };

    extern WyrmguardPack const WYRMGUARD_PACKS[WYRMGUARD_PACK_COUNT];

    // One snapshot of an ACTIVE pack, as every Wyrmguard trigger/action/multiplier sees it.
    struct WyrmguardPackView
    {
        int packIndex = BwlWyrmguardRules::NONE;
        Creature* guards[BwlWyrmguardRules::SLOTS] = {};   // nullptr = dead or not loaded
        BwlWyrmguardRules::PackState state{};
        BwlWyrmguardRules::Assignment assign{};
        int skull = BwlWyrmguardRules::NONE;               // slot carrying the group's skull
        uint32 msSinceActive = 0;
        uint32 settledMs[BwlWyrmguardRules::SLOTS] = {};   // how long each slot has been continuously settled
    };

    // True (and fills view) when a keyed pack within 120 y of the bot is alive and in combat.
    // Guards are resolved by SPAWN ID, never via "find target"/threat lists (spec §3).
    bool GetActiveWyrmguardPack(Player* bot, WyrmguardPackView& view);
    // out[r] = the group member in tank role r (0 main, 1..3 assist 0..2), dead members included.
    void GetTankRolePlayers(Player* bot, Player* out[BwlWyrmguardRules::ROLES]);
    int GetTankRole(Player* bot);                                    // NONE when not a role tank
    // The skull marker: bot is the lowest-numbered role tank (0..3) that is alive, in world and on the
    // bot's map. Not upstream IsMainTank - that returns the flagged member even when dead, so a
    // dead main tank would leave nobody marking.
    bool IsWyrmguardMarker(Player* bot);
    int SlotOfGuard(WyrmguardPackView const& view, Unit const* unit);  // NONE when not a pack guard
    bool IsWyrmguardMelee(Player* bot);                              // !tank && !ranged
    // The DPS target a non-tank should have: skull (ranged, or released melee), else a safe
    // non-Wyrmguard attacker, else nullptr = hold fire (spec §4.6).
    Unit* GetWyrmguardDpsTarget(Player* bot, PlayerbotAI* botAI, WyrmguardPackView const& view);
    // Spec §4.8a: bot's threat on unit / the highest threat of a living group tank on it, in float
    // (upstream's uint8 "threat" value wraps at 256). No living tank has threat: 0 (no cap).
    float WyrmguardThreatRatio(Player* bot, Unit* unit);
    // DPS threat caps shared by the Wyrmguard packs (spec §4.8a) and the BWL-wide discipline (spec
    // 2026-10-08). The 0.80-0.90 gap is the hysteresis: the stop fires above it, the vetoes lift below it.
    constexpr float BWL_THREAT_LIMIT = 0.80f;   // multiplier vetoes threat actions at/above this
    constexpr float BWL_THREAT_STOP = 0.90f;    // hold fire (stops auto-attack/auto-shot) at/above this
    // ~one big level-60 crit with threat mods: a ratio checked before a cast does not bound the cast's
    // own threat (a moonkin at 79 % lands a Starfire at ~190 % while tank threat is still small).
    constexpr float BWL_THREAT_HEADROOM = 1500.0f;
    // The 0.80 veto gate: bot threat + BWL_THREAT_HEADROOM >= limit x the top living tank's threat on unit.
    // False when no living tank has threat (loose/untanked: no cap). The 0.90 hard stops use the plain
    // ratio - they only stop autos that are already running.
    bool BwlThreatBlocked(Player* bot, Unit* unit, float limit);

    // ---- BWL-wide DPS threat discipline (spec 2026-10-08) ----
    // Every BWL fight without its own threat rules (Chromaggus is taunt-immune: a tank that loses him
    // can only win him back on raw threat).
    bool IsBwlThreatDisciplined(Player* bot);                 // map 469, not a tank, not a healer
    // A living creature in combat that a living group tank has threat on. Otherwise there is no cap:
    // the unit is loose, or it is trash nobody tanks.
    bool IsBwlThreatDisciplineTarget(Player* bot, Unit* unit);
    // The Wyrmguard pack view or the Nefarian view is active: those fights keep their own rules.
    bool BwlFightHasOwnThreatRules(Player* bot);
    // The pack slot a melee bot must step away from right now, or NONE (spec §4.7).
    int WyrmguardStompThreat(Player* bot, WyrmguardPackView const& view);
    // Spec §4.8a healer positioning: the distinct present, living players holding a living pack guard
    // (view.assign.holderRole). Returns the count written to out.
    int GetWyrmguardHolderTanks(Player* bot, WyrmguardPackView const& view, Player* out[BwlWyrmguardRules::SLOTS]);
    // Line of sight from a spot (ground z) to a player, as WorldObject::IsWithinLOSInMap measures it
    // between two players: each end raised by its own collision height.
    bool WyrmguardSpotInLos(Player* bot, float x, float y, float z, Player* target);

    // ---- Ebonroc keep-out zone (spec §4.9) ----
    constexpr uint32 EBONROC_SPAWN_ID = 85780;
    constexpr int EBONROC_PATH_POINTS = 7;
    extern BwlWyrmguardRules::Pt const EBONROC_PATH[EBONROC_PATH_POINTS];
    extern Position const PACK1_STATION_CENTROID;   // spec §4.3a: the step-out heads here
    // On map 469 while Ebonroc (by spawn id) is alive and NOT in combat and any pack-1 guard lives.
    bool EbonrocKeepOutActive(Player* bot);
    float EbonrocPathDistance(Player* bot);         // 2D distance to his patrol polyline

    // ---- Nefarian, phases 2 and 3 (overlay patch 0020, spec 2026-10-07) ----
    // blackwing_lair.h DATA_NEFARIAN. Hardcoded because a module cannot include a script-local header.
    constexpr uint32 BWL_DATA_NEFARIAN = 7;
    constexpr int NEF_MELEE_PER_SIDE = 5;
    constexpr int NEF_RANGED_PER_SIDE = 8;
    constexpr int NEF_HEALER_PER_SIDE = 5;

    // Authored by tools/bwl-nefarian-stations.py (spec §4.2a). [0] = side +1, [1] = side -1
    // (BwlNefarianRules::FlankSide / EscapePoint convention).
    struct NefarianStations
    {
        Position mt;                                  // T: foot of the throne steps
        Position park;                                // P: where Nefarian stands, facing T
        Position melee[2][NEF_MELEE_PER_SIDE];
        Position ranged[2][NEF_RANGED_PER_SIDE];
        Position healer[2][NEF_HEALER_PER_SIDE];
        Position otStaging;
    };
    extern NefarianStations const NEF_STATIONS;

    enum class NefRole
    {
        MainTank,
        OffTank,
        Melee,
        Ranged,
        Healer
    };

    struct NefarianView
    {
        Creature* nefarian = nullptr;
        BwlNefarianRules::Phase phase = BwlNefarianRules::Phase::None;
        Player* mainTank = nullptr;     // his victim if a living group tank, else lowest living tank role
        bool parked = false;            // Nefarian within PARKED_RADIUS of P
        bool siphon = false;            // Improved BoP (23415) on Nefarian
        uint32 msSinceLanded = 0;
    };

    // Gate (spec §4.1): map 469, boss state IN_PROGRESS, Nefarian alive + selectable (landed).
    bool GetNefarianView(Player* bot, NefarianView& view);
    // Nefarian's current victim when it is a living group member with a tank strategy, else the lowest
    // living 0020 tank role. `nefarian` may be null (role rule only).
    Player* GetNefarianMainTank(Player* bot, Creature* nefarian);
    NefRole GetNefRole(Player* bot, NefarianView const& view);
    int NefRoleOrdinal(Player* bot, NefRole role, NefarianView const& view);   // GUID order within the role
    bool IsPhysicalDps(Player* bot);
    bool HasBrokenRangedWeapon(Player* bot);                                   // hunters only
    uint32 FindSpareRangedWeapon(Player* bot);                                 // 0 = none in the bags
    // Spec §9 #1, the MT's order of business each tick: close to melee/LOS (Close), take Nefarian as
    // the target (Attack), back him to T while he is on the MT and in melee (Back), face him at T (Face).
    enum class NefMtStep
    {
        None,
        Close,
        Attack,
        Back,
        Face
    };
    constexpr float NEF_MT_FACE_ARC = static_cast<float>(M_PI) / 6.0f;
    constexpr float NEF_MT_CHASE_SLACK = 5.0f;    // he is on the MT and this close: he walks to it
    constexpr float NEF_MT_APPROACH_DIST = 1.0f;  // MoveTo(WorldObject*) edge distance; melee is 4/3 y
    NefMtStep GetNefMtStep(Player* bot, NefarianView const& view, Unit* currentTarget);
    inline bool NefMtStationHasWork(Player* bot, NefarianView const& view, Unit* currentTarget)
    {
        return GetNefMtStep(bot, view, currentTarget) != NefMtStep::None;
    }
    // Where a non-MT bot should move right now (cone escape, else its station when parked). False = stay.
    bool GetNefPositionTarget(Player* bot, PlayerbotAI* botAI, NefarianView const& view, Position& dest);
    Unit* GetNefDpsTarget(Player* bot, PlayerbotAI* botAI, NefarianView const& view);  // nullptr = hold fire
    Unit* GetNefOffTankTarget(Player* bot, NefarianView const& view);   // an add to hold; nullptr = none
    // Spec §9 #2: bot's threat on Nefarian over its role's denominator (BwlNefarianRules::ThreatRatio):
    // OTs against the MT, everyone else against the backup tank (else the MT). 0 = no cap; the MT is 0.
    float NefThreatRatio(Player* bot, NefarianView const& view);
    // An idle off-tank's Nefarian hold: the DPS rule (MT not yet his victim, fail-open 20 s) at its own
    // MT-denominated ratio.
    bool OffTankHoldsFireOnNefarian(Player* bot, NefarianView const& view);
    inline char const* NefTankHealSpellName(BwlNefarianRules::TankHeal spell)
    {
        return spell == BwlNefarianRules::TankHeal::Shield ? "power word: shield" : "renew";
    }
    Player* GetNefPolymorphDispelTarget(Player* bot, NefarianView const& view);
    // Tank-only PW:S / Renew pick for a priest under Corrupted Healing. False = nothing to cast.
    bool GetNefTankHeal(Player* bot, PlayerbotAI* botAI, NefarianView const& view, Player*& tank,
                        BwlNefarianRules::TankHeal& spell);
    inline BwlNefarianRules::Pt ToNefPt(WorldObject const* o)
    {
        return BwlNefarianRules::Pt{o->GetPositionX(), o->GetPositionY()};
    }
}

#endif
