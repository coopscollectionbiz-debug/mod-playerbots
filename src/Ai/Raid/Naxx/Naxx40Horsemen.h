/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Four Horsemen (Individual Progression's level-60 Naxxramas): encounter gate, per-instance state and the
// view every Horsemen trigger, action and multiplier reads. Pure rules: Naxx40HorsemenRules.h. Spec (overlay repo):
// docs/superpowers/specs/2026-10-27-playerbot-naxx40-four-horsemen-design.md.

#ifndef PLAYERBOTS_NAXX40HORSEMEN_H
#define PLAYERBOTS_NAXX40HORSEMEN_H

#include <array>
#include <cstdint>
#include <memory>

#include "Naxx40HorsemenRules.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

namespace Naxx40Horsemen
{
    constexpr uint32_t NAXX40_MAP_ID = 533;

    // A view is valid for the current tick only. horse[] and any Creature*/Player* taken from it (ActiveTank) must
    // never be stored across ticks: the unit can despawn or log out. Re-read the view each tick instead.
    struct HorsemenView
    {
        Creature* horse[4]{};  // alive Naxx40 horsemen by Horse, else null
        std::array<bool, 4> alive{};
        // Current pairs after RefillPairs, healer groups, DPS halves; spareTanks = the free pool. Shared and
        // immutable (a refresh publishes a new one), so reading it is free: use view.assign->. Non-null whenever
        // GetView returned true.
        std::shared_ptr<Naxx40HorsemenRules::Assignment const> assign;
    };

    // false = strategy inert: stations still PROVISIONAL, not map 533, the instance's BOSS_HORSEMAN (12) not
    // IN_PROGRESS, or no Naxx40 horseman entry alive. On a failed gate the instance's state is dropped, so the next
    // pull re-latches the assignment from scratch; so does an instance entry not refreshed for 10 s (a new attempt,
    // or a reused instance id). Refreshes the shared state at most every 500 ms.
    bool GetView(Player* bot, HorsemenView& view);

    // Stack amount of MARK_SPELL[h] on `who` (any caster); 0 when absent or h is None.
    uint8_t MarkStacks(Unit* who, Naxx40HorsemenRules::Horse h);

    // The horseman this bot is working this turn (None = safe spot / not set). Per bot, per instance, under the
    // state mutex; dropped with the instance state.
    Naxx40HorsemenRules::Horse CurrentHorse(Player* bot);
    void SetCurrentHorse(Player* bot, Naxx40HorsemenRules::Horse h);
    // True once SetCurrentHorse moved this bot off the first horseman it worked this attempt (spec §4.3d stagger).
    bool HasMoved(Player* bot);
    // This healer's index inside its healer group (0 or 1 in groups 0-3); 0xFF when not a healer.
    uint8_t MyHealerSeat(Player* bot, HorsemenView const& view);
    // This DPS's seat (0 or 1) among the members of its half with the same role (melee or ranged), alternating in
    // the half's order (spec §4.2d stagger); 0xFF when not a DPS. Members not in the world are skipped.
    uint8_t MyDpsSeat(Player* bot, HorsemenView const& view);

    // Whisper edges to a human RESTING tank (spec §4.1), per pair, keyed on the pair's current active tank (a flip
    // starts over): edge 1 = "get ready" sent, 2 = "taunt now" sent. Returns true, and records it, only when `edge` is
    // higher than what was already sent to this pair under activeGuid, so each edge goes out once per swap. Under the
    // state mutex; false when no attempt is latched.
    bool ClaimWhisperEdge(Player* bot, Naxx40HorsemenRules::Horse h, uint64_t activeGuid, uint8_t edge);

    // Spec §4.2f: IndividualProgression.doableNaxx40Bosses_4H (IP's weak marks), read once from the shared config.
    bool DoableMarks();

    // Spec §4.3f: true, and recorded, the first time this bot asks in the current attempt; false afterwards or when
    // no attempt is latched. Under the state mutex; dropped with the instance state.
    bool ClaimManaRefill(Player* bot);

    // Blaumeux's tank walk: the current index into Naxx40HorsemenStations::BLAU_POINTS, per
    // instance, under the state mutex. Set is ignored when no attempt is latched (never resurrects a dropped state).
    uint8_t BlauCircleIndex(Player* bot);
    void SetBlauCircleIndex(Player* bot, uint8_t idx);

    // The living active tank of horseman h on the bot's map, else null.
    Player* ActiveTank(Player* bot, HorsemenView const& view, Naxx40HorsemenRules::Horse h);

    // The horseman whose pair holds this bot (iAmActive = it is the active tank), None for a free / non-tank.
    Naxx40HorsemenRules::Horse MyPair(Player* bot, HorsemenView const& view, bool& iAmActive);

    // The bot is in the free tank pool (assign->spareTanks: tanks beyond 8, and a dead horseman's pair). Spares wait
    // on the safe spot and step in only through RefillPairs.
    bool IsSpareTank(Player* bot, HorsemenView const& view);

    // 0..4 = the bot's healer group (4 = floater), 0xFF if it is not a latched healer.
    uint8_t MyHealerGroup(Player* bot, HorsemenView const& view);

    // 0/1 = the bot's DPS half, 0xFF if it is not a latched DPS.
    uint8_t MyDpsHalf(Player* bot, HorsemenView const& view);

    // Spec §4.3a: another living latched healer (not `bot`) whose CurrentHorse is h stands still within heal reach
    // (36 y, 2-D) of `tank` (h's living active tank). False for h None or a null tank.
    bool OtherHealerCovers(Player* bot, HorsemenView const& view, Naxx40HorsemenRules::Horse h, Player* tank);

    // Spec §4.3 tank heal (stock bot heals only cover the healer's own raid subgroup plus the main tank). Non-null only
    // for a living, standing (no cast-time spell while moving) latched healer with a class fast heal (flash heal /
    // flash of light / lesser healing wave / regrowth, the Aq40 tank-heal pick) whose CurrentHorse has a living active
    // tank below 90 % health, within 36 y (2-D) and in LOS, when the cast can go off now (CanCastSpell plus the real
    // mana cost). `spell` is set to the spell name on success. Null = stock healing runs.
    Player* TankHealTarget(Player* bot, PlayerbotAI* botAI, char const*& spell);
}

#endif
