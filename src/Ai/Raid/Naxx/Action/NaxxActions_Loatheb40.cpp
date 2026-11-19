/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Loatheb consumables, tank cooldowns, healer jobs and spore kills (overlay patch 0022). Spec (overlay repo):
// docs/superpowers/specs/2026-10-30-playerbot-naxx40-loatheb-design.md §4.
//
// Return-value rules (memory: an action returning false hands the tick to the default actions):
//   - true while the action owns the tick: travelling (a leg issued, or MoveTo refused a duplicate of the leg still
//     running), stopping to cast, holding a bandage channel, and on the tick a cast or item use went off;
//   - false when it has nothing to do this tick, so the rotation runs. Naxx40LoathebMultiplier (NaxxMultipliers.cpp)
//     vetoes the stock positive spells of latched healers, so falling through never spends a healer's heal.
// Attack() is issued once per target, never per tick (memory: Attack cancels movement; it also sets "current target",
// so a target already set counts as accepted).

#include <algorithm>
#include <iterator>
#include <limits>
#include <list>
#include <string>
#include <vector>

#include "Creature.h"
#include "Group.h"
#include "Item.h"
#include "Log.h"
#include "Naxx40Loatheb.h"
#include "NaxxActions.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "Timer.h"
#include "WorldPacket.h"
#include "WorldSession.h"

using namespace Naxx40LoathebRules;

namespace
{
    constexpr uint32 USE_SETTLE_MS = 1500;     // no second use attempt within this (a refused use is not spammed)
    constexpr uint32 SPORE_RETRY_MS = 1000;    // a refused Attack(spore) is not retried on the same spore within this
    constexpr float HEALER_DPS_RANGE = 30.0f;   // Smite / Holy Fire / Wrath / Moonfire / Lightning Bolt / Exorcism
    // Spec §3: the Vanilla 2-min potion cooldown. In combat the core starts no potion cooldown, it only remembers the
    // last potion (Player::SetLastPotionId) and refuses every later one until combat ends (Spell::CheckCast).
    constexpr uint32 POTION_COOLDOWN_MS = 120000;
    // Heavy Runecloth Bandage needs First Aid 225; the `raid` cheat grants Artisan First Aid (step 4, 300/300).
    constexpr uint16 FIRST_AID_STEP = 4;
    constexpr uint16 FIRST_AID_GRANT = 300;

    // Spec §4.2: damage spells off IP's Corrupted Mind lockout lists (naxx40_spells.sql), first castable wins.
    // Paladins: Exorcism only (Loatheb is undead); Holy Shock is bound to the lockout and never used.
    char const* const PRIEST_DPS[] = {"holy fire", "smite", nullptr};
    char const* const DRUID_DPS[] = {"wrath", "moonfire", nullptr};
    char const* const SHAMAN_DPS[] = {"lightning bolt", nullptr};
    char const* const PALADIN_DPS[] = {"exorcism", nullptr};

    // Spec §4.3: ShieldWall / LastStand per tank class, first castable name wins.
    char const* const WARRIOR_WALL[] = {"shield wall", nullptr};
    char const* const WARRIOR_STAND[] = {"last stand", nullptr};
    char const* const PALADIN_WALL[] = {"divine protection", nullptr};
    char const* const PALADIN_STAND[] = {"lay on hands", nullptr};
    char const* const DRUID_WALL[] = {"barkskin", nullptr};
    char const* const DRUID_STAND[] = {"survival instincts", "frenzied regeneration", nullptr};
    char const* const DK_WALL[] = {"icebound fortitude", nullptr};
    char const* const DK_STAND[] = {"vampiric blood", nullptr};

