/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTMOUNTPOLICY_H
#define PLAYERBOTS_PLAYERBOTMOUNTPOLICY_H

#include <cstdint>
#include <vector>

// Single source of truth for random-bot ground mounts:
//  * the racial vendor mount lists InitMounts() grants (all 12 playable races on this server),
//  * which of those spells are "foreign" to a bot of a given race (left over from a race change),
//  * the class mounts and the ground-mount priority used by CheckMountStateAction.
// Deliberately free of core headers so it can be unit tested on its own (apps/tests).
namespace PlayerbotMountPolicy
{
// Race / class ids as used by this server (stock ids plus mod-worgoblin's Goblin 9 and Worgen 12).
enum : uint8_t
{
    RACE_ID_HUMAN = 1,
    RACE_ID_ORC = 2,
    RACE_ID_DWARF = 3,
    RACE_ID_NIGHTELF = 4,
    RACE_ID_UNDEAD = 5,
    RACE_ID_TAUREN = 6,
    RACE_ID_GNOME = 7,
    RACE_ID_TROLL = 8,
    RACE_ID_GOBLIN = 9,
    RACE_ID_BLOODELF = 10,
    RACE_ID_DRAENEI = 11,
    RACE_ID_WORGEN = 12,
};
enum : uint8_t
{
    CLASS_ID_PALADIN = 2,
    CLASS_ID_DEATH_KNIGHT = 6,
    CLASS_ID_WARLOCK = 9
};

enum : uint32_t
{
    MOUNT_GOBLIN_TRIKE = 87090,
    MOUNT_GOBLIN_TURBO_TRIKE = 87091,
    MOUNT_RUNNING_WILD = 87840,
    // Paladin. 13819/23214 are the Alliance-style spells, 34769/34767 the Thalassian (Blood Elf) ones.
    // Undead/Tauren paladins learn 34767 from the Horde charger quest; mod-paladin-chargers re-skins
    // all four per race, so the spell id only matters when a bot knows two of the same tier.
    MOUNT_PALADIN_WARHORSE = 13819,
    MOUNT_PALADIN_CHARGER = 23214,
    MOUNT_PALADIN_THALASSIAN_WARHORSE = 34769,
    MOUNT_PALADIN_THALASSIAN_CHARGER = 34767,
    MOUNT_WARLOCK_FELSTEED = 5784,
    MOUNT_WARLOCK_DREADSTEED = 23161,
    MOUNT_DK_ACHERUS_DEATHCHARGER = 48778,
};

// Share of ground mounts a Worgen Paladin takes on its class mount when Running Wild is in the same
// speed bucket; the rest go to Running Wild, so both stay reachable.
constexpr uint32_t WORGEN_PALADIN_CLASS_MOUNT_PERCENT = 70;

struct RacialMounts
{
    std::vector<uint32_t> slow;  // 60% ground (Apprentice riding)
    std::vector<uint32_t> fast;  // 100% ground (Journeyman riding)
};

// Racial vendor mounts of a race; nullptr for an unknown race.
RacialMounts const* GetRacialMounts(uint8_t race);

// True when the spell is a racial vendor mount of any race.
bool IsRacialVendorMount(uint32_t spellId);
// True when the spell is a racial vendor mount of some race and NOT one of `race`'s own.
// Class, flying, quest, reward and achievement mounts are never "foreign".
bool IsForeignRacialMount(uint8_t race, uint32_t spellId);
// All racial vendor mounts foreign to `race` (what a race-change cleanup may remove).
std::vector<uint32_t> GetForeignRacialMounts(uint8_t race);

// Class mount of the given class (Paladin, Warlock, Death Knight).
bool IsClassMount(uint8_t cls, uint32_t spellId);

enum class MountCategory : uint8_t
{
    Other = 0,
    OwnRacial = 1,
    Class = 2,
    ForeignRacial = 3
};
MountCategory Categorize(uint8_t race, uint8_t cls, uint32_t spellId);

// Orders same-speed ground mount candidates by preference; foreign racial mounts are dropped.
//  1. class mounts (Paladin race-appropriate spell first; Worgen Paladins: Running Wild instead
//     when roll >= WORGEN_PALADIN_CLASS_MOUNT_PERCENT),
//  2. own racial mounts (Running Wild, Trikes, the race's vendor mounts),
//  3. anything else (special/reward mounts).
// Within a group the start is rotated by `roll` so equal mounts are varied; the caller tries the
// list in order and casts the first castable one, so an unusable spell is never forced.
// `roll` is 0..99.
std::vector<uint32_t> RankGroundMounts(uint8_t race, uint8_t cls, std::vector<uint32_t> const& candidates,
                                       uint32_t roll);
}  // namespace PlayerbotMountPolicy

#endif
