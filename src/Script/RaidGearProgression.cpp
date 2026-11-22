/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Raid gear progression (WHE 11-22): when a raid boss is killed, every bot in a group with a real player gets a small
// upgrade in its weakest slots, never above that raid's own loot level. Clearing Onyxia/MC/BWL with fresh ilvl-66 bots
// walks them up through the tiers towards Naxx even when only a few real items drop. Bot-only groups (DungeonClear
// test runs, random bot raids) get nothing, so test results stay comparable.
//
// Items come from PlayerbotFactory::UpgradeEquipmentSlot (the level-80 endgame progression's picker: legitimate
// blue/epic items, spec-scored, only if better than the incumbent). The replaced item is destroyed rather than left
// in the bags, so bags never fill up and stop the upgrades.

#include "ChatHelper.h"
#include "Config.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotFactory.h"
#include "Random.h"
#include "Playerbots.h"
#include "ScriptMgr.h"

#include <algorithm>
#include <vector>

namespace
{
    struct RaidCap
    {
        uint32 map;
        uint32 minLevel, maxLevel;  // bot level bracket the raid is for (Onyxia 249 and Naxx 533 exist twice)
        uint32 maxItemLevel;        // the raid's own loot level: upgrades never go past it
    };

    constexpr RaidCap RAID_CAPS[] = {
        // Vanilla (level 60, Individual Progression)
        {309, 1, 64, 66},   // Zul'Gurub
        {409, 1, 64, 66},   // Molten Core
        {249, 1, 64, 70},   // Onyxia's Lair
        {509, 1, 64, 70},   // Ruins of Ahn'Qiraj
        {469, 1, 64, 76},   // Blackwing Lair
        {531, 1, 64, 81},   // Temple of Ahn'Qiraj
        {533, 1, 64, 88},   // Naxxramas 40
        // TBC (level 70)
        {532, 65, 74, 115},  // Karazhan
        {565, 65, 74, 125},  // Gruul's Lair
        {544, 65, 74, 125},  // Magtheridon's Lair
        {568, 65, 74, 128},  // Zul'Aman
        {548, 65, 74, 128},  // Serpentshrine Cavern
        {550, 65, 74, 134},  // Tempest Keep
        {534, 65, 74, 146},  // Hyjal Summit
        {564, 65, 74, 151},  // Black Temple
        {580, 65, 74, 164},  // Sunwell Plateau
        // WotLK (level 80)
        {533, 75, 80, 200},  // Naxxramas
        {615, 75, 80, 200},  // Obsidian Sanctum
        {624, 75, 80, 213},  // Vault of Archavon
        {616, 75, 80, 213},  // Eye of Eternity
        {603, 75, 80, 226},  // Ulduar
        {249, 75, 80, 232},  // Onyxia's Lair (80)
        {649, 75, 80, 245},  // Trial of the Crusader
        {631, 75, 80, 264},  // Icecrown Citadel
        {724, 75, 80, 271},  // Ruby Sanctum
    };

    uint32 CapFor(uint32 map, uint32 level)
    {
        for (RaidCap const& c : RAID_CAPS)
            if (c.map == map && level >= c.minLevel && level <= c.maxLevel)
                return c.maxItemLevel;
        return 0;
    }

    bool GroupHasRealPlayerOnMap(Player* bot)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* m = ref->GetSource())
                if (m->GetMapId() == bot->GetMapId() && m->GetInstanceId() == bot->GetInstanceId() && IsRealPlayer(m))
                    return true;
        return false;
    }

    void UpgradeBot(Player* bot, uint32 cap, uint32 step, uint32 slotsPerKill, uint32 bossId)
    {
        struct SlotLevel { uint8 slot; uint32 ilvl; };
        std::vector<SlotLevel> slots;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (slot == EQUIPMENT_SLOT_BODY || slot == EQUIPMENT_SLOT_TABARD)
                continue;
            Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item || !item->GetTemplate() || item->GetTemplate()->ItemLevel >= cap)
                continue;
            slots.push_back({slot, item->GetTemplate()->ItemLevel});
        }
        // Weakest first; a slot with nothing in the window (or no better item) passes to the next one.
        std::stable_sort(slots.begin(), slots.end(), [](SlotLevel const& a, SlotLevel const& b) { return a.ilvl < b.ilvl; });

        PlayerbotFactory factory(bot, bot->GetLevel());
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        uint32 done = 0;
        for (SlotLevel const& s : slots)
        {
            if (done >= slotsPerKill)
                break;
            Item* old = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, s.slot);
            if (!old)
                continue;
            ObjectGuid const oldGuid = old->GetGUID();
            uint32 const oldEntry = old->GetEntry();
            if (!factory.UpgradeEquipmentSlot(s.slot, s.ilvl + 1, std::min(cap, s.ilvl + step), true))
                continue;
            ++done;
            // UpgradeEquipmentSlot moved the incumbent to the bags: drop it.
            if (Item* stored = bot->GetItemByGuid(oldGuid))
                bot->DestroyItem(stored->GetBagSlot(), stored->GetSlot(), true);
            Item* now = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, s.slot);
            ItemTemplate const* proto = now ? now->GetTemplate() : nullptr;
            LOG_INFO("playerbots", "[RaidGear] {} (lvl {}) boss {} map {}: slot {} {} (ilvl {}) -> {} (ilvl {}), cap {}",
                     bot->GetName(), bot->GetLevel(), bossId, bot->GetMapId(), s.slot, oldEntry, s.ilvl,
                     proto ? proto->ItemId : 0, proto ? proto->ItemLevel : 0, cap);
            if (botAI && proto)
                botAI->TellMaster("Raid upgrade: " + ChatHelper::FormatItem(proto));
        }
    }
}

