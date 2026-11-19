/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Anub'Rekhan (Individual Progression's level-60 Naxxramas) raid spread for Impale: encounter gate, the
// attempt's latched slots and the per-bot spot. Pure rules: Naxx40AnubRules.h. Spec (overlay repo):
// docs/superpowers/specs/2026-10-30-playerbot-naxx40-anubrekhan-design.md.

#ifndef PLAYERBOTS_NAXX40ANUB_H
#define PLAYERBOTS_NAXX40ANUB_H

#include <cstdint>
#include <memory>

#include "Naxx40AnubRules.h"

class Creature;
class Player;
class PlayerbotAI;

namespace Naxx40Anub
{
    constexpr uint32_t NAXX40_MAP_ID = 533;

    // Valid for the current tick only: never store anub across ticks (memory: no unit pointer across ticks).
    struct AnubView
    {
        Creature* anub = nullptr;  // alive, non-null whenever GetView returned true
        // The attempt's slots, latched once at the first grouped refresh. Shared and immutable.
        std::shared_ptr<Naxx40AnubRules::Plan const> plan;
        float fallbackBearing = 0.0f;  // latched with the plan (Naxx40AnubRules::FallbackBearing)
        bool swarm = false;            // Locust Swarm is up or being cast, this tick
    };

    // false = strategy inert: not map 533, not RAID_DIFFICULTY_10MAN_HEROIC (IP's Naxx40), the instance's BOSS_ANUB
    // (6) not IN_PROGRESS, no living Anub'Rekhan (351009, resolved by entry), or no plan latched yet. On a failed gate
    // the instance's state is dropped, so the next pull re-latches; so does an entry not refreshed for 10 s.
    bool GetView(Player* bot, AnubView& view);

    // The bot's latched slot, null for a human, a self bot, a tank, or a bot that was not alive in the raid at engage.
    Naxx40AnubRules::Slot const* SlotOf(Player* bot, AnubView const& view);

    struct Spot
    {
        float x = 0.0f;
        float y = 0.0f;
        float tolerance = 0.0f;  // re-move only when farther than this
    };

    // Spec §3: where the bot should stand this tick; false = no positioning for it (no slot, dead, or its target is not
    // Anub'Rekhan and out of reach from the spot: melee on any other target, ranged DPS on one out of spell range).
    // Melee on their arc slot during the swarm are kept there by Naxx40AnubMultiplier (it vetoes their movers toward
    // him), not by holding the tick, so stock target choosers can still send them to the Crypt Guard IP summons 3 s
    // into every swarm.
    bool SpotFor(Player* bot, PlayerbotAI* botAI, AnubView const& view, Spot& spot);
}

#endif
