/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPMultipliers.h"

#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "SWPHelpers.h"

using namespace SunwellPlateauHelpers;

float KalecgosDpsBalanceMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != SUNWELL_MAP_ID || !bot->IsInCombat())
        return 1.0f;

    if (botAI->IsTank(bot) || botAI->IsHeal(bot))
        return 1.0f;  // tanks keep threat, healers keep healing

    // Only damage actions are ever held (same idiom as IllidariCouncilWaitForDps) -
    // cures (Curse of Boundless Agony!) and battle-res must keep flowing.
    bool const isDamageAction =
        dynamic_cast<AttackAction*>(action) ||
        (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastHealingSpellAction*>(action) &&
         !dynamic_cast<CastCureSpellAction*>(action) && !dynamic_cast<CurePartyMemberAction*>(action) &&
         !dynamic_cast<ResurrectPartyMemberAction*>(action));
    if (!isDamageAction)
        return 1.0f;

    KalecgosSnapshot const& snap = GetKalecgosSnapshot(bot);
    if (!snap.valid)
        return 1.0f;

    bool const spectral = IsInSpectralRealm(bot);
    bool const myBanished = spectral ? snap.demonBanished : snap.dragonBanished;
    float const myPct = spectral ? snap.demonPct : snap.dragonPct;
    float const otherPct = spectral ? snap.dragonPct : snap.demonPct;

    if (myBanished)
        return 0.0f;  // this half is done - all damage on it is wasted

    // Hold this side's damage while it is more than 5% ahead (lower HP) so both halves
    // cross the 10%-absolute Crazed Rage threshold (44807) and reach banish together -
    // a half that races ahead leaves the other realm fighting an enraged boss alone.
    if (myPct + 5.0f < otherPct)
        return 0.0f;

    return 1.0f;
}

