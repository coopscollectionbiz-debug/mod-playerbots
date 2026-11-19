/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Princess Huhuran multipliers (overlay patch 0016). Spec:
// docs/superpowers/specs/2026-10-20-playerbot-aq40-huhuran-design.md (overlay repo), §4.2-4.3.

#include "Aq40Multipliers.h"

#include "Aq40Helpers.h"
#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"

#include <string>
#include <unordered_set>

using namespace TempleOfAhnQirajHelpers;

namespace
{
    bool IsHuhuranAction(Action* action) { return action->getName().rfind("aq40 huhuran", 0) == 0; }
}

// The C'Thun in-combat mover set (Aq40CthunMovementMultiplier). Type tests before any view lookup: this runs
// for every queued action of every bot on every tick. NEVER blanket-veto MovementAction: AttackAction and
// MeleeAction derive from it, and that would silence auto-attack.
float Aq40HuhuranMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID || IsHuhuranAction(action))
        return 1.0f;

    bool const isGenericMover = dynamic_cast<CombatFormationMoveAction*>(action) ||
                                dynamic_cast<RearFlankAction*>(action) ||
                                dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<FollowAction*>(action);
    if (!isGenericMover)
        return 1.0f;

    HuhuranView view;
    if (!GetHuhuranView(bot, view))
        return 1.0f;

    // A ring holder, on its spot OR still walking to it: the veto must not fail open while travelling, or
    // follow/formation pulls it off course every tick.
    Position spot;
    if (GetHuhuranSpot(bot, botAI, view, spot))
        return 0.0f;

    // An off-tank with a melee spot beside the MT (rev 2, spec §7.2 #3), on it or walking to it: "aq40 huhuran
    // off tank engage" owns its movement. Never while the opener hold runs (no spot is used then).
    if (PlayerbotAI::IsTank(bot) && bot != view.mainTank && !HuhuranOpenerHold(view) &&
        GetHuhuranOffTankSpot(bot, view, spot))
        return 0.0f;

    // The MT holds position once she is parked (rev 7, spec §12.2 #2): stock "tank face" put her back to the
    // raid, but the rings surround her, so pull 6's MT circled her (834 runs, 24 moves). "aq40 huhuran park
    // boss" turns it in place instead. Unparked, it owns its movement while it has work (the drag).
    if (bot == view.mainTank &&
        (view.parked || GetHuhuranMtStep(bot, view, AI_VALUE(Unit*, "current target")) != HuhuranMtStep::None))
        return 0.0f;

    return 1.0f;
}

// The opener hold (rev 2, spec §7.2 #1): the Nefarian hold-fire set (BwlNefarianMultiplier) - the generic target
// choosers plus the melee / auto-shot / wand starters - and, for a bot already on her, its threat-bearing
// non-heal actions. Non-tank bots only, until she has been parked once this attempt. Time-bounded by
// HoldFireOnHuhuran's 45 s fail-open, so this is never a permanent targeting veto.
float Aq40HuhuranTargetHoldMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID || IsHuhuranAction(action))
        return 1.0f;

    // Exact classes, never AttackAction (MeleeAction and our own actions derive from it).
    bool const isTargetChooser = dynamic_cast<DpsAssistAction*>(action) ||
                                 dynamic_cast<DpsAoeAction*>(action) ||
                                 dynamic_cast<TankAssistAction*>(action) ||
                                 dynamic_cast<AggressiveTargetAction*>(action) ||
                                 dynamic_cast<AttackAnythingAction*>(action) ||
                                 dynamic_cast<AttackLeastHpTargetAction*>(action) ||
                                 dynamic_cast<AttackRtiTargetAction*>(action);
    bool const isAutoAttack = dynamic_cast<MeleeAction*>(action) || dynamic_cast<CastAutoShotAction*>(action) ||
                              dynamic_cast<CastShootAction*>(action);
    // Heals are Aoe-typed (CastHealingSpellAction) and never held.
    bool const isThreatAction = action->getThreatType() != Action::ActionThreatType::None &&
                                !dynamic_cast<CastHealingSpellAction*>(action);
    if (!isTargetChooser && !isAutoAttack && !isThreatAction)
        return 1.0f;
    if (PlayerbotAI::IsTank(bot))
        return 1.0f;

    // Attack starters and threat actions are held only against her: cheap entry test before the view.
    Unit* current = nullptr;
    if (!isTargetChooser)
    {
        current = AI_VALUE(Unit*, "current target");
        if (!current || current->GetEntry() != NPC_HUHURAN)
            return 1.0f;
    }

    HuhuranView view;
    if (!GetHuhuranView(bot, view) || !HuhuranOpenerHold(view))
        return 1.0f;

    // She is the only hostile in her room: every generic chooser would hand her straight back.
    if (isTargetChooser)
        return 0.0f;
    return current == view.huhuran ? 0.0f : 1.0f;
}

