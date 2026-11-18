/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MotionMaster.h"
#include "NaxxActions.h"
#include "NaxxHelpers.h"
#include "Playerbots.h"
#include "SpellMgr.h"

using namespace NaxxHelpers;

bool NaxxFaerlinaMindControlAction::Execute(Event /*event*/)
{
    Unit* worshipper = GetNearestNpc(botAI, NPC_NAXXRAMAS_WORSHIPPER, NAXX_ADD_SEARCH_DISTANCE);
    if (!worshipper)
        return false;

    if (bot->GetDistance2d(worshipper) > sPlayerbotAIConfig.spellDistance)
        return MoveNear(worshipper, sPlayerbotAIConfig.spellDistance, MovementPriority::MOVEMENT_COMBAT);

    return botAI->CastSpell("mind control", worshipper);
}

bool NaxxFaerlinaEmbraceAction::Execute(Event /*event*/)
{
    Unit* worshipper = GetCharmedWorshipper(bot);
    Unit* faerlina = GetNearestNpc(botAI, NPC_GRAND_WIDOW_FAERLINA, NAXX_ADD_SEARCH_DISTANCE);
    if (!worshipper || !faerlina)
        return false;

    // Walk the Worshipper to her, then sacrifice it on her once she is frenzied; earlier it is wasted.
    constexpr float EMBRACE_MARGIN = 2.0f;
    if (worshipper->GetExactDist2d(faerlina) > GetWidowsEmbraceRadius(worshipper) - EMBRACE_MARGIN)
    {
        if (!worshipper->isMoving())
            worshipper->GetMotionMaster()->MovePoint(0, faerlina->GetPosition());
        return true;
    }

    if (!IsFaerlinaFrenzied(faerlina))
        return false;

    worshipper->CastSpell(faerlina, sSpellMgr->GetSpellIdForDifficulty(SPELL_WIDOWS_EMBRACE, worshipper), false);
    return true;
}

bool NaxxFaerlinaBringWorshipperAction::Execute(Event /*event*/)
{
    Unit* faerlina = GetNearestNpc(botAI, NPC_GRAND_WIDOW_FAERLINA, NAXX_ADD_SEARCH_DISTANCE);
    if (!faerlina)
        return false;

    return MoveTo(faerlina, faerlina->GetCombatReach(), MovementPriority::MOVEMENT_COMBAT);
}