// Two jobs, both of them "stop generic behaviour from undoing the formation":
//
//  1. MOVEMENT. Every tank bot runs the "tank face" strategy (AiFactory.cpp), and TankFaceAction
//     exists to shuffle the tank around the boss until the boss faces AWAY from the group's
//     average angle. This encounter needs the exact opposite - the raid stands IN FRONT, on
//     purpose, to divide Meteor Slash - so that action fights the design on every tick, and with
//     two tanks it produced the reported "both tanks just ran around the boss in a circle". The
//     same applies to "behind" (RearFlankAction) for melee dps, which drags them out of the cone.
//     Vetoing those is also what makes stationing a melee bot safe at all: a station plus an
//     unvetoed generic mover is a tug-of-war, and the bot visibly shuttles.
//
//  2. THREAT. The off tank builds nothing for the opening seconds so the main tank can take an
//     unassailable lead, and a tank the boss has stopped attacking stays quiet while he is the
//     dirtier of the two, so his old threat total cannot snatch the boss straight back.
//
// Note it must NEVER veto AttackAction for a movement reason: AttackAction derives from
// MovementAction (and MeleeAction derives from AttackAction), so a blanket dynamic_cast on
// MovementAction silences auto-attack and the tank does nothing at all.
float BrutallusTankHoldMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != SUNWELL_MAP_ID)
        return 1.0f;

    if (action->getName() == "brutallus taunt boss" || action->getName() == "brutallus move to station")
        return 1.0f;  // never veto this fight's own positioning or swap

    if (!bot->IsInCombat())
        return 1.0f;  // out of combat: no veto, and do not even read the snapshot

    BrutallusSnapshot const& snap = GetBrutallusSnapshot(bot);
    if (!snap.valid)
        return 1.0f;

    // Only a bot standing at the fight may ever be silenced (see BRUTALLUS_PARTICIPANT_RANGE): a
    // veto that reaches a bot elsewhere in the instance makes it look broken, not busy.
    if (bot->GetExactDist2d(snap.bossX, snap.bossY) > BRUTALLUS_PARTICIPANT_RANGE)
        return 1.0f;

    BrutallusTankRole const tankRole = GetBrutallusTankRole(bot, botAI);
    bool const isTank = botAI->IsTank(bot);

    // ---- 1. movement vetoes ----
    // Formation shuffling is never wanted here, for anybody: the raid's shape is authored. But
    // "reach spell"/"reach melee" (ReachTargetAction) is a range safety valve, so it is only taken
    // away from the bots the formation already guarantees are in range - the tanks and the melee,
    // who stand inside melee range by construction. Ranged keep theirs.
    bool const isFormationMover = dynamic_cast<CombatFormationMoveAction*>(action) ||  // incl. tank face
                                  dynamic_cast<RearFlankAction*>(action);              // "behind"
    bool const isReach = dynamic_cast<ReachTargetAction*>(action) != nullptr;

    if (isFormationMover || isReach)
    {
        // BOTS WE DELIBERATELY PARK FAR AWAY ARE CHECKED FIRST, before the range fail-open below.
        // Getting this order wrong rebuilt the oscillator this fight has already produced twice: the
        // Burn quarantine sits at 30y+, which is ALWAYS past the fail-open range, so a burned bot
        // that reached its isolation spot got generic movement handed straight back, was dragged
        // toward the raid, re-entered the range, was vetoed and shoved out again - shuttling through
        // the gap between the two lanes, where a moving boss can catch it with either cone. It looked
        // like a Burn positioning bug; it was this line ordering.
        //
        // Gated on anchorSet because a burned bot has no isolation spot to walk to until the anchor
        // exists, and vetoing its movement before then would just freeze it in the raid.
        bool const parkedOutside = snap.anchorSet &&
                                   (bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BURN_DOT)) ||
                                    GetBrutallusSlashStacks(bot) >= BRUTALLUS_BAIL_STACKS);
        if (parkedOutside)
            return 0.0f;

        // Fail open for anyone who genuinely cannot reach the boss any more - being frozen out of
        // the fight is worse than being out of formation.
        if (bot->GetExactDist2d(snap.bossX, snap.bossY) > BRUTALLUS_ANCHOR_FREE_RANGE)
            return 1.0f;

        // Formation movers are vetoed from the START of the fight, BEFORE the anchor latches, and
        // that ordering is half the fix for a misaligned raid. TankFaceAction only runs once the
        // tank is ALREADY in melee range, so vetoing it can never stop him closing - but leaving it
        // live during the approach let it walk him round toward the boss's back, the boss turned to
        // follow him, and the formation that latched a moment later was measured off an axis he had
        // already left. (The other half is BRUTALLUS_ANCHOR_SETTLE_MS.)
        if (isFormationMover)
            return 0.0f;

        // "reach melee"/"reach spell" is the opposite case: it is how the tank closes that last gap,
        // and the anchor cannot latch until he does. So it stays live until the latch, after which
        // every tank and melee bot is inside melee range by construction anyway.
        if (!snap.anchorSet)
            return 1.0f;

        return (isTank || botAI->IsMelee(bot)) ? 0.0f : 1.0f;
    }

    if (!isTank)
        return 1.0f;  // the threat rules below are the tank pair's only

    // ---- 2. threat vetoes ----
    bool freeze = false;

    if (tankRole == BrutallusTankRole::Off &&
        getMSTimeDiff(snap.combatStartMs, getMSTime()) < BRUTALLUS_OFFTANK_OPENING_MS)
    {
        // The opening window: run to lane 1, do not touch him. Both tanks starting from zero
        // threat is what made them trade the boss back and forth for the first seconds.
        freeze = true;
    }
    else if (snap.victimGuid && snap.victimGuid != bot->GetGUID())
    {
        // ONLY ever silence the tank the boss has already stopped attacking - never the one
        // currently holding. An earlier build froze the holder at 2 stacks so the relief tank could
        // out-threat him, but nothing guarantees the next-highest threat is a TANK: it can be a
        // caster, and then the boss turns and chases them, sweeping the cone across the whole raid
        // (observed - all 24 bots stacking in lockstep). Freezing a tank who is already off the
        // boss cannot hand aggro to anybody.
        Unit* holder = botAI->GetUnit(snap.victimGuid);
        Player* holderPlayer = holder ? holder->ToPlayer() : nullptr;
        if (holderPlayer && botAI->IsTank(holderPlayer))
            freeze = GetBrutallusSlashStacks(bot) > GetBrutallusSlashStacks(holderPlayer);
    }

    if (!freeze)
        return 1.0f;

    bool const isDamage =
        dynamic_cast<AttackAction*>(action) ||
        (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastHealingSpellAction*>(action) &&
         !dynamic_cast<CastCureSpellAction*>(action) && !dynamic_cast<CurePartyMemberAction*>(action) &&
         !dynamic_cast<ResurrectPartyMemberAction*>(action));

    return isDamage ? 0.0f : 1.0f;
}

