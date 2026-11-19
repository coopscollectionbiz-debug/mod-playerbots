/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPActions.h"

#include <cmath>

#include "GameObject.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"

using namespace EncounterHelpers;

#include "SWPHelpers.h"
#include "WorldPacket.h"

using namespace SunwellPlateauHelpers;

namespace
{
constexpr float RIFT_CLICK_RANGE = 4.0f;
}

// General

bool SunwellEraseTimersAndTrackersAction::Execute(Event /*event*/)
{
    // Per-instance trackers: the Kalecgos and Brutallus snapshots (BT pattern).
    //
    // THE BOT'S OWN COMBAT FLAG IS NOT A SIGNAL THAT THE ENCOUNTER ENDED. This trigger fires for
    // any bot in Sunwell that is not in combat, and these entries are SHARED RAID STATE - the
    // Brutallus one holds the latched anchor every station is measured from and the pull timestamp
    // the off tank's opening window runs off. A DEAD bot is not in combat, so the first casualty
    // was wiping the formation for the entire raid: the anchor re-latched on a fresh axis, every
    // zone moved, the off tank walked to a new lane bearing mid-fight, and his no-damage window
    // re-armed so he never built the threat the tank handoff spends (which is why the swap needed
    // 5 stacks instead of 3). The resting tank is another one - he deliberately stops attacking and
    // drops combat.
    //
    // So erase only when the BOSS says the fight is over. The snapshot getters already do exactly
    // that, including the "this bot just cannot see him" case; this is the out-of-combat sweep for
    // an instance nobody is fighting in, so let the getter make the call.
    if (Creature* dragon = FindKalecgosDragon(bot))
    {
        if (!dragon->IsAlive() || !dragon->IsInCombat())
            kalecgosSnapshotByInstance.erase(bot->GetMap()->GetInstanceId());
    }

    if (Creature* brutallus = FindBrutallus(bot))
    {
        if (!brutallus->IsAlive() || !brutallus->IsInCombat())
            brutallusSnapshotByInstance.erase(bot->GetMap()->GetInstanceId());
    }

    // ALWAYS false. This is bookkeeping, not behaviour, and it is bound at ACTION_EMERGENCY + 11
    // - higher than everything a bot normally does. Reporting success here stops the engine for
    // the tick, so `follow` (relevance 1) and every command never run and the bot looks broken:
    // parked, ignoring follow/stay, needing a relog. Runtime-confirmed on the tank bots, whose
    // multiplier was re-creating a tracker entry for this action to keep deleting.
    return false;
}

// Kalecgos

