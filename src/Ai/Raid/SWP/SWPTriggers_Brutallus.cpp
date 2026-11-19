/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPTriggers.h"

#include "Playerbots.h"
#include "EncounterHelpers.h"

using namespace EncounterHelpers;

#include "SWPHelpers.h"

using namespace SunwellPlateauHelpers;

// Shared gate: the encounter is live and this bot is a participant.
// Gate on the BOSS being in combat, never on the bot's own combat flag: the resting tank
// deliberately stops attacking (see BrutallusTankHoldMultiplier) and would otherwise drop
// combat and abandon his station.
static bool BrutallusActive(Player* bot, Creature** bossOut = nullptr)
{
    if (!IsInSunwell(bot))
        return false;

    Creature* boss = FindBrutallus(bot);
    if (!boss || !boss->IsAlive() || !boss->IsInCombat())
        return false;

    // A bot milling about elsewhere in the instance is not in this fight and must not be handed
    // a station (nor, via the multiplier, a veto).
    if (bot->GetExactDist2d(boss->GetPositionX(), boss->GetPositionY()) > BRUTALLUS_PARTICIPANT_RANGE * 2.0f)
        return false;

    if (bossOut)
        *bossOut = boss;
    return true;
}

bool BrutallusOutOfStationTrigger::IsActive()
{
    Creature* boss = nullptr;
    if (!BrutallusActive(bot, &boss))
        return false;

    Position station;
    if (!GetBrutallusStation(bot, botAI, boss, station))
        return false;

    return bot->GetExactDist2d(station.GetPositionX(), station.GetPositionY()) >
           BRUTALLUS_STATION_TOLERANCE;
}

bool BrutallusTankSwapTrigger::IsActive()
{
    if (!botAI->IsTank(bot))
        return false;

    Creature* boss = nullptr;
    if (!BrutallusActive(bot, &boss))
        return false;

    Unit* victim = boss->GetVictim();
    if (victim == bot)
        return false;  // already holding him

    if (!bot->IsWithinDist(boss, BRUTALLUS_TAUNT_RANGE))
        return false;

    // The opening window belongs to the main tank alone. The off tank taunting here is half of why
    // the two of them traded the boss back and forth at the pull: with nobody holding him yet the
    // "aggro accident" branch below is true for BOTH tanks, so both grab, and each grab re-swings
    // the cone across a raid that is still forming up.
    BrutallusSnapshot const& snap = GetBrutallusSnapshot(bot);
    if (snap.valid && GetBrutallusTankRole(bot, botAI) != BrutallusTankRole::Main &&
        getMSTimeDiff(snap.combatStartMs, getMSTime()) < BRUTALLUS_OFFTANK_OPENING_MS)
    {
        return false;
    }

    // Aggro accident (nobody, or a non-tank holding him): whichever tank bot notices first
    // grabs him back. Otherwise swap only once the holder is deep enough in stacks AND we
    // are cleaner than he is - that second half is what paces the rotation against the 40s
    // decay, since swapping back early would hand the cone to a lane that hasn't shed yet.
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (!holder || !botAI->IsTank(holder))
        return true;

    uint32 const holderStacks = GetBrutallusSlashStacks(holder);
    if (holderStacks < BRUTALLUS_SWAP_STACKS)
        return false;

    return GetBrutallusSlashStacks(bot) < holderStacks;
}

bool BrutallusCanPurgeBurnTrigger::IsActive()
{
    if (!bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BURN_DOT)))
        return false;

    // The classes with an ability that strips Burn - see BrutallusPurgeBurnAction, including why the
    // rogue is on the list despite his attributes saying otherwise.
    if (bot->getClass() != CLASS_MAGE && bot->getClass() != CLASS_PALADIN &&
        bot->getClass() != CLASS_ROGUE)
    {
        return false;
    }

    Creature* boss = nullptr;
    return BrutallusActive(bot, &boss);
}

bool BrutallusBurningTrigger::IsActive()
{
    if (!bot->HasAura(static_cast<uint32>(SunwellSpells::SPELL_BURN_DOT)))
        return false;

    Creature* boss = nullptr;
    if (!BrutallusActive(bot, &boss))
        return false;

    if (boss->GetVictim() == bot)
        return false;  // the holder stays put; his next Stomp removes his Burn

    // Same station machinery, just at emergency relevance: while burned, GetBrutallusStation
    // returns the quarantine spot, so this fires until the bot is clear of the formation and
    // then goes quiet. No move-away action to oscillate against.
    Position station;
    if (!GetBrutallusStation(bot, botAI, boss, station))
        return false;

    return bot->GetExactDist2d(station.GetPositionX(), station.GetPositionY()) >
           BRUTALLUS_STATION_TOLERANCE;
}
