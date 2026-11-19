/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BWLMultipliers.h"
#include "AttackAction.h"
#include "BWLActions.h"
#include "BWLHelpers.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidBearActions.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "WarriorActions.h"

using namespace BlackwingLairHelpers;

static constexpr float VAELASTRASZ_BA_SAFE_DISTANCE = 20.0f;
static constexpr float VAELASTRASZ_BA_BOSS_DISTANCE = 30.0f;

float RazorgoreTankMultiplier::GetValue(Action* action)
{
    if (!PlayerbotAI::IsTank(bot))
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "razorgore the untamed");
    if (!boss)
        return 1.0f;

    if (AreRazorgoreEggsAlive(botAI))
    {
        // Off-tank picks up boss, blocks TankAssistAction to avoid changing targets
        if (IsRazorgoreOffTank(bot) && bot->GetVictim() != nullptr && dynamic_cast<TankAssistAction*>(action))
            return 0.0f;
        return 1.0f;
    }

    // All eggs destroyed. Tanks not targeted by the boss don't run into the Cleave.
    if (boss->GetVictim() != bot && dynamic_cast<TankFaceAction*>(action))
        return 0.0f;
    return 1.0f;
}

float VaelastraszTankMultiplier::GetValue(Action* action)
{
    if (!PlayerbotAI::IsTank(bot))
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "vaelastrasz the corrupt");
    if (!boss)
        return 1.0f;

    // Stay at rear flank position and don't move in front of the boss.
    if (dynamic_cast<TankFaceAction*>(action))
    {
        Unit* victim = boss->GetVictim();
        Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
        if (victim != bot && victimPlayer && PlayerbotAI::IsTank(victimPlayer))
            return 0.0f;
    }

    return 1.0f;
}

float VaelastraszBurningAdrenalineMultiplier::GetValue(Action* action)
{
    if (bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_BURNING_ADRENALINE)))
    {
        if (dynamic_cast<MovementAction*>(action))
        {
            if (dynamic_cast<BwlVaelastraszMoveAwayAction*>(action))
            {
                // Allow movement when boss is dead or bot was knocked away
                Unit* boss = AI_VALUE2(Unit*, "find target", "vaelastrasz the corrupt");
                if (!boss || !boss->IsAlive() || bot->GetDistance2d(boss) > VAELASTRASZ_BA_BOSS_DISTANCE)
                    return 1.0f;
            }

            // Block ALL other movement once bot is far enough from non-BA bots
            if (!IsNonBABotNearPosition(bot, bot->GetPosition(), VAELASTRASZ_BA_SAFE_DISTANCE) ||
                !dynamic_cast<BwlVaelastraszMoveAwayAction*>(action))
                return 0.0f;
        }
        // Also block charge actions
        if (dynamic_cast<CastReachTargetSpellAction*>(action))
            return 0.0f;
    }

    return 1.0f;
}

float NefarianCorruptedHealingMultiplier::GetValue(Action* action)
{
    // Priest call: a holy heal that is not a heal-over-time puts a damage-over-time effect on its target instead.
    if (bot->getClass() != CLASS_PRIEST ||
        !bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_CORRUPTED_HEALING)))
        return 1.0f;

    auto* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction)
        return 1.0f;

    static std::unordered_set<std::string> const directHeals = {
        "lesser heal", "heal", "greater heal", "flash heal", "binding heal", "penance", "prayer of healing",
        "circle of healing", "prayer of mending", "desperate prayer", "divine hymn", "holy nova"};
    std::string const spell = spellAction->getSpell();
    if (directHeals.count(spell))
        return 0.0f;

    // Empowered Renew adds an instant direct heal to Renew.
    if (spell == "renew" && (bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_EMPOWERED_RENEW_R1)) ||
                             bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_EMPOWERED_RENEW_R2)) ||
                             bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_EMPOWERED_RENEW_R3))))
        return 0.0f;

    // The glyph adds a direct heal to the shield.
    if (spell == "power word: shield" &&
        bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_GLYPH_OF_POWER_WORD_SHIELD)))
        return 0.0f;

    return 1.0f;
}

