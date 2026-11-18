/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SWPMultipliers.h"
#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "SWPActions.h"
#include "SWPHelpers.h"
#include "SpellAuras.h"
#include <ctime>

using namespace EncounterHelpers;
using namespace SunwellPlateauHelpers;

// The unit an attack or spell action is aimed at, for actions that pick their own target.
static Unit* GetActionTarget(PlayerbotAI* botAI, Action* action)
{
    if (dynamic_cast<CastSpellAction*>(action))
        return action->GetTarget();
    if (dynamic_cast<AttackAction*>(action))
        return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return nullptr;
}

static bool IsCurseRemovalSpell(std::string const& spell)
{
    return spell == "remove curse" || spell == "remove lesser curse" || spell == "cleanse spirit";
}

static bool IsAttackAction(Action* action)
{
    if (auto* spellAction = dynamic_cast<CastSpellAction*>(action))
        return spellAction->getThreatType() != Action::ActionThreatType::None;
    return dynamic_cast<AttackAction*>(action) != nullptr;
}

float KalecgosMultiplier::GetValue(Action* action)
{
    if (bot->GetMapId() != MAP_SUNWELL_PLATEAU)
        return 1.0f;

    // Curse of Boundless Agony only ever moves: guides decurse tanks at once and others after 15s.
    if (auto* spellAction = dynamic_cast<CastSpellAction*>(action))
    {
        if (IsCurseRemovalSpell(spellAction->getSpell()))
        {
            Unit* target = action->GetTarget();
            Aura const* curse = target ? target->GetAura(SPELL_CURSE_OF_BOUNDLESS_AGONY_PLR) : nullptr;
            if (!curse && target)
                curse = target->GetAura(SPELL_CURSE_OF_BOUNDLESS_AGONY);

            Player* targetPlayer = target ? target->ToPlayer() : nullptr;
            if (curse && !(targetPlayer && PlayerbotAI::IsTank(targetPlayer)) &&
                (time(nullptr) - curse->GetApplyTime()) * IN_MILLISECONDS < KALECGOS_CURSE_KEEP_MS)
                return 0.0f;
            return 1.0f;
        }
    }

    if (PlayerbotAI::IsTank(bot) || !IsAttackAction(action))
        return 1.0f;

    Unit* target = GetActionTarget(botAI, action);
    if (!target || (target->GetEntry() != NPC_KALECGOS && target->GetEntry() != NPC_SATHROVARR))
        return 1.0f;

    // Both dragons enrage at 10%, so the raid brings them down together: whichever is ahead waits.
    bool const isKalecgos = target->GetEntry() == NPC_KALECGOS;
    Unit* other = FindCreatureAnyRealm(bot, isKalecgos ? NPC_SATHROVARR : NPC_KALECGOS);
    if (!other || !other->IsAlive() || other->HasAura(SPELL_KALECGOS_BANISH))
        return 1.0f;

    float const targetPct = target->GetHealthPct();
    float const otherPct = other->GetHealthPct();

    // Sathrovarr goes to 1% first; if Kalecgos is banished first, no more portals open.
    if (isKalecgos && targetPct <= KALECGOS_HOLD_PCT)
        return 0.0f;

    if (targetPct < otherPct - KALECGOS_HEALTH_SYNC_PCT && !(!isKalecgos && targetPct <= KALECGOS_HOLD_PCT))
        return 0.0f;

    return 1.0f;
}

float BrutallusMultiplier::GetValue(Action* action)
{
    // The stacked tank lets the other tank keep him instead of taunting straight back.
    if (!IsTauntAction(bot, action))
        return 1.0f;

    if (GetAuraStackAmount(bot, SPELL_METEOR_SLASH) < BRUTALLUS_SWAP_STACKS && !bot->HasAura(SPELL_BRUTALLUS_STOMP))
        return 1.0f;

    return AI_VALUE2(Unit*, "find target", "brutallus") ? 0.0f : 1.0f;
}

float FelmystBreathMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<MovementAction*>(action) || dynamic_cast<SwpFelmystDodgeLaneAction*>(action) ||
        dynamic_cast<SwpAvoidHazardAction*>(action) || dynamic_cast<AvoidAoeAction*>(action))
        return 1.0f;

    // While a lane of fog is coming, nobody walks anywhere else: it could be back into the lane.
    Unit* boss = AI_VALUE2(Unit*, "find target", "felmyst");
    if (!boss)
        boss = GetNearestNpc(botAI, NPC_FELMYST, SWP_ADD_SEARCH_DISTANCE);

    return GetFelmystBreathLane(boss) >= 0 ? 0.0f : 1.0f;
}

float SunwellAssignedTankMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<TankAssistAction*>(action) || !PlayerbotAI::IsTank(bot))
        return 1.0f;

    bool const twinsTank = (GetAlythessTank(bot) == bot || IsSacrolashTank(bot)) &&
                           AI_VALUE2(Unit*, "find target", "grand warlock alythess");
    bool const sentinelTank = GetGroupAssistTank(bot, 0) == bot &&
                              GetNearestNpc(botAI, NPC_VOID_SENTINEL, SWP_ADD_SEARCH_DISTANCE);
    bool const handTank = GetAssignedHand(botAI) != nullptr;

    return twinsTank || sentinelTank || handTank ? 0.0f : 1.0f;
}

float KiljaedenDragonMultiplier::GetValue(Action* action)
{
    // A possessed dragon is flown by the dragon action alone; the rider's own body can't act.
    if (GetPossessedDrake(bot))
        return dynamic_cast<SwpRideDragonAction*>(action) ? 1.0f : 0.0f;

    // While Darkness is cast, the raid holds on the dragon instead of walking out of its shield.
    if (!dynamic_cast<MovementAction*>(action) || dynamic_cast<SwpStackOnDragonAction*>(action))
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "kil'jaeden");
    return IsUsingDarkness(boss) && GetShieldDrake(botAI) ? 0.0f : 1.0f;
}
