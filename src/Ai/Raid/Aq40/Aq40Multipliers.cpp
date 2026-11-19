/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Multipliers.h"

#include <string>

#include "Aq40Helpers.h"
#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"

using namespace TempleOfAhnQirajHelpers;

namespace
{
    bool IsTwinsAction(Action* action) { return action->getName().rfind("aq40 twins ", 0) == 0; }
}

float TwinEmperorsMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID)
        return 1.0f;

    // INVARIANT (Revision 2 §11.2, the no-drag rule): a bot a twin is attacking that is not that twin's designated
    // active tank (TwinsWrongVictimOf) never moves except to dodge Explode / Blizzard / (Revision 7 §16.1) Arcane
    // Burst or by "aq40 twins victim hold" (Vek'lor: hold 12-40 y from him; Vek'nilash: walk to his active melee
    // tank, or (Revision 11 §20.1) kite him to the drag station). Every other MovementAction is vetoed, deliberately
    // including AttackAction / MeleeAction (they chase), reach and heal reach, follow, formation and this strategy's
    // own spot, station, landing and stage moves: across four dev pulls a twin followed whatever moved its victim, and
    // that is how the twins came together. This
    // overrides the "never blanket-veto MovementAction" note below ONLY for wrong victims. Non-movement actions
    // (casts that need no move) are untouched.
    // Checked first, ahead of the name exemptions; the type test before the predicate keeps it cheap.
    if (dynamic_cast<MovementAction*>(action))
    {
        std::string const& name = action->getName();
        if (name != "aq40 twins victim hold" && name != "aq40 twins clear explode" &&
            name != "aq40 twins clear blizzard" && name != "aq40 twins clear burst" &&
            TwinsWrongVictimOf(bot, botAI))
            return 0.0f;
        // Revision 10 §19.3: inside the landing window a healer makes no move but the Explode / Blizzard dodges and
        // the landing clear: hold spot, step-in, heal reach, follow and every other mover are vetoed (healers that
        // walked toward their tank stood nearest the waking boss and died). After the wrong-victim veto, ahead of the
        // name exemptions, so the strategy's own hold spot is vetoed too.
        bool const exemptDodge = name == "aq40 twins clear explode" || name == "aq40 twins clear blizzard" ||
                                 name == "aq40 twins landing clear";
        if (TwinsHealerFrozen(bot, exemptDodge))
            return 0.0f;
    }

    // NAME EXEMPTIONS FIRST. Every Twins movement action IS a MovementAction, so without this the veto would
    // silence the very moves it exists to protect.
    if (IsTwinsAction(action))
        return 1.0f;
    // Revision 4 §13.2: the stock heal reach ("reach party member to heal", a ReachTargetAction) is vetoed again
    // below, like the other generic movers. Ruling R16 H1 exempted it, and in dev pull 6 it fired ~180 times, walking
    // healers straight at their targets, often round a boss to the far side. A healer now steps in instead: its owned
    // spot becomes the step-in point toward its side's tank (GetTwinsSpot, TwinsMe::stepIn).

    // TYPE TEST BEFORE THE PREDICATE: these kinds are the only actions this can veto, so everything else returns
    // early without evaluating a chain that reaches grid searches. No combat gate: the predicate gates on the
    // encounter being IN_PROGRESS, and a station veto must not fail open for a bot that is not in combat yet.
    //
    // NEVER blanket-veto MovementAction - AttackAction and MeleeAction both derive from it, so
    // that would silence auto-attack for the whole fight. (The one exception is the wrong-victim veto above.)
    bool const isFormationMover = dynamic_cast<CombatFormationMoveAction*>(action) ||  // tank face, set behind
                                  dynamic_cast<RearFlankAction*>(action);
    bool const isReach = dynamic_cast<ReachTargetAction*>(action) != nullptr;
    bool const isFollow = dynamic_cast<FollowAction*>(action) != nullptr;
    if (!isFormationMover && !isReach && !isFollow)
        return 1.0f;

    // Ruling R13 F3: no latched bot follows its master during the fight, healers and Dps included. Idle DPS (held
    // by the threat cap) sat in the non-combat engine and followed the master out of the room. Only FollowAction:
    // the other generic movers keep the per-bot suppression below.
    if (isFollow)
    {
        TwinsView view;
        if (GetTwinsView(bot, view) && GetTwinsMe(bot).role != Aq40TwinsRules::Role::None)
            return 0.0f;
    }

    // Ruling R15: an active warlock out of range of an off-station Vek'lor owns no spot; native caster reach brings it
    // to spell distance (in range but out of LOS it owns its LOS point instead: final-review I1). Follow and
    // formation movers stay vetoed below.
    if (isReach && TwinsWarlockMayReach(bot, botAI))
        return 1.0f;

    if (TwinsShouldSuppressGenericMovement(bot, botAI))
        return 0.0f;
    // Form-up (spec §5.5): a bot holding a staging spot must not follow its master (or be walked off by any other
    // generic mover) before the pull. IsTwinsFormUp gates on the encounter being NOT_STARTED, so the two never
    // overlap.
    Position spot;
    return GetTwinsStagingSpot(bot, spot) ? 0.0f : 1.0f;
}

float TwinEmperorsTargetHoldMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID)
        return 1.0f;

    // Type test first: it is the only cheap test here, and this runs for every queued action of
    // every bot on every tick. Matched by EXACT class, never by AttackAction - MeleeAction derives
    // from it and so does this fight's own focus action.
    bool const isTargetChooser = dynamic_cast<DpsAssistAction*>(action) ||
                                 dynamic_cast<DpsAoeAction*>(action) ||
                                 dynamic_cast<TankAssistAction*>(action) ||
                                 dynamic_cast<AggressiveTargetAction*>(action) ||
                                 dynamic_cast<AttackAnythingAction*>(action) ||
                                 dynamic_cast<AttackLeastHpTargetAction*>(action) ||
                                 dynamic_cast<AttackRtiTargetAction*>(action);
    if (!isTargetChooser)
        return 1.0f;

    if (!IsTwinsEncounterActive(bot))
        return 1.0f;

    // TWO separate reasons to veto, and collapsing them is the bug this shape avoids:
    //   * we have an opinion (a mutated bug, or my assigned twin) - hold it;
    //   * we are deliberately WITHHOLDING because our school cannot land - hold that too.
    // `return GetTwinsAttackTarget(...) ? 0.0f : 1.0f` would read the second case as "no opinion"
    // and hand the immune twin straight back through the bot's own attackers list.
    if (GetTwinsAttackTarget(bot, botAI))
        return 0.0f;

    return IsTwinsHoldingFire(bot, botAI) ? 0.0f : 1.0f;
}

// Spec §5.6 + ruling R8, the Huhuran threat-multiplier shape (Aq40HuhuranThreatMultiplier), copied: every
// threat-typed action plus the exact auto-attack restarters (MeleeAction, CastAutoShotAction, CastShootAction, none
// of which has a subclass), aimed at a twin (the bot's current target). Two reasons to veto:
//   * holding fire on a twin (IsTwinsHoldingFire: waiting tanks always; DPS on Vek'nilash until he is on his
//     designated active tank, failing open 8 s after the landing window or engage; never on Vek'lor, Revision 5);
//   * the threat cap: own + headroom (0, ruling R13) >= 0.80 of that twin's DESIGNATED active tank's threat
//     (TwinsThreatVetoed; the warlock is not a role tank, so a role-tank denominator would leave Vek'lor uncapped;
//     Revision 5: Vek'lor's is his current victim's when that is another player or pet),
//     plus upstream's AoE rule for a capped bot.
// Heals are never capped, whoever casts them. Healers, the active tanks and unlatched bots are never capped. Cheap
// type, role and target-entry tests before any view lookup: this runs for every queued action of every bot.
float TwinEmperorsThreatMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID || IsTwinsAction(action))
        return 1.0f;

    Action::ActionThreatType const threatType = action->getThreatType();
    bool const isAutoAttack = dynamic_cast<MeleeAction*>(action) || dynamic_cast<CastAutoShotAction*>(action) ||
                              dynamic_cast<CastShootAction*>(action);
    if (threatType == Action::ActionThreatType::None && !isAutoAttack)
        return 1.0f;
    // Heals are Aoe-typed (CastHealingSpellAction) and must never be limited.
    if (dynamic_cast<CastHealingSpellAction*>(action) || PlayerbotAI::IsHeal(bot))
        return 1.0f;

    Unit* const current = AI_VALUE(Unit*, "current target");
    if (!current || (current->GetEntry() != NPC_VEKLOR && current->GetEntry() != NPC_VEKNILASH))
        return 1.0f;

    TwinsView view;
    if (!GetTwinsView(bot, view) || (current != view.veklor && current != view.veknilash))
        return 1.0f;

    if (IsTwinsHoldingFire(bot, botAI))
        return 0.0f;
    if (AI_VALUE(bool, "neglect threat"))
        return 1.0f;
    if (TwinsThreatVetoed(bot, current))
        return 0.0f;

    if (threatType != Action::ActionThreatType::Aoe || AI_VALUE2(uint8, "threat", "aoe") < 50)
        return 1.0f;
    // Upstream's AoE rule, for a capped bot only: latched, not a healer, not an active tank, and the twin has a
    // living designated tank to protect.
    TwinsMe const me = GetTwinsMe(bot);
    if (me.role == Aq40TwinsRules::Role::None || me.role == Aq40TwinsRules::Role::Healer)
        return 1.0f;
    Player* const vlTank = TwinsActiveTankFor(view, bot, AQT_DATA_VEKLOR);
    Player* const vnTank = TwinsActiveTankFor(view, bot, AQT_DATA_VEKNILASH);
    if (vlTank == bot || vnTank == bot)
        return 1.0f;
    Player* const tank = current == view.veklor ? vlTank : vnTank;
    return tank ? 0.0f : 1.0f;
}