    char const* const* DpsSpells(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_PRIEST:
                return PRIEST_DPS;
            case CLASS_DRUID:
                return DRUID_DPS;
            case CLASS_SHAMAN:
                return SHAMAN_DPS;
            case CLASS_PALADIN:
                return PALADIN_DPS;
            default:
                return nullptr;
        }
    }

    char const* const* TankCdSpells(uint8 cls, TankCd cd)
    {
        bool const wall = cd == TankCd::ShieldWall;
        switch (cls)
        {
            case CLASS_WARRIOR:
                return wall ? WARRIOR_WALL : WARRIOR_STAND;
            case CLASS_PALADIN:
                return wall ? PALADIN_WALL : PALADIN_STAND;
            case CLASS_DRUID:
                return wall ? DRUID_WALL : DRUID_STAND;
            case CLASS_DEATH_KNIGHT:
                return wall ? DK_WALL : DK_STAND;
            default:
                return nullptr;
        }
    }

    // The first name of the list the bot can cast on target now; null when none (or no list).
    char const* FirstCastable(PlayerbotAI* botAI, char const* const* names, Unit* target)
    {
        for (; names && *names; ++names)
            if (botAI->CanCastSpell(*names, target))
                return *names;
        return nullptr;
    }

    // The item's on-use spell: the first spell slot with an on-use trigger; 0 when none or the template is missing.
    // Not always slot 0: the Major Healthstones (9421 / 19012 / 19013) carry their use spell (11732 / 23476 / 23477) in
    // slot 1 with slot 0 empty, which made every healthstone read "not ready" in dev pull 1 (spec §4.7 item 7).
    uint32 UseSpellOf(uint32 itemId)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            return 0;
        for (auto const& slot : proto->Spells)
            if (slot.SpellId > 0 && slot.SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                return uint32(slot.SpellId);
        return 0;
    }

    bool CanSupply(Player* bot, PlayerbotAI* botAI) { return botAI->HasCheat(BotCheatMask::raid) && !IsSelfBot(bot); }

    // Under the `raid` cheat a bot below the item's First Aid rank is granted Artisan First Aid, so CanUseItem passes
    // and the bandage can be chosen at all (most bots never roll First Aid). Runs in the readiness path, before
    // ChooseConsumable; a no-op once the skill is there.
    void GrantFirstAid(Player* bot, ItemTemplate const* proto, bool canSupply)
    {
        if (!canSupply || proto->RequiredSkill != SKILL_FIRST_AID ||
            bot->GetSkillValue(SKILL_FIRST_AID) >= proto->RequiredSkillRank)
            return;
        uint16 const step = bot->GetSkillValue(SKILL_FIRST_AID) ? bot->GetSkillStep(SKILL_FIRST_AID) : FIRST_AID_STEP;
        bot->SetSkill(SKILL_FIRST_AID, std::max<uint16>(step, FIRST_AID_STEP), FIRST_AID_GRANT, FIRST_AID_GRANT);
        LOG_DEBUG("playerbots", "[Naxx40Loatheb] {} granted First Aid {}", bot->GetName(), FIRST_AID_GRANT);
    }

    // Off cooldown (the core's item / category cooldown on its use spell), usable by this bot (level, First Aid skill),
    // and either in the bags or suppliable. With the cheat on the inventory is never scanned here.
    bool ItemReady(Player* bot, uint32 itemId, bool canSupply)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        uint32 const spell = UseSpellOf(itemId);
        if (!proto || !spell)
            return false;
        GrantFirstAid(bot, proto, canSupply);
        if (bot->CanUseItem(proto) != EQUIP_ERR_OK || bot->HasSpellCooldown(spell))
            return false;
        return canSupply || bot->GetItemCount(itemId) > 0;
    }

    // The potion on top of ItemReady: the core's in-combat lock (GetLastPotionId) is clear, or under the `raid` cheat
    // this bot's last Shadow Protection Potion this attempt is POTION_COOLDOWN_MS old (the use then clears the lock).
    bool PotionReady(Player* bot, bool canSupply)
    {
        if (!ItemReady(bot, POTION_ITEM, canSupply))
            return false;
        return !bot->GetLastPotionId() || (canSupply && Naxx40Loatheb::MsSinceLastPotion(bot) >= POTION_COOLDOWN_MS);
    }

    // Uses the item now through the core's own use handler (the stock UseItemAction path: WorldSession::
    // HandleUseItemOpcode, which applies the real cooldowns). Synchronous, unlike PlayerbotAI::ImbueItem's queued
    // packet, so the caller can read whether the core accepted it. target null = no explicit target.
    void UseItemNow(Player* bot, Item* item, uint32 spellId, Unit* target)
    {
        uint8 const castCount = 1;
        uint32 const glyphIndex = 0;
        uint8 const castFlags = 0;
        WorldPacket packet(CMSG_USE_ITEM);
        packet << item->GetBagSlot() << item->GetSlot() << castCount << spellId << item->GetGUID() << glyphIndex
               << castFlags;
        if (target)
        {
            packet << uint32(TARGET_FLAG_UNIT);
            packet << target->GetGUID().WriteAsPacked();
        }
        else
            packet << uint32(TARGET_FLAG_NONE);
        bot->GetSession()->HandleUseItemOpcode(packet);
    }

    // The core accepted the use: an instant use spell is cast inside Spell::prepare, so its effect is already there;
    // the bandage is a channel that is running (or has applied Recently Bandaged).
    bool UseAccepted(Player* bot, Consumable c, uint32 spellId)
    {
        switch (c)
        {
            case Consumable::Potion:
                return bot->HasAura(SHADOW_ABSORB_SPELL);
            case Consumable::Healthstone:
                return bot->HasSpellCooldown(spellId);
            case Consumable::Bandage:
                return Naxx40LoathebAi::IsBandaging(bot) || bot->HasAura(RECENTLY_BANDAGED_SPELL);
            default:
                return false;
        }
    }

    // The healthstone to use: the best one in the bags that is ready, else the suppliable Major Healthstone; 0 = none.
    uint32 HealthstoneFor(Player* bot, bool canSupply)
    {
        for (size_t i = std::size(HEALTHSTONE_ITEMS); i-- > 0;)
            if (bot->GetItemCount(HEALTHSTONE_ITEMS[i]) > 0 && ItemReady(bot, HEALTHSTONE_ITEMS[i], false))
                return HEALTHSTONE_ITEMS[i];
        return canSupply && ItemReady(bot, HEALTHSTONE_ITEMS[0], true) ? HEALTHSTONE_ITEMS[0] : 0;
    }

    // The item to use, stored into the bags first under the `raid` cheat when missing.
    Item* SupplyItem(Player* bot, PlayerbotAI* botAI, uint32 itemId)
    {
        if (bot->GetItemCount(itemId) == 0 && CanSupply(bot, botAI))
        {
            bot->StoreNewItemInBestSlots(itemId, 1);
            LOG_DEBUG("playerbots", "[Naxx40Loatheb] {} supplied item {}", bot->GetName(), itemId);
        }
        Item* item = bot->GetItemByEntry(itemId);
        return item && bot->CanUseItem(item) == EQUIP_ERR_OK ? item : nullptr;
    }

    char const* ConsumableName(Consumable c)
    {
        switch (c)
        {
            case Consumable::Potion:
                return "shadow protection potion";
            case Consumable::Healthstone:
                return "healthstone";
            case Consumable::Bandage:
                return "bandage";
            case Consumable::StepOut:
                return "step out";
            default:
                return "none";
        }
    }

    // Spec §4.7 item 7 debug: why the suppliable Major Healthstone (or none in the bags) is not ready.
    std::string StoneBlocker(Player* bot, bool canSupply)
    {
        uint32 const itemId = HEALTHSTONE_ITEMS[0];
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            return Acore::StringFormat("no item template {}", itemId);
        uint32 const spell = UseSpellOf(itemId);
        if (!spell)
            return Acore::StringFormat("item {} has no on-use spell", itemId);
        InventoryResult const res = bot->CanUseItem(proto);
        if (res != EQUIP_ERR_OK)
            return Acore::StringFormat("CanUseItem refused ({})", uint32(res));
        if (bot->HasSpellCooldown(spell))
            return Acore::StringFormat("use spell {} on cooldown", spell);
        if (!canSupply)
            return "none in the bags and no raid cheat";
        return "unknown";
    }

    // Spec §4.7 item 7 debug: at most once per bot per Doom window (Naxx40Loatheb::ClaimStoneNote), a bot in the
    // after-Doom window below HEALTHSTONE_BELOW_PCT that is not using a healthstone says why.
    void NoteStoneNotReady(Player* bot, ConsumableInput const& in, Consumable chosen, bool canSupply)
    {
        if (!sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG) || !Naxx40Loatheb::ClaimStoneNote(bot))
            return;
        std::string reason;
        if (in.isHealer && in.msSinceOwnHeal > JUST_HEALED_MS)
            reason = "healer keeps its heal (not just healed)";
        else if (chosen == Consumable::Potion)
            reason = "shadow protection potion first";
        else if (Naxx40Loatheb::HealthstoneClaimed(bot))
            reason = "already used this attempt";
        else if (!in.healthstoneReady)
            reason = StoneBlocker(bot, canSupply);
        else
            reason = "not chosen";
        LOG_DEBUG("playerbots", "[Naxx40Loatheb] {} healthstone not ready: {} ({:.0f}% hp, chose {})", bot->GetName(),
                  reason, in.healthPct, ConsumableName(chosen));
    }

    bool HealerClassOf(uint8 cls, HealerClass& out)
    {
        switch (cls)
        {
            case CLASS_PALADIN:
                out = HealerClass::Paladin;
                return true;
            case CLASS_DRUID:
                out = HealerClass::Druid;
                return true;
            case CLASS_PRIEST:
                out = HealerClass::Priest;
                return true;
            case CLASS_SHAMAN:
                out = HealerClass::Shaman;
                return true;
            default:
                return false;
        }
    }

    // A heal the bot may start. Standing: PlayerbotAI::CanCastSpell in full. Moving: CanCastSpell refuses EVERY
    // cast-time spell while the bot moves ("Casting time and bot is moving"), so a heal trigger gated on it goes
    // quiet whenever upstream's position action walks the healer, and the action's stop-then-cast never runs. While
    // moving the gate is only "knows the spell and has the mana for it": the action stops the bot and re-checks in
    // full on the next tick. Never dropped entirely, or a bot that can't cast would oscillate between stopping and
    // the position action.
    bool HealCastable(Player* bot, PlayerbotAI* botAI, char const* name, Unit* target)
    {
        if (!name || !target)
            return false;
        if (!bot->isMoving())
            return botAI->CanCastSpell(name, target);
        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
        SpellInfo const* const info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info || !bot->HasSpell(spellId))
            return false;
        int32 const cost = info->PowerType == POWER_MANA ? info->CalcPowerCost(bot, info->GetSchoolMask()) : 0;
        return int32(bot->GetPower(POWER_MANA)) >= cost;
    }

    // A raid heal's candidate: injured (below BANDAGE_BELOW_PCT) and within HEAL_REACH (2-D).
    struct HealCandidate
    {
        float pct;
        Player* player;
    };
}

