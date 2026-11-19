/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BWLTriggers.h"
#include "BWLHelpers.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Playerbots.h"
#include "StringFormat.h"
#include "Timer.h"

using namespace BlackwingLairHelpers;

// General

bool BwlSuppressionDeviceTrigger::IsActive()
{
    // Until MoP, only rogues could disarm suppression devices.
    // If raid cheats are enabled, any bot can disarm the devices.
    if (botAI->HasCheat(BotCheatMask::raid) || bot->IsClass(CLASS_ROGUE))
    {
        GuidVector gos = AI_VALUE(GuidVector, "nearest game objects");
        for (auto i = gos.begin(); i != gos.end(); ++i)
        {
            GameObject const* go = botAI->GetGameObject(*i);
            if (IsActiveSuppressionDeviceInRange(go, bot))
                return true;
        }
    }
    return false;
}

// Razorgore the Untamed

bool BwlRazorgoreNotMindControlledTrigger::IsActive()
{
    if (Unit* boss = AI_VALUE2(Unit*, "find target", "razorgore the untamed"))
        return !boss->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_MINDCONTROL));
    return false;
}

// Vaelastrasz the Corrupt

bool BwlVaelastraszPositioningTrigger::IsActive()
{
    // Prevent non-tanks from rotating the boss while the tanks gain threat.
    if (Unit* boss = AI_VALUE2(Unit*, "find target", "vaelastrasz the corrupt"))
        return boss->GetVictim() != bot;
    return false;
}

bool BwlVaelastraszBurningAdrenalineTrigger::IsActive()
{
    // No check for Vaelastrasz, because bots may still have burning adrenaline even after Vaelastrasz died.
    return bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_BURNING_ADRENALINE));
}

// Broodlord Lashlayer

bool BwlBroodlordRangedTooCloseTrigger::IsActive()
{
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    if (Unit* boss = AI_VALUE2(Unit*, "find target", "broodlord lashlayer"))
    {
        // In case the bot pulled aggro, prevent it from kiting the boss through the room.
        if (boss->GetVictim() != bot)
            return bot->GetDistance2d(boss) < BROODLORD_SAFE_DISTANCE;
    }
    return false;
}

// Firemaw / Ebonroc / Flamegor

bool BwlBlackDrakeNotVictimTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", bossName);
    if (!boss)
        return false;

    // The tank holding the boss stays where it is to avoid rotating the boss.
    return boss->GetVictim() != bot;
}

// Chromaggus

bool BwlAfflictionBronzeTrigger::IsActive()
{
    return bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_BROOD_AFFLICTION_BRONZE));
}

// Nefarian

bool BwlNefarianPositioningTrigger::IsActive()
{
    // A DPS or tank on a Bone Construct has to reach it, wherever it stands around him.
    if (!PlayerbotAI::IsHeal(bot) && IsBoneConstruct(AI_VALUE(Unit*, "current target")))
        return false;

    // Prevent non-tanks from rotating the boss while the tanks gain threat.
    if (Unit* boss = AI_VALUE2(Unit*, "find target", "nefarian"))
        return boss->GetVictim() != bot;
    return false;
}

bool BwlNefarianWildMagicTrigger::IsActive()
{
    return bot->getClass() == CLASS_MAGE &&
        bot->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_WILD_MAGIC));
}

bool BwlNefarianCorruptedTotemTrigger::IsActive()
{
    // Shaman call: the shamans drop totems that buff Nefarian and his adds or burn the raid. DPS kill them.
    if (!PlayerbotAI::IsDps(bot) || !AI_VALUE2(Unit*, "find target", "nefarian"))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive() && IsCorruptedTotem(target))
        return false;

    return FindCorruptedTotem(botAI) != nullptr;
}

// Nefarian phases 2 and 3 (overlay patch 0020, spec 2026-10-07)

bool BwlNefarianMtStationTrigger::IsActive()
{
    NefarianView view;
    if (!GetNefarianView(bot, view) || view.mainTank != bot)
        return false;
    return NefMtStationHasWork(bot, view, AI_VALUE(Unit*, "current target"));
}

bool BwlNefarianPositionTrigger::IsActive()
{
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    Position dest;
    return GetNefPositionTarget(bot, botAI, view, dest);
}

bool BwlNefarianDpsTargetTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;
    // Healers stay healers, except a druid locked in cat form (spec §4.5).
    if (PlayerbotAI::IsHeal(bot) &&
        !(bot->getClass() == CLASS_DRUID && bot->HasAura(BwlNefarianRules::SPELL_CALL_DRUID)))
        return false;
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    return GetNefDpsTarget(bot, botAI, view) != AI_VALUE(Unit*, "current target");
}

bool BwlNefarianOtTargetTrigger::IsActive()
{
    NefarianView view;
    if (!GetNefarianView(bot, view) || GetNefRole(bot, view) != NefRole::OffTank)
        return false;
    Unit* const current = AI_VALUE(Unit*, "current target");
    if (Unit* const add = GetNefOffTankTarget(bot, view))
        return current != add ||
               (add->GetEntry() == BwlNefarianRules::NPC_CORRUPTED_INFERNAL && add->GetVictim() != bot);
    // No add: Nefarian (spec §9 #2), never taunted; hold fire at the MT-denominated cap.
    if (OffTankHoldsFireOnNefarian(bot, view))
        return current == view.nefarian || bot->GetVictim() == view.nefarian;
    return current != view.nefarian;
}

bool BwlNefarianTremorTrigger::IsActive()
{
    if (bot->getClass() != CLASS_SHAMAN)
        return false;
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    // By name: era-talents shamans know a clone (931080 / 947070), not stock 8143 (spec §9 #3).
    return !AI_VALUE2(bool, "has totem", "tremor totem") && botAI->CanCastSpell("tremor totem", bot);
}

bool BwlNefarianDecurseTankTrigger::IsActive()
{
    uint8 const cls = bot->getClass();
    if (cls != CLASS_MAGE && cls != CLASS_DRUID)
        return false;
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    return view.mainTank && view.mainTank->HasAura(BwlNefarianRules::SPELL_VEIL_OF_SHADOW) &&
           bot->GetDistance(view.mainTank) <= 40.0f && botAI->CanCastSpell("remove curse", view.mainTank);
}

bool BwlNefarianDispelPolymorphTrigger::IsActive()
{
    uint8 const cls = bot->getClass();
    if (cls != CLASS_PRIEST && cls != CLASS_PALADIN)
        return false;
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    Player* const target = GetNefPolymorphDispelTarget(bot, view);
    return target && botAI->CanCastSpell(cls == CLASS_PRIEST ? "dispel magic" : "cleanse", target);
}

bool BwlNefarianPriestTankOnlyTrigger::IsActive()
{
    if (bot->getClass() != CLASS_PRIEST || !bot->HasAura(BwlNefarianRules::SPELL_CALL_PRIEST))
        return false;
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    Player* tank = nullptr;
    BwlNefarianRules::TankHeal spell = BwlNefarianRules::TankHeal::None;
    // Only when the pick is castable now (GCD, fear, range): no fallback to another tank (spec §9 #4).
    return GetNefTankHeal(bot, botAI, view, tank, spell) && botAI->CanCastSpell(NefTankHealSpellName(spell), tank);
}

bool BwlNefarianHunterRearmTrigger::IsActive()
{
    if (!HasBrokenRangedWeapon(bot))   // hunters only
        return false;
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    return FindSpareRangedWeapon(bot) != 0;
}

bool BwlNefarianRogueSurviveTrigger::IsActive()
{
    if (bot->getClass() != CLASS_ROGUE || !bot->HasAura(BwlNefarianRules::SPELL_CALL_ROGUE))
        return false;
    NefarianView view;
    if (!GetNefarianView(bot, view))
        return false;
    return !botAI->HasAura("cloak of shadows", bot) && !botAI->HasAura("evasion", bot) &&
           (botAI->CanCastSpell("cloak of shadows", bot) || botAI->CanCastSpell("evasion", bot));
}

