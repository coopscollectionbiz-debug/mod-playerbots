/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BWLHelpers.h"
#include "AiFactory.h"
#include "AiObjectContext.h"
#include "Bag.h"
#include "Creature.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"
#include "Pet.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "Timer.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace BlackwingLairHelpers
{
    bool IsActiveSuppressionDeviceInRange(GameObject const* go, Player const* bot)
    {
        constexpr float suppressionDeviceInteractionDistance = 15.0f;
        return go &&
               go->GetEntry() == static_cast<uint32>(BlackwingLairGameObjects::GO_SUPPRESSION_DEVICE) &&
               go->GetDistance(bot) < suppressionDeviceInteractionDistance &&
               go->GetGoState() == GO_STATE_READY;
    }

    bool AreRazorgoreEggsAlive(PlayerbotAI* botAI)
    {
        GuidVector gos = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest game objects")->Get();
        for (auto const& guid : gos)
        {
            GameObject const* go = botAI->GetGameObject(guid);
            if (go && go->GetEntry() == static_cast<uint32>(BlackwingLairGameObjects::GO_BLACK_DRAGON_EGG))
                return true;
        }
        return false;
    }

    bool IsRazorgoreOffTank(Player* bot)
    {
        return PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
    }

    bool IsNonBABotNearPosition(Player const* bot, Position const& position, float distance)
    {
        Group const* group = bot->GetGroup();
        if (!group)
            return false;

        for (GroupReference const* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player const* p = gref->GetSource();
            if (!p || p == bot || !p->IsAlive() || p->GetMapId() != bot->GetMapId())
                continue;

            if (p->HasAura(static_cast<uint32>(BlackwingLairSpells::SPELL_BURNING_ADRENALINE)))
                continue;

            if (p->GetDistance2d(position.GetPositionX(), position.GetPositionY()) < distance)
                return true;
        }

        return false;
    }

    bool IsCorruptedWhelp(Unit const* unit)
    {
        if (!unit)
            return false;

        const uint32 entry = unit->GetEntry();
        return entry == static_cast<uint32>(BlackwingLairNPCs::NPC_CORRUPTED_RED_WHELP) ||
            entry == static_cast<uint32>(BlackwingLairNPCs::NPC_CORRUPTED_GREEN_WHELP) ||
            entry == static_cast<uint32>(BlackwingLairNPCs::NPC_CORRUPTED_BLUE_WHELP) ||
            entry == static_cast<uint32>(BlackwingLairNPCs::NPC_CORRUPTED_BRONZE_WHELP);
    }

    bool IsCorruptedTotemEntry(uint32 entry)
    {
        return entry == static_cast<uint32>(BlackwingLairNPCs::NPC_CORRUPTED_FIRE_NOVA_TOTEM) ||
            entry == static_cast<uint32>(BlackwingLairNPCs::NPC_CORRUPTED_STONESKIN_TOTEM) ||
            entry == static_cast<uint32>(BlackwingLairNPCs::NPC_CORRUPTED_HEALING_TOTEM) ||
            entry == static_cast<uint32>(BlackwingLairNPCs::NPC_CORRUPTED_WINDFURY_TOTEM);
    }

    bool IsCorruptedTotem(Unit const* unit)
    {
        return unit && IsCorruptedTotemEntry(unit->GetEntry());
    }

    Unit* FindCorruptedTotem(PlayerbotAI* botAI)
    {
        // Ranged take the nearest totem, melee only one they can already reach.
        Player* bot = botAI->GetBot();
        Unit* nearest = nullptr;
        float nearestDistance = 0.0f;
        for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->RefGet())
        {
            if (!IsCorruptedTotemEntry(guid.GetEntry()))
                continue;

            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive() || (!bot->IsWithinMeleeRange(unit) && !PlayerbotAI::IsRanged(bot)))
                continue;

            float const distance = bot->GetExactDist(unit);
            if (!nearest || distance < nearestDistance)
            {
                nearest = unit;
                nearestDistance = distance;
            }
        }
        return nearest;
    }

    bool IsBoneConstruct(Unit const* unit)
    {
        return unit && unit->GetEntry() == static_cast<uint32>(BlackwingLairNPCs::NPC_BONE_CONSTRUCT);
    }

    bool HasBoneConstructAttacker(PlayerbotAI* botAI)
    {
        for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->RefGet())
        {
            if (guid.GetEntry() != static_cast<uint32>(BlackwingLairNPCs::NPC_BONE_CONSTRUCT))
                continue;

            Unit* unit = botAI->GetUnit(guid);
            if (unit && unit->IsAlive() && unit->IsInCombat())
                return true;
        }
        return false;
    }

    // ---- Death Talon Wyrmguard packs (overlay patch 0020) ----
    // Generated by tools/bwl-wyrmguard-stations.py pick; spec §4.3a. Pack 2 slot 1 is owner-placed
    // in game (2026-10-06) so melee can reach slot 0 without crossing it.
    WyrmguardPack const WYRMGUARD_PACKS[WYRMGUARD_PACK_COUNT] = {
        { { 84634, 84628, 84635 }, Position(-7394.91f, -956.35f, 465.15f),
          { Position(-7419.90f, -916.30f, 465.00f), Position(-7454.90f, -931.30f, 465.00f), Position(-7449.90f, -896.30f, 465.00f) } },
        { { 84648, 84639, 84650 }, Position(-7443.73f, -1044.83f, 477.02f),
          { Position(-7433.70f, -1039.80f, 477.20f), Position(-7424.41f, -1010.11f, 477.17f), Position(-7383.70f, -1039.80f, 477.10f) } },
    };

    // Ebonroc's ramp patrol (creature guid 85780, path 857800); spec §2.3 / §4.9. Same points as
    // tools/bwl-wyrmguard-stations.py EBONROC_PATH.
    BwlWyrmguardRules::Pt const EBONROC_PATH[EBONROC_PATH_POINTS] = {
        {-7358.2f, -994.3f}, {-7368.8f, -983.1f}, {-7378.3f, -977.8f}, {-7392.4f, -983.4f},
        {-7405.9f, -987.8f}, {-7418.1f, -976.6f}, {-7414.4f, -963.3f},
    };
    Position const PACK1_STATION_CENTROID(-7441.57f, -914.63f, 465.0f);

    namespace
    {
        constexpr float PACK_ACTIVE_RANGE = 120.0f;
        constexpr uint32 PACK_ACTIVE_GAP_MS = 5000;
        constexpr float MELEE_SIDE_TARGET_CLEARANCE = BwlWyrmguardRules::KEEP_AWAY + 5.0f;

        Creature* FindSpawn(Map* map, uint32 spawnId)
        {
            auto const& store = map->GetCreatureBySpawnIdStore();
            auto it = store.find(spawnId);
            return it == store.end() ? nullptr : it->second;
        }

        // Per (instance, pack) activation clock + per-slot settled-since times (spec §4.8a skull
        // lead). Map threads of different BWL instances call this concurrently, so it is locked. A
        // PACK_ACTIVE_GAP_MS gap with nobody seeing the pack active (wipe, reset) starts a fresh
        // clock, so a stale entry never pre-expires the melee timeout or the skull lead.
        std::mutex g_packClockMutex;
        struct PackClock
        {
            uint32 firstMs;
            uint32 lastMs;
            uint32 settledSince[BwlWyrmguardRules::SLOTS];  // 0 = not settled
            bool held[BwlWyrmguardRules::SLOTS];            // settled and on its holder at the last call
        };
        std::unordered_map<uint64, PackClock> g_packClock;

        // Updates the clock from this call's observation. settledMs[s] = how long slot s has been
        // continuously settled; lost[s] = it was settled on its holder at the last call and now has
        // a living victim that is not its holder (reported once: held drops with onHolder).
        uint32 UpdatePackClock(Map const* map, int packIndex, bool const settled[BwlWyrmguardRules::SLOTS],
                               bool const onHolder[BwlWyrmguardRules::SLOTS],
                               bool const victimElsewhere[BwlWyrmguardRules::SLOTS],
                               uint32 settledMs[BwlWyrmguardRules::SLOTS], bool lost[BwlWyrmguardRules::SLOTS],
                               bool& firstSighting)
        {
            uint64 const key = (uint64(map->GetInstanceId()) << 1) | uint64(packIndex);
            uint32 now = getMSTime();
            if (!now)
                now = 1;  // 0 means "not settled"
            std::lock_guard<std::mutex> lock(g_packClockMutex);
            auto [it, inserted] = g_packClock.try_emplace(key, PackClock{now, now, {}, {}});
            PackClock& clock = it->second;
            firstSighting = inserted || getMSTimeDiff(clock.lastMs, now) > PACK_ACTIVE_GAP_MS;
            if (firstSighting)
            {
                clock.firstMs = now;
                for (int s = 0; s < BwlWyrmguardRules::SLOTS; ++s)
                {
                    clock.settledSince[s] = 0;
                    clock.held[s] = false;
                }
            }
            clock.lastMs = now;
            for (int s = 0; s < BwlWyrmguardRules::SLOTS; ++s)
            {
                lost[s] = clock.held[s] && victimElsewhere[s];
                clock.held[s] = settled[s] && onHolder[s];
                if (!settled[s])
                    clock.settledSince[s] = 0;
                else if (!clock.settledSince[s])
                    clock.settledSince[s] = now;
                settledMs[s] = clock.settledSince[s] ? getMSTimeDiff(clock.settledSince[s], now) : 0;
            }
            return getMSTimeDiff(clock.firstMs, now);
        }

        // The highest threat on `unit` of a living group tank other than `who` (0 = none has any).
        float MaxLivingTankThreat(Player* groupRef, Unit* who, Unit* unit)
        {
            float maxTank = 0.0f;
            if (Group* group = groupRef->GetGroup())
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* member = ref->GetSource();
                    if (!member || member == who || !member->IsAlive() || !PlayerbotAI::IsTank(member))
                        continue;
                    float const t = unit->GetThreatMgr().GetThreat(member);
                    if (t > maxTank)
                        maxTank = t;
                }
            }
            return maxTank;
        }

        // Threat of `who` on `unit` over the highest threat of a living group tank other than `who`.
        float ThreatRatioOf(Player* groupRef, Unit* who, Unit* unit)
        {
            if (!groupRef || !who || !unit)
                return 0.0f;
            float const own = unit->GetThreatMgr().GetThreat(who);
            float const maxTank = MaxLivingTankThreat(groupRef, who, unit);
            // No living tank has threat: the guard is loose (every tank holding it died). There is no
            // tank to protect, so no cap - burning it down beats DPS trickling at it and freezing.
            if (maxTank <= 0.0f)
                return 0.0f;
            return own / maxTank;
        }
    }

    // Stable role order (spec §4.2), so roles never shuffle when a tank dies: role 0 is the
    // MEMBER_FLAG_MAINTANK holder, else the FIRST tank in member order alive or dead (upstream's
    // GetMainTankGuid falls back to the first LIVING tank, which reshuffles every role on a death).
    // Assist roles mirror IsAssistTankOfIndex(index, indexLivingOnly=false): group assistants first,
    // then the other tanks, in member order, dead included.
    void GetTankRolePlayers(Player* bot, Player* out[BwlWyrmguardRules::ROLES])
    {
        for (int r = 0; r < BwlWyrmguardRules::ROLES; ++r)
            out[r] = nullptr;

        Group* group = bot->GetGroup();
        if (!group)
            return;

        ObjectGuid mainTank;
        for (Group::MemberSlot const& slot : group->GetMemberSlots())
        {
            if (slot.flags & MEMBER_FLAG_MAINTANK)
            {
                mainTank = slot.guid;
                break;
            }
        }
        if (mainTank.IsEmpty())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (member && PlayerbotAI::IsTank(member))
                {
                    mainTank = member->GetGUID();
                    break;
                }
            }
        }

        std::vector<Player*> assistants;
        std::vector<Player*> others;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member)
                continue;
            if (member->GetGUID() == mainTank)
            {
                out[0] = member;
                continue;
            }
            if (!PlayerbotAI::IsTank(member))
                continue;
            (group->IsAssistant(member->GetGUID()) ? assistants : others).push_back(member);
        }
        assistants.insert(assistants.end(), others.begin(), others.end());
        for (size_t i = 0; i < assistants.size() && i + 1 < size_t(BwlWyrmguardRules::ROLES); ++i)
            out[i + 1] = assistants[i];
    }

    int GetTankRole(Player* bot)
    {
        Player* byRole[BwlWyrmguardRules::ROLES];
        GetTankRolePlayers(bot, byRole);
        for (int r = 0; r < BwlWyrmguardRules::ROLES; ++r)
            if (byRole[r] == bot)
                return r;
        return BwlWyrmguardRules::NONE;
    }

    bool IsWyrmguardMarker(Player* bot)
    {
        Player* byRole[BwlWyrmguardRules::ROLES];
        GetTankRolePlayers(bot, byRole);
        for (int r = 0; r < BwlWyrmguardRules::ROLES; ++r)
        {
            Player* const p = byRole[r];
            // Bots only: a human tank (or a selfbot) is never expected to mark, so skip them.
            if (p && p->IsInWorld() && p->GetMap() == bot->GetMap() && p->IsAlive() &&
                GET_PLAYERBOT_AI(p) && !IsSelfBot(p))
                return p == bot;
        }
        return false;
    }

    int SlotOfGuard(WyrmguardPackView const& view, Unit const* unit)
    {
        if (!unit)
            return BwlWyrmguardRules::NONE;
        for (int s = 0; s < BwlWyrmguardRules::SLOTS; ++s)
            if (view.guards[s] == unit)
                return s;
        return BwlWyrmguardRules::NONE;
    }

    bool IsWyrmguardMelee(Player* bot)
    {
        return !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsRanged(bot);
    }

    bool GetActiveWyrmguardPack(Player* bot, WyrmguardPackView& view)
    {
        using namespace BwlWyrmguardRules;

        if (bot->GetMapId() != BWL_MAP_ID)
            return false;
        Map* map = bot->GetMap();
        if (!map)
            return false;

        int best = NONE;
        float bestDist = PACK_ACTIVE_RANGE;
        for (int i = 0; i < WYRMGUARD_PACK_COUNT; ++i)
        {
            float const d = bot->GetExactDist2d(WYRMGUARD_PACKS[i].center);
            if (d >= bestDist)
                continue;
            for (uint32 spawnId : WYRMGUARD_PACKS[i].spawnIds)
            {
                Creature* c = FindSpawn(map, spawnId);
                if (c && c->IsAlive() && c->IsInCombat())
                {
                    best = i;
                    bestDist = d;
                    break;
                }
            }
        }
        if (best == NONE)
            return false;

        WyrmguardPack const& pack = WYRMGUARD_PACKS[best];
        view = WyrmguardPackView{};
        view.packIndex = best;

        bool guardAlive[SLOTS];
        int resolved = 0;
        for (int s = 0; s < SLOTS; ++s)
        {
            Creature* c = FindSpawn(map, pack.spawnIds[s]);
            if (c)
                ++resolved;
            bool const alive = c && c->IsAlive();
            view.guards[s] = alive ? c : nullptr;
            guardAlive[s] = alive;
            view.state.alive[s] = alive;
            view.state.station[s] = {pack.stations[s].GetPositionX(), pack.stations[s].GetPositionY()};
            view.state.guard[s] = alive ? Pt{c->GetPositionX(), c->GetPositionY()} : Pt{0.0f, 0.0f};
        }

        Player* byRole[ROLES];
        GetTankRolePlayers(bot, byRole);
        bool tankAlive[ROLES];
        for (int r = 0; r < ROLES; ++r)
            tankAlive[r] = byRole[r] && byRole[r]->IsInWorld() && byRole[r]->GetMap() == map &&
                           byRole[r]->IsAlive();
        view.assign = Assign(tankAlive, guardAlive);

        // Ownership follows aggro: a present role tank holding a guard it was not assigned adopts it.
        int victimRole[SLOTS];
        for (int s = 0; s < SLOTS; ++s)
        {
            victimRole[s] = NONE;
            Unit* const victim = view.guards[s] ? view.guards[s]->GetVictim() : nullptr;
            if (!victim)
                continue;
            for (int r = 0; r < ROLES; ++r)
                if (tankAlive[r] && byRole[r] == victim)
                {
                    victimRole[s] = r;
                    break;
                }
        }
        AdoptByVictim(view.assign, view.state, victimRole);

        for (int s = 0; s < SLOTS; ++s)
        {
            int const role = view.assign.holderRole[s];
            view.state.onHolder[s] = view.guards[s] && role != NONE && byRole[role] &&
                                     view.guards[s]->GetVictim() == byRole[role];
        }

        if (Group* group = bot->GetGroup())
        {
            ObjectGuid const skull = group->GetTargetIcon(RtiTargetValue::skullIndex);
            for (int s = 0; s < SLOTS; ++s)
                if (view.guards[s] && view.guards[s]->GetGUID() == skull)
                    view.skull = s;
        }

        bool settled[SLOTS];
        bool victimElsewhere[SLOTS];
        for (int s = 0; s < SLOTS; ++s)
        {
            settled[s] = IsSettled(view.state, view.assign, s);
            Unit* const victim = view.guards[s] ? view.guards[s]->GetVictim() : nullptr;
            victimElsewhere[s] = victim && victim->IsAlive() && !view.state.onHolder[s];
        }
        bool lost[SLOTS];
        bool firstSighting = false;
        view.msSinceActive = UpdatePackClock(map, best, settled, view.state.onHolder, victimElsewhere,
                                             view.settledMs, lost, firstSighting);
        if (firstSighting)
            LOG_INFO("playerbots", "[BWLWyrmguard] pack {} active in instance {}: {}/3 spawn ids resolved",
                     best + 1, map->GetInstanceId(), resolved);

        // Spec §4.8a: who pulls a settled guard off its holder, and at what threat ratio.
        if (sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
        {
            for (int s = 0; s < SLOTS; ++s)
            {
                if (!lost[s] || !view.guards[s])
                    continue;
                Unit* const victim = view.guards[s]->GetVictim();
                if (!victim)
                    continue;
                LOG_DEBUG("playerbots", "[BWLWyrmguard] aggro lost: pack {} slot {} -> {} (ratio {:.2f})", best + 1, s,
                          victim->GetName(), ThreatRatioOf(bot, victim, view.guards[s]));
            }
        }
        return true;
    }

    Unit* GetWyrmguardDpsTarget(Player* bot, PlayerbotAI* botAI, WyrmguardPackView const& view)
    {
        using namespace BwlWyrmguardRules;

        bool const melee = !PlayerbotAI::IsRanged(bot);
        bool const held = melee && MeleeHeld(view.state, view.assign, view.skull, view.msSinceActive);
        if (view.skull != NONE && !held)
        {
            // Hard stop (spec §4.8a): auto-attack/auto-shot are no engine actions, so the multiplier
            // cannot stop them. nullptr = hold fire; the bot re-attacks once below the stop ratio.
            if (WyrmguardThreatRatio(bot, view.guards[view.skull]) >= BWL_THREAT_STOP)
                return nullptr;
            return view.guards[view.skull];
        }

        // Otherwise only a non-Wyrmguard is allowed - and for melee only one far enough from every
        // living pack guard that walking to it cannot put the bot inside a Stomp (Krixix fails this).
        auto allowed = [&](Unit* u)
        {
            if (!u || !u->IsAlive() || u->GetEntry() == uint32(BlackwingLairNPCs::NPC_DEATH_TALON_WYRMGUARD))
                return false;
            if (!melee)
                return true;
            for (Creature* guard : view.guards)
                if (guard && u->GetExactDist2d(guard) < MELEE_SIDE_TARGET_CLEARANCE)
                    return false;
            return true;
        };

        AiObjectContext* ctx = botAI->GetAiObjectContext();
        Unit* current = ctx->GetValue<Unit*>("current target")->Get();
        if (allowed(current))
            return current;

        Unit* best = nullptr;
        float bestDist = 0.0f;
        for (ObjectGuid const& guid : ctx->GetValue<GuidVector>("attackers")->Get())
        {
            Unit* u = botAI->GetUnit(guid);
            if (!allowed(u))
                continue;
            float const d = bot->GetExactDist2d(u);
            if (!best || d < bestDist)
            {
                best = u;
                bestDist = d;
            }
        }
        return best;
    }

    float WyrmguardThreatRatio(Player* bot, Unit* unit)
    {
        return ThreatRatioOf(bot, bot, unit);
    }

    bool BwlThreatBlocked(Player* bot, Unit* unit, float limit)
    {
        if (!bot || !unit)
            return false;
        float const tankMax = MaxLivingTankThreat(bot, bot, unit);
        if (tankMax <= 0.0f)
            return false;
        return unit->GetThreatMgr().GetThreat(bot) + BWL_THREAT_HEADROOM >= limit * tankMax;
    }

    // ---- BWL-wide DPS threat discipline (spec 2026-10-08) ----

    bool IsBwlThreatDisciplined(Player* bot)
    {
        return bot->GetMapId() == BWL_MAP_ID && !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsHeal(bot);
    }

    bool IsBwlThreatDisciplineTarget(Player* bot, Unit* unit)
    {
        if (!unit || !unit->IsAlive() || !unit->IsInCombat() || !unit->ToCreature())
            return false;
        return MaxLivingTankThreat(bot, bot, unit) > 0.0f;
    }

    bool BwlFightHasOwnThreatRules(Player* bot)
    {
        WyrmguardPackView wyrmguard;
        if (GetActiveWyrmguardPack(bot, wyrmguard))
            return true;
        NefarianView nefarian;
        return GetNefarianView(bot, nefarian);
    }

    bool EbonrocKeepOutActive(Player* bot)
    {
        if (bot->GetMapId() != BWL_MAP_ID)
            return false;
        Map* map = bot->GetMap();
        if (!map)
            return false;

        Creature* ebonroc = FindSpawn(map, EBONROC_SPAWN_ID);
        if (!ebonroc || !ebonroc->IsAlive() || ebonroc->IsInCombat())
            return false;

        for (uint32 spawnId : WYRMGUARD_PACKS[0].spawnIds)
        {
            Creature* c = FindSpawn(map, spawnId);
            if (c && c->IsAlive())
                return true;
        }
        return false;
    }

    float EbonrocPathDistance(Player* bot)
    {
        return BwlWyrmguardRules::DistToPath(BwlWyrmguardRules::Pt{bot->GetPositionX(), bot->GetPositionY()},
                                             EBONROC_PATH, EBONROC_PATH_POINTS);
    }

    int WyrmguardStompThreat(Player* bot, WyrmguardPackView const& view)
    {
        using namespace BwlWyrmguardRules;

        bool const held = MeleeHeld(view.state, view.assign, view.skull, view.msSinceActive);
        int const exempt = (held || view.skull == NONE) ? NONE : view.assign.station[view.skull];
        return StompThreat(view.state, view.assign, Pt{bot->GetPositionX(), bot->GetPositionY()}, exempt);
    }

    int GetWyrmguardHolderTanks(Player* bot, WyrmguardPackView const& view, Player* out[BwlWyrmguardRules::SLOTS])
    {
        using namespace BwlWyrmguardRules;

        Player* byRole[ROLES];
        GetTankRolePlayers(bot, byRole);
        int n = 0;
        for (int s = 0; s < SLOTS; ++s)
        {
            int const r = view.assign.holderRole[s];
            if (!view.state.alive[s] || r == NONE)
                continue;
            Player* const tank = byRole[r];
            // Same presence test as tankAlive in GetActiveWyrmguardPack: a fold onto a dead main tank
            // leaves holderRole 0 pointing at a corpse.
            if (!tank || !tank->IsInWorld() || tank->GetMap() != bot->GetMap() || !tank->IsAlive())
                continue;
            bool dup = false;
            for (int i = 0; i < n; ++i)
                if (out[i] == tank)
                    dup = true;
            if (!dup)
                out[n++] = tank;
        }
        return n;
    }

    bool WyrmguardSpotInLos(Player* bot, float x, float y, float z, Player* target)
    {
        return bot->GetMap()->isInLineOfSight(x, y, z + bot->GetCollisionHeight(), target->GetPositionX(),
                                              target->GetPositionY(),
                                              target->GetPositionZ() + target->GetCollisionHeight(),
                                              bot->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS,
                                              VMAP::ModelIgnoreFlags::Nothing);
    }

    // ---- Nefarian, phases 2 and 3 (overlay patch 0020, spec 2026-10-07) ----
    // Generated by tools/bwl-nefarian-stations.py pick; spec §4.2a.
    NefarianStations const NEF_STATIONS = {
        Position(-7563.35f, -1245.78f, 476.80f),
        Position(-7559.14f, -1243.08f, 476.80f),
        { { Position(-7556.08f, -1247.04f, 476.80f), Position(-7555.75f, -1246.76f, 476.80f), Position(-7555.44f, -1246.45f, 476.80f), Position(-7555.16f, -1246.11f, 476.80f), Position(-7554.91f, -1245.76f, 476.80f) }, { Position(-7561.46f, -1238.65f, 476.80f), Position(-7561.06f, -1238.47f, 476.80f), Position(-7560.65f, -1238.32f, 476.80f), Position(-7560.23f, -1238.20f, 476.80f), Position(-7559.80f, -1238.13f, 476.80f) } },
        { { Position(-7557.54f, -1254.97f, 476.80f), Position(-7554.51f, -1254.15f, 476.80f), Position(-7551.80f, -1252.58f, 476.80f), Position(-7549.60f, -1250.36f, 476.80f), Position(-7556.87f, -1259.93f, 476.80f), Position(-7552.59f, -1258.77f, 476.80f), Position(-7548.75f, -1256.54f, 476.80f), Position(-7545.62f, -1253.39f, 476.80f) }, { Position(-7569.28f, -1236.66f, 476.80f), Position(-7567.27f, -1234.26f, 476.80f), Position(-7564.71f, -1232.45f, 476.80f), Position(-7561.77f, -1231.37f, 476.80f), Position(-7573.50f, -1233.99f, 476.80f), Position(-7570.66f, -1230.58f, 476.80f), Position(-7567.03f, -1228.02f, 476.80f), Position(-7562.86f, -1226.49f, 476.80f) } },
        { { Position(-7557.80f, -1252.99f, 476.80f), Position(-7556.10f, -1252.61f, 476.80f), Position(-7554.49f, -1251.94f, 476.80f), Position(-7556.07f, -1256.74f, 476.80f), Position(-7553.74f, -1256.00f, 476.80f) }, { Position(-7567.59f, -1237.73f, 476.80f), Position(-7566.53f, -1236.35f, 476.80f), Position(-7565.25f, -1235.16f, 476.80f), Position(-7570.27f, -1234.59f, 476.80f), Position(-7568.62f, -1232.79f, 476.80f) } },
        Position(-7553.74f, -1251.50f, 476.80f),
    };

    namespace
    {
        constexpr float NEF_SEARCH_RANGE = 250.0f;       // the room is ~110 y across; under the 533 y clamp
        constexpr float NEF_ADD_RANGE = 60.0f;
        constexpr float NEF_CONSTRUCT_RANGE = 120.0f;
        constexpr float NEF_SPELL_RANGE = 40.0f;
        constexpr uint32 NEF_CLOCK_GAP_MS = 10000;        // unseen this long = new attempt
        constexpr uint32 NEF_SCAN_INTERVAL_MS = 500;      // grid-scan reuse window (per instance)
        constexpr uint32 NEF_DIAG_INTERVAL_MS = 5000;     // "[BWLNefarian] diag" line (per instance)
        constexpr float NEF_DIAG_FAR_RANGE = 40.0f;       // diag "far N": living members beyond this
        constexpr uint32 NEF_MANA_REFILL_WINDOW_MS = 5000; // one-time refill only this soon after landing

        // Per-instance landing clock, last logged phase and the cached Nefarian / Bone Construct scan.
        // Map threads of different instances call this concurrently, so it is locked (memory: module
        // globals need locking). The scan cache keeps the two grid searches off the per-action hot path:
        // the boss state is already IN_PROGRESS through phase 1, when Nefarian does not exist yet, and the
        // "not found" result is cached too.
        std::mutex g_nefClockMutex;
        struct NefClock
        {
            uint32 firstMs = 0;   // 0 = landing clock not started
            uint32 lastMs = 0;
            BwlNefarianRules::Phase phase = BwlNefarianRules::Phase::None;
            ObjectGuid nefGuid;   // empty = no Nefarian found by the last scan
            bool constructAlive = false;
            uint32 scanMs = 0;    // 0 = never scanned
            uint32 diagMs = 0;    // last "[BWLNefarian] diag" line; 0 = never
            std::unordered_set<ObjectGuid> manaRefilled;   // bots refilled this attempt (cleared on a new one)
        };
        std::unordered_map<uint32, NefClock> g_nefClock;

        uint32 NowMs()
        {
            uint32 const now = getMSTime();
            return now ? now : 1;
        }

        // Cached scan for the instance, or false when it is missing or older than NEF_SCAN_INTERVAL_MS.
        bool GetCachedNefScan(uint32 instanceId, uint32 now, ObjectGuid& nefGuid, bool& constructAlive)
        {
            std::lock_guard<std::mutex> lock(g_nefClockMutex);
            auto it = g_nefClock.find(instanceId);
            if (it == g_nefClock.end() || !it->second.scanMs ||
                getMSTimeDiff(it->second.scanMs, now) >= NEF_SCAN_INTERVAL_MS)
                return false;
            nefGuid = it->second.nefGuid;
            constructAlive = it->second.constructAlive;
            return true;
        }

        void StoreNefScan(uint32 instanceId, uint32 now, ObjectGuid nefGuid, bool constructAlive)
        {
            std::lock_guard<std::mutex> lock(g_nefClockMutex);
            NefClock& clock = g_nefClock[instanceId];
            clock.nefGuid = nefGuid;
            clock.constructAlive = constructAlive;
            clock.scanMs = now;
        }

        // Called only once the view gate has passed (Nefarian found and selectable).
        uint32 UpdateNefClock(Map const* map, BwlNefarianRules::Phase phase)
        {
            uint32 const now = NowMs();
            std::lock_guard<std::mutex> lock(g_nefClockMutex);
            NefClock& clock = g_nefClock[map->GetInstanceId()];
            if (!clock.firstMs || getMSTimeDiff(clock.lastMs, now) > NEF_CLOCK_GAP_MS)
            {
                clock.firstMs = now;
                clock.phase = BwlNefarianRules::Phase::None;
                clock.manaRefilled.clear();
            }
            clock.lastMs = now;
            if (clock.phase != phase)
            {
                LOG_DEBUG("playerbots", "[BWLNefarian] instance {}: phase {}", map->GetInstanceId(),
                          phase == BwlNefarianRules::Phase::P3 ? "P3" : "P2");
                clock.phase = phase;
            }
            return getMSTimeDiff(clock.firstMs, now);
        }

        // True the first time `guid` asks this attempt (claims it). Each bot asks only for itself.
        bool ClaimNefManaRefill(uint32 instanceId, ObjectGuid guid)
        {
            std::lock_guard<std::mutex> lock(g_nefClockMutex);
            return g_nefClock[instanceId].manaRefilled.insert(guid).second;
        }

        // True at most once per NEF_DIAG_INTERVAL_MS per instance (claims the slot when it returns true).
        bool NefDiagDue(uint32 instanceId, uint32 now)
        {
            std::lock_guard<std::mutex> lock(g_nefClockMutex);
            NefClock& clock = g_nefClock[instanceId];
            if (clock.diagMs && getMSTimeDiff(clock.diagMs, now) < NEF_DIAG_INTERVAL_MS)
                return false;
            clock.diagMs = now;
            return true;
        }

        // Spec §9 #1 / §4.8 item 5: why the MT is (not) on Nefarian, one line per instance every 5 s.
        void LogNefDiag(Player* bot, NefarianView const& view)
        {
            Creature* const nef = view.nefarian;
            // Living group members farther than NEF_DIAG_FAR_RANGE from him (fix round 3, G2).
            int farCount = 0;
            if (Group* group = bot->GetGroup())
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                    if (Player* member = ref->GetSource())
                        if (member->IsInWorld() && member->GetMap() == bot->GetMap() && member->IsAlive() &&
                            member->GetExactDist2d(nef) > NEF_DIAG_FAR_RANGE)
                            ++farCount;
            Position const& p = NEF_STATIONS.park;
            Position const& t = NEF_STATIONS.mt;
            Unit* const victim = nef->GetVictim();
            Player* const mt = view.mainTank;
            LOG_DEBUG("playerbots",
                      "[BWLNefarian] diag nef ({:.1f}, {:.1f}) distP {:.1f} parked {} | victim {} dist {:.1f} | "
                      "MT {} distT {:.1f} melee {} los {} combat {} | far {}",
                      nef->GetPositionX(), nef->GetPositionY(),
                      nef->GetExactDist2d(p.GetPositionX(), p.GetPositionY()), view.parked,
                      victim ? victim->GetName() : "none", victim ? victim->GetExactDist2d(nef) : -1.0f,
                      mt ? mt->GetName() : "none",
                      mt ? mt->GetExactDist2d(t.GetPositionX(), t.GetPositionY()) : -1.0f,
                      mt ? mt->IsWithinMeleeRange(nef) : false, mt ? mt->IsWithinLOSInMap(nef) : false,
                      mt ? mt->IsInCombat() : false, farCount);
        }

        std::vector<uint32> const NEF_ADD_ENTRIES = {14662, 14663, 14664, 14666, 14668, 14261, 14262,
                                                     14263, 14264, 14265, 14302, 14605};

        // Live, attackable adds near the bot, as rules candidates (Nefarian appended by the caller).
        void CollectAdds(Player* bot, std::vector<Creature*>& units, std::vector<BwlNefarianRules::Cand>& cands)
        {
            std::list<Creature*> found;
            bot->GetCreatureListWithEntryInGrid(found, NEF_ADD_ENTRIES, NEF_ADD_RANGE);
            for (Creature* c : found)
            {
                if (!c || !c->IsAlive() || !bot->IsValidAttackTarget(c))
                    continue;
                units.push_back(c);
                cands.push_back({BwlNefarianRules::KindOfEntry(c->GetEntry()), bot->GetExactDist2d(c)});
            }
        }
    }

    Player* GetNefarianMainTank(Player* bot, Creature* nefarian)
    {
        // Aggro decides (spec fix round 2): after a tank death the next tank holds him, and a resurrected
        // lower role must not reclaim "MT" while he is on someone else - the tank he is hitting is the MT.
        if (nefarian)
            if (Unit* const victim = nefarian->GetVictim())
                if (Player* const v = victim->ToPlayer())
                    if (v->IsInWorld() && v->GetMap() == bot->GetMap() && v->IsAlive() &&
                        (v == bot || bot->IsInSameRaidWith(v)) && PlayerbotAI::IsTank(v))
                        return v;
        Player* byRole[BwlWyrmguardRules::ROLES];
        GetTankRolePlayers(bot, byRole);
        for (Player* p : byRole)
            if (p && p->IsInWorld() && p->GetMap() == bot->GetMap() && p->IsAlive())
                return p;
        return nullptr;
    }

    bool GetNefarianView(Player* bot, NefarianView& view)
    {
        using namespace BwlNefarianRules;

        if (bot->GetMapId() != BWL_MAP_ID)
            return false;
        InstanceScript* instance = bot->GetInstanceScript();
        if (!instance || instance->GetBossState(BWL_DATA_NEFARIAN) != IN_PROGRESS)
            return false;
        Map* const map = bot->GetMap();
        uint32 const instanceId = map->GetInstanceId();
        uint32 const now = NowMs();
        ObjectGuid nefGuid;
        bool constructAlive = false;
        Creature* nef = nullptr;
        bool fresh = GetCachedNefScan(instanceId, now, nefGuid, constructAlive);
        if (fresh && !nefGuid.IsEmpty())
        {
            nef = map->GetCreature(nefGuid);
            if (!nef)
                fresh = false;   // despawned since the scan: rescan now
        }
        if (!fresh)
        {
            // Scan outside the lock: one instance's bots share one map thread, so no two scans of the same
            // entry race; other instances only touch their own entries.
            nef = bot->FindNearestCreature(NPC_NEFARIAN, NEF_SEARCH_RANGE, true);
            constructAlive = nef && bot->FindNearestCreature(NPC_BONE_CONSTRUCT, NEF_CONSTRUCT_RANGE, true);
            StoreNefScan(instanceId, now, nef ? nef->GetGUID() : ObjectGuid::Empty, constructAlive);
        }
        if (!nef || !nef->IsAlive() || nef->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return false;

        view = NefarianView{};
        view.nefarian = nef;
        view.phase = GetPhase(true, nef->GetHealthPct(), constructAlive);
        view.mainTank = GetNefarianMainTank(bot, nef);
        view.parked = Parked(ToNefPt(nef), Pt{NEF_STATIONS.park.GetPositionX(), NEF_STATIONS.park.GetPositionY()});
        view.siphon = nef->HasAura(SPELL_IMPROVED_BOP);
        view.msSinceLanded = UpdateNefClock(bot->GetMap(), view.phase);
        // Owner-approved raid cheat: phase 1 ends -> every living mana-using BOT starts the landing at full
        // mana, once per attempt (keyed by bot GUID in the instance's landing clock, cleared when the clock
        // resets). Only in the first NEF_MANA_REFILL_WINDOW_MS after landing, never for humans/selfbots.
        if (view.msSinceLanded <= NEF_MANA_REFILL_WINDOW_MS && bot->IsAlive() && bot->GetMaxPower(POWER_MANA) > 0 &&
            !IsSelfBot(bot))
            if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot))
                if (botAI->HasCheat(BotCheatMask::raid) && ClaimNefManaRefill(instanceId, bot->GetGUID()))
                {
                    LOG_DEBUG("playerbots", "[BWLNefarian] mana refill {} {}/{}", bot->GetName(),
                              bot->GetPower(POWER_MANA), bot->GetMaxPower(POWER_MANA));
                    bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));
                }
        if (sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
        {
            if (NefDiagDue(instanceId, now))
                LogNefDiag(bot, view);
            static uint32 const calls[] = {SPELL_CALL_WARRIOR, SPELL_CALL_DRUID, SPELL_CALL_PRIEST, SPELL_CALL_MAGE,
                                           SPELL_CALL_ROGUE, SPELL_CALL_PALADIN, SPELL_CALL_SHAMAN, SPELL_CALL_WARLOCK};
            for (uint32 call : calls)
                if (Aura* aura = bot->GetAura(call))
                    if (aura->GetDuration() > aura->GetMaxDuration() - 1000)   // first second only
                        LOG_DEBUG("playerbots", "[BWLNefarian] {} class call {} (role {})", bot->GetName(), call,
                                  static_cast<int>(GetNefRole(bot, view)));
        }
        return true;
    }

    bool HasBrokenRangedWeapon(Player* bot)
    {
        if (bot->getClass() != CLASS_HUNTER)
            return false;
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
        if (!item)
            return true;
        return item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 && item->GetUInt32Value(ITEM_FIELD_DURABILITY) == 0;
    }

    uint32 FindSpareRangedWeapon(Player* bot)
    {
        auto usable = [bot](Item* item) -> bool
        {
            if (!item)
                return false;
            ItemTemplate const* proto = item->GetTemplate();
            if (!proto || (proto->InventoryType != INVTYPE_RANGED && proto->InventoryType != INVTYPE_RANGEDRIGHT &&
                           proto->InventoryType != INVTYPE_THROWN))
                return false;
            if (item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 && item->GetUInt32Value(ITEM_FIELD_DURABILITY) == 0)
                return false;
            return bot->CanUseItem(proto) == EQUIP_ERR_OK;
        };
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot); usable(item))
                return item->GetEntry();
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
            if (Bag* b = bot->GetBagByPos(bag))
                for (uint32 slot = 0; slot < b->GetBagSize(); ++slot)
                    if (Item* item = b->GetItemByPos(slot); usable(item))
                        return item->GetEntry();
        return 0;
    }

    bool IsPhysicalDps(Player* bot)
    {
        switch (bot->getClass())
        {
            case CLASS_WARRIOR:
            case CLASS_ROGUE:
            case CLASS_HUNTER:
            case CLASS_DEATH_KNIGHT:
                return true;
            case CLASS_DRUID:
                return AiFactory::GetPlayerSpecTab(bot) == DRUID_TAB_FERAL;
            case CLASS_SHAMAN:
                return AiFactory::GetPlayerSpecTab(bot) == SHAMAN_TAB_ENHANCEMENT;
            case CLASS_PALADIN:
                return AiFactory::GetPlayerSpecTab(bot) == PALADIN_TAB_RETRIBUTION;
            default:
                return false;
        }
    }

    NefMtStep GetNefMtStep(Player* bot, NefarianView const& view, Unit* currentTarget)
    {
        Creature* const nef = view.nefarian;
        bool const victim = nef->GetVictim() == bot;
        bool const melee = bot->IsWithinMeleeRange(nef);
        if (!bot->IsWithinLOSInMap(nef))
            return NefMtStep::Close;
        // Out of melee: close in - unless he is already on the MT and nearly there (he walks to his
        // victim; running at him would undo every backing step toward T).
        if (!melee && !(victim && bot->GetDistance(nef) <= NEF_MT_CHASE_SLACK))
            return NefMtStep::Close;
        if (currentTarget != nef)
            return NefMtStep::Attack;
        if (!victim || !melee)
            return NefMtStep::None;   // taunt-immune: earn aggro with the normal rotation first (spec §4.2)
        Position const& t = NEF_STATIONS.mt;
        if (bot->GetExactDist2d(t.GetPositionX(), t.GetPositionY()) > BwlNefarianRules::STATION_ARRIVE)
            return NefMtStep::Back;
        if (!bot->HasInArc(NEF_MT_FACE_ARC, nef))
            return NefMtStep::Face;
        return NefMtStep::None;
    }

    NefRole GetNefRole(Player* bot, NefarianView const& view)
    {
        if (bot == view.mainTank)
            return NefRole::MainTank;
        if (PlayerbotAI::IsTank(bot))
            return NefRole::OffTank;
        // Forced cat form and a broken bow both turn the bot into melee for the call / the fight.
        if ((bot->getClass() == CLASS_DRUID && bot->HasAura(BwlNefarianRules::SPELL_CALL_DRUID)) ||
            HasBrokenRangedWeapon(bot))
            return NefRole::Melee;
        if (PlayerbotAI::IsHeal(bot))
            return NefRole::Healer;
        return PlayerbotAI::IsRanged(bot) ? NefRole::Ranged : NefRole::Melee;
    }

    int NefRoleOrdinal(Player* bot, NefRole role, NefarianView const& view)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return 0;
        int ordinal = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || member->GetMap() != bot->GetMap())
                continue;
            if (member->GetGUID() < bot->GetGUID() && GetNefRole(member, view) == role)
                ++ordinal;
        }
        return ordinal;
    }

    bool GetNefPositionTarget(Player* bot, PlayerbotAI* botAI, NefarianView const& view, Position& dest)
    {
        using namespace BwlNefarianRules;

        // His current victim never moves: he always faces it, so it is always in his front cone, and an
        // escape step only makes him turn after it (tanks ran in circles until they died).
        if (view.nefarian->GetVictim() == bot)
            return false;
        NefRole const role = GetNefRole(bot, view);
        if (role == NefRole::MainTank)
            return false;
        int const ordinal = NefRoleOrdinal(bot, role, view);
        Pt const nef = ToNefPt(view.nefarian);
        float const facing = view.nefarian->GetOrientation();
        bool const melee = role == NefRole::Melee || role == NefRole::OffTank;

        if (InDanger(nef, facing, ToNefPt(bot)))
        {
            Pt const e = EscapePoint(nef, facing, FlankSide(ordinal), melee ? MELEE_ESCAPE_RADIUS : RANGED_ESCAPE_RADIUS);
            dest = Position(e.x, e.y, bot->GetPositionZ());
            return true;
        }
        // Station keeping (and the "come in" rule below) only for bots fighting Nefarian (or with nothing to
        // fight); healers always. An off-tank fighting Nefarian takes a melee flank slot (spec §9 #2) - unless
        // he is on it, when it stays put rather than drag him; with nothing to do it waits at OT staging.
        Unit* const current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        bool const busyElsewhere = current && current->IsAlive() && current != view.nefarian;
        bool const onNefarian = current == view.nefarian;
        if (role == NefRole::OffTank &&
            (busyElsewhere || (onNefarian ? view.nefarian->GetVictim() == bot : !bot->getAttackers().empty())))
            return false;
        if ((role == NefRole::Melee || role == NefRole::Ranged) && busyElsewhere)
            return false;

        if (view.parked)
        {
            Position station;
            if (role == NefRole::OffTank && !onNefarian)
                station = NEF_STATIONS.otStaging;
            else
            {
                bool const meleeSlot = role == NefRole::Melee || role == NefRole::OffTank;
                int const perSide = meleeSlot ? NEF_MELEE_PER_SIDE
                                  : role == NefRole::Ranged ? NEF_RANGED_PER_SIDE : NEF_HEALER_PER_SIDE;
                Slot const slot = SlotFor(ordinal, perSide);
                int const s = slot.side == 1 ? 0 : 1;
                station = meleeSlot ? NEF_STATIONS.melee[s][slot.index]
                        : role == NefRole::Ranged ? NEF_STATIONS.ranged[s][slot.index]
                                                  : NEF_STATIONS.healer[s][slot.index];
            }
            // The bot is out of both cones here (the escape above returned otherwise); a station that his
            // live position/facing puts inside a cone is skipped, and the "come in" rule decides instead.
            if (StationUsable(nef, facing, Pt{station.GetPositionX(), station.GetPositionY()}))
            {
                if (bot->GetExactDist2d(station.GetPositionX(), station.GetPositionY()) <= STATION_ARRIVE)
                    return false;
                dest = station;
                return true;
            }
        }

        // Fix round 3 (G2): not parked, or no usable station - a bot far from him (or without LOS) comes in
        // to its role radius, on the line toward itself, or its own side's escape point when that line
        // point is in a cone. Nefarian's z: a far bot's own z can be off the dais level.
        float const radius = melee ? APPROACH_RADIUS_MELEE
                           : role == NefRole::Healer ? APPROACH_RADIUS_HEALER : APPROACH_RADIUS_RANGED;
        Pt const self = ToNefPt(bot);
        float const dist = Dist(nef, self);
        // LOS only matters (and is only paid for) inside the slack ring.
        if (!NeedsApproach(dist, radius, dist > radius + APPROACH_SLACK || bot->IsWithinLOSInMap(view.nefarian)))
            return false;
        Pt const a = ApproachPoint(nef, facing, self, radius, SideOf(nef, facing, self));
        if (Dist(a, self) <= STATION_ARRIVE)
            return false;
        dest = Position(a.x, a.y, view.nefarian->GetPositionZ());
        return true;
    }

    Unit* GetNefDpsTarget(Player* bot, PlayerbotAI* /*botAI*/, NefarianView const& view)
    {
        using namespace BwlNefarianRules;

        std::vector<Creature*> units;
        std::vector<Cand> cands;
        CollectAdds(bot, units, cands);
        units.push_back(view.nefarian);
        cands.push_back({Kind::Nefarian, bot->GetExactDist2d(view.nefarian)});

        int const pick = PickTarget(view.phase, view.siphon && IsPhysicalDps(bot), cands.data(),
                                    static_cast<int>(cands.size()));
        if (pick == NONE)
            return nullptr;
        Creature* const target = units[pick];
        if (target != view.nefarian)
            return target;

        bool const mtIsVictim = view.mainTank && view.nefarian->GetVictim() == view.mainTank;
        if (HoldFireOnNefarian(mtIsVictim, view.msSinceLanded, NefThreatRatio(bot, view)))
            return nullptr;
        return target;
    }

    float NefThreatRatio(Player* bot, NefarianView const& view)
    {
        using namespace BwlNefarianRules;

        Creature* const nef = view.nefarian;
        if (!nef || bot == view.mainTank)
            return 0.0f;
        ThreatManager& mgr = nef->GetThreatMgr();
        ThreatRole const role = PlayerbotAI::IsTank(bot) ? ThreatRole::OffTank : ThreatRole::Other;
        float const mtThreat = view.mainTank ? mgr.GetThreat(view.mainTank) : 0.0f;
        float backup = 0.0f;
        if (role == ThreatRole::Other)
        {
            if (Group* group = bot->GetGroup())
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* const m = ref->GetSource();
                    if (!m || m == view.mainTank || m->GetMap() != bot->GetMap() || !m->IsAlive() ||
                        !PlayerbotAI::IsTank(m))
                        continue;
                    float const t = mgr.GetThreat(m);
                    if (t > backup)
                        backup = t;
                }
            }
        }
        return ThreatRatio(role, mgr.GetThreat(bot), mtThreat, backup);
    }

    bool OffTankHoldsFireOnNefarian(Player* bot, NefarianView const& view)
    {
        bool const mtIsVictim = view.mainTank && view.nefarian->GetVictim() == view.mainTank;
        return BwlNefarianRules::HoldFireOnNefarian(mtIsVictim, view.msSinceLanded, NefThreatRatio(bot, view));
    }

    Unit* GetNefOffTankTarget(Player* bot, NefarianView const& /*view*/)
    {
        std::vector<Creature*> units;
        std::vector<BwlNefarianRules::Cand> cands;
        CollectAdds(bot, units, cands);
        int const pick = BwlNefarianRules::PickOtTarget(cands.data(), static_cast<int>(cands.size()));
        return pick == BwlNefarianRules::NONE ? nullptr : units[pick];
    }

    Player* GetNefPolymorphDispelTarget(Player* bot, NefarianView const& view)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return nullptr;
        std::vector<Player*> players;
        std::vector<BwlNefarianRules::DispelInfo> infos;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* m = ref->GetSource();
            if (!m || !m->IsAlive() || m->GetMap() != bot->GetMap() ||
                !m->HasAura(BwlNefarianRules::SPELL_WILD_POLYMORPH) || bot->GetDistance(m) > NEF_SPELL_RANGE)
                continue;
            players.push_back(m);
            infos.push_back({m == view.mainTank, PlayerbotAI::IsHeal(m), m->GetHealthPct()});
        }
        int const pick = BwlNefarianRules::PickDispel(infos.data(), static_cast<int>(infos.size()));
        return pick == BwlNefarianRules::NONE ? nullptr : players[pick];
    }

    bool GetNefTankHeal(Player* bot, PlayerbotAI* botAI, NefarianView const& view, Player*& tank,
                        BwlNefarianRules::TankHeal& spell)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;
        std::vector<Player*> players;
        std::vector<BwlNefarianRules::TankInfo> infos;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* m = ref->GetSource();
            if (!m || !m->IsAlive() || m->GetMap() != bot->GetMap() || !PlayerbotAI::IsTank(m) ||
                bot->GetDistance(m) > NEF_SPELL_RANGE)
                continue;
            bool const isMt = m == view.mainTank;
            players.push_back(m);
            infos.push_back({isMt || !m->getAttackers().empty(), isMt,
                             m->HasAura(BwlNefarianRules::SPELL_WEAKENED_SOUL),
                             botAI->HasAura("renew", m, false, true), m->GetHealthPct()});
        }
        BwlNefarianRules::TankHealPick const pick =
            BwlNefarianRules::PickTankHeal(infos.data(), static_cast<int>(infos.size()));
        if (pick.index == BwlNefarianRules::NONE)
            return false;
        tank = players[pick.index];
        spell = pick.spell;
        return true;
    }
}
