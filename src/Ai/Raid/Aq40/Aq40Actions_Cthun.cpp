/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Actions.h"

#include "Aq40CthunHelpers.h"
#include "Aq40Helpers.h"
#include "MotionMaster.h"
#include "Opcodes.h"
#include "Playerbots.h"
#include "SpellAuras.h"
#include "WorldPacket.h"
#include "WorldSession.h"

using namespace TempleOfAhnQirajHelpers;

bool Aq40CthunEraseTrackersAction::Execute(Event /*event*/)
{
    CthunEraseTrackers(bot);
    return false;
}

bool Aq40CthunExitStomachAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;  // grab stun / falling in

    float const dist = bot->GetExactDist2d(CTHUN_STOMACH_EXIT_X, CTHUN_STOMACH_EXIT_Y);
    if (dist <= CTHUN_EXIT_REACHED)
    {
        bot->StopMoving();
        // Bots have no client, so the area trigger the script waits for is never sent. Send it the
        // way ReachAreaTriggerAction does; the handler itself checks we are inside the 5y radius.
        // A selfbot's own client sends it.
        if (!IsSelfBot(bot))
        {
            WorldPacket packet(CMSG_AREATRIGGER);
            packet << uint32(AT_CTHUN_STOMACH_EXIT);
            packet.rpos(0);
            bot->GetSession()->HandleAreaTriggerOpcode(packet);
        }
        CthunStartExitHold(bot);

        Aura* acid = bot->GetAura(SPELL_CTHUN_DIGESTIVE_ACID);
        LOG_DEBUG("playerbots", "[Aq40Cthun] {} stomach exit sent, acid {}", bot->GetName(),
                  acid ? uint32(acid->GetStackAmount()) : 0u);
        return true;
    }

    if (MoveTo(bot->GetMapId(), CTHUN_STOMACH_EXIT_X, CTHUN_STOMACH_EXIT_Y, CTHUN_STOMACH_EXIT_Z,
               false, false, false, false, MovementPriority::MOVEMENT_FORCED))
    {
        LOG_DEBUG("playerbots", "[Aq40Cthun] {} stomach exit walk ({}), dist {:.0f}", bot->GetName(),
                  CthunExitReason(bot), dist);
        return true;
    }
    // While walking a false is just MoveTo's already-heading-there guard, not a failure.
    if (!bot->isMoving())
        LOG_DEBUG("playerbots", "[Aq40Cthun] {} exit move failed z {:.1f} dist {:.1f} canMove {} moving {}",
                  bot->GetName(), bot->GetPositionZ(), dist, botAI->CanMove() ? 1 : 0,
                  bot->isMoving() ? 1 : 0);

    // MoveTo also reports false for "already heading there", so only fall back when standing
    // still: the stomach may be off the navmesh, so walk a straight line with no path generation.
    if (!bot->isMoving())
    {
        bot->GetMotionMaster()->MovePoint(0, CTHUN_STOMACH_EXIT_X, CTHUN_STOMACH_EXIT_Y,
                                          CTHUN_STOMACH_EXIT_Z, FORCED_MOVEMENT_NONE, 0.0f, 0.0f,
                                          false);
        LOG_DEBUG("playerbots", "[Aq40Cthun] {} stomach exit straight-line fallback ({}), dist {:.0f}",
                  bot->GetName(), CthunExitReason(bot), dist);
        return true;
    }
    return false;
}

bool Aq40CthunDismountAction::Execute(Event /*event*/)
{
    if (!CthunShouldDismount(bot, botAI))
        return false;
    // Same path as CheckMountStateAction::Dismount.
    if (bot->isMoving())
        bot->StopMoving();
    WorldPacket emptyPacket;
    bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
    LOG_DEBUG("playerbots", "[Aq40Cthun] {} dismount in place", bot->GetName());
    return true;
}

bool Aq40CthunDropToFloorAction::Execute(Event /*event*/)
{
    // NOT MoveFall: its spline sets MOVEMENTFLAG_FALLING, which a bot never clears (no landing
    // opcode), leaving isMoving() true forever and every cast-time spell impossible. Teleport onto
    // the ground instead, then scrub any stale falling flags.
    float groundZ = 0.0f;
    CthunAirborne const state = GetCthunAirborneState(bot, botAI, groundZ);
    if (state == CthunAirborne::None)
        return false;

    float const z = bot->GetPositionZ();
    if (state == CthunAirborne::Hovering)
        bot->NearTeleportTo(bot->GetPositionX(), bot->GetPositionY(), groundZ + 0.5f,
                            bot->GetOrientation());
    bot->RemoveUnitMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR);

    LOG_DEBUG("playerbots", "[Aq40Cthun] {} {} z {:.1f} ground {:.1f}", bot->GetName(),
              state == CthunAirborne::Hovering ? "hovering, teleport down" : "stale fall flag, scrub",
              z, groundZ);
    return true;
}