namespace Naxx40LoathebAi
{
    bool IsBandaging(Player* bot)
    {
        Spell const* channel = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        return channel && channel->GetSpellInfo() && channel->GetSpellInfo()->Id == UseSpellOf(BANDAGE_ITEM);
    }

    Consumable ConsumableFor(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view,
                             bool healthstoneClaimed)
    {
        if (!bot->IsAlive() || !view.loatheb)
            return Consumable::None;
        // Cheap gates first: every choice needs Doom on the bot (potion) or the after-Doom window and some damage.
        bool const hasDoom = bot->HasAura(DOOM_SPELL);
        bool const afterDoom = view.msSinceDoomResolved <= AFTER_DOOM_WINDOW_MS;
        float const hp = bot->GetHealthPct();
        if (!hasDoom && (!afterDoom || hp >= BANDAGE_BELOW_PCT))
            return Consumable::None;

        bool const canSupply = CanSupply(bot, botAI);
        ConsumableInput in{};
        in.hasDoom = hasDoom;
        in.potionReady = hasDoom && PotionReady(bot, canSupply);
        in.hasShadowAbsorb = bot->HasAura(SHADOW_ABSORB_SPELL);
        in.msSinceDoomResolved = view.msSinceDoomResolved;
        in.healthPct = hp;
        in.healthstoneClaimed = healthstoneClaimed;
        in.healthstoneReady = afterDoom && hp < HEALTHSTONE_BELOW_PCT && HealthstoneFor(bot, canSupply) != 0;
        in.recentlyBandaged = bot->HasAura(RECENTLY_BANDAGED_SPELL);
        in.bandageReady = afterDoom && !in.recentlyBandaged && ItemReady(bot, BANDAGE_ITEM, canSupply);
        in.isBossVictim = view.loatheb->GetVictim() == bot;
        in.isTank = PlayerbotAI::IsTank(bot);
        Naxx40Loatheb::HealerJob const job = Naxx40Loatheb::MyHealerJob(bot, view);
        in.isHealer = job != Naxx40Loatheb::HealerJob::None;
        in.isRotationHealer = job == Naxx40Loatheb::HealerJob::Rotation;
        in.msSinceOwnHeal = MsSinceOwnHeal(Naxx40Loatheb::LockoutRemainingMs(bot));
        // Surface-to-surface, as the Poison Shock radius is checked against both units' sizes.
        in.distToBoss = bot->GetDistance2d(view.loatheb);
        Consumable const chosen = ChooseConsumable(in);
        if (!healthstoneClaimed && afterDoom && hp < HEALTHSTONE_BELOW_PCT && chosen != Consumable::Healthstone)
            NoteStoneNotReady(bot, in, chosen, canSupply);
        return chosen;
    }