float NefarianCorruptedTotemMultiplier::GetValue(Action* action)
{
    // Keeps DPS on a corrupted totem until it dies instead of switching back to the dps, AoE or RTI target.
    if ((!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<DpsAoeAction*>(action) &&
         !dynamic_cast<AttackRtiTargetAction*>(action)) ||
        !PlayerbotAI::IsDps(bot))
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && target->IsAlive() && IsCorruptedTotem(target) ? 0.0f : 1.0f;
}

// Death Talon Wyrmguard packs (overlay patch 0020)
//
// EXACT-class matches only: MeleeAction derives from AttackAction and AttackAction from
// MovementAction, so a base-class veto would silence auto-attack. The cheap type tests run before
// the pack lookup - this is evaluated for every queued action of every bot on every tick.
// No IsInCombat gate (review C1): a bot holding fire has neither dealt nor taken damage, so it is
// out of combat - exactly the bot these vetoes exist for. Pack activation is the gate.
float BwlWyrmguardMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != BWL_MAP_ID)
        return 1.0f;

    std::string const& name = action->getName();
    if (name.rfind("bwl wyrmguard", 0) == 0 || name.rfind("bwl death talon wyrmguard", 0) == 0)
        return 1.0f;

    bool const isTargetChooser = dynamic_cast<DpsAssistAction*>(action) ||
                                 dynamic_cast<DpsAoeAction*>(action) ||
                                 dynamic_cast<TankAssistAction*>(action) ||
                                 dynamic_cast<AggressiveTargetAction*>(action) ||
                                 dynamic_cast<AttackAnythingAction*>(action) ||
                                 dynamic_cast<AttackLeastHpTargetAction*>(action) ||
                                 dynamic_cast<AttackRtiTargetAction*>(action);
    bool const isFormationMover = dynamic_cast<CombatFormationMoveAction*>(action) ||
                                  dynamic_cast<FollowAction*>(action);
    bool const isTaunt = EncounterHelpers::IsTauntAction(bot, action);
    // Threat limit (spec §4.8a): DPS only. Healing spells are Aoe-typed (CastHealingSpellAction), so
    // healers and any non-healer's heal are exempt - the limit must never stop a heal.
    bool const isThreatLimited = action->getThreatType() != Action::ActionThreatType::None &&
                                 !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsHeal(bot) &&
                                 !dynamic_cast<CastHealingSpellAction*>(action);
    if (!isTargetChooser && !isFormationMover && !isTaunt && !isThreatLimited)
        return 1.0f;

    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return 1.0f;

    if (PlayerbotAI::IsTank(bot))
    {
        int const role = GetTankRole(bot);
        if (role == BwlWyrmguardRules::NONE)
            return 1.0f;

        // AoE / ally taunts are never right during a pack: a Challenging Shout at the pull grabs
        // all three guards for 6 s (review I1).
        if (dynamic_cast<CastChallengingShoutAction*>(action) ||
            dynamic_cast<CastChallengingRoarAction*>(action) ||
            dynamic_cast<CastRighteousDefenseAction*>(action) ||
            dynamic_cast<CastDeathGripAction*>(action))
            return 0.0f;

        // Single-target taunts fire at the current target: never at a guard another role holds -
        // even after this tank's own guard is dead.
        if (isTaunt)
        {
            int const s = SlotOfGuard(view, AI_VALUE(Unit*, "current target"));
            return (s != BwlWyrmguardRules::NONE && view.assign.holderRole[s] != role) ? 0.0f : 1.0f;
        }

        bool holds = false;
        for (int s = 0; s < BwlWyrmguardRules::SLOTS; ++s)
            holds = holds || view.assign.holderRole[s] == role;
        if (!holds)
            return 1.0f;

        // A holding tank keeps its own guard and its station.
        if (dynamic_cast<TankAssistAction*>(action) || isFormationMover)
            return 0.0f;
        return 1.0f;
    }

    // Non-tanks: "bwl wyrmguard dps target" is the only target chooser.
    if (isTargetChooser)
        return 0.0f;

    // Held melee are not walked back into the pack by follow/formation movement.
    if (isFormationMover && IsWyrmguardMelee(bot) &&
        BwlWyrmguardRules::MeleeHeld(view.state, view.assign, view.skull, view.msSinceActive))
        return 0.0f;

    // Healers position themselves (spec 4.8a "healer position"): follow/formation movement would drag
    // them off their spot, and a moving healer cannot cast (prod 2026-10-08).
    if (isFormationMover && PlayerbotAI::IsHeal(bot))
        return 0.0f;

    // mod-playerbots never enables the "threat" strategy for DPS, so without this they pull the
    // guards off their tanks (dev pull 2026-10-06). Same rule as upstream ThreatMultiplier.
    if (isThreatLimited && !AI_VALUE(bool, "neglect threat"))
    {
        if (action->getThreatType() == Action::ActionThreatType::Aoe && AI_VALUE2(uint8, "threat", "aoe") >= 50)
            return 0.0f;
        // Single-target limit only on a pack Wyrmguard: an add no tank holds (e.g. Krixix) would read
        // at the cap and stay un-attacked for the whole pack. Own float ratio, not upstream's uint8
        // "threat" value, which wraps at 256 (a bot at 300 % read 44 %) - spec §4.8a. The gate carries
        // absolute headroom (BwlThreatBlocked, spec 2026-10-08), shared with the BWL-wide discipline.
        Unit* const current = AI_VALUE(Unit*, "current target");
        if (SlotOfGuard(view, current) != BwlWyrmguardRules::NONE &&
            BwlThreatBlocked(bot, current, BWL_THREAT_LIMIT))
            return 0.0f;
    }

    return 1.0f;
}

