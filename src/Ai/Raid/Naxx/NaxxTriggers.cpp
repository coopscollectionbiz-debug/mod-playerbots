/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxTriggers.h"
#include "EncounterHelpers.h"
#include "NaxxHelpers.h"
#include "NaxxSpellIds.h"
#include "Playerbots.h"
#include "Timer.h"
#include "Trigger.h"

bool MutatingInjectionMeleeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
        return false;

    return MutatingInjectionTrigger::IsActive() && !botAI->IsRanged(bot);
}

bool MutatingInjectionRangedTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
        return false;

    return MutatingInjectionTrigger::IsActive() && botAI->IsRanged(bot);
}

bool AuraRemovedTrigger::IsActive()
{
    bool check = botAI->HasAura(name, bot, false, false, -1, true);
    bool ret = false;
    if (prev_check && !check)
        ret = true;

    prev_check = check;
    return ret;
}

bool MutatingInjectionRemovedTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
        return false;

    return HasNoAuraTrigger::IsActive() && botAI->GetState() == BOT_STATE_COMBAT && botAI->IsRanged(bot);
}

bool GrobbulusCloudTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
        return false;

    if (!botAI->IsMainTank(bot))
        return false;

    // bot->Yell("has aggro on " + boss->GetName() + " : " + to_string(AI_VALUE2(bool, "has aggro", "boss target")),
    // LANG_UNIVERSAL);
    if (!AI_VALUE2(bool, "has aggro", "boss target"))
        return false;

    uint32 now = getMSTime();
    bool poison_cloud_casting = false;
    if (boss->HasUnitState(UNIT_STATE_CASTING))
    {
        Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = boss->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

        if (spell)
            poison_cloud_casting = NaxxSpellIds::MatchesAnySpellId(spell->GetSpellInfo(), {NaxxSpellIds::PoisonCloud});

    }
    if (!poison_cloud_casting && last_cloud_ms != 0 && now - last_cloud_ms < CloudRotationDelayMs)
        return false;

    last_cloud_ms = now;
    return true;
}

bool HeiganMeleeTrigger::IsActive()
{
    return PlayerbotAI::IsMelee(bot) && helper.UpdateBossAI();
}

bool HeiganRangedTrigger::IsActive()
{
    return PlayerbotAI::IsRanged(bot) && helper.UpdateBossAI();
}

bool RazuviousTankTrigger::IsActive()
{
    Difficulty diff = bot->GetRaidDifficulty();
    if (diff == RAID_DIFFICULTY_10MAN_NORMAL)
        return helper.UpdateBossAI() && botAI->IsTank(bot);

    return helper.UpdateBossAI() && bot->getClass() == CLASS_PRIEST;
}

bool RazuviousNontankTrigger::IsActive()
{
    Difficulty diff = bot->GetRaidDifficulty();
    if (diff == RAID_DIFFICULTY_10MAN_NORMAL)
        return helper.UpdateBossAI() && !(botAI->IsTank(bot));

    return helper.UpdateBossAI() && !(bot->getClass() == CLASS_PRIEST);
}

bool FourHorsemenAttractorsTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
        return false;

    return helper.IsAttracter(bot);
}

bool FourHorsemenExceptAttractorsTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
        return false;

    return !helper.IsAttracter(bot);
}

bool SapphironGroundTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
        return false;

    return helper.IsPhaseGround();
}

bool SapphironFlightTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
        return false;

    return helper.IsPhaseFlight();
}

bool GluthTrigger::IsActive() { return helper.UpdateBossAI(); }

bool GluthMainTankMortalWoundTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
        return false;

    if (!botAI->IsAssistTankOfIndex(bot, 0))
        return false;

    Unit* mt = AI_VALUE(Unit*, "main tank");
    if (!mt)
        return false;

    Aura* aura = NaxxSpellIds::GetAnyAura(mt, {NaxxSpellIds::MortalWound10, NaxxSpellIds::MortalWound25});
    if (!aura)
    {
        // Fallback to name for custom spell data.
        aura = botAI->GetAura("mortal wound", mt, false, true);
    }
    if (!aura || aura->GetStackAmount() < 5)
        return false;

    return true;
}

bool KelthuzadTrigger::IsActive() { return helper.UpdateBossAI(); }

