/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <unordered_map>
#include "NaxxActions.h"
#include "ObjectGuid.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SharedDefines.h"

namespace
{
constexpr uint32 SPELL_UNDERSTUDY_TAUNT = 29060;
constexpr uint32 SPELL_BONE_BARRIER     = 29061;
constexpr uint32 SPELL_BLOOD_STRIKE     = 61696;

// Matched by entry, not name: mod-individual-progression's Naxx40 student (351084) is named
// "Deathknight Understudy", so a name match never found it and no priest mind-controlled.
constexpr uint32 NPC_UNDERSTUDY    = 16803;
constexpr uint32 NPC_UNDERSTUDY_40 = 351084;

// First understudy in the bot's attackers that nobody has mind-controlled yet.
Unit* FindFreeUnderstudy(PlayerbotAI* botAI, GuidVector const& attackers)
{
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        uint32 entry = unit->GetEntry();
        if (entry != NPC_UNDERSTUDY && entry != NPC_UNDERSTUDY_40)
            continue;

        if (unit->GetCharmerGUID())
            continue;

        return unit;
    }
    return nullptr;
}
}

bool RazuviousUseObedienceCrystalAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    // bot->GetCharm
    if (Unit* charm = bot->GetCharm())
    {
        Unit* target = AI_VALUE2(Unit*, "find target", "instructor razuvious");
        if (!target)
            return false;

        if (charm->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_ACTIVE) == NULL_MOTION_TYPE)
        {
            charm->GetMotionMaster()->Clear();
            charm->GetMotionMaster()->MoveChase(target);
            charm->GetAI()->AttackStart(target);
        }
        Aura* forceObedience = botAI->GetAura("force obedience", charm);
        uint32 duration_time;
        if (!forceObedience)
        {
            forceObedience = botAI->GetAura("mind control", charm);
            duration_time = 60000;
        }
        else
            duration_time = 90000;

        if (!forceObedience)
            return false;

        if (charm->GetDistance(target) <= 0.51f)
        {
            // taunt
            bool tauntUseful = true;
            if (forceObedience->GetDuration() <= int32(duration_time - 5000))
            {
                Unit* victim = target->GetVictim();
                if (victim && victim->HasAura(SPELL_BONE_BARRIER))
                    tauntUseful = false;

                if (forceObedience->GetDuration() <= 3000)
                    tauntUseful = false;

            }
            if (forceObedience->GetDuration() >= int32(duration_time - 500))
                tauntUseful = false;

            if (tauntUseful && !charm->HasSpellCooldown(29060))
            {
                // shield
                if (!charm->HasSpellCooldown(SPELL_BONE_BARRIER))
                {
                    charm->CastSpell(charm, SPELL_BONE_BARRIER, true);
                    charm->AddSpellCooldown(SPELL_BONE_BARRIER, 0, 30 * 1000);
                }
                charm->CastSpell(target, SPELL_UNDERSTUDY_TAUNT, true);
                charm->AddSpellCooldown(SPELL_UNDERSTUDY_TAUNT, 0, 20 * 1000);
            }
            // strike (the WotLK understudy only; the Naxx40 one has no Blood Strike)
            if (charm->GetEntry() == NPC_UNDERSTUDY && !charm->HasSpellCooldown(SPELL_BLOOD_STRIKE))
            {
                charm->CastSpell(target, SPELL_BLOOD_STRIKE, true);
                charm->AddSpellCooldown(SPELL_BLOOD_STRIKE, 0, 4 * 1000);
            }
        }
    }
    else
    {
        Difficulty diff = bot->GetRaidDifficulty();
        if (diff == RAID_DIFFICULTY_10MAN_NORMAL)
        {
            GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
            for (auto i = npcs.begin(); i != npcs.end(); i++)
            {
                Creature* unit = botAI->GetCreature(*i);
                if (!unit)
                    continue;

                if (botAI->IsMainTank(bot) && unit->GetSpawnId() != 128352)
                    continue;

                if (!botAI->IsMainTank(bot) && unit->GetSpawnId() != 128353)
                    continue;

                if (MoveTo(unit, 0.0f, MovementPriority::MOVEMENT_COMBAT))
                    return true;

                Creature* creature = bot->GetNPCIfCanInteractWith(*i, UNIT_NPC_FLAG_SPELLCLICK);
                if (!creature)
                    continue;

                creature->HandleSpellClick(bot);
                return true;
            }
        }
        else
        {
            Unit* target = FindFreeUnderstudy(botAI, context->GetValue<GuidVector>("attackers")->Get());
            // Diagnostic (2026-11-20 overnight): Naxx40 Razuvious wiped with 3 priests and the Understudies on the
            // raid; say what each priest does, at most every 10 s.
            static std::unordered_map<uint64, uint32> lastSay;
            uint32 const now = getMSTime();
            uint32& last = lastSay[bot->GetGUID().GetRawValue()];
            bool const say = !last || getMSTimeDiff(last, now) > 10000;
            if (say)
                last = now ? now : 1;
            if (target)
            {
                if (bot->GetDistance2d(target) > sPlayerbotAIConfig.spellDistance)
                    return MoveNear(target, sPlayerbotAIConfig.spellDistance, MovementPriority::MOVEMENT_COMBAT);
                bool const ok = botAI->CastSpell("mind control", target);
                if (say)
                    LOG_INFO("module", "[Naxx40 Razuvious] {} mind control on {} ({:.0f} yd): {}", bot->GetName(),
                             target->GetName(), bot->GetDistance(target), ok ? "cast" : "refused");
                return ok;
            }
            if (say)
                LOG_INFO("module", "[Naxx40 Razuvious] {} found no free understudy among {} attackers", bot->GetName(),
                         context->GetValue<GuidVector>("attackers")->Get().size());
        }
    }
    return false;
}

bool RazuviousTargetAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    Unit* razuvious = AI_VALUE2(Unit*, "find target", "instructor razuvious");
    Unit* understudy = FindFreeUnderstudy(botAI, context->GetValue<GuidVector>("attackers")->Get());
    Unit* target = nullptr;
    if (botAI->IsTank(bot))
        target = understudy;
    else
        target = razuvious;

    if (AI_VALUE(Unit*, "current target") == target)
        return false;

    return Attack(target);
}
