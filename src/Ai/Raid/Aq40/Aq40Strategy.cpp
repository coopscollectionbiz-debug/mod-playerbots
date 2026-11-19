/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Strategy.h"

#include "Aq40Helpers.h"
#include "Aq40CthunHelpers.h"
#include "Aq40Multipliers.h"
#include "Playerbots.h"

// Relevances are set against how OFTEN each competitor fires, not just how urgent it is: an
// always-active action starves anything ranked under it. ACTION_RAID is 60, ACTION_EMERGENCY 90.
void RaidAq40Strategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Housekeeping, and harmless: only fires out of combat with the encounter not running.
    triggers.push_back(new TriggerNode("aq40 twins not in combat", {
        NextAction("aq40 twins erase trackers", ACTION_EMERGENCY + 11) }));

    // ~3000 fire in 15y on a 3s fuse - the only mechanic here with a hard clock.
    triggers.push_back(new TriggerNode("aq40 twins in explode radius", {
        NextAction("aq40 twins clear explode", ACTION_EMERGENCY + 4) }));

    // ~1500 per 2s in 10y plus a -30% slow.
    triggers.push_back(new TriggerNode("aq40 twins in blizzard", {
        NextAction("aq40 twins clear blizzard", ACTION_EMERGENCY + 3) }));

    // Above generic target selection so bots actually switch to a Mutated Bug (+300% health,
    // +1800% physical damage, 240s), below the movement emergencies because dying while dutifully
    // retargeting is a poor trade.
    triggers.push_back(new TriggerNode("aq40 twins wrong target", {
        NextAction("aq40 twins focus target", ACTION_EMERGENCY + 1) }));

    // ---- v2 (spec 2026-10-22 §5): two fixed sides A/B, a melee tank and a warlock tank per side, one active and
    // one waiting for the twin that lands there at the next Twin Teleport. ----

    // The 2 s landing window: everyone but the pickup tank (and every pet) leaves both clear radii, so the pickup
    // tank is the nearest body when the twin lands (+2000 threat to the nearest). Above the hazard dodges: a lost
    // roll means a twin dragged across the room.
    triggers.push_back(new TriggerNode("aq40 twins in landing zone", {
        NextAction("aq40 twins landing clear", ACTION_EMERGENCY + 6) }));

    // Revision 7 §16.1, the universal burst escape: any bot within 12.5 y of a living, non-passive Vek'lor steps out
    // to 16 y, every role included. Native combat movement (melee chasing a bug or its target, reach) still walked
    // bots into Arcane Burst; the §13.3 berth only routes the strategy's own moves. Below the landing clear, above the
    // Explode dodge (+4).
    triggers.push_back(new TriggerNode("aq40 twins in burst range", {
        NextAction("aq40 twins clear burst", ACTION_EMERGENCY + 5) }));

    // Revision 2 §11.2, the no-drag rule: a wrong victim never drags a twin - 12-40 y from Vek'lor, or walk
    // Vek'nilash to his side's melee tank. Revision 11 §20.1: while the twins are closer than 100 y, or with no living
    // active melee tank, any Vek'nilash victim kites him to the drag station instead. The movement multiplier vetoes
    // every other mover for it. Above focus target, so the wrong victim moves before it retargets.
    triggers.push_back(new TriggerNode("aq40 twins wrong victim", {
        NextAction("aq40 twins victim hold", ACTION_EMERGENCY + 2) }));

    // Revision 3 §12.2: while the twins are closer than 100 y the active melee tank kites Vek'nilash back toward the
    // authored station farther from Vek'lor; otherwise nobody drags him and his side's anchor follows him.
    triggers.push_back(new TriggerNode("aq40 twins boss off station", {
        NextAction("aq40 twins hold station", ACTION_RAID + 4) }));

    // Healers, waiting tanks, the active warlock and (Revision 3 §12.1) melee in the pre-swap move walk to their owned
    // spots, all anchored on the bosses (§12.2) and pushed out of burst range (Revision 9 §18.2). No caster keep-out
    // and (Revision 9 §18.1) no ranged arcs: ranged Dps use native reach to range.
    triggers.push_back(new TriggerNode("aq40 twins off spot", {
        NextAction("aq40 twins hold spot", ACTION_RAID + 3) }));

    // The active warlock, wherever it can cast from (ruling R13 F1): Searing Pain by name (Shadow Ward when he is on
    // it).
    triggers.push_back(new TriggerNode("aq40 twins warlock tank due", {
        NextAction("aq40 twins warlock tank", ACTION_RAID + 2) }));

    // The hard stop of a running auto-attack / auto-shot / wand on a twin: at 0.90 of its designated active tank's
    // threat, or while holding fire (ruling R8). The multiplier's vetoes cannot stop one already running.
    triggers.push_back(new TriggerNode("aq40 twins threat hold", {
        NextAction("aq40 twins threat hold", ACTION_RAID + 1) }));

    // Lowest of the family, so it cannot starve anything (tied with the threat hold, which only fires while the bot
    // is attacking; the pet action returns false once the pets are where they belong).
    triggers.push_back(new TriggerNode("aq40 twins pet on wrong twin", {
        NextAction("aq40 twins direct pets", ACTION_RAID + 1) }));

    // Revision 6 §15.1: side tank heals. Bot healers only heal their own subgroup plus the raid's main tank, and the
    // warlock tanks sit in healerless subgroups, so each latched healer heals its own side's designated tanks by name.
    // Above every stock heal, below the movement holds (Huhuran's Berserk tank heal shape). Tied with the threat hold
    // (never active for a healer) and the pet action (a healer's Shadowfiend at most; false once it is placed). Stock
    // healing runs whenever no side tank needs it.
    triggers.push_back(new TriggerNode("aq40 twins tank heal", {
        NextAction("aq40 twins tank heal", ACTION_RAID + 1) }));

    // ---- form-up and the pull (spec §5.5, rewritten 2026-10-22). Out of combat only (both need IsTwinsFormUp). ----

    // Every bot walks to its staging spot and holds it (follow vetoed); the lowest-guid melee tank whispers the
    // master once everyone is on their spot. Same row as hold spot: the two never run at once.
    triggers.push_back(new TriggerNode("aq40 twins form up", {
        NextAction("aq40 twins stage", ACTION_RAID + 3) }));

    // The owner's "twins pull" (a chat-command trigger, named exactly that): only warlock A acts, on S_A. The top of
    // the family, so the cast is never queued behind a stage step.
    triggers.push_back(new TriggerNode("twins pull", {
        NextAction("aq40 twins pull", ACTION_EMERGENCY + 12) }));

    // NOTE what is absent: melee DPS movement, except the landing clear, the wrong-victim hold and the pre-swap
    // move (Revision 3 §12.1). After a window a melee bot's twin is beside it, and native combat movement engages it.
    // Fixed (anchored) spots are for bodies that must be in a known place at the roll: healers and the four
    // designated tanks (Revision 9 §18.1: ranged Dps are native too).

    // ---- C'Thun ----
    // The two encounters never run at once, so relevance values overlapping the Twins ones are fine.

    triggers.push_back(new TriggerNode("aq40 cthun not in combat", {
        NextAction("aq40 cthun erase trackers", ACTION_EMERGENCY + 11) }));

    // Bots cannot fire the client-side exit trigger; this walks them there and sends it.
    triggers.push_back(new TriggerNode("aq40 cthun in stomach", {
        NextAction("aq40 cthun exit stomach", ACTION_EMERGENCY + 6) }));

    // Bots get no client gravity: the swallow and the stomach exit leave them hovering, and a fall's
    // FALLING flag never clears (every cast-time spell fails). Teleport down / scrub the flag.
    // Bots ride in with a mounted master; once in place they must dismount to fight.
    triggers.push_back(new TriggerNode("aq40 cthun mounted in place", {
        NextAction("aq40 cthun dismount", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode("aq40 cthun stomach airborne", {
        NextAction("aq40 cthun drop to floor", ACTION_EMERGENCY + 7) }));

    // ~50k per tick, 5y either side of the Eye's facing, unlimited range.
    triggers.push_back(new TriggerNode("aq40 cthun in glare path", {
        NextAction("aq40 cthun dodge glare", ACTION_EMERGENCY + 5) }));

    // Break the Green Beam chain at its source during the 2s cast.
    triggers.push_back(new TriggerNode("aq40 cthun near beam target", {
        NextAction("aq40 cthun clear beam", ACTION_EMERGENCY + 4) }));

    // The Eye's first four beams all go to the puller; it drags them to the far side so the raid
    // can spread.
    triggers.push_back(new TriggerNode("aq40 cthun opening puller", {
        NextAction("aq40 cthun run to far side", ACTION_EMERGENCY + 3) }));

    // Stomach: walk into range of the Flesh Tentacle (pathfinding reach fails in there). Below
    // "focus target" so the target is chosen first.
    triggers.push_back(new TriggerNode("aq40 cthun stomach far from tentacle", {
        NextAction("aq40 cthun stomach close in", ACTION_EMERGENCY) }));

    // Below the hazard moves: dying while dutifully retargeting is a poor trade.
    triggers.push_back(new TriggerNode("aq40 cthun wrong target", {
        NextAction("aq40 cthun focus target", ACTION_EMERGENCY + 1) }));

    // FIXED spots, spaced >= 12y by construction: everyone returns to their own spot whenever off it
    // (melee stage off the puller's lane until its 4th beam has landed). No disperse.
    triggers.push_back(new TriggerNode("aq40 cthun not formed", {
        NextAction("aq40 cthun form up", ACTION_RAID + 3) }));

    // ---- Huhuran ----
    // Spec docs/superpowers/specs/2026-10-20-playerbot-aq40-huhuran-design.md (overlay repo), §4.2-4.3.

    triggers.push_back(new TriggerNode("aq40 huhuran not in combat", {
        NextAction("aq40 huhuran erase trackers", ACTION_EMERGENCY + 11) }));

    // The MT's drag onto P: close, take her, back her toward T while she is on it and not parked.
    triggers.push_back(new TriggerNode("aq40 huhuran mt has work", {
        NextAction("aq40 huhuran park boss", ACTION_RAID + 4) }));

    // Stock off-tanks idle while she is on the MT (FindTargetForTankStrategy); she is taunt-immune, so
    // backup threat must be built by attacking. Rev 2 (spec §7.2 #3): also walks each off-tank to its melee
    // spot beside the MT, so an MT death leaves her turning to a body where the MT stood.
    triggers.push_back(new TriggerNode("aq40 huhuran off tank idle", {
        NextAction("aq40 huhuran off tank engage", ACTION_RAID + 3) }));

    // Ring spots, latched at engage (rev 4, spec §9.2): ranged and healers form the rings while the MT drags.
    triggers.push_back(new TriggerNode("aq40 huhuran off spot", {
        NextAction("aq40 huhuran hold spot", ACTION_RAID + 3) }));

    // Rev 2 (spec §7.2 #2): the threat hard stop, at BWL's "bwl threat hold" relevance. The multiplier's
    // vetoes cannot stop an auto-attack or auto-shot that is already running. Healers are never active.
    triggers.push_back(new TriggerNode("aq40 huhuran threat hold", {
        NextAction("aq40 huhuran threat hold", ACTION_RAID + 1) }));

    // Rev 2 (spec §7.2 #4): Berserk tank healers. BWL's tank-only priest heal sits at ACTION_RAID + 1, which
    // is above every stock heal (ACTION_CRITICAL_HEAL 30 and its +N rows); below the hold-spot row so a
    // displaced tank healer walks back first. Healers are never active on the threat hold, so the two
    // rows never compete.
    triggers.push_back(new TriggerNode("aq40 huhuran tank heal", {
        NextAction("aq40 huhuran tank heal", ACTION_RAID + 1) }));

    // Rev 3 (spec §8.2 #2): the MT's Berserk defensive outranks the park/threat rows (and its own rotation):
    // pull 2's MT died within ~5 s of Berserk. Off-GCD or instant, so it costs the tank rotation one tick.
    triggers.push_back(new TriggerNode("aq40 huhuran mt berserk defensive", {
        NextAction("aq40 huhuran mt berserk defensive", ACTION_EMERGENCY) }));

    LOG_DEBUG("playerbots", "[Aq40Cthun] strategy loaded: cthun={} eye={} exitAT={} acidExit={}",
              TempleOfAhnQirajHelpers::AQT_DATA_CTHUN,
              TempleOfAhnQirajHelpers::AQT_DATA_EYE_OF_CTHUN,
              TempleOfAhnQirajHelpers::AT_CTHUN_STOMACH_EXIT,
              TempleOfAhnQirajHelpers::CTHUN_ACID_EXIT_STACKS);

    LOG_DEBUG("playerbots",
              "[Aq40Twins] strategy loaded: twins={} veklor={} veknilash={} A=({:.1f},{:.1f}) B=({:.1f},{:.1f}) "
              "sep={:.1f}",
              TempleOfAhnQirajHelpers::AQT_DATA_TWIN_EMPERORS,
              TempleOfAhnQirajHelpers::AQT_DATA_VEKLOR,
              TempleOfAhnQirajHelpers::AQT_DATA_VEKNILASH,
              TempleOfAhnQirajHelpers::TWINS_STATIONS[0].station.GetPositionX(),
              TempleOfAhnQirajHelpers::TWINS_STATIONS[0].station.GetPositionY(),
              TempleOfAhnQirajHelpers::TWINS_STATIONS[1].station.GetPositionX(),
              TempleOfAhnQirajHelpers::TWINS_STATIONS[1].station.GetPositionY(),
              TempleOfAhnQirajHelpers::TWINS_STATIONS[0].station.GetExactDist2d(
                  TempleOfAhnQirajHelpers::TWINS_STATIONS[1].station));
    if constexpr (TempleOfAhnQirajHelpers::TWINS_STATIONS_PROVISIONAL)
        LOG_WARN("playerbots", "[Aq40Twins] stations A/B are PROVISIONAL (unprobed placeholder geometry): the Twin "
                               "Emperors strategy is INERT until they are authored with tools/aq40-twins-stations.py");

    LOG_DEBUG("playerbots", "[Aq40Huhuran] strategy loaded: huhuran={} npc={} park=({:.1f},{:.1f})",
              TempleOfAhnQirajHelpers::AQT_DATA_HUHURAN,
              TempleOfAhnQirajHelpers::NPC_HUHURAN,
              TempleOfAhnQirajHelpers::HUHURAN_PARK.GetPositionX(),
              TempleOfAhnQirajHelpers::HUHURAN_PARK.GetPositionY());
    if constexpr (TempleOfAhnQirajHelpers::HUHURAN_STATIONS_PROVISIONAL)
        LOG_WARN("playerbots", "[Aq40Huhuran] park/MT stations are PROVISIONAL (patrol centroid): the Huhuran "
                               "strategy is INERT (only the Wyvern Sting dispel rule runs) until they are authored "
                               "with tools/aq40-huhuran-stations.py");
}

void RaidAq40Strategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    // ALL THREE ship together. Shipping the target-hold veto later cost the first attempt four revisions, and the
    // threat multiplier is what enforces hold-fire (ruling R8).
    multipliers.push_back(new TwinEmperorsMovementMultiplier(botAI));
    multipliers.push_back(new TwinEmperorsTargetHoldMultiplier(botAI));
    multipliers.push_back(new TwinEmperorsThreatMultiplier(botAI));
    multipliers.push_back(new Aq40CthunMovementMultiplier(botAI));
    multipliers.push_back(new Aq40CthunTargetHoldMultiplier(botAI));
    multipliers.push_back(new Aq40CthunCastSafetyMultiplier(botAI));
    // ---- Huhuran ----
    multipliers.push_back(new Aq40HuhuranMovementMultiplier(botAI));
    multipliers.push_back(new Aq40HuhuranTargetHoldMultiplier(botAI));
    multipliers.push_back(new Aq40HuhuranTauntMultiplier(botAI));
    multipliers.push_back(new Aq40HuhuranThreatMultiplier(botAI));
    multipliers.push_back(new Aq40HuhuranCooldownHoldMultiplier(botAI));
}
