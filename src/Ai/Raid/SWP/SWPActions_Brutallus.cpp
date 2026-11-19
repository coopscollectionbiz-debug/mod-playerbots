/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPActions.h"

#include <algorithm>
#include <cmath>

#include "Playerbots.h"
#include "SWPHelpers.h"
#include "ThreatManager.h"

using namespace SunwellPlateauHelpers;

bool BrutallusMoveToStationAction::Execute(Event /*event*/)
{
    Creature* boss = FindBrutallus(bot);
    if (!boss)
        return false;

    Position station;
    if (!GetBrutallusStation(bot, botAI, boss, station))
        return false;

    float const distance = bot->GetExactDist2d(station.GetPositionX(), station.GetPositionY());

    // Diagnostics: station, distance, and the Meteor Slash stack count as the CODE sees it. Every
    // stack-driven decision (the tank handoff, the bail failsafe) reads this, and a fight where
    // the tank visibly reached 4 stacks produced zero swap triggers - so the number the code sees
    // is the thing to measure, not the number on screen. `role` distinguishes the two assigned
    // tanks (1 = main, 2 = off) from everyone else, which is how to confirm the raid's own Main
    // Tank / Main Assist assignments actually reached the strategy.
    BrutallusTankRole const tankRole = GetBrutallusTankRole(bot, botAI);
    LOG_DEBUG("playerbots",
              "[Brutallus] {} role {} station ({:.1f},{:.1f}) dist {:.1f} slash {} burn {}",
              bot->GetName(), uint32(tankRole), station.GetPositionX(), station.GetPositionY(),
              distance, GetBrutallusSlashStacks(bot),
              bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BURN_DOT)) ? 1 : 0);

    if (distance <= BRUTALLUS_STATION_TOLERANCE)
        return false;

    if (!botAI->CanMove())
        return false;

    // MOVEMENT_FORCED, and then ALWAYS fall through. The priority is what protects the walk:
    // IsWaitingForLastMove refuses any same-or-lower-priority order (generic melee, tank face and
    // combat formation move all run below FORCED) for the leg's duration, so the position holds
    // without this action having to claim the tick. Claiming it starved every heal and attack in
    // the raid one build ago; surrendering it without the priority bump let generic movement drag
    // everyone back into a clump the build before that. This is the combination that does neither
    // - measured: 1158 samples 5-10y off station and 1420 at 10-25y, i.e. a tug-of-war.
    MoveTo(bot->GetMapId(), station.GetPositionX(), station.GetPositionY(), station.GetPositionZ(),
           false, false, false, false, MovementPriority::MOVEMENT_FORCED);

    return false;
}

// Which classes can strip Burn and therefore never need to leave their slash zone.
//
// Burn (46394) is strippable at all because its ATTR0 (0x04000800) does NOT include
// SPELL_ATTR0_NO_IMMUNITIES - and note that Burn being UNDISPELLABLE is irrelevant here, since
// SpellInfo::CanDispelAura only rejects passive and NO_IMMUNITIES auras and never looks at the
// dispel type.
//
// Ice Block (45438) and Divine Shield (642) both carry SPELL_ATTR1_IMMUNITY_PURGES_EFFECT (0x8000),
// which is the flag SpellInfo::ApplyAllSpellImmunitiesTo checks before clearing pre-applied auras.
//
// CLOAK OF SHADOWS (31224) IS HERE ON RUNTIME EVIDENCE, NOT ON THAT FLAG - it removes Burn in game
// (confirmed in a live kill) even though its DBC attributes do not show the purge bit: ATTR1 reads
// 0x00020020, its effects are (APPLY_AURA 186, TRIGGER_SPELL, APPLY_AURA 87), and there is no
// spell_linked_spell or spell_custom_attr row for it. So do NOT remove the rogue from this list
// after re-reading the attribute table and concluding it cannot work - the observed behaviour wins,
// and the mechanism simply is not visible in the data checked so far.
//
// Any entry here is safe even if its ability turns out not to purge: the isolation station stays
// keyed on the aura, so a bot whose purge failed still walks out on the next tick.
static char const* BrutallusBurnPurgeSpell(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
            return "ice block";
        case CLASS_PALADIN:
            return "divine shield";
        case CLASS_ROGUE:
            return "cloak of shadows";
        default:
            return nullptr;
    }
}