// Nefarian phases 2 and 3 (overlay patch 0020, spec 2026-10-07 §4.6)
//
// Mover / chooser / taunt matching is EXACT dynamic_cast (MeleeAction derives from AttackAction).
// Per-call vetoes match CastSpellAction::getSpell() against BwlNefarianRules' lists, so self, party and
// macro-generated variants are all caught. Cheap type tests run before the view lookup.
float BwlNefarianMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != BWL_MAP_ID)
        return 1.0f;
    // Upstream's own Nefarian action (#2979) shares our "bwl nefarian" prefix. Inside our gate it would
    // compete with this strategy's totem/target rules, so it is muted below. Upstream's "nefarian rear
    // flank" (BossRearFlankAction, #2736) is a RearFlankAction and falls under the mover veto.
    bool const upstreamNefAction = action->getName() == "bwl nefarian kill corrupted totem";
    if (!upstreamNefAction && action->getName().rfind("bwl nefarian", 0) == 0)
        return 1.0f;

    bool const isTargetChooser = dynamic_cast<DpsAssistAction*>(action) ||
                                 dynamic_cast<DpsAoeAction*>(action) ||
                                 dynamic_cast<TankAssistAction*>(action) ||
                                 dynamic_cast<AggressiveTargetAction*>(action) ||
                                 dynamic_cast<AttackAnythingAction*>(action) ||
                                 dynamic_cast<AttackLeastHpTargetAction*>(action) ||
                                 dynamic_cast<AttackRtiTargetAction*>(action);
    bool const isMover = dynamic_cast<CombatFormationMoveAction*>(action) ||
                         dynamic_cast<FollowAction*>(action) ||
                         dynamic_cast<RearFlankAction*>(action);
    bool const isAoeTaunt = dynamic_cast<CastChallengingShoutAction*>(action) ||
                            dynamic_cast<CastChallengingRoarAction*>(action) ||
                            dynamic_cast<CastRighteousDefenseAction*>(action) ||
                            dynamic_cast<CastDeathGripAction*>(action);
    bool const isTank = PlayerbotAI::IsTank(bot);
    bool const threatAction = action->getThreatType() != Action::ActionThreatType::None &&
                              !dynamic_cast<CastHealingSpellAction*>(action);
    bool const isThreatLimited = threatAction && !isTank && !PlayerbotAI::IsHeal(bot);
    bool const isTankThreat = threatAction && isTank;   // the MT is exempted below, once the view is known
    bool const isTaunt = EncounterHelpers::IsTauntAction(bot, action);
    CastSpellAction* const spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!isTargetChooser && !isMover && !isAoeTaunt && !isThreatLimited && !isTankThreat && !isTaunt &&
        !spellAction && !upstreamNefAction)
        return 1.0f;

    NefarianView view;
    if (!GetNefarianView(bot, view))
        return 1.0f;

    if (upstreamNefAction)
        return 0.0f;   // our "bwl nefarian dps target" owns the totems

    if (isMover)
        return 0.0f;   // positions are "bwl nefarian mt station" / "bwl nefarian position"
    if (isTargetChooser)
        return 0.0f;   // MT: the "co -tank assist" tip; OTs and DPS: our own choosers
    if (isAoeTaunt && bot == view.mainTank)
        return 0.0f;
    Unit* const current = (isTaunt || isThreatLimited || isTankThreat) ? AI_VALUE(Unit*, "current target") : nullptr;
    if (isTaunt && current == view.nefarian)
        return 0.0f;   // taunt-immune: a wasted GCD for any tank

    // DPS against the backup tank, off-tanks against the MT (spec §9 #2; NefThreatRatio). The MT reads 0.
    if ((isThreatLimited || (isTankThreat && bot != view.mainTank)) && !AI_VALUE(bool, "neglect threat"))
    {
        float const ratio = NefThreatRatio(bot, view);
        if (ratio >= BwlNefarianRules::THREAT_LIMIT &&
            (action->getThreatType() == Action::ActionThreatType::Aoe || current == view.nefarian))
            return 0.0f;
    }

    if (!spellAction)
        return 1.0f;
    std::string const spell = spellAction->getSpell();
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            if (bot->HasAura(BwlNefarianRules::SPELL_CALL_PRIEST) && BwlNefarianRules::IsCorruptedHealingSpell(spell))
                return 0.0f;
            break;
        case CLASS_DRUID:
            if (bot->HasAura(BwlNefarianRules::SPELL_CALL_DRUID) && !BwlNefarianRules::IsCatAbility(spell))
                return 0.0f;
            break;
        case CLASS_WARRIOR:
            if (bot->HasAura(BwlNefarianRules::SPELL_CALL_WARRIOR) && BwlNefarianRules::IsStanceSpell(spell))
                return 0.0f;
            break;
        case CLASS_HUNTER:
            if (HasBrokenRangedWeapon(bot) && BwlNefarianRules::IsHunterShot(spell))
                return 0.0f;
            break;
        case CLASS_SHAMAN:
            if (BwlNefarianRules::IsNonTremorEarthTotem(spell))
                return 0.0f;
            break;
        default:
            break;
    }
    return 1.0f;
}