// Spec §9 #5: Sunder only while neither Revenge nor Shield Slam can be cast right now.
bool BwlNefarianMtSunderTrigger::IsActive()
{
    if (bot->getClass() != CLASS_WARRIOR || bot->GetPower(POWER_RAGE) < 150)   // rage is in tenths
        return false;
    NefarianView view;
    if (!GetNefarianView(bot, view) || view.mainTank != bot)
        return false;
    Creature* const nef = view.nefarian;
    if (AI_VALUE(Unit*, "current target") != nef || !bot->IsWithinMeleeRange(nef))
        return false;
    return botAI->CanCastSpell("sunder armor", nef) && !botAI->CanCastSpell("revenge", nef) &&
           !botAI->CanCastSpell("shield slam", nef);
}

// Trash

bool BwlDeathTalonWyrmguardTankTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    // Patch 0020: the two keyed packs use fixed stations ("bwl wyrmguard tank"); this 16 y spacing
    // would drag a tank off its station. It stays live for the lone Wyrmguard + Overseers pack.
    WyrmguardPackView view;
    if (GetActiveWyrmguardPack(bot, view))
        return false;

    return AI_VALUE2(Unit*, "find target", "death talon wyrmguard");
}

bool BwlDeathTalonWyrmguardRangedTrigger::IsActive()
{
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    // Patch 0020: keyed packs are visible from the first tick, even to a healer/caster that has
    // never threatened them (find target only scans the bot's own threat list).
    WyrmguardPackView view;
    if (GetActiveWyrmguardPack(bot, view))
        return true;

    return AI_VALUE2(Unit*, "find target", "death talon wyrmguard");
}

// Death Talon Wyrmguard packs (overlay patch 0020)

bool BwlWyrmguardTankTrigger::IsActive()
{
    int const role = GetTankRole(bot);
    if (role == BwlWyrmguardRules::NONE)
        return false;

    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;

    for (int s = 0; s < BwlWyrmguardRules::SLOTS; ++s)
        if (view.assign.holderRole[s] == role)
            return true;
    return false;
}

namespace
{
    constexpr uint32 NO_SKULL_LOG_INTERVAL_MS = 2000;

    // Why NextSkull marks nothing: per slot alive / holder role / on holder / distance to its station /
    // min distance to a living guard held on another station / time continuously settled (skull lead).
    std::string DescribeNoSkull(WyrmguardPackView const& view)
    {
        using namespace BwlWyrmguardRules;
        PackState const& p = view.state;
        std::string out;
        for (int s = 0; s < SLOTS; ++s)
        {
            if (!p.alive[s])
            {
                out += Acore::StringFormat(" | slot {}: dead", s);
                continue;
            }
            float const toStation = Dist2d(p.guard[s], p.station[view.assign.station[s]]);
            float minOther = -1.0f;
            for (int t = 0; t < SLOTS; ++t)
            {
                if (t == s || !p.alive[t] || view.assign.station[t] == view.assign.station[s])
                    continue;
                float const d = Dist2d(p.guard[s], p.guard[t]);
                if (minOther < 0.0f || d < minOther)
                    minOther = d;
            }
            out += Acore::StringFormat(
                " | slot {}: alive holderRole={} onHolder={} toStation={:.1f} minOther={:.1f} settledMs={}", s,
                view.assign.holderRole[s], p.onHolder[s] ? 1 : 0, toStation, minOther, view.settledMs[s]);
        }
        return out;
    }
}

bool BwlWyrmguardMarkSkullTrigger::IsActive()
{
    if (!IsWyrmguardMarker(bot))
        return false;

    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;

    int const next = BwlWyrmguardRules::NextSkull(view.state, view.assign, view.skull, view.settledMs);

    // Diagnostics: two or more guards up, skull not on one of them, and still nothing to mark.
    if (next == BwlWyrmguardRules::NONE && sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
    {
        int alive = 0;
        for (int s = 0; s < BwlWyrmguardRules::SLOTS; ++s)
            alive += view.state.alive[s] ? 1 : 0;
        bool const skullOnLiving = view.skull != BwlWyrmguardRules::NONE && view.state.alive[view.skull];
        uint32 const now = getMSTime();
        if (alive >= 2 && !skullOnLiving &&
            (!lastNoSkullLogMs || getMSTimeDiff(lastNoSkullLogMs, now) >= NO_SKULL_LOG_INTERVAL_MS))
        {
            lastNoSkullLogMs = now ? now : 1;
            LOG_DEBUG("playerbots", "[BWLWyrmguard] no skull: {} pack {}{}", bot->GetName(), view.packIndex + 1,
                      DescribeNoSkull(view));
        }
    }

    return next != BwlWyrmguardRules::NONE && next != view.skull;
}

bool BwlWyrmguardDpsTargetTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsHeal(bot))
        return false;

    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;

    return GetWyrmguardDpsTarget(bot, botAI, view) != AI_VALUE(Unit*, "current target");
}