bool AnubrekhanTrigger::IsActive() {
    Unit* boss = AI_VALUE2(Unit*, "find target", "anub'rekhan");
    if (!boss)
        return false;

    return true;
}

bool FaerlinaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grand widow faerlina");
    if (!boss)
        return false;

    return true;
}

bool MaexxnaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "maexxna");
    if (!boss)
        return false;

    return !botAI->IsTank(bot);
}

//bool PatchwerkTankTrigger::IsActive()
//{
//    Unit* boss = AI_VALUE2(Unit*, "find target", "patchwerk");
//    if (!boss)
//    {
//        return false;
//    }
//    return !botAI->IsTank(bot) && !botAI->IsRanged(bot);
//}
//
//bool PatchwerkRangedTrigger::IsActive()
//{
//    Unit* boss = AI_VALUE2(Unit*, "find target", "patchwerk");
//    if (!boss)
//    {
//        return false;
//    }
//    return !botAI->IsTank(bot) && botAI->IsRanged(bot);
//}
//
//bool PatchwerkNonTankTrigger::IsActive()
//{
//    Unit* boss = AI_VALUE2(Unit*, "find target", "patchwerk");
//    if (!boss)
//    {
//        return false;
//    }
//    return !botAI->IsTank(bot);
//}

bool LoathebTrigger::IsActive() { return helper.UpdateBossAI(); }

bool ThaddiusPhasePetTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
        return false;

    return helper.IsPhasePet();
}

bool ThaddiusPhaseTransitionTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
        return false;

    return helper.IsPhaseTransition();
}

bool ThaddiusPhaseThaddiusTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
        return false;

    return helper.IsPhaseThaddius();
}

using namespace NaxxHelpers;

static bool IsTankPlayer(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && PlayerbotAI::IsTank(player);
}

bool NaxxKillOrderMarkTrigger::IsActive()
{
    return bot->IsInCombat() && EncounterHelpers::IsMechanicTrackerBot(bot, MAP_NAXXRAMAS) &&
           GetKillOrderMarkTarget(botAI);
}

bool NaxxFaerlinaMindControlTrigger::IsActive()
{
    // 25-player only: there a dying Worshipper doesn't cast Widow's Embrace, so a priest's Worshipper must.
    if (!Is25Player(bot) || GetFaerlinaPriest(bot) != bot || GetCharmedWorshipper(bot) ||
        bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return false;

    Unit* faerlina = GetNearestNpc(botAI, NPC_GRAND_WIDOW_FAERLINA, NAXX_ADD_SEARCH_DISTANCE);
    return IsFaerlinaFrenzied(faerlina) && GetNearestNpc(botAI, NPC_NAXXRAMAS_WORSHIPPER, NAXX_ADD_SEARCH_DISTANCE);
}

bool NaxxFaerlinaEmbraceTrigger::IsActive()
{
    return GetCharmedWorshipper(bot) != nullptr;
}

bool NaxxFaerlinaBringWorshipperTrigger::IsActive()
{
    // 10-player: the tank holding the Worshippers drags one beside a frenzied Faerlina for the kill.
    if (Is25Player(bot) || !PlayerbotAI::IsTank(bot))
        return false;

    Unit* faerlina = GetNearestNpc(botAI, NPC_GRAND_WIDOW_FAERLINA, NAXX_ADD_SEARCH_DISTANCE);
    if (!IsFaerlinaFrenzied(faerlina) || faerlina->GetVictim() == bot)
        return false;

    for (ObjectGuid const guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == NPC_NAXXRAMAS_WORSHIPPER && unit->GetVictim() == bot &&
            unit->GetExactDist2d(faerlina) >= GetWidowsEmbraceRadius(unit))
            return true;
    }
    return false;
}

bool NaxxMaexxnaWebWrapTrigger::IsActive()
{
    // Ranged DPS free the cocooned players.
    if (!PlayerbotAI::IsRangedDps(bot) || !AI_VALUE2(Unit*, "find target", "maexxna"))
        return false;

    Unit* victim = bot->GetVictim();
    if (victim && victim->IsAlive() && victim->GetEntry() == NPC_WEB_WRAP)
        return false;

    return GetNearestNpc(botAI, NPC_WEB_WRAP, NAXX_ADD_SEARCH_DISTANCE) != nullptr;
}

