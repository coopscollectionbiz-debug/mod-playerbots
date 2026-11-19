/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDBOSSHELPER_H
#define PLAYERBOTS_ULDBOSSHELPER_H

#include "AiObject.h"
#include "AiObjectContext.h"
#include "EventMap.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ScriptedCreature.h"
#include "UldGeometry.h"
#include <string>
#include <unordered_map>
#include <vector>

constexpr uint32 ULDUAR_MAP_ID = 603;

enum UlduarIDs
{
    // Iron Assembly
    SPELL_LIGHTNING_TENDRILS_10_MAN = 61887,
    SPELL_LIGHTNING_TENDRILS_25_MAN = 63486,
    SPELL_OVERLOAD_10_MAN = 61869,
    SPELL_OVERLOAD_25_MAN = 63481,
    SPELL_OVERLOAD_10_MAN_2 = 63485,
    SPELL_OVERLOAD_25_MAN_2 = 61886,
    SPELL_RUNE_OF_POWER = 64320,

    // Kologarn
    NPC_RIGHT_ARM = 32934,
    NPC_LEFT_ARM = 32933,
    NPC_RUBBLE = 33768,
    NPC_EYE_LEFT = 33632,
    NPC_EYE_RIGHT = 33802,
    SPELL_CRUNCH_ARMOR = 64002,
    SPELL_CRUNCH_ARMOR_10 = 63355,

    SPELL_FOCUSED_EYEBEAM_10_2 = 63346,
    SPELL_FOCUSED_EYEBEAM_10 = 63347,
    SPELL_FOCUSED_EYEBEAM_25_2 = 63976,
    SPELL_FOCUSED_EYEBEAM_25 = 63977,

    // Hodir
    NPC_SNOWPACKED_ICICLE = 33174,
    NPC_SNOWPACKED_ICICLE_FALLING = 33173,
    NPC_TOASTY_FIRE = 33342,
    SPELL_FLASH_FREEZE = 61968,
    SPELL_BITING_COLD_PLAYER_AURA = 62039,

    // Freya
    NPC_SNAPLASHER = 32916,
    NPC_STORM_LASHER = 32919,
    NPC_DETONATING_LASHER = 32918,
    NPC_ANCIENT_WATER_SPIRIT = 33202,
    NPC_ANCIENT_CONSERVATOR = 33203,
    NPC_HEALTHY_SPORE = 33215,
    NPC_EONARS_GIFT = 33228,
    GOBJECT_NATURE_BOMB = 194902,

    // Thorim
    NPC_DARK_RUNE_ACOLYTE_I = 32886,
    NPC_CAPTURED_MERCENARY_SOLDIER_ALLY = 32885,
    NPC_CAPTURED_MERCENARY_SOLDIER_HORDE = 32883,
    NPC_CAPTURED_MERCENARY_CAPTAIN_ALLY = 32908,
    NPC_CAPTURED_MERCENARY_CAPTAIN_HORDE = 32907,
    NPC_JORMUNGAR_BEHEMOT = 32882,
    NPC_DARK_RUNE_WARBRINGER = 32877,
    NPC_DARK_RUNE_EVOKER = 32878,
    NPC_DARK_RUNE_CHAMPION = 32876,
    NPC_DARK_RUNE_COMMONER = 32904,
    NPC_IRON_RING_GUARD = 32874,
    NPC_RUNIC_COLOSSUS = 32872,
    NPC_ANCIENT_RUNE_GIANT = 32873,
    NPC_DARK_RUNE_ACOLYTE_G = 33110,
    NPC_IRON_HONOR_GUARD = 32875,
    SPELL_UNBALANCING_STRIKE = 62130,

    // Mimiron
    NPC_LEVIATHAN_MKII = 33432,
    NPC_VX001 = 33651,
    NPC_AERIAL_COMMAND_UNIT = 33670,
    NPC_BOMB_BOT = 33836,
    NPC_ROCKET_STRIKE_N = 34047,
    NPC_ASSAULT_BOT = 34057,
    NPC_PROXIMITY_MINE = 34362,
    SPELL_P3WX2_LASER_BARRAGE_1 = 63293,
    SPELL_P3WX2_LASER_BARRAGE_2 = 63297,
    SPELL_SPINNING_UP = 63414,
    SPELL_SHOCK_BLAST = 63631,
    SPELL_P3WX2_LASER_BARRAGE_3 = 64042,
    SPELL_P3WX2_LASER_BARRAGE_AURA_1 = 63274,
    SPELL_P3WX2_LASER_BARRAGE_AURA_2 = 63300,