bool KalecgosMeleeFlankBossAction::Execute(Event /*event*/)
{
    Creature* dragon = FindKalecgosDragon(bot);
    if (!dragon)
        return false;

    // Step to the flank nearer the bot: 90 degrees off the dragon's facing, just inside
    // melee range. Frost Breath owns the front cone, Tail Lash the rear cone.
    float const reach = std::max(5.0f, dragon->GetCombatReach());
    float const facing = dragon->GetOrientation();

    float const lx = dragon->GetPositionX() + std::cos(facing + float(M_PI) / 2) * reach;
    float const ly = dragon->GetPositionY() + std::sin(facing + float(M_PI) / 2) * reach;
    float const rx = dragon->GetPositionX() + std::cos(facing - float(M_PI) / 2) * reach;
    float const ry = dragon->GetPositionY() + std::sin(facing - float(M_PI) / 2) * reach;

    bool const leftCloser = bot->GetDistance2d(lx, ly) <= bot->GetDistance2d(rx, ry);

    return MoveTo(bot->GetMapId(), leftCloser ? lx : rx, leftCloser ? ly : ry,
                  dragon->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool KalecgosTankPositionBossAction::Execute(Event /*event*/)
{
    return MoveTo(bot->GetMapId(), KALECGOS_TANK_POSITION.GetPositionX(),
                  KALECGOS_TANK_POSITION.GetPositionY(), KALECGOS_TANK_POSITION.GetPositionZ(),
                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

bool KalecgosDisperseRangedAction::Execute(Event /*event*/)
{
    Creature* dragon = FindKalecgosDragon(bot);
    if (!dragon)
        return false;

    // Too close to the boss: step straight out (small steps - the platform has edges,
    // and the stray-recovery trigger is the net, not the plan).
    if (bot->GetExactDist2d(dragon) < KALECGOS_RANGED_MIN_RANGE)
        return MoveAway(dragon, 6.0f);

    if (Unit* crowder = GetNearestPlayerInRadius(bot, KALECGOS_RANGED_SPACING))
        return MoveAway(crowder, KALECGOS_RANGED_SPACING);

    return false;
}

bool KalecgosEnterSpectralRiftAction::Execute(Event /*event*/)
{
    GameObject* rift =
        bot->FindNearestGameObject(static_cast<uint32>(SunwellObjects::GO_SPECTRAL_RIFT), 100.0f);
    if (!rift)
        return false;

    if (bot->GetDistance(rift) > RIFT_CLICK_RANGE)
        return MoveTo(rift->GetMapId(), rift->GetPositionX(), rift->GetPositionY(),
                      rift->GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_FORCED);

    bot->GetMotionMaster()->Clear();
    bot->StopMoving();
    bot->SetFacingToObject(rift);

    // Double click, Aq20-crystal style. For this GOOBER the FIRST packet (GameObject::Use)
    // casts the rift spell (Data10 = 44811); the report-use packet only fires GossipHello
    // and is kept for parity with GOs that hang their scripts there. Harmless if redundant.
    WorldPacket data1(CMSG_GAMEOBJ_USE);
    data1 << rift->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data1);

    WorldPacket data2(CMSG_GAMEOBJ_USE);
    data2 << rift->GetGUID();
    bot->GetSession()->HandleGameobjectReportUse(data2);

    return true;
}

bool KalecgosReturnToPlatformAction::Execute(Event /*event*/)
{
    return MoveTo(bot->GetMapId(), KALECGOS_PLATFORM_CENTER.GetPositionX(),
                  KALECGOS_PLATFORM_CENTER.GetPositionY(), KALECGOS_PLATFORM_CENTER.GetPositionZ(),
                  false, false, false, false, MovementPriority::MOVEMENT_FORCED);
}

bool KalecgosTankPickUpSathrovarrAction::Execute(Event /*event*/)
{
    Creature* demon = FindSathrovarr(bot);
    if (!demon || !demon->IsAlive())
        return false;

    if (demon->GetVictim() != bot)
        CastClassTaunt(bot, botAI, demon);

    return Attack(demon);
}

bool KalecgosAttackSathrovarrAction::Execute(Event /*event*/)
{
    Creature* demon = FindSathrovarr(bot);
    if (!demon || !demon->IsAlive())
        return false;

    return Attack(demon);
}

bool KalecgosHealKalecAction::Execute(Event /*event*/)
{
    Creature* kalec = FindKalecFriendly(bot);
    if (!kalec || !kalec->IsAlive())
        return false;

    // Out of the ~40y heal range: close in first (the spectral teleport usually lands close,
    // but a kited Sathrovarr can drag the fight away from Kalec).
    if (bot->GetDistance(kalec) > 35.0f)
        return MoveTo(kalec->GetMapId(), kalec->GetPositionX(), kalec->GetPositionY(),
                      kalec->GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT);

    // CanCastSpell refuses cast-time spells while isMoving(), and every opener below has a
    // cast time - stop first or this action fails every tick (the patch-0007 healer trap).
    if (bot->isMoving())
    {
        bot->GetMotionMaster()->Clear(false);
        bot->StopMoving();
    }

    // Cast by NAME so the bot's own known rank resolves - hardcoded WotLK max-rank ids
    // (the Valithria approach) silently no-op on a level-70 raid.
    auto tryCast = [&](char const* name) -> bool
    {
        if (botAI->CanCastSpell(name, kalec))
            return botAI->CastSpell(name, kalec);
        return false;
    };

    switch (bot->getClass())
    {
        case CLASS_DRUID:
            return tryCast("regrowth") || tryCast("healing touch");
        case CLASS_SHAMAN:
            return tryCast("lesser healing wave") || tryCast("healing wave");
        case CLASS_PRIEST:
            return tryCast("flash heal") || tryCast("greater heal");
        case CLASS_PALADIN:
            return tryCast("flash of light") || tryCast("holy light");
        default:
            return false;
    }
}
