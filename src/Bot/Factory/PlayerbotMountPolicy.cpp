/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PlayerbotMountPolicy.h"

#include <algorithm>
#include <array>

namespace PlayerbotMountPolicy
{
namespace
{
struct RaceEntry
{
    uint8_t race;
    RacialMounts mounts;
};

// Same spells and order as the stock InitMounts() switch, except Blood Elf: 33660 (Swift Pink
// Hawkstrider) is a 100% mount and was in the slow list, and 35025 was listed twice.
std::array<RaceEntry, 12> const& Table()
{
    static std::array<RaceEntry, 12> const table = {{
        {RACE_ID_HUMAN, {{470, 6648, 458, 472}, {23228, 23227, 23229}}},
        {RACE_ID_ORC, {{6654, 6653, 580}, {23250, 23252, 23251}}},
        {RACE_ID_DWARF, {{6899, 6777, 6898}, {23238, 23239, 23240}}},
        {RACE_ID_NIGHTELF, {{10789, 8394, 10793}, {23219, 23220, 63637}}},
        {RACE_ID_UNDEAD, {{17463, 17464, 17462}, {17465, 23246, 66846}}},
        {RACE_ID_TAUREN, {{18990, 18989, 64657}, {23249, 23248, 23247}}},
        {RACE_ID_GNOME, {{10969, 17453, 10873, 17454}, {23225, 23223, 23222}}},
        {RACE_ID_TROLL, {{10796, 10799, 8395}, {23241, 23242, 23243}}},
        {RACE_ID_GOBLIN, {{MOUNT_GOBLIN_TRIKE}, {MOUNT_GOBLIN_TURBO_TRIKE}}},
        {RACE_ID_BLOODELF, {{35020, 35022, 35018}, {33660, 35025, 35027}}},
        {RACE_ID_DRAENEI, {{34406, 35711, 35710}, {35713, 35712, 35714}}},
        // Running Wild covers both tiers; mod-worgoblin scales its speed with Riding.
        {RACE_ID_WORGEN, {{MOUNT_RUNNING_WILD}, {MOUNT_RUNNING_WILD}}},
    }};
    return table;
}

bool Contains(std::vector<uint32_t> const& v, uint32_t id) { return std::find(v.begin(), v.end(), id) != v.end(); }

bool IsOwnRacial(uint8_t race, uint32_t spellId)
{
    RacialMounts const* own = GetRacialMounts(race);
    return own && (Contains(own->slow, spellId) || Contains(own->fast, spellId));
}

// Lower is preferred. Blood Elves keep the Thalassian spells first; everyone else the Alliance-style
// ones (both are re-skinned per race by mod-paladin-chargers when a race display is configured).
uint32_t ClassMountOrder(uint8_t race, uint32_t spellId)
{
    bool const thalassianFirst = race == RACE_ID_BLOODELF;
    switch (spellId)
    {
        case MOUNT_PALADIN_CHARGER:
        case MOUNT_PALADIN_WARHORSE:
            return thalassianFirst ? 1 : 0;
        case MOUNT_PALADIN_THALASSIAN_CHARGER:
        case MOUNT_PALADIN_THALASSIAN_WARHORSE:
            return thalassianFirst ? 0 : 1;
        default:
            return 0;
    }
}

void AppendRotated(std::vector<uint32_t>& out, std::vector<uint32_t> const& group, uint32_t roll)
{
    if (group.empty())
        return;
    size_t const start = roll % group.size();
    for (size_t i = 0; i < group.size(); ++i)
        out.push_back(group[(start + i) % group.size()]);
}
}  // namespace

RacialMounts const* GetRacialMounts(uint8_t race)
{
    for (RaceEntry const& entry : Table())
        if (entry.race == race)
            return &entry.mounts;
    return nullptr;
}

bool IsRacialVendorMount(uint32_t spellId)
{
    for (RaceEntry const& entry : Table())
        if (Contains(entry.mounts.slow, spellId) || Contains(entry.mounts.fast, spellId))
            return true;
    return false;
}

bool IsForeignRacialMount(uint8_t race, uint32_t spellId)
{
    return IsRacialVendorMount(spellId) && !IsOwnRacial(race, spellId);
}

std::vector<uint32_t> GetForeignRacialMounts(uint8_t race)
{
    std::vector<uint32_t> out;
    for (RaceEntry const& entry : Table())
    {
        if (entry.race == race)
            continue;
        for (std::vector<uint32_t> const* list : {&entry.mounts.slow, &entry.mounts.fast})
            for (uint32_t id : *list)
                if (!IsOwnRacial(race, id) && !Contains(out, id))
                    out.push_back(id);
    }
    return out;
}

bool IsClassMount(uint8_t cls, uint32_t spellId)
{
    switch (cls)
    {
        case CLASS_ID_PALADIN:
            return spellId == MOUNT_PALADIN_WARHORSE || spellId == MOUNT_PALADIN_CHARGER ||
                   spellId == MOUNT_PALADIN_THALASSIAN_WARHORSE || spellId == MOUNT_PALADIN_THALASSIAN_CHARGER;
        case CLASS_ID_WARLOCK:
            return spellId == MOUNT_WARLOCK_FELSTEED || spellId == MOUNT_WARLOCK_DREADSTEED;
        case CLASS_ID_DEATH_KNIGHT:
            return spellId == MOUNT_DK_ACHERUS_DEATHCHARGER;
        default:
            return false;
    }
}

MountCategory Categorize(uint8_t race, uint8_t cls, uint32_t spellId)
{
    if (IsClassMount(cls, spellId))
        return MountCategory::Class;
    if (IsOwnRacial(race, spellId))
        return MountCategory::OwnRacial;
    if (IsRacialVendorMount(spellId))
        return MountCategory::ForeignRacial;
    return MountCategory::Other;
}

std::vector<uint32_t> RankGroundMounts(uint8_t race, uint8_t cls, std::vector<uint32_t> const& candidates,
                                       uint32_t roll)
{
    std::vector<uint32_t> classMounts, own, other;
    for (uint32_t id : candidates)
    {
        if (Contains(classMounts, id) || Contains(own, id) || Contains(other, id))
            continue;
        switch (Categorize(race, cls, id))
        {
            case MountCategory::Class:
                classMounts.push_back(id);
                break;
            case MountCategory::OwnRacial:
                own.push_back(id);
                break;
            case MountCategory::Other:
                other.push_back(id);
                break;
            case MountCategory::ForeignRacial:
                break;  // never ridden
        }
    }

    // Deterministic class order (race-appropriate Paladin spell first), stable for equal keys.
    std::stable_sort(classMounts.begin(), classMounts.end(),
                     [race](uint32_t a, uint32_t b) { return ClassMountOrder(race, a) < ClassMountOrder(race, b); });

    std::vector<uint32_t> ranked;
    ranked.reserve(classMounts.size() + own.size() + other.size());

    bool const worgenPaladinRunsWild = race == RACE_ID_WORGEN && cls == CLASS_ID_PALADIN && !classMounts.empty() &&
                                       Contains(own, MOUNT_RUNNING_WILD) &&
                                       (roll % 100) >= WORGEN_PALADIN_CLASS_MOUNT_PERCENT;
    if (worgenPaladinRunsWild)
    {
        // Running Wild first this time; the class mount stays the fallback.
        ranked.push_back(MOUNT_RUNNING_WILD);
        own.erase(std::remove(own.begin(), own.end(), MOUNT_RUNNING_WILD), own.end());
    }

    ranked.insert(ranked.end(), classMounts.begin(), classMounts.end());
    AppendRotated(ranked, own, roll);
    AppendRotated(ranked, other, roll);
    return ranked;
}
}  // namespace PlayerbotMountPolicy
