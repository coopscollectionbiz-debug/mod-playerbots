/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxMultipliers.h"

#include <algorithm>

#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "DruidShapeshiftActions.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "Group.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "Naxx40Anub.h"
#include "Naxx40Horsemen.h"
#include "Naxx40Loatheb.h"
#include "Naxx40Patchwerk.h"
#include "NaxxActions.h"
#include "ReachTargetActions.h"
#include "NaxxHelpers.h"
#include "NaxxSpellIds.h"
#include "PaladinActions.h"
#include "PetsAction.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "RtiTargetValue.h"
#include "ScriptedCreature.h"
#include "ShamanActions.h"
#include "Spell.h"
#include "UseMeetingStoneAction.h"
#include "WarriorActions.h"
#include "WipeAction.h"

float GrobbulusMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
        return 1.0f;

    if (dynamic_cast<AvoidAoeAction*>(action))
        return botAI->IsMainTank(bot) ? 0.0f : 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    // Naxx40: with Grobbulus on him, the main tank waits at his wall waypoint for the boss instead of running out to
    // meet him. A Warrior's Charge + "reach melee" met him mid-room and the first sprays faced the raid (Fallout Slime
    // 25, wipe at 76%, tr-20261122-113921-5); in the clean kills the boss walked to the tank at the wall.
    if (boss->GetEntry() == 351003 && botAI->IsMainTank(bot) && boss->GetVictim() == bot &&
        bot->GetDistance(boss) > 5.0f)
    {
        std::string const& name = action->getName();
        if (name == "charge" || name == "intercept" || name == "reach melee" || name == "feral charge - bear")
            return 0.0f;
    }

    return 1.0f;
}

float HeiganDanceMultiplier::GetValue(Action* action)
{
    // Cheap action-type checks first; the encounter state is only looked up for actions we may have to block.
    if (dynamic_cast<HeiganDanceAction*>(action) || dynamic_cast<CurePartyMemberAction*>(action) ||
        dynamic_cast<WipeAction*>(action))
        return 1.0f;

    bool repositions = dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FleeAction*>(action) ||
                       dynamic_cast<CastDisengageAction*>(action) || dynamic_cast<CastBlinkBackAction*>(action);
    bool moves = dynamic_cast<MovementAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action);
    auto* spellAction = dynamic_cast<CastSpellAction*>(action);
    bool timedCast = spellAction && !dynamic_cast<CastMeleeSpellAction*>(action);
    if (!repositions && !moves && !timedCast)
        return 1.0f;

    if (!helper.UpdateBossAI())
        return 1.0f;

    // Generic repositioning must never pull a bot off its safe spot or off the platform.
    if (repositions)
        return 0.0f;

    // Ranged bots on the platform during the slow dance are free to act as usual.
    if (!helper.ShouldDance())
        return 1.0f;

    // Dancing: only the dance moves us (charge/intercept/feral charge included - during the fast dance the boss
    // stands in his Plague Cloud). Everything that is not a cast is fine (target selection, facing, ...).
    if (moves)
        return 0.0f;

    // Casts are allowed while standing on the safe spot with enough time left before the next eruption.
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

    return helper.CanStandStillFor(castTime + 500) ? 1.0f : 0.0f;
}

float LoathebGenericMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "loatheb");
    if (!boss)
        return 1.0f;

    context->GetValue<bool>("neglect threat")->Set(true);
    if (botAI->GetState() == BOT_STATE_COMBAT &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) || dynamic_cast<FleeAction*>(action) ||
         dynamic_cast<CombatFormationMoveAction*>(action)))
    {
        return 0.0f;
    }
    if (!dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::NecroticAura10});
    if (!aura)
    {
        // Fallback to name for custom spell data.
        aura = botAI->GetAura("necrotic aura", bot);
    }
    if (!aura || aura->GetDuration() <= 1500)
        return 1.0f;

    return 0.0f;
}

float ThaddiusGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;
    // pet phase
    if (helper.IsPhasePet() &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) ||
         dynamic_cast<ReachPartyMemberToHealAction*>(action) || dynamic_cast<BuffOnMainTankAction*>(action)))
    {
        return 0.0f;
    }
    // die at the same time
    Unit* target = AI_VALUE(Unit*, "current target");
    Unit* feugen = AI_VALUE2(Unit*, "find target", "feugen");
    Unit* stalagg = AI_VALUE2(Unit*, "find target", "stalagg");
    // 11-26: opener discipline. A rogue and a hunter opened on the pets before their tanks (tr-20261126-173155-3:
    // Feugen on Rabiosoo at t0, then chasing healers 75 yd off his platform -> Tesla Coil Shock x29). A non-tank does
    // no damage to a pet until a tank holds it; healing is unaffected.
    if (helper.IsPhasePet() && target && (target == feugen || target == stalagg) && !botAI->IsTank(bot))
    {
        Unit* victim = target->GetVictim();
        Player* vp = victim ? victim->ToPlayer() : nullptr;
        bool const tankHolds = vp && GET_PLAYERBOT_AI(vp) ? GET_PLAYERBOT_AI(vp)->IsTank(vp) : false;
        if (!tankHolds && !dynamic_cast<CastHealingSpellAction*>(action) &&
            (dynamic_cast<AttackAction*>(action) || dynamic_cast<CastSpellAction*>(action)))
            return 0.0f;
    }
    if (helper.IsPhasePet() && target && feugen && stalagg && target->GetHealthPct() <= 40 &&
        (feugen->GetHealthPct() >= target->GetHealthPct() + 3 || stalagg->GetHealthPct() >= target->GetHealthPct() + 3))
    {
        if (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastHealingSpellAction*>(action))
            return 0.0f;
    }
    // magnetic pull
    // uint32 curr_timer = eventMap->GetTimer();
    // // if (curr_phase == 2 && bot->GetPositionZ() > 312.5f && dynamic_cast<MovementAction*>(action))
    // {
    // if (curr_phase == 2 && (curr_timer % 20000 >= 18000 || curr_timer % 20000 <= 2000) &&
    // dynamic_cast<MovementAction*>(action))
    // {
    //     // MotionMaster *mm = bot->GetMotionMaster();
    //     // mm->Clear();
    //     return 0.0f;
    // }
    // thaddius phase
    // if (curr_phase == 8 && dynamic_cast<FleeAction*>(action))
    // {
    //         return 0.0f;
    // }
    return 1.0f;
}

float SapphironGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    if (dynamic_cast<CastDeathGripAction*>(action) || dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    return 1.0f;
}

float InstructorRazuviousGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    context->GetValue<bool>("neglect threat")->Set(true);
    // Naxx40: ranged and healer mana users hold outside Disrupting Shout (26046, 30 yd, instant, every 25 s); do not
    // let a spell's range walk them back in (RazuviousTargetAction keeps them at 34 yd).
    if (Unit* razuvious = AI_VALUE2(Unit*, "find target", "instructor razuvious"))
        if (razuvious->GetEntry() == 351036 && razuvious->IsInCombat() && bot->IsInCombat() &&
            bot->getPowerType() == POWER_MANA &&
            (bot->getClass() == CLASS_PRIEST ? (botAI->IsHeal(bot) && !bot->GetCharmGUID()) : (botAI->IsRanged(bot) || botAI->IsHeal(bot))) &&
            dynamic_cast<ReachTargetAction*>(action))
            return 0.0f;
    if (botAI->GetState() == BOT_STATE_COMBAT &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
         dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action)))
    {
        return 0.0f;
    }
    return 1.0f;
}

float KelthuzadGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    if ((dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) || dynamic_cast<FleeAction*>(action)))
    {
        return 0.0f;
    }
    if (helper.IsPhaseOne())
    {
        // IP Naxx40: Raid Clear's off-tank pickup and DungeonClear's camp assist put tanks and melee on Soldiers, whose
        // Dark Blast procs on their own melee hit and hits everyone within 100 yd (KT P1 diag 11-22). Target choice in
        // phase 1 belongs to "kel'thuzad choose target".
        if (Unit* kt = AI_VALUE2(Unit*, "find target", "kel'thuzad"))
            if (kt->GetEntry() != 15990 &&
                (action->getName() == "rc offtank target" || action->getName() == "dungeon clear assist camp"))
                return 0.0f;
        if (dynamic_cast<CastTotemAction*>(action) || dynamic_cast<CastShadowfiendAction*>(action) ||
            dynamic_cast<CastRaiseDeadAction*>(action) || dynamic_cast<CastFeignDeathAction*>(action) ||
            dynamic_cast<CastInvisibilityAction*>(action) || dynamic_cast<CastVanishAction*>(action) ||
            dynamic_cast<PetAttackAction*>(action))
        {
            return 0.0f;
        }
    }
    if (helper.IsPhaseTwo())
    {
        if (dynamic_cast<CastBlizzardAction*>(action) || dynamic_cast<CastFrostNovaAction*>(action))
            return 0.0f;

    }
    return 1.0f;
}

float AnubrekhanGenericMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anub'rekhan");
    if (!boss)
        return 1.0f;

    if (NaxxSpellIds::HasAnyAura(
            boss, {NaxxSpellIds::LocustSwarm10, NaxxSpellIds::LocustSwarm10Alt, NaxxSpellIds::LocustSwarm25}) ||
        botAI->HasAura("locust swarm", boss))
    {
        if (dynamic_cast<FleeAction*>(action))
            return 0.0f;
    }
    return 1.0f;
}

float FourHorsemenGenericMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sir zeliek");
    if (!boss)
        return 1.0f;
    // Naxx40 Sir Zeliek: the Naxx40Horsemen strategy owns that fight (see FourHorsemenBossHelper::UpdateBossAI).
    if (boss->GetEntry() == NAXX40_ZELIEK_ENTRY)
        return 1.0f;

    context->GetValue<bool>("neglect threat")->Set(true);
    if ((dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action)))
        return 0.0f;

    return 1.0f;
}