    TankCd TankCdFor(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view, char const*& spell)
    {
        spell = nullptr;
        // A non-tank victim (threat neglected, the tank died) is healed as the main tank but gets no tank cooldowns.
        if (!bot->IsAlive() || view.mainTank != bot || !view.mainTankIsTank)
            return TankCd::None;
        float const hp = bot->GetHealthPct();
        if (hp >= SHIELD_WALL_PCT)
            return TankCd::None;  // cheap gate: both cooldowns wait below SHIELD_WALL_PCT
        char const* const wall = FirstCastable(botAI, TankCdSpells(bot->getClass(), TankCd::ShieldWall), bot);
        char const* const stand = FirstCastable(botAI, TankCdSpells(bot->getClass(), TankCd::LastStand), bot);
        TankCd const cd = ChooseTankCd(hp, wall != nullptr, stand != nullptr);
        spell = cd == TankCd::ShieldWall ? wall : cd == TankCd::LastStand ? stand : nullptr;
        return cd;
    }

    bool TankHealWanted(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view, bool& rotation)
    {
        rotation = false;
        Player* const tank = view.mainTank;
        if (!bot->IsAlive() || !tank || tank == bot || !tank->IsAlive() || !Naxx40Loatheb::BigHealSpellName(bot))
            return false;
        Naxx40Loatheb::HealerJob const job = Naxx40Loatheb::MyHealerJob(bot, view);
        if (job == Naxx40Loatheb::HealerJob::None)
            return false;
        uint32 due = UINT32_MAX;
        if (job == Naxx40Loatheb::HealerJob::Rotation && Naxx40Loatheb::RotationMayCast(bot, view, due))
        {
            rotation = true;
            return true;
        }
        // Emergency: any latched healer with its heal free, already in reach and sight (it never runs for one).
        if (tank->GetHealthPct() >= EMERGENCY_TANK_PCT || Naxx40Loatheb::LockoutRemainingMs(bot) >= 0)
            return false;
        if (bot->GetExactDist2d(tank) > HEAL_REACH || !bot->IsWithinLOSInMap(tank))
            return false;
        return HealCastable(bot, botAI, Naxx40Loatheb::BigHealSpellName(bot), tank);
    }

    RaidHeal RaidHealFor(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view, bool partyTaken,
                         bool topUpTaken, Player*& target, char const*& spell, bool& topUp)
    {
        target = nullptr;
        spell = nullptr;
        topUp = false;
        // Cheap gates first; the raid scan runs only for a raid healer with a free heal inside the after-Doom window.
        if (!bot->IsAlive() || view.msSinceDoomResolved > AFTER_DOOM_WINDOW_MS)
            return RaidHeal::None;
        HealerClass cls;
        if (!HealerClassOf(bot->getClass(), cls) ||
            Naxx40Loatheb::MyHealerJob(bot, view) != Naxx40Loatheb::HealerJob::Group ||
            Naxx40Loatheb::LockoutRemainingMs(bot) >= 0)
            return RaidHeal::None;
        Group* const group = bot->GetGroup();
        if (!group)
            return RaidHeal::None;

        uint8 injuredInParty = 0;
        Player* partyLow = nullptr;
        std::vector<HealCandidate> raid;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* p = ref->GetSource();
            if (!p || !p->IsInWorld() || p->GetMap() != bot->GetMap() || !p->IsAlive() ||
                p->GetHealthPct() >= BANDAGE_BELOW_PCT || bot->GetExactDist2d(p) > HEAL_REACH)
                continue;
            // Prayer of Healing reaches party members within PRAYER_RADIUS of the caster (3-D, as the core's search).
            if (group->SameSubGroup(bot, p) && bot->GetExactDist(p) <= PRAYER_RADIUS && bot->IsWithinLOSInMap(p))
            {
                if (injuredInParty < std::numeric_limits<uint8>::max())
                    ++injuredInParty;
                if (!partyLow || p->GetHealthPct() < partyLow->GetHealthPct())
                    partyLow = p;
            }
            // The rotation covers the main tank.
            if (p != view.mainTank)
                raid.push_back(HealCandidate{p->GetHealthPct(), p});
        }