// While Felmyst is airborne, stop generic movement from undoing the fog hop. She sits ~28y up,
// invincible and REACT_PASSIVE, so every generic mover is not just useless but actively harmful: it
// drags bots back toward a point underneath her, which is mid-room and therefore inside a fog lane.
//
// This fight's own actions are exempt BY NAME, and that exemption comes before anything else. The
// Brutallus oscillator had exactly this shape: a deliberately-placed destination plus a veto whose
// fail-open range was narrower than the destination, so the bot was shoved out, handed generic
// movement back, dragged in, and shoved out again.
//
// And never a blanket dynamic_cast<MovementAction*>: AttackAction derives from MovementAction (and
// MeleeAction from AttackAction), so that silences auto-attack and bots stop killing the adds - which
// are the only thing worth hitting during the flight phase.
float FelmystAirMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != SUNWELL_MAP_ID)
        return 1.0f;

    // Every one of this fight's own movements is exempt. "felmyst regroup" especially: it exists purely
    // to undo the vapor kite, and this multiplier is precisely what makes that necessary - it removes
    // ReachTargetAction for the whole flight phase, so a bot sent away has no generic way back and will
    // stand at the wall until it dies. Vetoing the one action that fixes that would be self-defeating.
    if (action->getName() == "felmyst leave fog lane" || action->getName() == "felmyst kite vapor" ||
        action->getName() == "felmyst flee encapsulate" || action->getName() == "felmyst regroup")
    {
        return 1.0f;
    }

    if (!bot->IsInCombat())
        return 1.0f;

    Creature* boss = FindFelmyst(bot);
    if (!boss || !boss->IsAlive() || !boss->IsInCombat())
        return 1.0f;

    // Two situations need generic movement out of the way. The GROUND-phase one is Encapsulate: the
    // flee is a deliberate 27y step AWAY from the raid, so leaving "combat formation move" and
    // "behind" live means the bot is dragged straight back into a 3500/s blast - the same station-vs-
    // generic-mover tug-of-war that Brutallus kept reproducing. The predicate is shared with the
    // trigger so the two can never disagree about whether a flee is in progress.
    if (!IsFelmystAirborne(boss) && !FelmystShouldFleeEncapsulate(bot, boss))
        return 1.0f;

    if (bot->GetExactDist2d(boss->GetPositionX(), boss->GetPositionY()) > FELMYST_PARTICIPANT_RANGE)
        return 1.0f;

    bool const isFormationMover = dynamic_cast<CombatFormationMoveAction*>(action) ||  // incl. tank face
                                  dynamic_cast<RearFlankAction*>(action);              // "behind"
    bool const isReach = dynamic_cast<ReachTargetAction*>(action) != nullptr;

    // "follow" has to go too. The first live test's action trace showed it running 546 times on a
    // single bot during the fight - it walks the bot back toward its master, which is the opposite
    // direction from the station for the entire air phase.
    bool const isFollow = dynamic_cast<FollowAction*>(action) != nullptr;

    return (isFormationMover || isReach || isFollow) ? 0.0f : 1.0f;
}