// Naxx40 Four Horsemen (overlay patch 0022; spec 2026-10-27 §4-§5). Every latched bot (a tank in a pair, resting or
// active; a spare tank; a DPS half; a healer group) is placed by the strategy's own actions, which return false once
// on their spot so the rotation runs: the stock choosers and movers below would otherwise re-target or walk the bot
// the moment they fall through (memory: custom targeting needs a veto). Unlatched bots keep stock behaviour.
//   - target choosers (all of ChooseTargetActions.h that apply in PvE, plus "attack my target"): vetoed;
//   - movers: tank face, follow, combat formation, reach melee, charge-type reach spells: vetoed. Stock "avoid aoe"
//     is NOT touched (ranged and healers rely on it for Blaumeux's Void Zones), nor "reach spell";
//   - healers: "reach party member to heal" (stock heals cover the raid subgroup, spread over four corners: it would
//     walk a healer into another horseman's marks); DPS and healers: debuff-on-attacker (it picks any attacker, i.e.
//     another horseman; the upstream Thaddius precedent);
//   - "caster form" (latched tanks only): every druid heal / cure / buff node has it as a prerequisite, so a druid
//     tank healing between turns left bear form and its Growl failed when its turn came. The heal itself then fails
//     on the form (NOT_SHAPESHIFT) and the engine moves on; the tank action re-shifts a druid that is out of form;
//   - "pet attack": vetoed while the owner is off its horseman (a DPS / healer whose turn is the safe spot, a tank that
//     stopped attacking); StopAttacking in NaxxActions_Horsemen40.cpp calls the pet back;
//   - threat (latched DPS only): on a horseman, every threat-generating action plus the auto-attack restarters is
//     vetoed while Naxx40HorsemenRules::ThreatBlocked(own, active tank) holds (THREAT_HEADROOM included); AoE above
//     the stock 50 % aoe threat. "naxx40 horsemen threat hold" stops an attack already running. Heals are never
//     limited.
// Mover / chooser matching is EXACT dynamic_cast on the stock classes (MeleeAction derives from AttackAction, so a
// base-class veto would silence auto-attack and our own actions); ours are exempt by name first. The cheap type tests
// run before the view lookup: this is evaluated for every queued action of every bot on every tick.
float Naxx40HorsemenMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != Naxx40Horsemen::NAXX40_MAP_ID)
        return 1.0f;
    if (action->getName().rfind("naxx40 horsemen", 0) == 0)
        return 1.0f;

    bool const isTargetChooser = dynamic_cast<DpsAssistAction*>(action) ||
                                 dynamic_cast<DpsAoeAction*>(action) ||
                                 dynamic_cast<TankAssistAction*>(action) ||
                                 dynamic_cast<AggressiveTargetAction*>(action) ||
                                 dynamic_cast<AttackAnythingAction*>(action) ||
                                 dynamic_cast<AttackLeastHpTargetAction*>(action) ||
                                 dynamic_cast<AttackRtiTargetAction*>(action) ||
                                 dynamic_cast<AttackMyTargetAction*>(action);
    bool const isMover = dynamic_cast<TankFaceAction*>(action) ||
                         dynamic_cast<FollowAction*>(action) ||
                         dynamic_cast<CombatFormationMoveAction*>(action) ||
                         dynamic_cast<ReachMeleeAction*>(action) ||
                         dynamic_cast<CastReachTargetSpellAction*>(action);
    bool const isReachToHeal = dynamic_cast<ReachPartyMemberToHealAction*>(action) != nullptr;
    bool const isDebuffOnAttacker = dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) != nullptr;
    bool const isPetAttack = dynamic_cast<PetAttackAction*>(action) != nullptr;
    bool const isCasterForm = dynamic_cast<CastCasterFormAction*>(action) != nullptr;
    Action::ActionThreatType const threatType = action->getThreatType();
    // No threat type, but they restart auto-attack, auto-shot and the wand (the BWL threat discipline's list).
    bool const isAutoAttack = dynamic_cast<MeleeAction*>(action) || dynamic_cast<CastAutoShotAction*>(action) ||
                              dynamic_cast<CastShootAction*>(action);
    bool const isThreatAction = (threatType != Action::ActionThreatType::None || isAutoAttack) &&
                                !dynamic_cast<CastHealingSpellAction*>(action);
    if (!isTargetChooser && !isMover && !isReachToHeal && !isDebuffOnAttacker && !isPetAttack && !isCasterForm &&
        !isThreatAction)
        return 1.0f;

    Naxx40Horsemen::HorsemenView view;
    if (!Naxx40Horsemen::GetView(bot, view))
        return 1.0f;

    bool iAmActive = false;
    bool const tank = Naxx40Horsemen::MyPair(bot, view, iAmActive) != Naxx40HorsemenRules::Horse::None ||
                      Naxx40Horsemen::IsSpareTank(bot, view);
    bool const dps = !tank && Naxx40Horsemen::MyDpsHalf(bot, view) != 0xFF;
    bool const healer = !tank && !dps && Naxx40Horsemen::MyHealerGroup(bot, view) != 0xFF;
    if (!tank && !dps && !healer)
        return 1.0f;

    if (isTargetChooser || isMover)
        return 0.0f;
    if (isCasterForm)
        return tank ? 0.0f : 1.0f;
    if (isReachToHeal && healer)
        return 0.0f;
    if (isDebuffOnAttacker && (dps || healer))
        return 0.0f;

    if (isPetAttack)
    {
        bool const ownerOff = tank ? !bot->GetVictim()
                                   : Naxx40Horsemen::CurrentHorse(bot) == Naxx40HorsemenRules::Horse::None;
        return ownerOff ? 0.0f : 1.0f;
    }

    // Threat: latched DPS only (tanks build it, healers' heals are exempt above).
    if (!isThreatAction || !dps || AI_VALUE(bool, "neglect threat"))
        return 1.0f;
    if (threatType == Action::ActionThreatType::Aoe && AI_VALUE2(uint8, "threat", "aoe") >= 50)
        return 0.0f;
    Unit* const current = AI_VALUE(Unit*, "current target");
    if (!current)
        return 1.0f;
    for (uint8 i = 0; i < 4; ++i)
    {
        if (view.horse[i] != current)
            continue;
        // No living active tank on him: no cap (nothing to stay under).
        Player* const activeTank = Naxx40Horsemen::ActiveTank(bot, view, static_cast<Naxx40HorsemenRules::Horse>(i));
        if (!activeTank || activeTank == bot)
            return 1.0f;
        ThreatManager& threat = current->GetThreatMgr();
        return Naxx40HorsemenRules::ThreatBlocked(threat.GetThreat(bot), threat.GetThreat(activeTank)) ? 0.0f : 1.0f;
    }
    return 1.0f;
}