    // General Vezax
    SPELL_MARK_OF_THE_FACELESS = 63276,
    SPELL_VEZAX_SHADOW_CRASH = 63277,

    // Yogg-Saron
    ACTION_ILLUSION_DRAGONS = 1,
    ACTION_ILLUSION_ICECROWN = 2,
    ACTION_ILLUSION_STORMWIND = 3,
    NPC_GUARDIAN_OF_YS = 33136,
    NPC_YOGG_SARON = 33288,
    NPC_OMINOUS_CLOUD = 33292,
    NPC_RUBY_CONSORT = 33716,
    NPC_AZURE_CONSORT = 33717,
    NPC_BRONZE_CONSORT = 33718,
    NPC_EMERALD_CONSORT = 33719,
    NPC_OBSIDIAN_CONSORT = 33720,
    NPC_ALEXTRASZA = 33536,
    NPC_MALYGOS_ILLUSION = 33535,
    NPC_NELTHARION = 33523,
    NPC_YSERA = 33495,
    GO_DRAGON_SOUL = 194462,
    NPC_SARA_PHASE_1 = 33134,
    NPC_LICH_KING_ILLUSION = 33441,
    NPC_IMMOLATED_CHAMPION = 33442,
    NPC_SUIT_OF_ARMOR = 33433,
    NPC_GARONA = 33436,
    NPC_KING_LLANE = 33437,
    NPC_DEATHSWORN_ZEALOT = 33567,
    NPC_INFLUENCE_TENTACLE = 33943,
    NPC_DEATH_ORB = 33882,
    NPC_BRAIN = 33890,
    NPC_CRUSHER_TENTACLE = 33966,
    NPC_CONSTRICTOR_TENTACLE = 33983,
    NPC_CORRUPTOR_TENTACLE = 33985,
    NPC_IMMORTAL_GUARDIAN = 33988,
    NPC_LAUGHING_SKULL = 33990,
    NPC_SANITY_WELL = 33991,
    NPC_DESCEND_INTO_MADNESS = 34072,
    NPC_MARKED_IMMORTAL_GUARDIAN = 36064,
    SPELL_SANITY = 63050,
    SPELL_BRAIN_LINK = 63802,
    SPELL_MALADY_OF_THE_MIND = 63830,
    SPELL_SHADOW_BARRIER = 63894,
    SPELL_TELEPORT_TO_CHAMBER = 63997,
    SPELL_TELEPORT_TO_ICECROWN = 63998,
    SPELL_TELEPORT_TO_STORMWIND = 63989,
    SPELL_TELEPORT_BACK = 63992,
    SPELL_CANCEL_ILLUSION_AURA = 63993,
    SPELL_INDUCE_MADNESS = 64059,
    SPELL_LUNATIC_GAZE_YS = 64163,
    GO_FLEE_TO_THE_SURFACE_PORTAL = 194625,

    // Buffs
    SPELL_FROST_TRAP = 13809
};

constexpr float ULDUAR_KOLOGARN_AXIS_Z_PATHING_ISSUE_DETECT = 420.0f;
constexpr float ULDUAR_KOLOGARN_EYEBEAM_RADIUS = 3.0f;
// The beam's target keeps running while the beam is within this range; ours.
constexpr float ULDUAR_KOLOGARN_EYEBEAM_KITE_DISTANCE = 10.0f;

// Focused Eyebeam — geometric beam-line vacate. All distances in yards; tune DANGER_WIDTH at runtime.
constexpr float ULDUAR_KOLOGARN_EYEBEAM_DANGER_WIDTH = 6.0f;    // half-width of the lethal beam corridor
constexpr float ULDUAR_KOLOGARN_EYEBEAM_MARGIN = 3.0f;          // extra standoff past the corridor when vacating
constexpr float ULDUAR_KOLOGARN_EYEBEAM_BOSS_STANDOFF = 10.0f;  // ignore the beam within this range of the boss (origin) so melee don't thrash
constexpr float ULDUAR_KOLOGARN_EYEBEAM_SEARCH_RADIUS = 80.0f;  // platform-sized eye search (well under the 533y grid clamp)
constexpr float ULDUAR_KOLOGARN_ARM_HOLD_PCT = 20.0f;  // hold the right arm at/below this HP% while the left arm is down (avoids double-arm Stone Shout)

