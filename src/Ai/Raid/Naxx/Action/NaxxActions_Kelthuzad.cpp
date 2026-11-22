/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "EncounterHelpers.h"
#include "NaxxHelpers.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

bool KelthuzadChooseTargetAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    Unit* const kelthuzad = AI_VALUE2(Unit*, "find target", "kel'thuzad");
    bool const naxx40 = kelthuzad && kelthuzad->GetEntry() != 15990;  // WotLK 15990; IP Naxx40 has its own entry
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    Unit* target = nullptr;
    Unit *target_soldier = nullptr, *target_weaver = nullptr, *target_abomination = nullptr, *target_kelthuzad = nullptr,
         *target_guardian = nullptr;
    for (auto i = attackers.begin(); i != attackers.end(); ++i)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (!unit)
            continue;

        if (botAI->EqualLowercaseName(unit->GetName(), "guardian of icecrown"))
        {
            if (!target_guardian)
                target_guardian = unit;
            else if (unit->GetVictim() && target_guardian->GetVictim() && unit->GetVictim()->ToPlayer() &&
                     target_guardian->GetVictim()->ToPlayer() && !botAI->IsAssistTank(unit->GetVictim()->ToPlayer()) &&
                     botAI->IsAssistTank(target_guardian->GetVictim()->ToPlayer()))
            {
                target_guardian = unit;
            }
            else if (unit->GetVictim() && target_guardian->GetVictim() && unit->GetVictim()->ToPlayer() &&
                     target_guardian->GetVictim()->ToPlayer() && !botAI->IsAssistTank(unit->GetVictim()->ToPlayer()) &&
                     !botAI->IsAssistTank(target_guardian->GetVictim()->ToPlayer()) &&
                     target_guardian->GetDistance2d(helper.center.first, helper.center.second) >
                         bot->GetDistance2d(unit))
            {
                target_guardian = unit;
            }
        }

        // IP Naxx40: Soldiers are picked up out to SOLDIER_REACH from the centre, beyond spell range too (the ranged
        // dps walks into range). With the stock 30 yd / spell-range limits a Soldier was ignored until it was about
        // to Dark Blast the raid, and 10-11 walked in together (11-21 P1 diag: soldiers in combat 0 <30 yd, 4-11 at
        // 30-60 yd, raid at the centre).
        constexpr float SOLDIER_REACH = 45.0f;
        bool const farSoldier = naxx40 && botAI->IsRanged(bot) &&
                                botAI->EqualLowercaseName(unit->GetName(), "soldier of the frozen wastes");
        if (unit->GetDistance2d(helper.center.first, helper.center.second) > (farSoldier ? SOLDIER_REACH : 30.0f))
            continue;

        if (!farSoldier && bot->GetDistance2d(unit) > sPlayerbotAIConfig.spellDistance)
            continue;

        if (botAI->EqualLowercaseName(unit->GetName(), "unstoppable abomination"))
        {
            if (target_abomination == nullptr ||
                target_abomination->GetDistance2d(helper.center.first, helper.center.second) >
                    unit->GetDistance2d(helper.center.first, helper.center.second))
            {
                target_abomination = unit;
            }
        }
        if (botAI->EqualLowercaseName(unit->GetName(), "soldier of the frozen wastes"))
        {
            if (target_soldier == nullptr ||
                target_soldier->GetDistance2d(helper.center.first, helper.center.second) >
                    unit->GetDistance2d(helper.center.first, helper.center.second))
            {
                target_soldier = unit;
            }
        }
        if (botAI->EqualLowercaseName(unit->GetName(), "soul weaver"))
        {
            if (target_weaver == nullptr || target_weaver->GetDistance2d(helper.center.first, helper.center.second) >
                                                unit->GetDistance2d(helper.center.first, helper.center.second))
                target_weaver = unit;
        }

        if (botAI->EqualLowercaseName(unit->GetName(), "kel'thuzad"))
            target_kelthuzad = unit;
    }
    std::vector<Unit*> targets;
    // IP Naxx40 phase 1: every ranged dps on Soldiers first, not two. A Soldier reaching the raid Dark Blasts it, and
    // guides have ranged and healers kill each one before it arrives while melee stay on the Abominations (research
    // 11-21). Melee keep the stock order: chasing Soldiers toward the alcoves pulled whole alcoves.
    if (botAI->IsRanged(bot))
    {
        if (naxx40 || botAI->GetRangedDpsIndex(bot) <= 1)
            targets = {target_soldier, target_weaver, target_abomination, target_kelthuzad};
        else
            targets = {target_weaver, target_soldier, target_abomination, target_kelthuzad};
    }
    else if (botAI->IsAssistTank(bot))
        targets = {target_abomination, target_guardian, target_kelthuzad};
    else
        targets = {target_abomination, target_kelthuzad};

    for (Unit* t : targets)
    {
        if (t)
        {
            target = t;
            break;
        }
    }
    if (context->GetValue<Unit*>("current target")->Get() == target)
        return false;

    if (target_kelthuzad && target == target_kelthuzad)
        return Attack(target, true);

    return Attack(target, false);
}