// Naxx40 Loatheb (overlay patch 0022; spec 2026-10-30 §4.2, §4.6, §4.8). Inside the gate only:
//   - our own "naxx40 loatheb *" actions: untouched (exempt by name first; they cast through PlayerbotAI::CastSpell);
//   - latched bot healers (Naxx40Loatheb::MyHealerJob): every stock CastSpellAction whose spell is positive
//     (SpellInfo::IsPositive: heals, HoTs, shields, buffs, friendly dispels) is vetoed, since any positive spell procs
//     the Corrupted Mind lockout and spends the healer's one heal per 20 s; "reach party member to heal" too (the
//     Horsemen precedent: with the stock heals vetoed it only walks a healer off its spot);
//   - a DPS bot with a spore to kill (Naxx40LoathebAi::SporeToKill, spec §4.8 item 3): upstream's "loatheb choose
//     target" is vetoed, so it does not pull the bot back to Loatheb while that spore lives and still qualifies.
//     Upstream's "loatheb position" stays on: the spore walks to the bot, nobody runs to it;
//   - any bot, tanks included, with a live spore within SPORE_CHOOSER_VETO: upstream's "loatheb choose target" is
//     vetoed, so its 1 y spore pick never fires and the bot keeps its current (Loatheb) target. IP's spore is a
//     hostile mob that walks into the tank or melee, and a tank that killed it would take Fungal Creep's threat
//     wipe;
//   - every bot (spec §4.9 item 4): the slows, roots and freezes in kLoathebNoSlowSpells are vetoed, since they stop a
//     spore walking to the ranged stack (pull 3: mages Frost Nova'd spores short of it). Exact spell-name match.
// The cheap type tests run before the view lookup: this is evaluated for every queued action of every bot.
namespace
{
    char const* const kLoathebNoSlowSpells[] = {
        "frost nova", "cone of cold", "frost shock", "earthbind totem", "concussive shot",
        "wing clip",  "frost trap",   "freezing trap", "hamstring",     "piercing howl",
    };

    bool IsLoathebNoSlowSpell(std::string const& spell)
    {
        for (char const* name : kLoathebNoSlowSpells)
            if (spell == name)
                return true;
        return false;
    }
}

float Naxx40LoathebMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != Naxx40Loatheb::NAXX40_MAP_ID)
        return 1.0f;
    if (action->getName().rfind("naxx40 loatheb", 0) == 0)
        return 1.0f;

    uint8 const cls = bot->getClass();
    bool const healerClass = cls == CLASS_PALADIN || cls == CLASS_DRUID || cls == CLASS_PRIEST || cls == CLASS_SHAMAN;
    // The classes that carry a kLoathebNoSlowSpells spell (shamans are already a healer class).
    bool const slowClass = cls == CLASS_MAGE || cls == CLASS_HUNTER || cls == CLASS_WARRIOR;
    auto* const spellAction = healerClass || slowClass ? dynamic_cast<CastSpellAction*>(action) : nullptr;
    bool const isSlow = spellAction && IsLoathebNoSlowSpell(spellAction->getSpell());
    bool const isReachToHeal = healerClass && dynamic_cast<ReachPartyMemberToHealAction*>(action) != nullptr;
    bool const isChooser = dynamic_cast<LoathebChooseTargetAction*>(action) != nullptr;
    if (!isSlow && !(healerClass && spellAction) && !isReachToHeal && !isChooser)
        return 1.0f;

    Naxx40Loatheb::LoathebView view;
    if (!Naxx40Loatheb::GetView(bot, view))
        return 1.0f;

    if (isSlow)
        return 0.0f;

    if (isChooser)
    {
        if (Naxx40LoathebAi::SporeToKill(bot, botAI, view))
            return 0.0f;
        return Naxx40LoathebAi::SporeWithin(bot, Naxx40LoathebRules::SPORE_CHOOSER_VETO) ? 0.0f : 1.0f;
    }

    if (Naxx40Loatheb::MyHealerJob(bot, view) == Naxx40Loatheb::HealerJob::None)
        return 1.0f;
    if (isReachToHeal)
        return 0.0f;
    uint32 const spellId = AI_VALUE2(uint32, "spell id", spellAction->getSpell());
    SpellInfo const* const info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return info && info->IsPositive() ? 0.0f : 1.0f;
}

