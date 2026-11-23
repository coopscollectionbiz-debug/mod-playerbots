/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "Ai/Raid/RaidDiag.h"
#include "EncounterHelpers.h"
#include "NaxxHelpers.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

#include <mutex>
#include <unordered_map>

// Debug diag (11-22, playerbots AiPlayerbot.RaidDiagnostics): Naxx40 Kel'Thuzad phase 1, one line per instance every 5 s. Soldiers (351073) are summoned
// every 3.1 s onto a random raid member from the room edge; this measures how far they get before they die, where the
// raid stands relative to the room centre, and what ranged and melee are targeting.
static void KelthuzadPhaseOneDiag(Player* bot, Unit* kelthuzad, std::pair<float, float> const& center)
{
    if (!RaidDiagEnabled())
        return;
    static std::mutex m;
    static std::unordered_map<uint32, uint32> last;
    static std::unordered_map<uint32, uint32> start;
    uint32 const now = getMSTime();
    uint32 const inst = bot->GetInstanceId();
    {
        std::lock_guard<std::mutex> lock(m);
        if (!start.count(inst) || getMSTimeDiff(start[inst], now) > 900000)
            start[inst] = now;
        if (last.count(inst) && getMSTimeDiff(last[inst], now) < 5000)
            return;
        last[inst] = now;
    }
    Group* group = bot->GetGroup();
    if (!group)
        return;
    float sx = 0, sy = 0;
    uint32 alive = 0;
    std::vector<Player*> members;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* p = ref->GetSource())
            if (p->IsAlive() && p->GetMapId() == bot->GetMapId())
            {
                members.push_back(p);
                sx += p->GetPositionX();
                sy += p->GetPositionY();
                ++alive;
            }
    if (!alive)
        return;
    float const cx = sx / alive, cy = sy / alive;
    float spread = 0;
    for (Player* p : members)
        spread += p->GetExactDist2d(cx, cy);
    spread /= alive;
    // Adds in combat by distance from the raid centroid: <10 / 10-25 / 25-45 / >45.
    auto bands = [&](uint32 entry, uint32& inCombat, uint32 (&b)[4], uint32& onMelee)
    {
        std::list<Creature*> list;
        bot->GetCreatureListWithEntryInGrid(list, entry, 120.0f);
        for (Creature* c : list)
        {
            if (!c->IsAlive() || !c->IsInCombat())
                continue;
            ++inCombat;
            float const d = c->GetExactDist2d(cx, cy);
            ++b[d < 10 ? 0 : d < 25 ? 1 : d < 45 ? 2 : 3];
            if (Unit* v = c->GetVictim())
                if (Player* vp = v->ToPlayer())
                    if (PlayerbotAI* vai = GET_PLAYERBOT_AI(vp))
                        if (!vai->IsRanged(vp))
                            ++onMelee;
        }
    };
    uint32 sN = 0, sB[4] = {}, sM = 0, aN = 0, aB[4] = {}, aM = 0, wN = 0, wB[4] = {}, wM = 0;
    bands(351073, sN, sB, sM);
    bands(351074, aN, aB, aM);
    bands(351075, wN, wB, wM);
    uint32 rS = 0, rW = 0, rA = 0, rNone = 0, mS = 0, mA = 0, mNone = 0;
    for (Player* p : members)
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(p);
        if (!ai)
            continue;
        Unit* t = ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        uint32 const e = t && t->ToCreature() ? t->GetEntry() : 0;
        if (ai->IsRanged(p) && !ai->IsHeal(p))
            ++(e == 351073 ? rS : e == 351075 ? rW : e == 351074 ? rA : rNone);
        else if (!ai->IsRanged(p) && !ai->IsTank(p))
            ++(e == 351073 ? mS : e == 351074 ? mA : mNone);
    }
    RAID_DIAG("playerbots", "[Naxx40KT P1] t={}s alive={} raid@centre {:.1f}yd spread {:.1f} | soldiers {} [<10:{} 10-25:{} 25-45:{} >45:{}] | abom {} [{} {} {} {}] on-melee {} | weavers {} [{} {} {} {}] | ranged S{} W{} A{} none{} | melee S{} A{} none{}",
             getMSTimeDiff(start[inst], now) / 1000, alive, std::sqrt((cx - center.first) * (cx - center.first) + (cy - center.second) * (cy - center.second)),
             spread, sN, sB[0], sB[1], sB[2], sB[3], aN, aB[0], aB[1], aB[2], aB[3], aM, wN, wB[0], wB[1], wB[2], wB[3], rS, rW, rA,
             rNone, mS, mA, mNone);
    (void)kelthuzad;
}

bool KelthuzadChooseTargetAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;
    if (helper.IsPhaseOne())
        if (Unit* kt = AI_VALUE2(Unit*, "find target", "kel'thuzad"))
            if (kt->GetEntry() != 15990)
                KelthuzadPhaseOneDiag(bot, kt, helper.center);

    // Naxx40 P1: a Soldier's Dark Blast aura (28458) procs on its own melee hit and blasts everyone within 100 yd
    // (28457, radius index 12). Melee who engaged Soldiers made them swing: 20-30 bots died in 5 s at t=10
    // (tr-20261122-110619-3, -112430-2). Melee never fight Soldiers in phase 1; only ranged kill them, before they arrive.
    if (helper.IsPhaseOne() && !botAI->IsRanged(bot))
        if (Unit* kt = AI_VALUE2(Unit*, "find target", "kel'thuzad"))
            if (kt->GetEntry() != 15990)
                if (Unit* cur = AI_VALUE(Unit*, "current target"))
                    if (cur->GetEntry() == 351073)
                    {
                        bot->AttackStop();
                        context->GetValue<Unit*>("current target")->Set(nullptr);
                    }

    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    Unit* target = nullptr;
    Unit *target_soldier = nullptr, *target_weaver = nullptr, *target_abomination = nullptr, *target_kelthuzad = nullptr,
         *target_guardian = nullptr;
    uint32 abominationsAtCentre = 0;
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

        if (unit->GetDistance2d(helper.center.first, helper.center.second) > 30.0f)
            continue;

        if (bot->GetDistance2d(unit) > sPlayerbotAIConfig.spellDistance)
            continue;

        if (botAI->EqualLowercaseName(unit->GetName(), "unstoppable abomination"))
        {
            if (unit->GetDistance2d(helper.center.first, helper.center.second) < 12.0f)
                ++abominationsAtCentre;
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
    Unit* const kelthuzad = AI_VALUE2(Unit*, "find target", "kel'thuzad");
    bool const naxx40 = kelthuzad && kelthuzad->GetEntry() != 15990;  // WotLK 15990; IP Naxx40 has its own entry
    if (botAI->IsRanged(bot))
    {
        // Naxx40 P1 diag (tr-20261122-104902-2): no Soldier ever got within 10 yd of the raid (ranged shot them at
        // 25-45 yd), but 5-6 Abominations sat in the raid for over a minute with 4-7 melee on them (Abomination melee
        // + Mortal Wound 23 of 45 deaths). Ranged keep Soldiers first only when one is close; otherwise two or more
        // Abominations at the centre come before far Soldiers and Weavers.
        bool const soldierClose =
            target_soldier && target_soldier->GetDistance2d(helper.center.first, helper.center.second) < 20.0f;
        // Weavers before Abominations: with Abominations first, Wail of Souls took 19 of 47 (tr-20261122-110745-4).
        bool const weaverNear =
            target_weaver && target_weaver->GetDistance2d(helper.center.first, helper.center.second) < 30.0f;
        if (naxx40 && !soldierClose && weaverNear && !botAI->IsHeal(bot))
            targets = {target_weaver, target_abomination, target_soldier, target_kelthuzad};
        else if (naxx40 && !soldierClose && abominationsAtCentre >= 2 && !botAI->IsHeal(bot))
            targets = {target_abomination, target_soldier, target_weaver, target_kelthuzad};
        else if (naxx40 || botAI->GetRangedDpsIndex(bot) <= 1)
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
        // Naxx40 P1: everyone but the tanks holds the centre. At t=10 s the raid drifted 14-17 yd toward IP's gate pack
        // with melee on its Soldiers, and Dark Blast killed 20 in 5 s (tr-20261122-110619-3).
        Unit* const kt = AI_VALUE2(Unit*, "find target", "kel'thuzad");
        if (kt && kt->GetEntry() != 15990 && !botAI->IsTank(bot) &&
            bot->GetDistance2d(helper.center.first, helper.center.second) > 12.0f)
            return MoveInside(NAXX_MAP_ID, helper.center.first, helper.center.second, bot->GetPositionZ(), 3.0f,
                              MovementPriority::MOVEMENT_FORCED);
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