// Pit instakill rectangle from boss_kologarn_pit_kill_bunny (x/y footprint of the chasm — never move here).
constexpr float ULDUAR_KOLOGARN_PIT_MIN_X = 1782.0f;
constexpr float ULDUAR_KOLOGARN_PIT_MAX_X = 1832.0f;
constexpr float ULDUAR_KOLOGARN_PIT_MIN_Y = -56.0f;
constexpr float ULDUAR_KOLOGARN_PIT_MAX_Y = 8.0f;

constexpr float ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD = 429.6094f;
constexpr float ULDUAR_THORIM_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;
constexpr float ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;
constexpr float ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 300.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 200.0f;
constexpr float ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS = 50.0f;

extern const Position ULDUAR_THORIM_NEAR_ARENA_CENTER;
extern const Position ULDUAR_THORIM_NEAR_ENTRANCE_POSITION;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3;
extern const Position ULDUAR_THORIM_JUMP_END_POINT;
extern const Position ULDUAR_THORIM_PHASE2_TANK_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE1_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE2_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE3_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE1RANGE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE1MELEE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE2RANGE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE2MELEE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE3RANGE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE3MELEE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT;
extern const Position ULDUAR_VEZAX_MARK_OF_THE_FACELESS_SPOT;
extern const Position ULDUAR_YOGG_SARON_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT;

// True if the given x/y is over the Kologarn chasm footprint — a vacate destination here is a death.
inline bool IsOverKologarnPit(float x, float y)
{
    return UldPointInRect2D(x, y, ULDUAR_KOLOGARN_PIT_MIN_X, ULDUAR_KOLOGARN_PIT_MAX_X,
                            ULDUAR_KOLOGARN_PIT_MIN_Y, ULDUAR_KOLOGARN_PIT_MAX_Y);
}

// Collect the currently-active Focused Eyebeam eyes near the bot (nearest of each variant).
// One eye is active per cast; two entries cover a rare left+right overlap.
inline void KologarnCollectEyes(Player* bot, std::vector<Unit*>& eyes)
{
    if (Creature* e = bot->FindNearestCreature(NPC_EYE_LEFT, ULDUAR_KOLOGARN_EYEBEAM_SEARCH_RADIUS, true))
        eyes.push_back(e);
    if (Creature* e = bot->FindNearestCreature(NPC_EYE_RIGHT, ULDUAR_KOLOGARN_EYEBEAM_SEARCH_RADIUS, true))
        eyes.push_back(e);
}

// Resolve a live Kologarn arm by NPC entry (robust; independent of any bot's threat list).
inline Unit* KologarnFindArm(Player* bot, uint32 entry)
{
    return bot->FindNearestCreature(entry, 100.0f, true);
}

// True when finishing the right arm now would risk both arms dead at once (Stone Shout): the right
// arm is low AND the left arm is dead or also low. Bots DPS the boss body instead until the left is back.
inline bool KologarnShouldHoldRightArm(Player* bot)
{
    Unit* rightArm = KologarnFindArm(bot, NPC_RIGHT_ARM);
    if (!rightArm || rightArm->GetHealthPct() >= ULDUAR_KOLOGARN_ARM_HOLD_PCT)
        return false;

    Unit* leftArm = KologarnFindArm(bot, NPC_LEFT_ARM);
    return !leftArm || leftArm->GetHealthPct() < ULDUAR_KOLOGARN_ARM_HOLD_PCT;
}

class RazorscaleBossHelper : public AiObject
{
public:
    // Enums and constants specific to Razorscale
    enum RazorscaleUnits : uint32
    {
        UNIT_RAZORSCALE          = 33186,
        UNIT_DARK_RUNE_SENTINEL  = 33846,
        UNIT_DARK_RUNE_WATCHER   = 33453,
        UNIT_DARK_RUNE_GUARDIAN  = 33388,
        UNIT_DEVOURING_FLAME     = 34188,
    };

    enum RazorscaleGameObjects : uint32
    {
        GO_RAZORSCALE_HARPOON_1 = 194519,
        GO_RAZORSCALE_HARPOON_2 = 194541,
        GO_RAZORSCALE_HARPOON_3 = 194542,
        GO_RAZORSCALE_HARPOON_4 = 194543,
    };

