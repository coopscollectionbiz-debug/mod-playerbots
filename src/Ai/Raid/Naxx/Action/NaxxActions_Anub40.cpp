/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Anub'Rekhan raid spread (overlay patch 0022). Spec (overlay repo):
// docs/superpowers/specs/2026-10-30-playerbot-naxx40-anubrekhan-design.md §3, §4.
//
// Return-value rules (memory: an action returning false hands the tick to the default actions):
//   - true while the bot travels to its spot (a leg issued, or MoveTo refused a duplicate of the leg still running);
//   - on the spot (within its tolerance): false, so the rotation and the stock target choosers run. Melee on their arc
//     slot during the swarm are kept off Anub'Rekhan by Naxx40AnubMultiplier (it vetoes their movers toward him), so a
//     melee bot can still retarget the Crypt Guard summoned 3 s into the swarm and leave the spread for it.
// The spot depends only on Anub'Rekhan's position and the latched slot, and a bot re-moves only once it is more than
// the tolerance off it, so it never creeps or re-issues a move every tick (bots cannot cast while moving).

#include "Naxx40Anub.h"
#include "NaxxActions.h"
#include "Player.h"
#include "Playerbots.h"

bool Naxx40AnubSpreadAction::Execute(Event /*event*/)
{
    Naxx40Anub::AnubView view;
    Naxx40Anub::Spot spot;
    if (!Naxx40Anub::GetView(bot, view) || !Naxx40Anub::SpotFor(bot, botAI, view, spot))
        return false;
    if (bot->GetExactDist2d(spot.x, spot.y) <= spot.tolerance || !botAI->CanMove())
        return false;

    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(spot.x, spot.y, z);
    bool const moved = MoveTo(bot->GetMapId(), spot.x, spot.y, z, false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT, true);
    return moved || bot->isMoving();
}