        // The most injured raid member in reach and sight; a single-target heal skips a bot channelling a bandage.
        std::sort(raid.begin(), raid.end(),
                  [](HealCandidate const& a, HealCandidate const& b) { return a.pct < b.pct; });
        bool const singleTarget = cls != HealerClass::Shaman;
        Player* raidTarget = nullptr;
        for (HealCandidate const& c : raid)
        {
            if (singleTarget && IsBandaging(c.player))
                continue;
            if (c.player == bot || bot->IsWithinLOSInMap(c.player))
            {
                raidTarget = c.player;
                break;
            }
        }

        // Spec §4.9 item 1: the window's first raid heal tops up a main tank below MT_TOPUP_BELOW_PCT in reach and
        // sight (any player Loatheb is attacking, the raid healer itself included). The claim is read here, taken by
        // the action right before the cast. A top-up spell that is not castable falls back to the raid choice.
        Player* const tank = view.mainTank;
        if (tank && tank->IsAlive() && tank->IsInWorld() && tank->GetMap() == bot->GetMap() &&
            tank->GetHealthPct() < MT_TOPUP_BELOW_PCT)
        {
            bool const inReach =
                tank == bot || (bot->GetExactDist2d(tank) <= HEAL_REACH && bot->IsWithinLOSInMap(tank));
            bool const claimedTopUp = topUpTaken || Naxx40Loatheb::TankTopUpClaimed(bot);
            if (MainTankTopUp(view.msSinceDoomResolved, tank->GetHealthPct(), inReach, claimedTopUp))
            {
                RaidHeal const kind = ChooseRaidHeal(cls, view.msSinceDoomResolved, injuredInParty, false,
                                                     raidTarget != nullptr, true);
                char const* const s = kind == RaidHeal::Chain ? "chain heal" : Naxx40Loatheb::BigHealSpellName(bot);
                if (kind != RaidHeal::None && s && HealCastable(bot, botAI, s, tank))
                {
                    spell = s;
                    target = tank;
                    topUp = true;
                    return kind;
                }
            }
        }