bool KelthuzadPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    if (helper.IsPhaseOne())
    {
        // Diagnostic (2026-11-21, temporary): P1 wipes to Dark Blast + Abominations on full gear. Every 10 s, from one
        // bot: adds in combat by distance from helper.center, and this bot's own distance from it.
        {
            static uint32 lastMs = 0;
            uint32 const nowMs = getMSTime();
            if (botAI->IsMainTank(bot) && (!lastMs || getMSTimeDiff(lastMs, nowMs) > 10000))
            {
                lastMs = nowMs ? nowMs : 1;
                uint32 counts[3][3] = {};
                char const* names[3] = {"soldier of the frozen wastes", "unstoppable abomination", "soul weaver"};
                for (ObjectGuid const guid : AI_VALUE(GuidVector, "possible targets"))
                {
                    Unit* u = botAI->GetUnit(guid);
                    if (!u || !u->IsAlive() || !u->IsInCombat())
                        continue;
                    for (int k = 0; k < 3; ++k)
                        if (botAI->EqualLowercaseName(u->GetName(), names[k]))
                        {
                            float const d = u->GetDistance2d(helper.center.first, helper.center.second);
                            ++counts[k][d < 30.0f ? 0 : d < 60.0f ? 1 : 2];
                        }
                }
                LOG_INFO("module", "[Naxx40 KT P1] center {:.1f} {:.1f}; tank {:.1f} yd from it; in combat <30/30-60/>60: "
                         "soldiers {}/{}/{} aboms {}/{}/{} weavers {}/{}/{}",
                         helper.center.first, helper.center.second, bot->GetDistance2d(helper.center.first, helper.center.second),
                         counts[0][0], counts[0][1], counts[0][2], counts[1][0], counts[1][1], counts[1][2], counts[2][0],
                         counts[2][1], counts[2][2]);
            }
        }
        if (AI_VALUE(Unit*, "current target") == nullptr)
            return MoveInside(NAXX_MAP_ID, helper.center.first, helper.center.second, bot->GetPositionZ(), 3.0f,
                              MovementPriority::MOVEMENT_COMBAT);
    }
    else if (helper.IsPhaseTwo())
    {
        Unit* shadow_fissure = helper.GetAnyShadowFissure();
        if (!shadow_fissure || !bot->IsWithinDistInMap(shadow_fissure, 10.0f))
        {
            float distance, angle;
            if (botAI->IsMainTank(bot))
            {
                if (AI_VALUE2(bool, "has aggro", "current target"))
                    return MoveTo(NAXX_MAP_ID, helper.tank_pos.first, helper.tank_pos.second, bot->GetPositionZ(), false, false, false,
                                  false, MovementPriority::MOVEMENT_COMBAT);
                else
                    return false;
            }
            else if (botAI->IsRanged(bot))
            {
                uint32 index = botAI->GetRangedIndex(bot);
                if (index < 8)
                {
                    distance = 20.0f;
                    angle = index * M_PI / 4;
                }
                else
                {
                    distance = 32.0f;
                    angle = (index - 8) * M_PI / 4;
                }
                float dx, dy;
                dx = helper.center.first + cos(angle) * distance;
                dy = helper.center.second + sin(angle) * distance;
                return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            else if (botAI->IsTank(bot))
            {
                Unit* cur_tar = AI_VALUE(Unit*, "current target");
                if (cur_tar && cur_tar->GetVictim() && cur_tar->GetVictim()->ToPlayer() &&
                    botAI->EqualLowercaseName(cur_tar->GetName(), "guardian of icecrown") &&
                    botAI->IsAssistTank(cur_tar->GetVictim()->ToPlayer()))
                {
                    return MoveTo(NAXX_MAP_ID, helper.assist_tank_pos.first, helper.assist_tank_pos.second, bot->GetPositionZ(),
                                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                }
                else
                    return false;
            }
        }
        else
        {
            float dx, dy;
            float angle;
            if (!botAI->IsRanged(bot))
                angle = shadow_fissure->GetAngle(helper.center.first, helper.center.second);
            else
                angle = bot->GetAngle(shadow_fissure) + M_PI;

            dx = shadow_fissure->GetPositionX() + cos(angle) * 10.0f;
            dy = shadow_fissure->GetPositionY() + sin(angle) * 10.0f;
            return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return false;
}

bool NaxxDetonateManaSpreadAction::Execute(Event /*event*/)
{
    constexpr float DETONATION_MARGIN = 2.0f;
    return MoveFromGroup(NaxxHelpers::GetManaDetonationRadius() + DETONATION_MARGIN);
}

bool NaxxChainsMarkAction::Execute(Event /*event*/)
{
    return EncounterHelpers::MarkTargetWithMoon(bot, NaxxHelpers::GetChainsCcTarget(botAI));
}
