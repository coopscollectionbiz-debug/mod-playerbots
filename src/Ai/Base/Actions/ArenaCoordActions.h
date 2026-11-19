/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_ARENACOORDACTIONS_H
#define _PLAYERBOT_ARENACOORDACTIONS_H

#include "Action.h"

// Use the PvP trinket (medallion, spell 42292) to break CC — gated by a sharpness-scaled
// reaction delay: full-sharp bots break instantly when it matters (self CC'd AND (healer
// also CC'd OR self < 50% HP)), low tiers sit in the CC for ~ReactMs first (probabilistic
// hold, expected wait ≈ ReactMs).
class ArenaPvpTrinketAction : public Action
{
public:
    ArenaPvpTrinketAction(PlayerbotAI* botAI) : Action(botAI, "arena pvp trinket") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