        uint8 const subgroup = group->GetMemberGroup(bot->GetGUID());
        bool claimed = partyTaken;
        if (!claimed && cls == HealerClass::Priest && injuredInParty >= 2)
            claimed = Naxx40Loatheb::PartyHealClaimed(bot, subgroup);
        RaidHeal kind = ChooseRaidHeal(cls, view.msSinceDoomResolved, injuredInParty, claimed, raidTarget != nullptr);
        if (kind == RaidHeal::Prayer && !HealCastable(bot, botAI, "prayer of healing", partyLow))
            kind = ChooseRaidHeal(cls, view.msSinceDoomResolved, injuredInParty, true, raidTarget != nullptr);
        switch (kind)
        {
            case RaidHeal::Prayer:
                spell = "prayer of healing";
                target = partyLow;
                return kind;
            case RaidHeal::Chain:
                spell = "chain heal";
                break;
            case RaidHeal::Big:
                spell = Naxx40Loatheb::BigHealSpellName(bot);
                break;
            default:
                return RaidHeal::None;
        }
        if (!spell || !HealCastable(bot, botAI, spell, raidTarget))
        {
            spell = nullptr;
            return RaidHeal::None;
        }
        target = raidTarget;
        return kind;
    }

    bool KillsSpores(Player* bot, Naxx40Loatheb::LoathebView const& view)
    {
        // Spec §4.8 item 3: tanks and healers (latched or not) never attack spores.
        return bot->IsAlive() && !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsHeal(bot) &&
               Naxx40Loatheb::MyHealerJob(bot, view) == Naxx40Loatheb::HealerJob::None;
    }

    Unit* CurrentSpore(Player* bot, PlayerbotAI* botAI)
    {
        Unit* const victim = bot->GetVictim();
        if (victim && victim->GetEntry() == SPORE_ENTRY)
            return victim;
        Unit* const current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        return current && current->GetEntry() == SPORE_ENTRY ? current : nullptr;
    }

    Creature* SporeToKill(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view)
    {
        return KillsSpores(bot, view) ? Naxx40Loatheb::EngageSpore(bot, view, CurrentSpore(bot, botAI)) : nullptr;
    }

    bool SporeWithin(Player* bot, float dist)
    {
        std::list<Creature*> found;
        // The grid check is surface-to-surface in 3D; the margin keeps a spore within dist in 2D (upstream's test).
        bot->GetCreatureListWithEntryInGrid(found, SPORE_ENTRY, dist + 5.0f);
        for (Creature* c : found)
            if (c && c->IsInWorld() && c->IsAlive() && bot->GetDistance2d(c) <= dist)
                return true;
        return false;
    }

    char const* HealerDpsSpell(Player* bot, PlayerbotAI* botAI, Naxx40Loatheb::LoathebView const& view)
    {
        Creature* const loatheb = view.loatheb;
        if (!bot->IsAlive() || !loatheb || !DpsSpells(bot->getClass()))
            return nullptr;
        Naxx40Loatheb::HealerJob const job = Naxx40Loatheb::MyHealerJob(bot, view);
        if (job == Naxx40Loatheb::HealerJob::None || Naxx40Loatheb::LockoutRemainingMs(bot) < 0)
            return nullptr;
        // Spec §4.7 item 5: raid healers keep their mana for healing once Doom is on its 15 s cycle.
        if (job == Naxx40Loatheb::HealerJob::Group && !RaidHealerMayDps(view.doomCount))
            return nullptr;
        uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
        if (!maxMana)
            return nullptr;
        float const manaPct = 100.0f * float(bot->GetPower(POWER_MANA)) / float(maxMana);
        uint32 due = view.msToDoomResolve;
        if (job == Naxx40Loatheb::HealerJob::Rotation)
            Naxx40Loatheb::RotationMayCast(bot, view, due);
        if (!HealerMayDps(manaPct, due))
            return nullptr;
        if (bot->GetDistance(loatheb) > HEALER_DPS_RANGE || !bot->IsWithinLOSInMap(loatheb))
            return nullptr;
        // CanCastSpell refuses every cast-time spell while the bot moves: while moving only instant spells are tried.
        // The AI pauses for the whole cast, so a spell starts only if it ends STOP_BEFORE_MS before this healer's heal
        // is due (HealerMayDps with the spell's cast time). CanCastSpell also refuses a spell on cooldown (spec §4.8
        // item 4: a paladin's Exorcism on cooldown no longer fires the trigger every tick).
        bool const moving = bot->isMoving();
        for (char const* const* names = DpsSpells(bot->getClass()); *names; ++names)
        {
            uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", *names)->Get();
            SpellInfo const* const info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
            if (!info)
                continue;
            uint32 const castMs = info->CalcCastTime(bot);
            if ((moving && castMs > 0) || !HealerMayDps(manaPct, due, castMs))
                continue;
            if (botAI->CanCastSpell(*names, loatheb))
                return *names;
        }
        return nullptr;
    }

    bool NeedsManaRefill(Player* bot, PlayerbotAI* botAI)
    {
        if (!bot->IsAlive() || !CanSupply(bot, botAI))
            return false;
        uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
        return maxMana && 100.0f * float(bot->GetPower(POWER_MANA)) < MANA_REFILL_BELOW_PCT * float(maxMana);
    }

    // Spec §4.2 (the Horsemen / Nefarian pattern, NaxxActions_Horsemen40.cpp): ONE full refill per healer per attempt;
    // a continuous regen was tried on Nefarian and gave effectively infinite mana.
    void RefillMana(Player* bot, PlayerbotAI* botAI)
    {
        if (!NeedsManaRefill(bot, botAI) || !Naxx40Loatheb::ClaimManaRefill(bot))
            return;
        bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));
        LOG_DEBUG("playerbots", "[Naxx40Loatheb] mana refill {}", bot->GetName());
    }
}

// Spec §4.1. A healthstone claim is per attempt (Naxx40Loatheb::ClaimHealthstone): the trigger chooses as if unclaimed,
// so on a refused claim the choice is re-made with the stone claimed (a bandage may still be due).
bool Naxx40LoathebConsumableAction::Execute(Event /*event*/)
{
    Naxx40Loatheb::LoathebView view;
    if (!Naxx40Loatheb::GetView(bot, view))
        return false;
    // Damage or movement breaks the bandage channel: own the tick until it ends.
    if (Naxx40LoathebAi::IsBandaging(bot))
    {
        if (bot->isMoving())
            bot->StopMoving();
        return true;
    }

    Consumable c = Naxx40LoathebAi::ConsumableFor(bot, botAI, view, false);
    if (c == Consumable::None)
        return false;
    if (c == Consumable::StepOut)
        return StepOut(view.loatheb);

    uint32 const now = getMSTime();
    if (lastUseMs && getMSTimeDiff(lastUseMs, now) < USE_SETTLE_MS)
        return false;
    if (bot->IsNonMeleeSpellCast(false))
        return false;  // finishing a cast; the use waits for the next tick
    // Claimed only once the stone is about to be used (a claim spent on a tick that bailed would lose it).
    if (c == Consumable::Healthstone && !Naxx40Loatheb::ClaimHealthstone(bot))
    {
        c = Naxx40LoathebAi::ConsumableFor(bot, botAI, view, true);
        if (c == Consumable::None)
            return false;
        if (c == Consumable::StepOut)
            return StepOut(view.loatheb);
    }

    uint32 const itemId = c == Consumable::Potion  ? POTION_ITEM
                          : c == Consumable::Bandage ? BANDAGE_ITEM
                                                     : HealthstoneFor(bot, CanSupply(bot, botAI));
    Item* const item = itemId ? SupplyItem(bot, botAI, itemId) : nullptr;
    if (!item)
        return false;
    // A channel: stand still first (a moving bandage is interrupted at once).
    if (c == Consumable::Bandage && bot->isMoving())
    {
        bot->StopMoving();
        return true;
    }
    // PotionReady let a 2-min-old potion through the core's in-combat lock: clear it right before the use.
    if (c == Consumable::Potion && bot->GetLastPotionId())
        bot->SetLastPotionId(0);
    uint32 const spellId = UseSpellOf(item->GetEntry());
    // The bandage gets an explicit self target: without one the core falls back to the bot's selection when that is
    // a valid ally (Spell::InitExplicitTargets), which would bandage the healed tank.
    UseItemNow(bot, item, spellId, c == Consumable::Bandage ? bot : nullptr);
    lastUseMs = now ? now : 1;
    bool const accepted = UseAccepted(bot, c, spellId);
    if (accepted)
        Naxx40Loatheb::NoteConsumable(bot, c);
    LOG_DEBUG("playerbots", "[Naxx40Loatheb] {} uses {} ({:.0f}% hp, doom {}): {}", bot->GetName(),
              ConsumableName(c), bot->GetHealthPct(), bot->HasAura(DOOM_SPELL) ? "on" : "off",
              accepted ? "accepted" : "refused");
    return true;
}