bool BrutallusPurgeBurnAction::Execute(Event /*event*/)
{
    char const* spell = BrutallusBurnPurgeSpell(bot);
    if (!spell)
        return false;

    // If this fails (cooldown - both are ~5 min, so this is roughly once per pull) the bot simply
    // keeps its Burn and the isolation station takes over on the next tick. Nothing to fall back to
    // explicitly: the station logic is already keyed on the aura.
    return botAI->CastSpell(spell, bot);
}

bool BrutallusTauntBossAction::Execute(Event /*event*/)
{
    Creature* boss = FindBrutallus(bot);
    if (!boss || !boss->IsAlive())
        return false;

    // A TAUNT ALONE CANNOT SWAP TANKS, which is why this took many live tests to get right.
    // Spell::EffectTaunt calls ThreatManager::MatchUnitThreatToHighestThreat, which sets the
    // taunter EQUAL to the current top - and ThreatManager refuses to change victim until a
    // contender breaks 110% of it. So the 3s taunt aura holds the boss, expires, threat is merely
    // tied, and the boss snaps back to whoever never stopped swinging. Measured: 67 taunts, zero
    // swaps; and in a later test the swap only landed once the off tank had out-damaged the main
    // tank's whole accumulated threat, by which point the holder was at 6 stacks and the raid's
    // clothies died to the next slash.
    //
    // So the handoff explicitly cuts the OUTGOING tank's threat. That is the same thing a real raid
    // does with a threat-drop on the swap, it is deterministic, and it is far safer than the
    // alternative of freezing the holder's damage: freezing hands the boss to whoever is next on
    // threat, which can be a caster, and then he turns and sweeps the cone across the raid.
    //
    // TWO details, and the first cost another live test:
    //  * CUT BEFORE TAUNTING. MatchUnitThreatToHighestThreat RAISES the taunter to whatever the top
    //    currently is, so a cut applied after the taunt lands is simply matched away - the outgoing
    //    tank is still top, the taunter is set equal to him, neither breaks 110%, and the boss
    //    returns to him when the aura expires. Cutting first means the match can only ever help.
    //  * The cut is COMPUTED, not a fixed percentage: aim 10% under the taunter's own threat divided
    //    by the 110% margin, i.e. the smallest reduction that actually changes the victim. That
    //    makes it self-limiting - once the outgoing tank is below the target this does nothing.
    Unit* const previous = boss->GetVictim();
    bool acted = false;

    if (previous && previous != bot)
    {
        // Only ever demote another TANK. Cutting a caster who grabbed the boss by accident would
        // just hand it to the next caster along.
        Player* previousTank = previous->ToPlayer();
        if (previousTank && botAI->IsTank(previousTank))
        {
            ThreatManager& threatMgr = boss->GetThreatMgr();
            float const mine = threatMgr.GetThreat(bot);
            float const theirs = threatMgr.GetThreat(previousTank);
            float const target = mine / 1.1f * 0.9f;

            // Require a real threat base of our own (at least half the holder's) before cutting
            // him. Without that guard a relief tank who has not been swinging - his opening window,
            // a late res - would strip the holder down to nothing and hand the boss to whichever
            // caster is next, and a caster holding him sweeps the cone across the raid.
            if (mine > 0.0f && theirs > target && mine >= theirs * 0.5f)
            {
                threatMgr.ScaleThreat(previousTank, std::max(target / theirs, 0.05f));
                acted = true;
            }
        }
    }

    if (previous != bot && CastClassTaunt(bot, botAI, boss))
        acted = true;

    // Unit::Attack returns false when already melee-attacking this target, which is a tank's
    // normal state - so this must not be the action's whole return value, or a perfectly good
    // taunt gets logged as FAILED and the engine falls through past it.
    if (Attack(boss))
        acted = true;

    return acted;
}