class PlayerbotsRaidGearProgressionScript : public GlobalScript
{
public:
    PlayerbotsRaidGearProgressionScript()
        : GlobalScript("PlayerbotsRaidGearProgressionScript", {GLOBALHOOK_ON_BEFORE_SET_BOSS_STATE})
    {
    }

    void OnBeforeSetBossState(uint32 id, EncounterState newState, EncounterState oldState, Map* instance) override
    {
        // Loading an instance replays its saved states from TO_BE_DECIDED: only a live kill counts.
        if (newState != DONE || oldState == DONE || oldState == TO_BE_DECIDED || !instance || !instance->IsRaid())
            return;
        if (!sConfigMgr->GetOption<bool>("AiPlayerbot.RaidGearProgression.Enable", true))
            return;
        uint32 const step = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("AiPlayerbot.RaidGearProgression.ItemLevelStep", 5));
        uint32 const slotsPerKill = sConfigMgr->GetOption<uint32>("AiPlayerbot.RaidGearProgression.SlotsPerKill", 1);
        if (!slotsPerKill)
            return;

        // Only a fought kill counts: BossAI goes IN_PROGRESS on the pull and DONE on the kill. DungeonClear boss-focus runs
        // mark the other 13 Naxx40 encounters NOT_STARTED -> DONE before the fight, which handed every bot 13 free
        // upgrades (11-22, tp-20261122-080202-1). The core ends combat before JustDied sets DONE, so a combat check
        // alone missed the Thaddius kill; it stays as a fallback for scripts that never set IN_PROGRESS.
        bool anyInCombat = oldState == IN_PROGRESS;
        for (auto const& ref : instance->GetPlayers())
            if (Player* p = ref.GetSource())
                if (p->IsInCombat())
                {
                    anyInCombat = true;
                    break;
                }
        if (!anyInCombat)
            return;

        // Local testing only (DungeonClear test runs have no real player): also upgrade bot-only groups.
        // Each bot rolls per kill, so upgrades feel like loot rather than a fixed reward (user, 11-22).
        uint32 const chance = std::min<uint32>(100, sConfigMgr->GetOption<uint32>("AiPlayerbot.RaidGearProgression.UpgradeChance", 30));
        bool const botOnlyGroups = sConfigMgr->GetOption<bool>("AiPlayerbot.RaidGearProgression.BotOnlyGroups", false);
        uint32 upgraded = 0, rolled = 0;
        for (auto const& ref : instance->GetPlayers())
        {
            Player* bot = ref.GetSource();
            if (!bot || !bot->IsInWorld() || IsRealPlayer(bot) || !GET_PLAYERBOT_AI(bot))
                continue;
            uint32 const cap = CapFor(instance->GetId(), bot->GetLevel());
            if (!cap || (!botOnlyGroups && !GroupHasRealPlayerOnMap(bot)))
                continue;
            ++upgraded;
            if (urand(1, 100) > chance)
                continue;
            UpgradeBot(bot, cap, step, slotsPerKill, id);
            ++rolled;
        }
        if (upgraded)
            LOG_INFO("playerbots", "[RaidGear] map {} instance {} boss state {} DONE: {} bot(s) eligible, {} won the {}% roll",
                     instance->GetId(), instance->GetInstanceId(), id, upgraded, rolled, chance);
    }
};

void AddSC_raid_gear_progression() { new PlayerbotsRaidGearProgressionScript(); }
