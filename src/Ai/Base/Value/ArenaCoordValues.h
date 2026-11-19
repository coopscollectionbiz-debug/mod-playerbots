/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_ARENACOORDVALUES_H
#define _PLAYERBOT_ARENACOORDVALUES_H

#include <vector>

#include "PlayerbotAI.h"
#include "Value.h"

class Player;
class Unit;

// Deterministic, team-shared arena kill target: every member computes the same function of
// world state, so the team focus-fires with no messaging. Priority: healer-spec enemies
// first, then squishiest armor class; heavy defensive auras defer a target (turtle swap);
// ties break by GUID for determinism.
class ArenaKillTargetValue : public UnitCalculatedValue
{
public:
    // checkInterval 1 = recompute on every Get(): a cached Unit* can dangle (an enemy leaving
    // the arena inside the cache window would flow stale into callers' derefs), and per-tick
    // recompute keeps "every member sees the same ordering" strictly true (no cache skew).
    // Calculate is cheap: <=10 players x 8 HasAura checks.
    ArenaKillTargetValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "arena kill target", 1) {}

    Unit* Calculate() override;

    static bool  IsHealerSpec(Player* p);
    static uint8 SquishRank(uint8 cls);   // 0 = squishiest (cloth), 3 = plate
    static bool  HasTurtleAura(Unit* u);  // Divine Shield, Ice Block, Dispersion, ...

    // Sharpness 1..4 for THIS bot (4 = full): grouped-with-real-player => 4, else by the
    // bot's own arena team rating vs ArenaRoster.TierCeilings bands (sConfigMgr, safe defaults).
    static uint8 SharpnessFor(Player* bot, uint8 arenaType);

    // Parse a csv config option into uint32 bands, LOG_WARN + defaults on malformed/short csv.
    // Values latch at first use (callers cache in function-local statics): conf edits need a
    // worldserver restart, `.reload config` does not re-read them.
    static std::vector<uint32> ParseBands(char const* key, std::vector<uint32> const& defaults);
};

#endif
