/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_WORGOBLINMOUNTS_H
#define PLAYERBOTS_WORGOBLINMOUNTS_H

#include "Define.h"

// Playable races added through ChrRaces by mod-worgoblin. This core has no RACE_ constants for them.
enum WorgoblinRaces : uint8
{
    RACE_WORGOBLIN_GOBLIN = 9,
    RACE_WORGOBLIN_WORGEN = 12,
};

// Racial mounts of those races, as defined by mod-worgoblin's spell data.
enum WorgoblinMountSpells : uint32
{
    SPELL_GOBLIN_TRIKE = 87090,        // 60%, taught by item 62461 (Riding 75)
    SPELL_GOBLIN_TURBO_TRIKE = 87091,  // 100%, taught by item 62462 (Riding 150)
    // Mount aura without Unit::Mount(): IsMounted() stays false while it is active. Its speed effect is
    // SPELL_AURA_MOD_INCREASE_SPEED, scripted by mod-worgoblin to 60% below Journeyman Riding and 100% from it.
    SPELL_RUNNING_WILD = 87840,
};

enum WorgoblinRidingSkill : uint32
{
    WORGOBLIN_RIDING_APPRENTICE = 75,
    WORGOBLIN_RIDING_JOURNEYMAN = 150,
};

#endif
