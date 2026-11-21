/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <algorithm>
#include <cmath>
#include <list>
#include <unordered_map>
#include <vector>
#include "NaxxActions.h"
#include "NaxxTriggers.h"
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
            // IP's Naxx40 Understudies (351084) stand at their dummies and never join the fight, so they are not in
            // "attackers" (every priest logged "no free understudy among 1 attackers"): look for one near the bot.
            if (!target)
            {
                std::list<Creature*> nearby;
                bot->GetCreatureListWithEntryInGrid(nearby, NPC_UNDERSTUDY_40, 80.0f);
                for (Creature* c : nearby)
                    if (c->IsAlive() && !c->GetCharmerGUID() && !c->HasAura(10912) && !c->HasAura(10911) && !c->HasAura(605))
                    {
                        target = c;
                        break;
                    }
            }
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
                // Mind Control's own range, not the generic spell distance: most refusals were OUT_OF_RANGE (97) from
                // priests standing between the two (tr-20261121-083605-2: 31 of 64 attempts).
                float range = sPlayerbotAIConfig.spellDistance;
                if (SpellInfo const* mcInfo = sSpellMgr->GetSpellInfo(605))
                    range = std::min(range, mcInfo->GetMaxRange(false, bot) - 3.0f);
                if (bot->GetDistance(target) > range)
                    return MoveNear(target, range - 2.0f, MovementPriority::MOVEMENT_COMBAT);
                bool const ok = botAI->CastSpell("mind control", target);
                if (say)
                {
                    int32 why = -1;  // temporary diagnostic: the cast check's reason
                    if (!ok)
                        if (SpellInfo const* mc = sSpellMgr->GetSpellInfo(605))
                        {
                            Spell* probe = new Spell(bot, mc, TRIGGERED_IGNORE_POWER_AND_REAGENT_COST);
                            probe->m_targets.SetUnitTarget(target);
                            why = static_cast<int32>(probe->CheckCast(true));
                            delete probe;
                        }
                    LOG_INFO("module", "[Naxx40 Razuvious] {} mind control on {} ({:.0f} yd): {} (check {}, moving {}, level {} vs {})",
                             bot->GetName(), target->GetName(), bot->GetDistance(target), ok ? "cast" : "refused", why,
                             bot->isMoving() ? 1 : 0, bot->GetLevel(), target->GetLevel());
                }
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
    // Naxx40: his engage puts all four Understudies on the raid at once, and every tank took the same first free one, so
    // the other three killed casters (Understudy melee 10-19 deaths a wipe, tr-20261121-091533-6..-092008-9). One free
    // Understudy per tank, by tank slot (main tank 0, assist tanks 1..).
    if (botAI->IsTank(bot) && razuvious && razuvious->GetEntry() == 351036)
    {
        std::list<Creature*> nearby;
        bot->GetCreatureListWithEntryInGrid(nearby, NPC_UNDERSTUDY_40, 80.0f);
        std::vector<Creature*> free;
        for (Creature* c : nearby)
            if (c->IsAlive() && !c->GetCharmerGUID())
                free.push_back(c);
        std::sort(free.begin(), free.end(), [](Creature* a, Creature* b) { return a->GetGUID() < b->GetGUID(); });
        // The main tank holds Razuvious until a Mind-Controlled Understudy taunts him (with every tank on an Understudy
        // his melee was the top killer, 28-29 a wipe, tr-20261121-121744-3/-122032-4); assist tanks take one each.
        if (botAI->IsMainTank(bot) || free.empty())
            understudy = razuvious;
        else
            understudy = free[size_t(std::max(0, PlayerbotAI::GetAssistTankIndex(bot))) % free.size()];
    }
    Unit* target = nullptr;
    if (botAI->IsTank(bot))
        target = understudy;
    else
        target = razuvious;

    if (AI_VALUE(Unit*, "current target") == target)
        return false;

    return Attack(target);
}

