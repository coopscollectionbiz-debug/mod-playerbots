/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Multipliers.h"

#include "Aq40CthunHelpers.h"
#include "Aq40Helpers.h"
#include "AttackAction.h"
#include "CheckMountStateAction.h"
#include "ChooseTargetActions.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

using namespace TempleOfAhnQirajHelpers;

float Aq40CthunMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID)
        return 1.0f;

    // Name exemptions first: each of these IS a MovementAction.
    std::string const& name = action->getName();
    if (name == "aq40 cthun exit stomach" || name == "aq40 cthun dodge glare" ||
        name == "aq40 cthun clear beam" || name == "aq40 cthun form up" ||
        name == "aq40 cthun run to far side" || name == "aq40 cthun drop to floor" ||
        name == "aq40 cthun stomach close in")
    {
        return 1.0f;
    }

    // Bots copy a mounted master's mount state; during the fight that would remount them right after
    // the dismount rule drops them. Riding IN is unaffected (they mounted before the pull).
    if (dynamic_cast<CheckMountStateAction*>(action) && IsCthunEncounterActive(bot))
        return 0.0f;

    // The Eye/body have no useful back: "set behind" only walks melee around it out of range.
    if (dynamic_cast<SetBehindTargetAction*>(action) && IsCthunEncounterActive(bot))
        return 0.0f;

    if (!bot->IsInCombat())
        return 1.0f;

    // Type test before the predicate. NEVER blanket-veto MovementAction: AttackAction and
    // MeleeAction derive from it, and that would silence auto-attack.
    bool const isFormationMover = dynamic_cast<CombatFormationMoveAction*>(action) ||
                                  dynamic_cast<RearFlankAction*>(action);
    bool const isReach = dynamic_cast<ReachTargetAction*>(action) != nullptr;
    bool const isFollow = dynamic_cast<FollowAction*>(action) != nullptr;
    if (!isFormationMover && !isReach && !isFollow)
        return 1.0f;

    // In the stomach, pathfinding-based reach can only fail (and waste the tick); the close-in
    // action walks the bot into range instead.
    if (isReach && IsInCthunStomach(bot))
        return 0.0f;

    return CthunShouldSuppressGenericMovement(bot, botAI) ? 0.0f : 1.0f;
}

float Aq40CthunTargetHoldMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID)
        return 1.0f;

    // Exact classes, never AttackAction (MeleeAction and our focus action derive from it).
    bool const isTargetChooser = dynamic_cast<DpsAssistAction*>(action) ||
                                 dynamic_cast<DpsAoeAction*>(action) ||
                                 dynamic_cast<TankAssistAction*>(action) ||
                                 dynamic_cast<AggressiveTargetAction*>(action) ||
                                 dynamic_cast<AttackAnythingAction*>(action) ||
                                 dynamic_cast<AttackLeastHpTargetAction*>(action) ||
                                 dynamic_cast<AttackRtiTargetAction*>(action);
    if (!isTargetChooser)
        return 1.0f;

    if (!IsCthunEncounterActive(bot))
        return 1.0f;

    // Two separate reasons to veto: we have an opinion, or we are deliberately withholding.
    if (GetCthunAttackTarget(bot, botAI))
        return 0.0f;
    return IsCthunHoldingFire(bot, botAI) ? 0.0f : 1.0f;
}

float Aq40CthunCastSafetyMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID)
        return 1.0f;

    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction || !IsCthunEncounterActive(bot))
        return 1.0f;

    // Cast time exactly as NaxxMultipliers' Heigan dance does it.
    uint32 spellId = AI_VALUE2(uint32, "spell id", spellAction->getSpell());
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 1.0f;

    uint32 castTime = spellInfo->CalcCastTime(bot);
    if (spellInfo->IsChanneled())
    {
        int32 duration = spellInfo->GetDuration();
        if (duration > 0)
            castTime += uint32(duration);
    }
    if (castTime == 0)
        return 1.0f;

    if (CthunShouldClearBeam(bot))
        return 0.0f;
    // A cast stops the walk (walk, stop, cast, walk), so non-healers hold cast-time spells while
    // forming up. Healers keep casting so the puller survives the opening beams.
    if (!PlayerbotAI::IsHeal(bot) && CthunShouldFormUp(bot, botAI))
        return 0.0f;
    if (CthunGlareSecondsToHit(bot) * 1000.0f < float(castTime + CTHUN_GLARE_CAST_PAD_MS))
        return 0.0f;
    return 1.0f;
}