float Naxx40AnubMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != Naxx40Anub::NAXX40_MAP_ID)
        return 1.0f;
    bool const isClump = dynamic_cast<AnubrekhanPositionAction*>(action) != nullptr;
    bool const isFormation = dynamic_cast<CombatFormationMoveAction*>(action) != nullptr;
    bool const isReachMelee = dynamic_cast<ReachMeleeAction*>(action) != nullptr;
    bool const isMelee = dynamic_cast<MeleeAction*>(action) != nullptr;
    if (!isClump && !isFormation && !isReachMelee && !isMelee)
        return 1.0f;
    Naxx40Anub::AnubView view;
    if (!Naxx40Anub::GetView(bot, view))
        return 1.0f;
    Naxx40AnubRules::Slot const* const slot = Naxx40Anub::SlotOf(bot, view);
    if (!slot)
        return 1.0f;
    // Upstream's swarm clump, and a MultiBot "disperse" formation that would ping-pong with the spread.
    if (isClump || isFormation)
        return 0.0f;
    // Melee on their arc slot during the swarm: no reach-melee or melee chase back into it toward him. Both are vetoed
    // only when aimed at him: a melee bot that retargets the Crypt Guard summoned 3 s into the swarm is no longer
    // positioned (Naxx40Anub::SpotFor) and must still be free to reach the guard.
    if (!slot->melee || !view.swarm)
        return 1.0f;
    return action->GetTarget() == view.anub ? 0.0f : 1.0f;
}