bool BwlWyrmguardMeleePositionTrigger::IsActive()
{
    if (!IsWyrmguardMelee(bot))
        return false;

    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;

    return WyrmguardStompThreat(bot, view) != BwlWyrmguardRules::NONE;
}

// Released melee on skull get behind it (out of Cleave) via the stock "rear flank" action, which
// works on the bot's current target and only fires from the front arc (its own isUseful). Only
// when the spot behind skull is itself Stomp-clear, or rear flank and the away rule would trade
// the bot back and forth (review I4).
bool BwlWyrmguardMeleeBehindTrigger::IsActive()
{
    if (!IsWyrmguardMelee(bot))
        return false;

    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view) || view.skull == BwlWyrmguardRules::NONE)
        return false;
    if (BwlWyrmguardRules::MeleeHeld(view.state, view.assign, view.skull, view.msSinceActive))
        return false;

    Creature* const skull = view.guards[view.skull];
    if (!skull || AI_VALUE(Unit*, "current target") != skull)
        return false;

    BwlWyrmguardRules::Pt const rear = BwlWyrmguardRules::RearPoint(
        BwlWyrmguardRules::Pt{skull->GetPositionX(), skull->GetPositionY()}, skull->GetOrientation());
    return BwlWyrmguardRules::RearSpotClear(view.state, view.assign, view.skull, rear);
}

// Every bot, every role, in and out of combat (spec §4.9).
bool BwlWyrmguardEbonrocKeepOutTrigger::IsActive()
{
    return EbonrocKeepOutActive(bot) && EbonrocPathDistance(bot) < BwlWyrmguardRules::EBONROC_KEEPOUT_ENTER;
}

// Spec §4.8a healer positioning: a healer moves only when its spot is not good - inside
// HEALER_STOMP_CLEAR of a living guard, or out of HEALER_RANGE or line of sight of a holder tank.
bool BwlWyrmguardHealerPositionTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot))
        return false;

    WyrmguardPackView view;
    if (!GetActiveWyrmguardPack(bot, view))
        return false;

    BwlWyrmguardRules::Pt const here{bot->GetPositionX(), bot->GetPositionY()};
    if (!BwlWyrmguardRules::StompClear(here, view.state))
        return true;

    Player* tanks[BwlWyrmguardRules::SLOTS];
    int const n = GetWyrmguardHolderTanks(bot, view, tanks);
    BwlWyrmguardRules::Pt tankPt[BwlWyrmguardRules::SLOTS];
    for (int i = 0; i < n; ++i)
        tankPt[i] = {tanks[i]->GetPositionX(), tanks[i]->GetPositionY()};
    if (BwlWyrmguardRules::TanksReached(here, tankPt, n) < n)
        return true;

    for (int i = 0; i < n; ++i)
        if (!bot->IsWithinLOSInMap(tanks[i]))
            return true;
    return false;
}

// BWL-wide DPS threat discipline (overlay patch 0020, spec 2026-10-08)
//
// The hard stop, on the plain ratio (no headroom): it only exists to stop an auto-attack or auto-shot that
// is already running. Active only while the bot is attacking, so it fires once per breach rather than
// every tick while the bot already holds; the multiplier's vetoes keep the attack from restarting until
// the ratio falls back under BWL_THREAT_LIMIT.
bool BwlThreatHoldTrigger::IsActive()
{
    if (!IsBwlThreatDisciplined(bot))
        return false;
    if (!bot->GetVictim() && !bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
        return false;
    if (AI_VALUE(bool, "neglect threat"))
        return false;

    Unit* const current = AI_VALUE(Unit*, "current target");
    if (!IsBwlThreatDisciplineTarget(bot, current) || WyrmguardThreatRatio(bot, current) < BWL_THREAT_STOP)
        return false;
    return !BwlFightHasOwnThreatRules(bot);
}
