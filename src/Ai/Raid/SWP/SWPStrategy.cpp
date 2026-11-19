/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPStrategy.h"

#include "SWPMultipliers.h"

void RaidSunwellPlateauStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // General
    triggers.push_back(new TriggerNode("sunwell bot is not in combat", {
        NextAction("sunwell erase timers and trackers", ACTION_EMERGENCY + 11) }));

    // Kalecgos
    triggers.push_back(new TriggerNode("kalecgos melee in breath or tail arc", {
        NextAction("kalecgos melee flank boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("kalecgos tank position boss", {
        NextAction("kalecgos tank position boss", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("kalecgos ranged clumped", {
        NextAction("kalecgos disperse ranged", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("kalecgos spectral realm needs tank", {
        NextAction("kalecgos enter spectral rift", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("kalecgos spectral realm needs healer", {
        NextAction("kalecgos enter spectral rift", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("kalecgos spectral realm needs dps", {
        NextAction("kalecgos enter spectral rift", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("kalecgos stray off platform", {
        NextAction("kalecgos return to platform", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode("kalecgos sathrovarr loose in spectral realm", {
        NextAction("kalecgos tank pick up sathrovarr", ACTION_EMERGENCY + 2) }));

    triggers.push_back(new TriggerNode("kalecgos spectral dps wrong target", {
        NextAction("kalecgos attack sathrovarr", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("kalecgos kalec needs healing", {
        NextAction("kalecgos heal kalec", ACTION_EMERGENCY + 2) }));

    // Brutallus
    triggers.push_back(new TriggerNode("brutallus tank swap", {
        NextAction("brutallus taunt boss", ACTION_EMERGENCY + 2) }));

    // Purge outranks the isolation walk: a bot that can strip Burn outright never needs to leave
    // its slash zone, which keeps a body in the cone and keeps it in healer range.
    triggers.push_back(new TriggerNode("brutallus can purge burn", {
        NextAction("brutallus purge burn", ACTION_EMERGENCY + 3) }));

    triggers.push_back(new TriggerNode("brutallus burning", {
        NextAction("brutallus move to station", ACTION_EMERGENCY + 1) }));

    triggers.push_back(new TriggerNode("brutallus out of station", {
        NextAction("brutallus move to station", ACTION_RAID + 1) }));

    // Felmyst
    // The vapor carrier outranks the station: it must run the OTHER way, and the station trigger
    // already stands down for it so the two can never fight over the same tick.
    triggers.push_back(new TriggerNode("felmyst chased by vapor", {
        NextAction("felmyst kite vapor", ACTION_EMERGENCY + 2) }));

    // Lowest of the Felmyst family on purpose: regrouping is only correct once nothing else applies,
    // and its own predicate stands down for every live mechanic (including any active fog lane) so it
    // can never walk a bot into one.
    triggers.push_back(new TriggerNode("felmyst stray from raid", {
        NextAction("felmyst regroup", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("felmyst in fog lane", {
        NextAction("felmyst leave fog lane", ACTION_EMERGENCY + 4) }));

    // Encapsulate pulses 3500 to everyone within 20y of the victim, once a second for six seconds -
    // ~21k on anyone who stays. Ranked below the fog only because being charmed is unrecoverable.
    triggers.push_back(new TriggerNode("felmyst near encapsulate", {
        NextAction("felmyst flee encapsulate", ACTION_EMERGENCY + 3) }));

    // A charmed raider fights the raid permanently (the charm never expires) and cannot be saved, so
    // killing them is the only answer. Above normal target selection so bots actually switch, but below
    // every survival move - dying to the fog while dutifully retargeting would be a poor trade.
    triggers.push_back(new TriggerNode("felmyst charmed raider loose", {
        NextAction("felmyst attack charmed raider", ACTION_EMERGENCY + 1) }));

    // Gas Nova is 100y and unavoidable, so dispelling is the only mitigation there is. Ranked above
    // healing on purpose: one Mass Dispel strips a ~1900 + 30s-DoT + mana-drain debuff off up to ten
    // raid members, which beats the single heal the cast displaces - and its 15s cooldown means this
    // can cost at most one tick per cooldown. It sits BELOW all three movement emergencies, and the
    // trigger additionally stands down whenever the bot owes a move.
    triggers.push_back(new TriggerNode("felmyst gas nova dispellable", {
        NextAction("felmyst mass dispel gas nova", ACTION_EMERGENCY + 1) }));

    // Eredar Twins
    // Conflagration is the only thing here that wipes a raid outright: a 10s aura applied to everyone
    // within 8y of the victim, each of them then pulsing 1600 into everything within 8y of THEMSELVES,
    // once a second. The victim outranks its neighbours because moving the epicentre solves the whole
    // problem at once, whereas each neighbour only saves itself.
    triggers.push_back(new TriggerNode("twins conflagration on me", {
        NextAction("twins flee conflagration", ACTION_EMERGENCY + 4) }));

    triggers.push_back(new TriggerNode("twins near conflagration", {
        NextAction("twins clear conflagration", ACTION_EMERGENCY + 3) }));

    // Focus discipline is ranked as an emergency because it is worth more than any single mechanic here:
    // Empower FULLY HEALS whichever twin survives (45366 is a 6,000,000 heal), so every point of damage
    // put into Alythess before Sacrolash dies is simply deleted - against a 6-minute enrage on 7M of
    // combined health. Above normal target selection so bots actually switch, below the movement
    // emergencies because dying while dutifully retargeting is a poor trade.
    triggers.push_back(new TriggerNode("twins wrong dps target", {
        NextAction("twins focus sacrolash", ACTION_EMERGENCY + 1) }));

    triggers.push_back(new TriggerNode("twins alythess untanked", {
        NextAction("twins tank alythess", ACTION_RAID + 2) }));

    // Confounding Blow does NOT drop threat on this core - it confuses her holder for 6s at -60% speed.
    // So the handoff is not about threat at all: it is about her chasing a tank who cannot steer, up to
    // ~17y of random walk every ~22s, toward the 50y mark where CheckInRoom fires raid-wide Fireblast for
    // 13,125 with no immunity possible. Ranked at EMERGENCY + 2 - above the focus switch, because a
    // wandering boss threatens the whole raid while a misdirected DPS only wastes damage.
    triggers.push_back(new TriggerNode("twins cotank confounded", {
        NextAction("twins relief taunt sacrolash", ACTION_EMERGENCY + 2) }));

    // The other half of the leash: undo drift that has already happened. Only her current tank fires this,
    // because she chases her victim, so walking him home walks her home.
    triggers.push_back(new TriggerNode("twins sacrolash overextended", {
        NextAction("twins pull sacrolash back", ACTION_RAID + 3) }));

    // One binding for both directions of blaze footwork - into a patch to convert away Dark Touched, out
    // of one to stop burning. Two bindings would be an in-and-out oscillator; the decision lives entirely
    // in GetTwinsBlazeStation.
    triggers.push_back(new TriggerNode("twins blaze footwork", {
        NextAction("twins blaze footwork", ACTION_RAID + 1) }));

    // Phase 2: Sacrolash is dead, so Shadow Nova is the only shadow left and the raid has to stand together
    // for it to strip Flame Touched off anybody. Just BELOW blaze footwork, because a bot standing in fire
    // should step out first and its own predicate stands down while it owes footwork - otherwise the two
    // would trade the same bot across a patch that lives 15s.
    triggers.push_back(new TriggerNode("twins out of alythess stack", {
        NextAction("twins stack on alythess", ACTION_RAID) }));

    // RANKED ABOVE BLAZE FOOTWORK, and the first kill is why. This was at ACTION_NORMAL + 2 = 11, against
    // blaze footwork at 61 - and because a fresh Blaze lands under Alythess' victim every ~3.8s, footwork is
    // permanently active for exactly the bot that most needs a cleanse. The trace showed the cleanse
    // triggering 12 times and executing 24 times without ever completing a move: it was pushed at 12 and
    // outranked on every single tick. A bot at 6+ Flame Touched stacks is taking more from the DoT than from
    // standing in a 2.5y patch, so it wins.
    //
    // General lesson: an always-active action starves anything ranked under it, so relevance has to be set
    // against how OFTEN its competitors fire, not just how urgent they are.
    triggers.push_back(new TriggerNode("twins needs shadow cleanse", {
        NextAction("twins seek shadow blades", ACTION_RAID + 4) }));

    // Pyrogenics is a self-buff on Alythess: +35% fire damage done for 15s, DispelType 1 = Magic, so an
    // offensive dispel strips roughly a quarter of her output. Lowest of the family - it is pure value,
    // never survival, and its own predicate stands down whenever the bot owes a move.
    triggers.push_back(new TriggerNode("twins pyrogenics up", {
        NextAction("twins dispel pyrogenics", ACTION_NORMAL + 1) }));

    // M'uru / Entropius
    //
    // Ordering here was resolved to numbers against every competitor BEFORE shipping, which is the Twins
    // lesson: an always-active action starves everything under it, so relevance has to be set against how
    // OFTEN a competitor fires and not just how urgent it is.
    //
    // Darkness outranks even the fiend dispel, and it is the one ordering that is genuinely arguable, so:
    // 45996 is 3000 shadow per second for 20s in a 15y circle AND applies aura 118 at -100%, so a bot
    // inside cannot be healed at all. Four ticks kills a level-70 bot and no healer can intervene. A bot
    // standing in that is worth nothing to the raid - including worth nothing as a dispeller - so it
    // leaves first. The window is short and its own predicate is narrow (inside 17y of a live or
    // 3-seconds-out zone), so it cannot starve the dispel across a whole Darkness cycle.
    triggers.push_back(new TriggerNode("muru in darkness", {
        NextAction("muru clear darkness", ACTION_EMERGENCY + 6) }));

    // THE wipe mechanic. 45944 is 5000 direct plus 2000 per second for ten seconds to every player within
    // FIFTY yards, MaxAffectedTargets 0 - verified literal: no spell script, no linked spell, no
    // condition, no SpellInfoCorrections entry, and no reference anywhere in src/server. For contrast,
    // M'uru's other room-wide AoE (46008) IS explicitly capped to 5 targets by
    // spell_gen_select_target_count_15_5, so the absence of a cap on 45944 is a decision and not an
    // oversight in the search.
    //
    // And damage cannot answer it: npc_dark_fiend::DamageTaken clamps every lethal hit to leave 1%, so a
    // fiend can be ground down forever and never dies. Only stripping 45934 (DispelType 1 = MAGIC)
    // removes one. So this is ranked above every other cast in the fight and above three of the four
    // movement rules.
    triggers.push_back(new TriggerNode("muru dark fiend up", {
        NextAction("muru dispel dark fiend", ACTION_EMERGENCY + 5) }));

    // Entropius' zone: 3000 per second, but only 3y wide and healable, so it sits under both of the above.
    triggers.push_back(new TriggerNode("muru in void zone", {
        NextAction("muru clear void zone", ACTION_EMERGENCY + 2) }));

    // The pull is survivable on its own - it is the four seconds of not acting that costs the raid.
    triggers.push_back(new TriggerNode("muru near singularity", {
        NextAction("muru flee singularity", ACTION_EMERGENCY + 1) }));

    // Melee DPS stepping off a Void Sentinel: 3750 per 3s inside 10y. Well below the emergencies because
    // it is steady, healable damage on one bot rather than something that kills or wipes.
    triggers.push_back(new TriggerNode("muru in shadow pulse", {
        NextAction("muru leave shadow pulse", ACTION_RAID + 5) }));

    // Above the retarget so a tank claims a loose add before any DPS re-picks around it.
    triggers.push_back(new TriggerNode("muru add untanked", {
        NextAction("muru tank add", ACTION_RAID + 3) }));

    // Lowest of the family on purpose, and nothing in this fight ranks under it - so it cannot starve
    // anything. It carries the handoff too: M'uru gains UNIT_FLAG_NOT_SELECTABLE the instant he is clamped
    // to 1 HP, a full 7 seconds before Entropius exists, and GetMuruFocusTarget refuses to return him
    // while that flag is set.
    triggers.push_back(new TriggerNode("muru wrong target", {
        NextAction("muru focus target", ACTION_RAID + 1) }));

    // Kil'jaeden
    //
    // Ordered against how OFTEN each competitor fires, not just how urgent it is - the Twins lesson. The
    // two numbers that set the whole scale:
    //   * Armageddon is 9999 fire in a 9y circle with six seconds of warning, up to three markers live at
    //     once, cycling every ~20s through phases 4 and 5. Instant death at level 70, so nothing outranks
    //     stepping out of one.
    //   * Darkness of a Thousand Souls is 47,499 shadow at radius 50000 - the entire map - on the natural
    //     expiry of an 8s aura. No positional answer exists. The ONLY answer is Shield of the Blue's -96%
    //     damage taken, which is a 12y bubble on a drake somebody has to be piloting.
    //
    // So the drake outranks the stack, and the stack outranks everything else.

    triggers.push_back(new TriggerNode("kj in armageddon", {
        NextAction("kj clear armageddon", ACTION_EMERGENCY + 6) }));

    // ABOVE THE STACK, and deliberately so. A pilot preempted for even one tick can miss the shield
    // window, and missing it kills the entire raid rather than the one bot that was preempted. The pilot
    // itself is immune to all schools and cannot move anyway (45838 plus UNIT_FLAG_DISABLE_MOVE), so there
    // is nothing this displaces that the bot could otherwise have done.
    triggers.push_back(new TriggerNode("kj piloting drake", {
        NextAction("kj drive drake", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode("kj darkness incoming", {
        NextAction("kj stack for darkness", ACTION_EMERGENCY + 4) }));

    // BELOW the stack on purpose: a pilot-to-be caught by a Darkness mid-walk is a normal raider with no
    // immunity, so it stacks first and resumes the fetch afterwards. It has 30s of refresh window against
    // an 8s Darkness, so the interruption always fits.
    triggers.push_back(new TriggerNode("kj orb available", {
        NextAction("kj claim orb", ACTION_EMERGENCY + 3) }));

    // Fire Bloom is ten hits of 1618 into 10y of allies over twenty seconds, which is what kills a stacked
    // raid slowly while the mechanics above kill it quickly. Its own predicate stands down inside a
    // Darkness window - not on priority grounds but because the instruction inverts there: Shield of the
    // Blue REMOVES 45641 on apply, so the carrier's cure is to be in the stack.
    triggers.push_back(new TriggerNode("kj fire bloom on me", {
        NextAction("kj quarantine fire bloom", ACTION_EMERGENCY + 2) }));

    // Every fixed position in this fight is a polar offset from the authored anchor, and the anchor only
    // stays true if the boss stays on it. He chases his victim, so a main tank who never leaves is a boss
    // who never leaves - the same trick as the Brutallus latch, minus the latching, because here the
    // anchor is where he spawns.
    triggers.push_back(new TriggerNode("kj boss off anchor", {
        NextAction("kj hold anchor", ACTION_RAID + 3) }));

    // The baseline spread, which Shadow Spike (8y area, every 3s) and Fire Bloom (10y) both demand. Ranked
    // under the anchor so the tank settles first - the ring is measured from where he is going.
    triggers.push_back(new TriggerNode("kj out of station", {
        NextAction("kj hold station", ACTION_RAID + 2) }));

    // Lowest of the family, so it cannot starve anything, and it carries both untargetable windows: KJ
    // holds UNIT_FLAG_NOT_SELECTABLE for his 11s rebirth and gains UNIT_FLAG_NON_ATTACKABLE on the killing
    // blow.
    triggers.push_back(new TriggerNode("kj wrong target", {
        NextAction("kj focus target", ACTION_RAID + 1) }));
}

void RaidSunwellPlateauStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    // Kalecgos
    multipliers.push_back(new KalecgosDpsBalanceMultiplier(botAI));

    // Brutallus
    multipliers.push_back(new BrutallusTankHoldMultiplier(botAI));

    // Felmyst
    multipliers.push_back(new FelmystAirMovementMultiplier(botAI));

    // Eredar Twins
    multipliers.push_back(new TwinsMovementMultiplier(botAI));

    // M'uru / Entropius
    multipliers.push_back(new MuruMovementMultiplier(botAI));
    multipliers.push_back(new MuruTargetHoldMultiplier(botAI));

    // Kil'jaeden
    multipliers.push_back(new KjMovementMultiplier(botAI));
    multipliers.push_back(new KjTargetHoldMultiplier(botAI));
}