// Naxx40 Patchwerk (overlay patch 0022; spec 2026-11-01 §3.1-§3.2, §3.4). Inside the gate only, bots only (never a
// self bot); our own "naxx40 patchwerk *" actions are exempt by name:
//   1. opener hold (view.openerHold: until the roles latch, failing open after OPENER_FAIL_OPEN_MS), non-tanks
//      (PlayerbotAI::IsTank) only: the 7 generic target choosers, and the exact auto-attack restarters (MeleeAction,
//      CastAutoShotAction, CastShootAction) plus threat-typed non-heal actions aimed at him or AoE-typed (the Huhuran
//      target hold);
//   2. after it: a capped bot (Naxx40Patchwerk::CapOf) whose current target is him has the auto restarters and every
//      threat-typed non-heal action aimed at him (Action::GetTarget) or AoE-typed vetoed while
//      Naxx40PatchwerkRules::ThreatVetoed holds (THREAT_HEADROOM included); self-cast defensives and buffs never are.
//      AoE also at upstream's "threat aoe" >= 50 (copied from BwlThreatDisciplineMultiplier, not called). "naxx40
//      patchwerk threat hold" stops an attack already running;
//   3. every role tank other than the MT (once latched): taunts (EncounterHelpers::IsTauntAction) at him are vetoed,
//      so a soaker, spare, resurrected or late tank running stock tank logic never rips him off the MT (a tank that
//      takes him anyway becomes MT).
// Exact classes, never AttackAction (MeleeAction and our own actions derive from it). The cheap type tests run before
// the view lookup: this is evaluated for every queued action of every bot on every tick. Never gates on IsInCombat.
float Naxx40PatchwerkMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != Naxx40Patchwerk::NAXX40_MAP_ID)
        return 1.0f;
    if (action->getName().rfind("naxx40 patchwerk", 0) == 0)
        return 1.0f;

    bool const isTargetChooser = dynamic_cast<DpsAssistAction*>(action) ||
                                 dynamic_cast<DpsAoeAction*>(action) ||
                                 dynamic_cast<TankAssistAction*>(action) ||
                                 dynamic_cast<AggressiveTargetAction*>(action) ||
                                 dynamic_cast<AttackAnythingAction*>(action) ||
                                 dynamic_cast<AttackLeastHpTargetAction*>(action) ||
                                 dynamic_cast<AttackRtiTargetAction*>(action);
    bool const isAutoAttack = dynamic_cast<MeleeAction*>(action) || dynamic_cast<CastAutoShotAction*>(action) ||
                              dynamic_cast<CastShootAction*>(action);
    Action::ActionThreatType const threatType = action->getThreatType();
    // Heals are Aoe-typed (CastHealingSpellAction) and never limited.
    bool const isThreatAction =
        threatType != Action::ActionThreatType::None && !dynamic_cast<CastHealingSpellAction*>(action);
    bool const isTaunt = !isTargetChooser && EncounterHelpers::IsTauntAction(bot, action);
    if (!isTargetChooser && !isAutoAttack && !isThreatAction && !isTaunt)
        return 1.0f;
    if (IsSelfBot(bot))
        return 1.0f;

    // Everything but the choosers applies only against him: cheap entry test before the view.
    Unit* current = nullptr;
    if (!isTargetChooser)
    {
        current = AI_VALUE(Unit*, "current target");
        if (!current || current->GetEntry() != Naxx40PatchwerkRules::PATCHWERK_ENTRY)
            return 1.0f;
    }

    Naxx40Patchwerk::PatchwerkView view;
    if (!Naxx40Patchwerk::GetView(bot, view) || (current && current != view.boss))
        return 1.0f;

    // A threat-typed action counts against him only when aimed at him or AoE-typed: a self-cast or friendly-target
    // spell (Shield Block, Holy Shield, Last Stand, Frenzied Regeneration, buffs) is never vetoed. GetTarget resolves a
    // value, so it is read only once a veto is otherwise due.
    auto const aimedAtHim = [&]()
    { return threatType == Action::ActionThreatType::Aoe || action->GetTarget() == view.boss; };

    // 1. Opener hold: he is the only hostile in his room, so every generic chooser would hand him straight back.
    if (view.openerHold)
    {
        if (PlayerbotAI::IsTank(bot))
            return 1.0f;
        if (isTargetChooser || isAutoAttack)
            return 0.0f;
        return isThreatAction && aimedAtHim() ? 0.0f : 1.0f;
    }
    if (isTargetChooser)
        return 1.0f;

    // 3. No taunts from any role tank other than the MT (IsTauntAction is tanks only): a soaker, a spare, a resurrected
    // or late tank would rip him off the MT. Only once latched, when an MT is known.
    if (isTaunt && view.latched && bot->GetGUID().GetRawValue() != view.roles.mt)
        return 0.0f;

    // 2. Threat caps.
    if (!isThreatAction && !isAutoAttack)
        return 1.0f;
    if (AI_VALUE(bool, "neglect threat"))
        return 1.0f;
    Naxx40PatchwerkRules::Cap const cap = Naxx40Patchwerk::CapOf(bot, view);
    if (!cap.capped)
        return 1.0f;
    if (threatType == Action::ActionThreatType::Aoe && AI_VALUE2(uint8, "threat", "aoe") >= 50)
        return 0.0f;
    // The auto restarters always hit him (his the current target); any other threat action only when aimed at him.
    if (!isAutoAttack && !aimedAtHim())
        return 1.0f;
    return Naxx40PatchwerkRules::ThreatVetoed(view.boss->GetThreatMgr().GetThreat(bot), cap) ? 0.0f : 1.0f;
}