bool NaxxNothNotTankedTrigger::IsActive()
{
    // Blink wipes his threat, so whoever he turns to is rarely a tank; the main tank takes him back.
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* noth = AI_VALUE2(Unit*, "find target", "noth the plaguebringer");
    Unit* victim = noth ? noth->GetVictim() : nullptr;
    if (!victim || IsTankPlayer(victim))
        return false;

    return EncounterHelpers::GetTakeoverTank(bot, victim) == bot;
}

bool NaxxPlaguedChampionRangeTrigger::IsActive()
{
    if (!PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsTank(bot))
        return false;

    return GetNearestNpc(botAI, NPC_PLAGUED_CHAMPION, PLAGUED_CHAMPION_SHOCK_DISTANCE) != nullptr;
}

bool NaxxGothikSideTrigger::IsActive()
{
    // Before the pull: the gate closes once the fight starts, so each bot gets to its side first.
    if (bot->IsInCombat() || !IsInGothikRoom(bot))
        return false;

    constexpr float GOTHIK_SEARCH_DISTANCE = 150.0f;
    Creature* gothik = bot->FindNearestCreature(NPC_GOTHIK, GOTHIK_SEARCH_DISTANCE);
    if (!gothik || gothik->IsInCombat())
        return false;

    return IsOnGothikDeadSide(bot) != IsGothikDeadSideMember(bot);
}

bool NaxxGothikNotTankedTrigger::IsActive()
{
    // His teleports reset threat; the tank on the side he lands on takes him.
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* gothik = AI_VALUE2(Unit*, "find target", "gothik the harvester");
    Unit* victim = gothik ? gothik->GetVictim() : nullptr;
    if (!victim || IsTankPlayer(victim))
        return false;

    bool const gateOpen = IsGothikGateOpen(bot);
    bool const gothikDeadSide = IsOnGothikDeadSide(gothik);
    return EncounterHelpers::GetTakeoverTank(bot, victim, [gateOpen, gothikDeadSide](Player const* tank)
        { return !gateOpen && IsOnGothikDeadSide(tank) != gothikDeadSide; }) == bot;
}

bool NaxxPatchwerkSoakerTrigger::IsActive()
{
    if (!IsHatefulStrikeSoaker(bot))
        return false;

    Unit* patchwerk = AI_VALUE2(Unit*, "find target", "patchwerk");
    return patchwerk && (bot->GetVictim() != patchwerk || !bot->IsWithinMeleeRange(patchwerk));
}

bool NaxxPatchwerkMeleeTrigger::IsActive()
{
    // Melee DPS stand behind him, out of his parries.
    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsTank(bot))
        return false;

    Unit* patchwerk = AI_VALUE2(Unit*, "find target", "patchwerk");
    return patchwerk && bot->GetVictim() == patchwerk;
}

bool NaxxThaneMeteorStackTrigger::IsActive()
{
    // Meteor splits between everyone within 8y of its target, so his attackers stand together.
    if (PlayerbotAI::IsTank(bot) || helper.IsAttracter(bot))
        return false;

    Unit* thane = AI_VALUE2(Unit*, "find target", "thane korth'azz");
    if (!thane || !thane->IsAlive())
        return false;

    // Only those attacking him; healers stay where they reach both front tanks.
    return AI_VALUE(Unit*, "current target") == thane &&
           bot->GetExactDist2d(thane) > thane->GetCombatReach() + THANE_METEOR_STACK_DISTANCE;
}

bool NaxxDetonateManaTrigger::IsActive()
{
    return bot->HasAura(SPELL_DETONATE_MANA) &&
           EncounterHelpers::GetNearestPlayerInRadius(bot, GetManaDetonationRadius()) != nullptr;
}

bool NaxxChainsMarkTrigger::IsActive()
{
    // 25-player: Kel'Thuzad mind-controls three raiders; guides crowd-control them instead of killing them.
    if (!bot->IsInCombat() || !EncounterHelpers::IsMechanicTrackerBot(bot, MAP_NAXXRAMAS) ||
        !AI_VALUE2(Unit*, "find target", "kel'thuzad"))
        return false;

    return GetChainsCcTarget(botAI) != nullptr;
}
