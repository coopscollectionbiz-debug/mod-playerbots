/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldExtraMultipliers.h"
#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UldExtraActions.h"
#include "UldExtraHelpers.h"

using namespace EncounterHelpers;
using namespace UlduarExtraHelpers;

float UlduarExtraMultiplier::GetValue(Action* action)
{
    if (bot->GetMapId() != MAP_ULDUAR)
        return 1.0f;

    // Iron Council tanks hold their own member instead of following the raid's target.
    if (dynamic_cast<TankAssistAction*>(action) && GetAssignedCouncilMember(botAI))
        return 0.0f;

    // Algalon: the stacked tank lets the other tank keep him rather than taunting straight back.
    if (IsTauntAction(bot, action) && GetAuraStackAmount(bot, SPELL_PHASE_PUNCH) >= PHASE_PUNCH_SWAP_STACKS)
        return 0.0f;

    // Hodir: during Frozen Blows the main tank leaves him to the frost-resistance off-tank.
    if (IsTauntAction(bot, action) && PlayerbotAI::IsMainTank(bot) && GetGroupAssistTank(bot, 0))
    {
        Unit* hodir = AI_VALUE2(Unit*, "find target", "hodir");
        if (hodir && HasDifficultyAura(hodir, SPELL_FROZEN_BLOWS, hodir))
            return 0.0f;
    }

    // Thorim: likewise while Unbalancing Strike is up.
    if (IsTauntAction(bot, action) && bot->HasAura(SPELL_UNBALANCING_STRIKE) &&
        AI_VALUE2(Unit*, "find target", "thorim"))
        return 0.0f;

    // Kologarn: likewise for Crunch Armor.
    if (IsTauntAction(bot, action) && GetCrunchArmorStacks(bot) >= CRUNCH_ARMOR_SWAP_STACKS &&
        AI_VALUE2(Unit*, "find target", "kologarn"))
        return 0.0f;

    if (dynamic_cast<MovementAction*>(action))
    {
        // Big Bang: bots walking into a Black Hole do nothing else on the way.
        if (!PlayerbotAI::IsMainTank(bot) && !dynamic_cast<UldAlgalonEnterBlackHoleAction*>(action))
        {
            Unit* algalon = AI_VALUE2(Unit*, "find target", "algalon the observer");
            int32 remaining = 0;
            if (IsCastingDifficultySpell(algalon, SPELL_BIG_BANG, &remaining) && remaining <= BIG_BANG_ENTER_LEAD_MS &&
                GetNearestNpc(botAI, NPC_BLACK_HOLE, ULDUAR_EXTRA_SEARCH_DISTANCE))
                return 0.0f;
        }

        // Ignis: the off-tank stands in Scorch on purpose while its constructs heat up.
        if ((dynamic_cast<AvoidAoeAction*>(action) || dynamic_cast<FleeAction*>(action)) &&
            PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
        {
            Position destination;
            if (GetIgnisConstructDestination(botAI, destination))
                return 0.0f;
        }
        return 1.0f;
    }

    // Ignis' Flame Jets and the Ground Tremor of Freya and Elder Stonebark interrupt spellcasting:
    // guides use only instant casts until they land.
    if (auto* spellAction = dynamic_cast<CastSpellAction*>(action))
    {
        Unit* ignis = AI_VALUE2(Unit*, "find target", "ignis the furnace master");
        Unit* freya = AI_VALUE2(Unit*, "find target", "freya");
        Unit* stonebark = AI_VALUE2(Unit*, "find target", "elder stonebark");
        if (IsCastingDifficultySpell(ignis, SPELL_FLAME_JETS) ||
            IsCastingDifficultySpell(freya, SPELL_GROUND_TREMOR_FREYA) ||
            IsCastingDifficultySpell(stonebark, SPELL_GROUND_TREMOR_STONEBARK))
        {
            uint32 const spellId = AI_VALUE2(uint32, "spell id", spellAction->getSpell());
            SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
            if (info && (info->CalcCastTime(bot) > 0 || info->IsChanneled()))
                return 0.0f;
        }
    }

    // General Vezax: hard mode needs six Saronite Vapors alive at once to form the Animus.
    if (IsUlduarHardMode(botAI) && (dynamic_cast<AttackAction*>(action) || dynamic_cast<CastSpellAction*>(action)))
    {
        Unit* target = dynamic_cast<CastSpellAction*>(action) ? action->GetTarget() : AI_VALUE(Unit*, "current target");
        if (target && target->GetEntry() == NPC_SARONITE_VAPORS)
            return 0.0f;
    }

    if (PlayerbotAI::IsTank(bot))
        return 1.0f;

    Unit* target = nullptr;
    if (dynamic_cast<CastSpellAction*>(action))
        target = action->GetTarget();
    else if (dynamic_cast<AttackAction*>(action))
        target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    switch (target->GetEntry())
    {
        case NPC_IRON_CONSTRUCT:
            // DPS only shatter Brittle constructs; damage before that is wasted.
            return HasDifficultyAura(target, SPELL_IGNIS_BRITTLE, target) ? 1.0f : 0.0f;
        case NPC_HEART_OF_THE_DECONSTRUCTOR:
            // Killing the heart starts hard mode.
            return IsUlduarHardMode(botAI) || target->GetHealthPct() > XT_HEART_STOP_PCT ? 1.0f : 0.0f;
        case NPC_FERAL_DEFENDER:
            // It revives on death with a void zone; guides leave it to the off-tank.
            return 0.0f;
        default:
            return 1.0f;
    }
}