// float GothikGenericMultiplier::GetValue(Action* action)
// {
//     Unit* boss = AI_VALUE2(Unit*, "find target", "gothik the harvester");
//     if (!boss)
//     {
//         return 1.0f;
//     }
//     BossAI* boss_ai = dynamic_cast<BossAI*>(boss->GetAI());
//     EventMap* eventMap = boss_botAI->GetEvents();
//     uint32 curr_phase = eventMap->GetPhaseMask();
//     if (curr_phase == 1 && (dynamic_cast<FollowAction*>(action)))
//     {
//         return 0.0f;
//     }
//     if (curr_phase == 1 && (dynamic_cast<AttackAction*>(action)))
//     {
//         Unit* target = action->GetTarget();
//         if (target == boss)
//         {
//             return 0.0f;
//         }
//     }
//     return 1.0f;
// }

float GluthGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    if ((dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<FleeAction*>(action) || dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) ||
         dynamic_cast<CastStarfallAction*>(action)))
    {
        return 0.0f;
    }

    if (botAI->IsMainTank(bot))
    {
        Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::MortalWound10, NaxxSpellIds::MortalWound25});
        if (!aura)
        {
            // Fallback to name for custom spell data.
            aura = botAI->GetAura("mortal wound", bot, false, true);
        }
        if (aura && aura->GetStackAmount() >= 5)
        {
            if (dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
                dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action))
            {
                return 0.0f;
            }
        }
    }
    if (dynamic_cast<PetAttackAction*>(action))
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (helper.IsZombieChow(target))
            return 0.0f;
    }
    return 1.0f;
}

static bool IsCrowdControlSpell(std::string const& spell)
{
    return spell == "polymorph" || spell == "cyclone" || spell == "hex" || spell == "freezing arrow" ||
           spell == "freezing trap" || spell == "repentance" || spell == "hibernate";
}

float NaxxRaidMechanicsMultiplier::GetValue(Action* action)
{
    using namespace NaxxHelpers;
    if (bot->GetMapId() != MAP_NAXXRAMAS)
        return 1.0f;

    // A priest holding a Worshipper keeps the Mind Control channel and only steers the Worshipper.
    if (GetCharmedWorshipper(bot))
        return dynamic_cast<NaxxFaerlinaEmbraceAction*>(action) ? 1.0f : 0.0f;

    // Gothik: bots hold their side of the room until the gate opens.
    if (dynamic_cast<FollowAction*>(action) && IsInGothikRoom(bot))
    {
        constexpr float GOTHIK_SEARCH_DISTANCE = 150.0f;
        Creature* gothik = bot->FindNearestCreature(NPC_GOTHIK, GOTHIK_SEARCH_DISTANCE);
        if (gothik && !(gothik->IsInCombat() && IsGothikGateOpen(bot)))
            return 0.0f;
    }

    // Hateful Strike soakers stay on Patchwerk and leave his threat to the main tank.
    if ((EncounterHelpers::IsTauntAction(bot, action) || dynamic_cast<TankAssistAction*>(action)) &&
        IsHatefulStrikeSoaker(bot) && AI_VALUE2(Unit*, "find target", "patchwerk"))
        return 0.0f;

    Unit* target = nullptr;
    auto* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (spellAction)
        target = action->GetTarget();
    else if (dynamic_cast<AttackAction*>(action))
        target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    // Worshippers are kept alive for Widow's Embrace; they die only when put on skull for it.
    if (target->GetEntry() == NPC_NAXXRAMAS_WORSHIPPER && !PlayerbotAI::IsTank(bot))
    {
        Group* group = bot->GetGroup();
        bool const isSkull = group && group->GetTargetIcon(RtiTargetValue::skullIndex) == target->GetGUID();
        if (!isSkull && (!spellAction || spellAction->getThreatType() != Action::ActionThreatType::None))
            return 0.0f;
    }

    // Raiders under Chains of Kel'Thuzad are crowd-controlled, not killed.
    if (target->IsPlayer() && target->HasAura(SPELL_CHAINS_OF_KELTHUZAD))
        return spellAction && IsCrowdControlSpell(spellAction->getSpell()) ? 1.0f : 0.0f;

    return 1.0f;
}