namespace
{
constexpr uint32 NPC_RAZUVIOUS_40 = 351036;
constexpr float SHOUT_SAFE_DISTANCE = 34.0f;  // Disrupting Shout (26046) reaches 30 yd

// Ranged and healer mana users (healer priests only while not holding a Mind Control), while Naxx40 Razuvious is fighting.
bool IsShoutRangeBot(PlayerbotAI* botAI, Player* bot, Unit* razuvious)
{
    if (!razuvious || razuvious->GetEntry() != NPC_RAZUVIOUS_40 || !razuvious->IsAlive() || !razuvious->IsInCombat() ||
        !bot->IsInCombat() || bot->getPowerType() != POWER_MANA)
        return false;
    // Shadow priests do the Mind Controls (they would ping-pong between this and MoveNear); healer priests step out.
    if (bot->getClass() == CLASS_PRIEST)
        return botAI->IsHeal(bot) && !bot->GetCharmGUID();
    return botAI->IsRanged(bot) || botAI->IsHeal(bot);
}
}

bool RazuviousShoutRangeTrigger::IsActive()
{
    Unit* razuvious = AI_VALUE2(Unit*, "find target", "instructor razuvious");
    return IsShoutRangeBot(botAI, bot, razuvious) && bot->GetExactDist2d(razuvious) < SHOUT_SAFE_DISTANCE - 1.0f;
}

// Disrupting Shout is IP's vanilla mana burn (26046: instant, 30 yd, every 25 s) and killed about half of every
// 40-player raid, priests most of all (tr-20261121-081520-2). Step out to 34 yd on a spot both the boss and the bot can
// see: a point behind a wall sent the whole raid round the corridors (tr-20261121-074556-3).
bool RazuviousKeepShoutRangeAction::Execute(Event /*event*/)
{
    Unit* razuvious = AI_VALUE2(Unit*, "find target", "instructor razuvious");
    if (!IsShoutRangeBot(botAI, bot, razuvious))
        return false;
    // Diagnostic (2026-11-21, temporary): healers 10-29 yd from him still died to the shout (tr-20261121-120757-2).
    static std::unordered_map<uint64, uint32> lastSay;
    uint32 const nowMs = getMSTime();
    uint32& lastMs = lastSay[bot->GetGUID().GetRawValue()];
    bool const say = !lastMs || getMSTimeDiff(lastMs, nowMs) > 10000;
    if (say)
        lastMs = nowMs ? nowMs : 1;
    float const base = razuvious->GetAngle(bot);
    static float const offsets[] = {0.0f, 0.4f, -0.4f, 0.8f, -0.8f, 1.2f, -1.2f, 1.6f, -1.6f};
    for (float off : offsets)
    {
        float const angle = base + off;
        float x = razuvious->GetPositionX() + SHOUT_SAFE_DISTANCE * std::cos(angle);
        float y = razuvious->GetPositionY() + SHOUT_SAFE_DISTANCE * std::sin(angle);
        float z = razuvious->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);
        if (std::fabs(z - razuvious->GetPositionZ()) > 4.0f)
            continue;
        if (!razuvious->IsWithinLOS(x, y, z + 2.0f) || !bot->IsWithinLOS(x, y, z + 2.0f))
            continue;
        bool const moved = MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        if (say)
            LOG_INFO("module", "[Naxx40 Razuvious] {} shout range: {:.1f} yd, moving to a spot {:.1f} yd off (offset {:.1f}): {}",
                     bot->GetName(), bot->GetExactDist2d(razuvious), bot->GetExactDist2d(x, y), off, moved ? "moving" : "MoveTo refused");
        return moved;
    }
    if (say)
        LOG_INFO("module", "[Naxx40 Razuvious] {} shout range: {:.1f} yd, no clear spot at 34 yd", bot->GetName(),
                 bot->GetExactDist2d(razuvious));
    return false;
}