// Taunt-immune (spec §4.2): a taunt at her is a wasted GCD for any tank. Copied from BWL's Nefarian veto
// (BwlNefarianMultiplier): taunt-type actions (EncounterHelpers::IsTauntAction, tanks only) while she is the
// current target. Encounter only: the view gate fails outside it.
float Aq40HuhuranTauntMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID || !EncounterHelpers::IsTauntAction(bot, action))
        return 1.0f;

    Unit* const current = AI_VALUE(Unit*, "current target");
    if (!current || current->GetEntry() != NPC_HUHURAN)
        return 1.0f;

    HuhuranView view;
    if (!GetHuhuranView(bot, view))
        return 1.0f;
    return current == view.huhuran ? 0.0f : 1.0f;
}

// Rev 2 (spec §7.2 #2): threat discipline after the opener hold, mirroring BwlThreatDisciplineMultiplier
// (copied, not called): the same veto set - every threat-typed action plus the exact auto-attack restarters
// (MeleeAction, CastAutoShotAction, CastShootAction, none of which has a subclass), heals never - against
// her only, for DPS and off-tanks (HuhuranThreatDisciplined: not a healer, not the MT). Order MT > backup > DPS
// (fix round 1, R11, the Nefarian rule): off-tanks against the MT's threat at 1.00 (rev 3, §8.2 #3), DPS
// against the top living off-tank's (fallback the MT's) at 0.80; veto at own + 1500 >= ratio x denominator,
// plus upstream's AoE rule. In Berserk all go against the MT with no headroom: off-tanks 1.05, DPS 1.00 (rev 5
// §10.3, R14). No cap when the denominator is 0. Cheap type, role and target tests before the view: this runs
// for every queued action of every bot on every tick.
float Aq40HuhuranThreatMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != AQ40_MAP_ID || IsHuhuranAction(action))
        return 1.0f;

    Action::ActionThreatType const threatType = action->getThreatType();
    bool const isAutoAttack = dynamic_cast<MeleeAction*>(action) || dynamic_cast<CastAutoShotAction*>(action) ||
                              dynamic_cast<CastShootAction*>(action);
    if (threatType == Action::ActionThreatType::None && !isAutoAttack)
        return 1.0f;
    // Heals are Aoe-typed (CastHealingSpellAction) and must never be limited, whoever casts them.
    if (dynamic_cast<CastHealingSpellAction*>(action) || PlayerbotAI::IsHeal(bot))
        return 1.0f;
    if (AI_VALUE(bool, "neglect threat"))
        return 1.0f;

    Unit* const current = AI_VALUE(Unit*, "current target");
    if (!current || current->GetEntry() != NPC_HUHURAN)
        return 1.0f;

    HuhuranView view;
    if (!GetHuhuranView(bot, view) || current != view.huhuran || !HuhuranThreatDisciplined(bot, view))
        return 1.0f;
    float const denom = HuhuranThreatDenominator(bot, view);
    if (denom <= 0.0f)
        return 1.0f;  // no tank threat on her to protect: no cap

    bool const veto = (threatType == Action::ActionThreatType::Aoe && AI_VALUE2(uint8, "threat", "aoe") >= 50) ||
                      HuhuranThreatVetoed(bot, view);
    return veto ? 0.0f : 1.0f;
}

// Rev 3 (spec §8.2 #1): offensive, raid, racial, trinket and tank cooldowns are held for Berserk. Pull 2 spent
// every one between 20 and 90 s (the stock boost triggers have no phase input) and the MT had nothing left
// when she went berserk. Exact registered action names (each checked against its context creator / Action
// constructor); each bot's own strategies decide which it actually has. Mana cooldowns (innervate, mana tide,
// potions, healthstone) and heals are deliberately absent. Bots only (not selfbots); 1.0 whenever the view
// is inert or Berserk is up, so the stock triggers then fire them as usual.
float Aq40HuhuranCooldownHoldMultiplier::GetValue(Action* action)
{
    static std::unordered_set<std::string> const kHeld = {
        // DPS cooldowns
        "death wish", "recklessness", "blade flurry", "adrenaline rush", "arcane power", "icy veins", "combustion",
        "presence of mind", "mirror image", "rapid fire", "bestial wrath", "elemental mastery", "power infusion",
        "avenging wrath", "divine favor", "berserk", "cold blood",
        // raid cooldowns
        "bloodlust", "heroism",
        // racials
        "blood fury", "berserking",
        // trinkets
        "use trinket",
        // tank defensives. barkskin / divine protection (rev 3 fix round 1, R13): a bear MT's stock "almost full
        // health" barkskin fired every ~60 s, so it was on cooldown at Berserk and the MT defensive could not fire.
        "shield wall", "last stand", "every man for himself", "barkskin", "divine protection"};

    if (!action || bot->GetMapId() != AQ40_MAP_ID || IsSelfBot(bot))
        return 1.0f;
    if (!kHeld.count(action->getName()))
        return 1.0f;

    HuhuranView view;
    if (!GetHuhuranView(bot, view) || view.berserk)
        return 1.0f;
    return 0.0f;
}