// Spec §4.7 item 2: leave Poison Shock before bandaging, to BANDAGE_CLEAR + 1 y (surface to surface) from Loatheb along
// the line from him to upstream's ranged spot, or that spot itself when he is closer to it (StepOutPoint). Never along
// the bot's own bearing: in dev pull 1 that sent off-tanks past the tank spot toward the wall and around the room. The
// point depends on Loatheb's position only, never the bot's, so it stays put while the bot walks it (memory: escape
// spots derived from the live position creep, and MoveTo refuses each re-issued order).
bool Naxx40LoathebConsumableAction::StepOut(Creature* loatheb)
{
    if (!botAI->CanMove())
        return false;
    float const reach = BANDAGE_CLEAR + 1.0f + loatheb->GetObjectSize() + bot->GetObjectSize();
    Point2 const spot = StepOutPoint(loatheb->GetPositionX(), loatheb->GetPositionY(), reach);
    float const x = spot.x;
    float const y = spot.y;
    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);
    bool const moved =
        MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    return moved || bot->isMoving();
}

bool Naxx40LoathebTankCooldownAction::Execute(Event /*event*/)
{
    Naxx40Loatheb::LoathebView view;
    if (!Naxx40Loatheb::GetView(bot, view))
        return false;
    char const* spell = nullptr;
    TankCd const cd = Naxx40LoathebAi::TankCdFor(bot, botAI, view, spell);
    if (cd == TankCd::None || !spell || !botAI->CastSpell(spell, bot))
        return false;
    LOG_DEBUG("playerbots", "[Naxx40Loatheb] tank {} uses {} at {:.0f}%", bot->GetName(), spell, bot->GetHealthPct());
    return true;
}