bool Aq40CthunStomachCloseInAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetCthunStomachApproachSpot(bot, botAI, spot))
        return false;

    float const dist = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());
    bool const moved = MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(),
                              spot.GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_FORCED);
    if (moved)
    {
        LOG_DEBUG("playerbots", "[Aq40Cthun] {} stomach close in to {:.1f},{:.1f},{:.1f} dist {:.0f} via MoveTo (ret 1)",
                  bot->GetName(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), dist);
        return true;
    }

    // Same fallback as the exit walk: the stomach may be off the navmesh, so walk a straight line
    // with no path generation - only when standing still (a false while walking is MoveTo's
    // already-heading-there guard).
    if (!bot->isMoving())
    {
        bot->GetMotionMaster()->MovePoint(0, spot.GetPositionX(), spot.GetPositionY(),
                                          spot.GetPositionZ(), FORCED_MOVEMENT_NONE, 0.0f, 0.0f,
                                          false);
        LOG_DEBUG("playerbots", "[Aq40Cthun] {} stomach close in to {:.1f},{:.1f},{:.1f} dist {:.0f} via MovePoint (MoveTo ret 0)",
                  bot->GetName(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), dist);
        return true;
    }
    LOG_DEBUG("playerbots", "[Aq40Cthun] {} stomach close in to {:.1f},{:.1f},{:.1f} dist {:.0f} no move (MoveTo ret 0, moving)",
              bot->GetName(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), dist);
    return false;
}

bool Aq40CthunDodgeGlareAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    Position spot;
    if (!GetCthunGlareDodgeSpot(bot, spot))
        return false;

    bool const moved =
        MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
               false, false, false, false, MovementPriority::MOVEMENT_FORCED);
    LOG_DEBUG("playerbots", "[Aq40Cthun] {} dodge glare moved {}", bot->GetName(), moved ? 1 : 0);
    return moved;
}

bool Aq40CthunClearBeamAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    // Try up to four snapped bearings and take the first the pathfinder accepts.
    for (uint32 attempt = 0; attempt < 4; ++attempt)
    {
        Position spot;
        if (!GetCthunBeamClearSpot(bot, spot, attempt))
            return false;
        if (MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                   false, false, false, false, MovementPriority::MOVEMENT_FORCED))
        {
            if (Player* puller = GetCthunOpeningPullerWithin(bot, CTHUN_PULLER_AVOID_RANGE))
                LOG_DEBUG("playerbots", "[Aq40Cthun] {} clear puller zone dist {:.0f}", bot->GetName(),
                          bot->GetExactDist2d(puller));
            else
                LOG_DEBUG("playerbots", "[Aq40Cthun] {} clear beam attempt {}", bot->GetName(), attempt);
            return true;
        }
    }
    return false;
}

bool Aq40CthunRunToFarSideAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    Position spot;
    if (!GetCthunPullerSpot(bot, spot))
        return false;

    bool const moved =
        MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
               false, false, false, false, MovementPriority::MOVEMENT_FORCED);
    LOG_DEBUG("playerbots", "[Aq40Cthun] {} puller run to far side moved {}", bot->GetName(),
              moved ? 1 : 0);
    return moved;
}

bool Aq40CthunFocusTargetAction::Execute(Event /*event*/)
{
    Unit* target = GetCthunAttackTarget(bot, botAI);
    return target && Attack(target);
}

bool Aq40CthunFormUpAction::Execute(Event /*event*/)
{
    if (!botAI->CanMove())
        return false;

    Position spot;
    if (!GetCthunFormUpSpot(bot, botAI, spot))
        return false;

    float const dist = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());
    bool const moved =
        MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
               false, false, false, false, MovementPriority::MOVEMENT_FORCED);
    if (moved)
    {
        LOG_DEBUG("playerbots", "[Aq40Cthun] {} form up dist {:.0f}", bot->GetName(), dist);
        return true;
    }

    // A false while walking is MoveTo's already-heading-there guard. Standing still means MoveTo
    // refused the spot (unreachable?): log it - spots are fixed, so there is nothing to give up to.
    if (!bot->isMoving())
        LOG_DEBUG("playerbots", "[Aq40Cthun] {} form up move refused, dist {:.0f}", bot->GetName(), dist);
    return false;
}
