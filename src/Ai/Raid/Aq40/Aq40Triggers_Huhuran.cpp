/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Princess Huhuran triggers (overlay patch 0016). Spec:
// docs/superpowers/specs/2026-10-20-playerbot-aq40-huhuran-design.md (overlay repo), §4.2-4.3.

#include "Aq40Triggers.h"

#include "Aq40Helpers.h"
#include "Playerbots.h"

using namespace TempleOfAhnQirajHelpers;

bool Aq40HuhuranNotInCombatTrigger::IsActive()
{
    return IsInAq40(bot) && !bot->IsInCombat() && !IsHuhuranEncounterActive(bot);
}

bool Aq40HuhuranMtHasWorkTrigger::IsActive()
{
    // Cheap role test first: the MT is always a role tank (GetHuhuranMainTank).
    if (!PlayerbotAI::IsTank(bot))
        return false;
    HuhuranView view;
    if (!GetHuhuranView(bot, view) || view.mainTank != bot)
        return false;
    if (GetHuhuranMtStep(bot, view, AI_VALUE(Unit*, "current target")) != HuhuranMtStep::None)
        return true;
    // Rev 7 (spec §12.2 #2): parked, the MT holds position (generic movers vetoed) and only turns to face her.
    return HuhuranMtShouldFace(bot, view);
}

// The fork's FindTargetForTankStrategy skips a target whose victim is the main tank, so stock off-tanks
// would idle; threat is the only way to pick her up when the MT dies (she is taunt-immune). Rev 2 (spec
// §7.2 #3): also true while the off-tank is off its melee spot beside the MT. Not while the off-tank is at
// the threat cap with no spot to walk to: the engage would only restart the auto-attack the hold stopped.
// Rev 6 (spec §11.2): also true while an off-tank standing (IsStopped) at its spot is not facing her: the movement
// multiplier vetoes stock "tank face" for it. A bot still moving is left alone so the facing never claims every tick.
bool Aq40HuhuranOffTankIdleTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;
    HuhuranView view;
    if (!GetHuhuranView(bot, view) || view.mainTank == bot || HuhuranOpenerHold(view))
        return false;
    Position spot;
    if (GetHuhuranOffTankSpot(bot, view, spot))
    {
        if (bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) > Aq40HuhuranRules::OFFTANK_SPOT_TOLERANCE)
            return true;
        if (bot->IsStopped() && !bot->HasInArc(HUHURAN_FACE_ARC, view.huhuran))
            return true;
    }
    if (AI_VALUE(Unit*, "current target") == view.huhuran)
        return false;
    return !HuhuranThreatVetoed(bot, view);
}

// True while the bot owns a ring spot and is off it. Within SLOT_TOLERANCE the trigger is false, so the
// hold action never re-issues the same destination (MoveTo's duplicate guard would refuse it).
bool Aq40HuhuranOffSpotTrigger::IsActive()
{
    if (GetHuhuranSlotRole(bot, botAI) == HuhuranSlotRole::None)
        return false;
    HuhuranView view;
    if (!GetHuhuranView(bot, view))
        return false;
    Position spot;
    if (!GetHuhuranSpot(bot, botAI, view, spot))
        return false;
    return bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) > Aq40HuhuranRules::SLOT_TOLERANCE;
}

// Rev 2 (spec §7.2 #2), mirroring BwlThreatHoldTrigger: the hard stop, on the plain ratio (no headroom). Active
// only while the bot is attacking, so it fires once per breach; the threat multiplier then keeps the attack
// from restarting until the bot is back under the 0.80 veto. Cheap map/role/attacking tests before the view.
bool Aq40HuhuranThreatHoldTrigger::IsActive()
{
    if (bot->GetMapId() != AQ40_MAP_ID || PlayerbotAI::IsHeal(bot))
        return false;
    if (!bot->GetVictim() && !bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
        return false;
    if (AI_VALUE(bool, "neglect threat"))
        return false;

    Unit* const current = AI_VALUE(Unit*, "current target");
    if (!current || current->GetEntry() != NPC_HUHURAN)
        return false;
    HuhuranView view;
    if (!GetHuhuranView(bot, view) || current != view.huhuran || !HuhuranThreatDisciplined(bot, view))
        return false;
    // Off-tanks at 1.05 of the MT (rev 3), DPS at 0.90 of the top living off-tank (fallback the MT): MT > backup
    // > DPS (R11).
    return HuhuranThreatHardStop(bot, view);
}

// Rev 2 (spec §7.2 #4): only when the pick is castable now (BWL BwlNefarianPriestTankOnlyTrigger rule).
bool Aq40HuhuranTankHealTrigger::IsActive()
{
    // Cheap role and class tests first.
    if (!PlayerbotAI::IsHeal(bot) || !Aq40TankHealSpellName(bot))
        return false;
    HuhuranView view;
    if (!GetHuhuranView(bot, view) || !view.berserk)
        return false;
    Player* tank = nullptr;
    char const* spell = nullptr;
    return GetHuhuranTankHeal(bot, botAI, view, tank, spell);
}

// Rev 3 (spec §8.2 #2): the MT has an unused Berserk defensive that it can cast now.
bool Aq40HuhuranMtBerserkDefensiveTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;
    HuhuranView view;
    if (!GetHuhuranView(bot, view) || !view.berserk || view.mainTank != bot)
        return false;
    char const* spell = nullptr;
    return GetHuhuranMtDefensive(bot, botAI, view, spell) != HuhuranMtDefensive::None;
}