// BWL-wide DPS threat discipline (overlay patch 0020, spec 2026-10-08)
//
// mod-playerbots never enables the "threat" strategy for DPS, and Chromaggus is taunt-immune: a boomkin
// pulled him first and he bounced between DPS (prod 2026-10-08). The Wyrmguard rule, generalized to every
// BWL fight without its own threat rules. Cheap type and role tests run before any value or view lookup:
// this is evaluated for every queued action of every bot on every tick.
float BwlThreatDisciplineMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != BWL_MAP_ID)
        return 1.0f;

    Action::ActionThreatType const threatType = action->getThreatType();
    // Exact classes (none has a subclass): no threat type, but they are what restart auto-attack,
    // auto-shot and the wand. Without them the hard stop at BWL_THREAT_STOP would be undone next tick.
    bool const isAutoAttack = dynamic_cast<MeleeAction*>(action) || dynamic_cast<CastAutoShotAction*>(action) ||
                              dynamic_cast<CastShootAction*>(action);
    if (threatType == Action::ActionThreatType::None && !isAutoAttack)
        return 1.0f;
    // Heals are Aoe-typed (CastHealingSpellAction) and must never be limited, whoever casts them.
    if (dynamic_cast<CastHealingSpellAction*>(action) || !IsBwlThreatDisciplined(bot))
        return 1.0f;
    if (AI_VALUE(bool, "neglect threat"))
        return 1.0f;

    Unit* const current = AI_VALUE(Unit*, "current target");
    if (!IsBwlThreatDisciplineTarget(bot, current))
        return 1.0f;

    // Same AoE rule as upstream ThreatMultiplier. The single target uses the shared float gate with
    // absolute headroom (BwlThreatBlocked), not upstream's uint8 "threat" value, which wraps at 256.
    bool const veto =
        (threatType == Action::ActionThreatType::Aoe && AI_VALUE2(uint8, "threat", "aoe") >= 50) ||
        BwlThreatBlocked(bot, current, BWL_THREAT_LIMIT);
    if (!veto || BwlFightHasOwnThreatRules(bot))
        return 1.0f;
    return 0.0f;
}