// Re-decided at execution (the trigger's pick may be a tick old). A rotation slot that is up walks into reach and sight
// of the main tank (its slot would otherwise stall the rotation); the emergency heal only fires from where it stands.
bool Naxx40LoathebTankHealAction::Execute(Event /*event*/)
{
    Naxx40Loatheb::LoathebView view;
    if (!Naxx40Loatheb::GetView(bot, view))
        return false;
    Naxx40LoathebAi::RefillMana(bot, botAI);
    bool rotation = false;
    if (!Naxx40LoathebAi::TankHealWanted(bot, botAI, view, rotation))
        return false;
    Player* const tank = view.mainTank;
    char const* const spell = Naxx40Loatheb::BigHealSpellName(bot);

    if (bot->GetExactDist2d(tank) > HEAL_REACH || !bot->IsWithinLOSInMap(tank))
    {
        if (!botAI->CanMove())
            return false;
        bool const moved = MoveTo(bot->GetMapId(), tank->GetPositionX(), tank->GetPositionY(), tank->GetPositionZ(),
                                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
        return moved || bot->isMoving();
    }
    // Cast-time heals never start while moving (PlayerbotAI::CanCastSpell): stop now, cast on the next tick.
    if (bot->isMoving())
    {
        bot->StopMoving();
        return true;
    }

    // The emergency heal is single-flight per instance (one per ROTATION_MIN_SPACING_MS): claimed here, right before
    // the cast, so a tick that bailed above never spends it.
    if (!rotation && !Naxx40Loatheb::ClaimEmergencyHeal(bot))
        return false;
    if (!botAI->CastSpell(spell, tank))
        return false;
    Naxx40Loatheb::NoteTankHealCast(bot, !rotation);
    Naxx40Loatheb::NoteRotationHeal(bot);  // ignored for a healer outside the rotation
    LOG_DEBUG("playerbots", "[Naxx40Loatheb] {} {} heal {} on {} ({:.0f}%)", bot->GetName(),
              rotation ? "rotation" : "emergency", spell, tank->GetName(), tank->GetHealthPct());
    return true;
}

// Spec §4.7 item 4 / §4.9 item 1. The main-tank top-up is claimed per Doom, and Prayer of Healing per (Doom,
// subgroup), right before the cast; a healer that loses either claim re-decides with it taken (a priest that loses its
// party falls back to its big heal on the most injured raid member). A refused cast gives the top-up claim back.
bool Naxx40LoathebGroupHealAction::Execute(Event /*event*/)
{
    Naxx40Loatheb::LoathebView view;
    if (!Naxx40Loatheb::GetView(bot, view))
        return false;
    Naxx40LoathebAi::RefillMana(bot, botAI);
    Player* target = nullptr;
    char const* spell = nullptr;
    bool topUp = false;
    RaidHeal kind = Naxx40LoathebAi::RaidHealFor(bot, botAI, view, false, false, target, spell, topUp);
    if (kind == RaidHeal::None || !target || !spell)
        return false;
    // Cast-time heals never start while moving (PlayerbotAI::CanCastSpell): stop now, cast on the next tick.
    if (bot->isMoving())
    {
        bot->StopMoving();
        return true;
    }
    if (topUp && !Naxx40Loatheb::ClaimTankTopUp(bot))
    {
        kind = Naxx40LoathebAi::RaidHealFor(bot, botAI, view, false, true, target, spell, topUp);
        if (kind == RaidHeal::None || !target || !spell)
            return false;
    }
    if (kind == RaidHeal::Prayer)
    {
        Group* const group = bot->GetGroup();
        if (!group || !Naxx40Loatheb::ClaimPartyHeal(bot, group->GetMemberGroup(bot->GetGUID())))
        {
            kind = Naxx40LoathebAi::RaidHealFor(bot, botAI, view, true, true, target, spell, topUp);
            if (kind == RaidHeal::None || !target || !spell)
                return false;
        }
    }
    if (!botAI->CastSpell(spell, target))
    {
        if (topUp)
            Naxx40Loatheb::ReleaseTankTopUp(bot);
        return false;
    }
    Naxx40Loatheb::NoteRaidHealCast(bot, kind, topUp);
    LOG_DEBUG("playerbots", "[Naxx40Loatheb] {} raid heal {} on {} ({:.0f}%, doom {}){}", bot->GetName(), spell,
              target->GetName(), target->GetHealthPct(), uint32(view.doomCount), topUp ? " (tank top-up)" : "");
    return true;
}

// Spec §4.9 item 3: spores walk to the raid, so nobody runs to one. A DPS bot attacks the spore SporeToKill names (its
// current spore while that stays within FUNGAL_RADIUS or the stack zone, else the nearest one within SPORE_STACK_RADIUS
// of the ranged spot, or a SPORE_STRAY_MS-old one within SPORE_ENGAGE; clear of Loatheb's victim either way) with ONE
// Attack() (memory: Attack cancels movement): melee close in
// through stock reach melee, casters cast from where they stand. While it attacks that spore the action has nothing to
// do and returns false, so the default combat actions fight it; Naxx40LoathebMultiplier vetoes upstream's "loatheb
// choose target" meanwhile, so it is not pulled back to Loatheb. Once the spore is dead or out of the rule, a bot still
// on it (victim or "current target") returns to Loatheb once (or, if that is refused, stops attacking), so neither is
// the spore on the next tick.
bool Naxx40LoathebSporeAction::Execute(Event /*event*/)
{
    Naxx40Loatheb::LoathebView view;
    if (!Naxx40Loatheb::GetView(bot, view))
        return false;
    if (!Naxx40LoathebAi::KillsSpores(bot, view))
        return false;
    Creature* const spore = Naxx40LoathebAi::SporeToKill(bot, botAI, view);
    if (spore)
    {
        if (bot->GetVictim() == spore || AI_VALUE(Unit*, "current target") == spore)
            return false;
        // A refused Attack is not re-issued on every tick.
        uint32 const now = getMSTime();
        if (lastSporeGuid == spore->GetGUID() && getMSTimeDiff(lastSporeMs, now) < SPORE_RETRY_MS)
            return false;
        lastSporeGuid = spore->GetGUID();
        lastSporeMs = now;
        bool const attacked = Attack(spore);
        LOG_DEBUG("playerbots", "[Naxx40Loatheb] {} attacks spore at {:.1f} y: {}", bot->GetName(),
                  bot->GetExactDist2d(spore), attacked ? "ok" : "refused");
        return attacked;
    }

    // SporeToKill keeps the spore the bot is on while it qualifies, so a spore still on it here is dead or out of the
    // rule.
    Unit* const stale = Naxx40LoathebAi::CurrentSpore(bot, botAI);
    if (!stale)
        return false;
    if (!Attack(view.loatheb))
    {
        context->GetValue<Unit*>("current target")->Set(nullptr);
        bot->AttackStop();
    }
    LOG_DEBUG("playerbots", "[Naxx40Loatheb] {} leaves spore ({})", bot->GetName(),
              stale->IsAlive() ? "out of the rule" : "dead");
    return true;
}

bool Naxx40LoathebHealerDpsAction::Execute(Event /*event*/)
{
    Naxx40Loatheb::LoathebView view;
    if (!Naxx40Loatheb::GetView(bot, view))
        return false;
    // The trigger also fires for a pending refill, which HealerMayDps (mana > 30 %) alone would never reach.
    Naxx40LoathebAi::RefillMana(bot, botAI);
    char const* const spell = Naxx40LoathebAi::HealerDpsSpell(bot, botAI, view);
    return spell && botAI->CastSpell(spell, view.loatheb);
}