    enum RazorscaleSpells : uint32
    {
        SPELL_CHAIN_1           = 49679,
        SPELL_CHAIN_2           = 49682,
        SPELL_CHAIN_3           = 49683,
        SPELL_CHAIN_4           = 49684,
        SPELL_SENTINEL_WHIRLWIND = 63806,
        SPELL_STUN_AURA         = 62794,
        // The core's current Fuse Armor; it stacks to 5, then turns into the Fused Armor stun.
        SPELL_FUSEARMOR         = 64821
    };

    static constexpr uint32 FUSEARMOR_THRESHOLD = 2;

    // Constants for arena parameters
    static constexpr float RAZORSCALE_FLYING_Z_THRESHOLD = 440.0f;
    static constexpr float RAZORSCALE_ARENA_CENTER_X = 587.54f;
    static constexpr float RAZORSCALE_ARENA_CENTER_Y = -175.04f;
    static constexpr float RAZORSCALE_ARENA_RADIUS = 30.0f;

    // Harpoon cooldown (seconds)
    static constexpr time_t HARPOON_COOLDOWN_DURATION = 5;

    // Structure for harpoon data
    struct HarpoonData
    {
        uint32 gameObjectEntry;
        uint32 chainSpellId;
    };

    explicit RazorscaleBossHelper(PlayerbotAI* botAI)
        : AiObject(botAI), _boss(nullptr) {}

    bool UpdateBossAI();
    Unit* GetBoss() const;

    bool IsGroundPhase() const;
    bool IsFlyingPhase() const;

    bool IsHarpoonFired(uint32 chainSpellId) const;
    static bool IsHarpoonReady(GameObject* harpoonGO);
    static void SetHarpoonOnCooldown(GameObject* harpoonGO);
    GameObject* FindNearestHarpoon(float x, float y, float z) const;

    static std::vector<HarpoonData> const& GetHarpoonData();

    void AssignRolesBasedOnHealth();
    bool AreRolesAssigned() const;
    bool CanSwapRoles() const;

private:
    Unit* _boss;

    // A map to track the last role swap *per bot* by their GUID
    static std::unordered_map<ObjectGuid, std::time_t> _lastRoleSwapTime;

    // The cooldown that applies to every bot
    static const std::time_t _roleSwapCooldown = 10;

    static std::unordered_map<ObjectGuid, time_t> _harpoonCooldowns;
};

// template <class BossAiType>
// class GenericBossHelper : public AiObject
// {
// public:
//     GenericBossHelper(PlayerbotAI* botAI, std::string name) : AiObject(botAI), _name(name) {}
//     virtual bool UpdateBossAI()
//     {
//         if (!bot->IsInCombat())
//         {
//             _unit = nullptr;
//         }
//         if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
//         {
//             _unit = nullptr;
//         }
//         if (!_unit)
//         {
//             _unit = AI_VALUE2(Unit*, "find target", _name);
//             if (!_unit)
//             {
//                 return false;
//             }
//             _target = _unit->ToCreature();
//             if (!_target)
//             {
//                 return false;
//             }
//             _ai = dynamic_cast<BossAiType*>(_target->GetAI());
//             if (!_ai)
//             {
//                 return false;
//             }
//             _event_map = &_ai->events;
//             if (!_event_map)
//             {
//                 return false;
//             }
//         }
//         if (!_event_map)
//         {
//             return false;
//         }
//         _timer = _event_map->GetTimer();
//         return true;
//     }
//     virtual void Reset()
//     {
//         _unit = nullptr;
//         _target = nullptr;
//         _ai = nullptr;
//         _event_map = nullptr;
//         _timer = 0;
//     }

// protected:
//     std::string _name;
//     Unit* _unit = nullptr;
//     Creature* _target = nullptr;
//     BossAiType* _ai = nullptr;
//     EventMap* _event_map = nullptr;
//     uint32 _timer = 0;
// };

// The Flash Freeze snowdrift lands where the Snowpacked Icicle falls; look for its target marker
// first, then the icicle itself, so either spawn layout works.
inline Creature* FindSnowpackedIcicle(Player* bot)
{
    constexpr float ICICLE_SEARCH_DISTANCE = 100.0f;
    if (Creature* target = bot->FindNearestCreature(NPC_SNOWPACKED_ICICLE, ICICLE_SEARCH_DISTANCE))
        return target;
    return bot->FindNearestCreature(NPC_SNOWPACKED_ICICLE_FALLING, ICICLE_SEARCH_DISTANCE);
}

#endif