// Keep generic movement from undoing a Conflagration flee, a blaze step or a cleanse trip.
//
// DELIBERATELY NARROW: this only bites while a bot actually owes one of those moves, unlike the Felmyst
// multiplier which covers a whole phase. That difference is the lesson from Felmyst's vapor carriers -
// a phase-wide veto removes ReachTargetAction for minutes at a time, so any bot the strategy sent away
// had no generic way back and needed a bespoke regroup action to rescue it from the wall. Most of this
// fight WANTS generic movement (melee chasing Sacrolash, ranged holding spell range), so taking it away
// for a few seconds at a time means the way home is never removed in the first place.
//
// The name exemptions come first, before any other test. That ordering is the Brutallus oscillator in
// miniature: a deliberately-placed destination plus a veto that can reach the action placing it means the
// bot is moved, vetoed, dragged back, and moved again.
//
// And never a blanket dynamic_cast<MovementAction*>: AttackAction derives from MovementAction (and
// MeleeAction from AttackAction), so that would silence auto-attack and the raid would stop doing damage
// against a 6-minute enrage.
float TwinsMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != SUNWELL_MAP_ID)
        return 1.0f;

    if (action->getName() == "twins flee conflagration" ||
        action->getName() == "twins clear conflagration" ||
        action->getName() == "twins blaze footwork" ||
        action->getName() == "twins seek shadow blades" ||
        action->getName() == "twins pull sacrolash back" ||
        action->getName() == "twins stack on alythess")
    {
        return 1.0f;
    }

    if (!bot->IsInCombat())
        return 1.0f;

    // Shared with every trigger, so the veto and the moves can never disagree about whether a move is
    // in progress - if they could, generic movement would be handed back mid-run and the bot would
    // shuttle in and out of an 8y blast.
    if (!TwinsShouldSuppressGenericMovement(bot, botAI))
        return 1.0f;

    bool const isFormationMover = dynamic_cast<CombatFormationMoveAction*>(action) ||  // incl. tank face
                                  dynamic_cast<RearFlankAction*>(action);              // "behind"
    bool const isReach = dynamic_cast<ReachTargetAction*>(action) != nullptr;
    bool const isFollow = dynamic_cast<FollowAction*>(action) != nullptr;

    return (isFormationMover || isReach || isFollow) ? 0.0f : 1.0f;
}

float MuruMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != SUNWELL_MAP_ID)
        return 1.0f;

    // Name exemptions FIRST. Every one of these four IS a MovementAction, so without this the veto would
    // silence the very moves it exists to protect.
    if (action->getName() == "muru clear darkness" ||
        action->getName() == "muru clear void zone" ||
        action->getName() == "muru flee singularity" ||
        action->getName() == "muru leave shadow pulse")
    {
        return 1.0f;
    }

    if (!bot->IsInCombat())
        return 1.0f;

    // Shared with every trigger, so the veto and the moves can never disagree about whether a move is in
    // progress. If they could, generic movement would be handed back mid-run and the bot would shuttle
    // straight back into a zone that cannot be healed through.
    if (!MuruShouldSuppressGenericMovement(bot, botAI))
        return 1.0f;

    // NEVER blanket-veto MovementAction: AttackAction and MeleeAction both derive from it, so doing that
    // silences auto-attack for the duration. Only the generic repositioners are cut.
    bool const isFormationMover = dynamic_cast<CombatFormationMoveAction*>(action) ||  // incl. tank face
                                  dynamic_cast<RearFlankAction*>(action);              // "behind"
    bool const isReach = dynamic_cast<ReachTargetAction*>(action) != nullptr;
    bool const isFollow = dynamic_cast<FollowAction*>(action) != nullptr;

    return (isFormationMover || isReach || isFollow) ? 0.0f : 1.0f;
}

float MuruTargetHoldMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != SUNWELL_MAP_ID)
        return 1.0f;

    // Type test FIRST, because it is the only cheap test here - resolving the focus target costs a grid
    // search, and this runs for every queued action of every bot on every tick.
    //
    // Matched by EXACT class, never by AttackAction: MeleeAction derives from it, and so do this fight's own
    // focus and tank actions. Vetoing the base class would silence auto-attack and the strategy itself.
    bool const isTargetChooser = dynamic_cast<DpsAssistAction*>(action) ||
                                 dynamic_cast<DpsAoeAction*>(action) ||
                                 dynamic_cast<TankAssistAction*>(action) ||
                                 dynamic_cast<AggressiveTargetAction*>(action) ||
                                 dynamic_cast<AttackAnythingAction*>(action) ||
                                 dynamic_cast<AttackLeastHpTargetAction*>(action) ||
                                 dynamic_cast<AttackRtiTargetAction*>(action);
    if (!isTargetChooser)
        return 1.0f;

    if (!IsMuruEncounterActive(bot))
        return 1.0f;

    // Only while this fight actually has an opinion, so nothing is silenced for free.
    //
    // Tanks and healers get nullptr from GetMuruFocusTarget by design, which means their generic assist
    // behaviour is left completely alone - `muru tank add` handles pickups and a healer must never be
    // handed an attack target at all.
    //
    // If AoE output on Void Spawn packs ever looks low, DpsAoeAction is the first of these to hand back:
    // its target is a clustered add rather than the boss, so it is the least likely of the seven to be
    // fighting the kill order.
    return GetMuruFocusTarget(bot, botAI) ? 0.0f : 1.0f;
}

float KjMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != SUNWELL_MAP_ID)
        return 1.0f;

    // Name exemptions FIRST, before any other test. Every one of these IS a MovementAction, so without
    // this the veto would silence the very moves it exists to protect - the Brutallus oscillator in
    // miniature.
    if (action->getName() == "kj clear armageddon" ||
        action->getName() == "kj quarantine fire bloom" ||
        action->getName() == "kj stack for darkness" ||
        action->getName() == "kj hold anchor" ||
        action->getName() == "kj hold station" ||
        action->getName() == "kj claim orb" ||
        action->getName() == "kj drive drake")
    {
        return 1.0f;
    }

    if (!bot->IsInCombat())
        return 1.0f;

    // TYPE TEST BEFORE THE PREDICATE, and this ordering is a fix rather than a style choice. These three
    // kinds are the ONLY actions this multiplier can ever veto, so anything else can return early without
    // evaluating the predicate chain at all - and that chain reaches grid searches. Rev 1 had the
    // predicate first, so a ~20-action queue paid for it twenty times per bot per tick instead of ~3.
    // Measured impact in the first kill was nil (three tick-diff warnings all fight, none during the pull)
    // but this is the shape that produced 400-500ms world ticks in Wintergrasp.
    //
    // NEVER blanket-veto MovementAction: AttackAction and MeleeAction both derive from it, so that would
    // silence auto-attack for the duration. Only the generic repositioners are cut.
    bool const isFormationMover = dynamic_cast<CombatFormationMoveAction*>(action) ||  // incl. tank face
                                  dynamic_cast<RearFlankAction*>(action);              // "behind"
    bool const isReach = dynamic_cast<ReachTargetAction*>(action) != nullptr;
    bool const isFollow = dynamic_cast<FollowAction*>(action) != nullptr;
    if (!isFormationMover && !isReach && !isFollow)
        return 1.0f;

    // Shared with every trigger so the veto and the moves can never disagree about whether a move is in
    // progress. If they could, generic movement would be handed back mid-run and the bot would walk out of
    // the shield in the second before 47,499 lands.
    return KjShouldSuppressGenericMovement(bot, botAI) ? 0.0f : 1.0f;
}

float KjTargetHoldMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != SUNWELL_MAP_ID)
        return 1.0f;

    // Type test FIRST, because it is the only cheap test here - resolving the focus target costs a grid
    // search and this runs for every queued action of every bot on every tick.
    //
    // Matched by EXACT class, never by AttackAction: MeleeAction derives from it, and so does this fight's
    // own focus action. Vetoing the base class would silence auto-attack and the strategy itself.
    bool const isTargetChooser = dynamic_cast<DpsAssistAction*>(action) ||
                                 dynamic_cast<DpsAoeAction*>(action) ||
                                 dynamic_cast<TankAssistAction*>(action) ||
                                 dynamic_cast<AggressiveTargetAction*>(action) ||
                                 dynamic_cast<AttackAnythingAction*>(action) ||
                                 dynamic_cast<AttackLeastHpTargetAction*>(action) ||
                                 dynamic_cast<AttackRtiTargetAction*>(action);
    if (!isTargetChooser)
        return 1.0f;

    if (!IsKjEncounterActive(bot))
        return 1.0f;

    // Only while this fight actually has an opinion, so nothing is silenced for free. Tanks and healers
    // get nullptr from GetKjFocusTarget by design, and so does a drake pilot - whose body is silenced and
    // pacified anyway.
    return GetKjFocusTarget(bot, botAI) ? 0.0f : 1.0f;
}
