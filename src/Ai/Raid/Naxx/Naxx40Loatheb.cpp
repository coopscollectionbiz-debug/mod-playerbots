/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Loatheb helper layer (overlay patch 0022). Spec (overlay repo):
// docs/superpowers/specs/2026-10-30-playerbot-naxx40-loatheb-design.md §3, §4.

#include "Naxx40Loatheb.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Creature.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "Timer.h"

namespace Naxx40Loatheb
{
    using namespace Naxx40LoathebRules;

    namespace
    {
        // An entry not refreshed for this long is a new attempt (whoever calls GetView after a wipe, or a reused
        // instance id): it is reset and re-latched from scratch.
        constexpr uint32 STALE_STATE_MS = 10000;
        // Spores spawn at a random point near Loatheb; the grid search around him reaches every one of them.
        constexpr float SPORE_SEARCH_RADIUS = 100.0f;
        constexpr uint8 MAX_SUBGROUPS = 8;

        // Per-instance encounter state. Bot AI of different instances runs on parallel map threads, so the
        // container is only touched under g_mutex (memory: module globals need locking), held briefly; callers get
        // copies (the HealerPlan as a shared immutable pointer), never references into it. One instance's bots
        // share one map thread, so a refresh of an instance (copy out, compute outside the lock, write back) can
        // never interleave with another refresh of it.
        struct Roster
        {
            ObjectGuid loathebGuid;  // empty = not resolved yet
            bool latched = false;    // the HealerPlan was built
            // Immutable once published: views share it without copying.
            std::shared_ptr<HealerPlan const> plan;

            // Doom clock (spec §3: Inevitable Doom, 10 s, then 2549 shadow).
            bool doomUp = false;      // DOOM_SPELL was on a living raid member at the last refresh
            uint32 doomSeenMs = 0;    // when doomLeftMs was read
            uint32 doomLeftMs = 0;    // the longest remaining DOOM_SPELL duration at doomSeenMs
            uint32 lastResolveMs = 0;  // 0 = no Doom resolved yet this attempt
            uint8 doomCount = 0;

            // Spores (spec §4.9 item 3): the ones alive at the last refresh, in discovery order, each with the time it
            // was first seen. Immutable once published: views share it without copying.
            std::shared_ptr<std::vector<SporeSeen> const> spores;
            uint32 sporeKills = 0;      // tracked spores found gone or dead (each counted once)
            uint32 sporesNearTank = 0;  // live spores within SPORE_TANK_EXCLUDE of Loatheb's victim at the last refresh
            uint32 sporesAtStack = 0;   // live spores within SPORE_STACK_RADIUS of RANGE_SPOT at the last refresh

            uint32 refreshMs = 0;  // 0 = never refreshed
            uint32 diagMs = 0;     // last "[Naxx40Loatheb] diag" line; 0 = never
        };

        // Spec §4.7 item 4: Prayer of Healing claimed on a subgroup for one Doom (the latched doom count). Spec §4.9
        // item 1 reuses it for the main-tank top-up (one per Doom per instance).
        struct PartyClaim
        {
            uint64_t claimant = 0;  // 0 = none
            uint8 doom = 0;
        };

        // Bot-written parts live here, beside the roster, so a refresh (which writes back only the roster) never
        // overwrites them.
        struct State
        {
            Roster roster;
            size_t lastCaster = SIZE_MAX;  // rotation index of the last tank heal; SIZE_MAX = none yet
            uint32 lastTankHealMs = 0;     // 0 = no rotation heal yet this attempt
            std::unordered_set<uint64_t> manaRefilled;  // ClaimManaRefill
            std::unordered_set<uint64_t> healthstone;   // ClaimHealthstone
            uint32 tankCasts = 0;                       // NoteTankHealCast: every tank heal
            uint32 emergencyCasts = 0;                  // NoteTankHealCast: the emergency ones
            uint32 pots = 0;  // NoteConsumable
            uint32 stones = 0;
            uint32 bandages = 0;
            std::unordered_map<uint64_t, uint32> lastPotionMs;  // NoteConsumable(Potion), for MsSinceLastPotion
            uint32 lastEmergencyMs = 0;                         // ClaimEmergencyHeal; 0 = none yet
            PartyClaim partyClaim[MAX_SUBGROUPS];               // ClaimPartyHeal
            uint32 raidHeals[3] = {0, 0, 0};                    // NoteRaidHealCast: prayer / chain / big
            uint32 topUps = 0;                                  // NoteRaidHealCast: main-tank top-ups
            PartyClaim topUpClaim;                              // ClaimTankTopUp
            std::unordered_map<uint64_t, uint8> topUpRefusedDoom;  // ReleaseTankTopUp: the doom count of a refused cast
            std::unordered_map<uint64_t, uint8> stoneNoteDoom;  // ClaimStoneNote: the doom count last noted
        };

        std::mutex g_mutex;
        std::unordered_map<uint32, State> g_stateByInstance;
        // g_stateByInstance.size(), written under g_mutex: keeps the gate-failed erase lock-free while no instance
        // holds state (every bot in every Naxx instance runs that path when the encounter is not in progress).
        std::atomic<uint32> g_stateCount{0};

        uint32 NowMs()
        {
            uint32 const now = getMSTime();
            return now ? now : 1;
        }

        void EraseState(uint32 instanceId)
        {
            if (!g_stateCount.load(std::memory_order_relaxed))
                return;
            std::lock_guard<std::mutex> lock(g_mutex);
            g_stateByInstance.erase(instanceId);
            g_stateCount.store(uint32(g_stateByInstance.size()), std::memory_order_relaxed);
        }

        // Gate failed after a scan (no living Loatheb). Drops the attempt like EraseState, but keeps the scan
        // timestamp so the map-wide scan runs at most once per REFRESH_INTERVAL_MS instead of on every call (the
        // "not found" result is cached too, as in BWLHelpers).
        void ResetState(uint32 instanceId, uint32 now)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            State& st = g_stateByInstance[instanceId];
            st = State{};
            st.roster.refreshMs = now;
            g_stateCount.store(uint32(g_stateByInstance.size()), std::memory_order_relaxed);
        }

        Player* FindPlayer(Map const* map, uint64_t raw)
        {
            if (!raw)
                return nullptr;
            Player* p = ObjectAccessor::GetPlayer(map, ObjectGuid(raw));
            return p && p->IsInWorld() && p->GetMap() == map ? p : nullptr;
        }

        // Loatheb by entry: the cached GUID through Map::GetCreature, else ONE map-wide pass over the spawn store (no
        // grid-search radius clamp). The caller limits this to once per REFRESH_INTERVAL_MS per instance.
        Creature* ResolveLoatheb(Map* map, Roster& r)
        {
            Creature* c = r.loathebGuid.IsEmpty() ? nullptr : map->GetCreature(r.loathebGuid);
            if (c)
                return c;
            for (auto const& entry : map->GetCreatureBySpawnIdStore())
            {
                Creature* cand = entry.second;
                if (cand && cand->IsInWorld() && cand->GetEntry() == LOATHEB_ENTRY)
                {
                    r.loathebGuid = cand->GetGUID();
                    return cand;
                }
            }
            return nullptr;
        }

        bool IsHealerClass(uint8 cls, HealerClass& out)
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

        // What the refresh needs from one pass over the raid members on the bot's map.
        struct RaidScan
        {
            std::vector<HealerMember> healers;  // AssignHealers input, member order
            int32 doomLeftMs = -1;  // longest DOOM_SPELL remaining on a living member, -1 = none
        };

        RaidScan ScanRaid(Player* bot, Group* group)
        {
            RaidScan s;
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* p = ref->GetSource();
                if (!p || !p->IsInWorld() || p->GetMap() != bot->GetMap() || !p->IsAlive())
                    continue;
                uint8 const sg = group->GetMemberGroup(p->GetGUID());
                if (!PlayerbotAI::IsTank(p) && PlayerbotAI::IsHeal(p))
                {
                    HealerClass cls;
                    if (sg < MAX_SUBGROUPS && IsHealerClass(p->getClass(), cls))
                    {
                        bool const human = !GET_PLAYERBOT_AI(p) || IsSelfBot(p);
                        s.healers.push_back(HealerMember{p->GetGUID().GetRawValue(), cls, sg, human});
                    }
                }
                if (Aura* doom = p->GetAura(DOOM_SPELL))
                    s.doomLeftMs = std::max<int32>(s.doomLeftMs, std::max<int32>(doom->GetDuration(), 0));
            }
            return s;
        }

        // Doom clock: while DOOM_SPELL is up, remember its remaining time; the refresh that no longer finds it
        // records the resolve (once per cast).
        void UpdateDoom(Roster& r, RaidScan const& s, uint32 now)
        {
            if (s.doomLeftMs >= 0)
            {
                r.doomUp = true;
                r.doomSeenMs = now;
                r.doomLeftMs = uint32(s.doomLeftMs);
            }
            else if (r.doomUp)
            {
                r.doomUp = false;
                r.lastResolveMs = now;
                if (r.doomCount < std::numeric_limits<uint8>::max())
                    ++r.doomCount;
            }
        }

        // Spec §4.7 item 6: the spore exclusion measures to Loatheb's current victim only (any unit); none = clear.
        float VictimDist(Creature* c, Unit* victim)
        {
            return victim ? c->GetExactDist2d(victim) : std::numeric_limits<float>::max();
        }

        // Spec §4.9 item 3: keeps the list of live spores the bots read (view.spores). A tracked spore that is gone or
        // dead counts once as a kill and is dropped; a newly found live one is appended with now as its first-seen
        // time. Also counts the live spores within SPORE_TANK_EXCLUDE of Loatheb's victim, and those within
        // SPORE_STACK_RADIUS of the ranged spot, for the diag.
        void UpdateSpores(Map* map, Creature* loatheb, Roster& r, uint32 now)
        {
            Unit* const victim = loatheb->GetVictim();
            auto live = std::make_shared<std::vector<SporeSeen>>();
            if (r.spores)
            {
                for (SporeSeen const& s : *r.spores)
                {
                    Creature* c = map->GetCreature(ObjectGuid(s.guid));
                    if (c && c->IsInWorld() && c->IsAlive())
                        live->push_back(s);
                    else
                        ++r.sporeKills;
                }
            }
            std::list<Creature*> found;
            loatheb->GetCreatureListWithEntryInGrid(found, SPORE_ENTRY, SPORE_SEARCH_RADIUS);
            for (Creature* c : found)
            {
                if (!c || !c->IsInWorld() || !c->IsAlive())
                    continue;
                uint64_t const raw = c->GetGUID().GetRawValue();
                if (std::none_of(live->begin(), live->end(), [raw](SporeSeen const& s) { return s.guid == raw; }))
                    live->push_back(SporeSeen{raw, now});
            }
            r.sporesNearTank = 0;
            r.sporesAtStack = 0;
            for (SporeSeen const& s : *live)
            {
                Creature* c = map->GetCreature(ObjectGuid(s.guid));
                if (!c)
                    continue;
                if (SporeNearVictim(VictimDist(c, victim)))
                    ++r.sporesNearTank;
                if (DistToRangeSpot(c->GetPositionX(), c->GetPositionY()) <= SPORE_STACK_RADIUS)
                    ++r.sporesAtStack;
            }
            r.spores = std::move(live);
        }

        // Counters the bots write (State), copied out under the lock for the diag line.
        struct DiagCounters
        {
            uint32 tankCasts = 0;
            uint32 emergencyCasts = 0;
            uint32 lastTankHealMs = 0;  // 0 = no rotation heal yet
            uint32 pots = 0;
            uint32 stones = 0;
            uint32 bandages = 0;
            uint32 raidHeals[3] = {0, 0, 0};  // prayer / chain / big
            uint32 topUps = 0;
        };

        std::string AvgManaPct(Map const* map, std::vector<uint64_t> const& guids)
        {
            uint32 sum = 0;
            uint32 n = 0;
            for (uint64_t raw : guids)
            {
                Player* p = FindPlayer(map, raw);
                uint32 const maxMana = p ? p->GetMaxPower(POWER_MANA) : 0;
                if (!p || !p->IsAlive() || !maxMana)
                    continue;
                sum += 100 * p->GetPower(POWER_MANA) / maxMana;
                ++n;
            }
            return n ? Acore::StringFormat("{}%", sum / n) : std::string("-");
        }

        uint32 MsToDoomResolve(Roster const& r, uint32 now)
        {
            if (!r.doomUp)
                return UINT32_MAX;
            uint32 const elapsed = getMSTimeDiff(r.doomSeenMs, now);
            return elapsed >= r.doomLeftMs ? 0 : r.doomLeftMs - elapsed;
        }

        // Loatheb's victim when it is a player, tank or not (spec §4.2: the heal target even if threat was neglected).
        Player* MainTankOf(Creature* loatheb)
        {
            Unit* victim = loatheb ? loatheb->GetVictim() : nullptr;
            return victim ? victim->ToPlayer() : nullptr;
        }

        // next = seconds until the next Doom resolves: the ticking one's remaining time, else "~" + the estimate from
        // the last resolve (MsToNextDoomResolve); "-" before the first. since = ms since the last rotation heal.
        void LogDiag(Map const* map, Creature* loatheb, Roster const& r, DiagCounters const& c, uint32 now)
        {
            uint32 const ticking = MsToDoomResolve(r, now);
            uint32 const sinceResolve = r.lastResolveMs ? getMSTimeDiff(r.lastResolveMs, now) : UINT32_MAX;
            uint32 const toNext = MsToNextDoomResolve(ticking, sinceResolve, r.doomCount);
            std::string const next = toNext == UINT32_MAX
                                         ? std::string("-")
                                         : Acore::StringFormat("{}{:.1f}s", ticking == UINT32_MAX ? "~" : "",
                                                               toNext / 1000.0f);
            Player* tank = MainTankOf(loatheb);
            std::string const tankStr =
                tank ? Acore::StringFormat("{}:{}%{}", tank->GetName(), uint32(tank->GetHealthPct()),
                                           PlayerbotAI::IsTank(tank) ? "" : "(not tank)")
                     : "none";
            size_t const rotSize = r.plan->rotation.size();
            std::string const since = c.lastTankHealMs ? Acore::StringFormat("{}", getMSTimeDiff(c.lastTankHealMs, now))
                                                       : std::string("-");
            LOG_DEBUG("playerbots",
                      "[Naxx40Loatheb] diag boss={}% doom={} next={} tank={} rot={}/{} since={} raidh={} casts={} "
                      "emerg={} rheals={}/{}/{} topup={} spores={} nearTank={} atStack={} pots={} stones={} "
                      "bandages={} mana=rot:{}/raid:{}",
                      uint32(loatheb->GetHealthPct()), uint32(r.doomCount), next, tankStr, rotSize,
                      RotationSpacingMs(rotSize), since, r.plan->group.size(), c.tankCasts, c.emergencyCasts,
                      c.raidHeals[0], c.raidHeals[1], c.raidHeals[2], c.topUps, r.sporeKills, r.sporesNearTank,
                      r.sporesAtStack, c.pots, c.stones, c.bandages, AvgManaPct(map, r.plan->rotation),
                      AvgManaPct(map, r.plan->group));
        }

        void FillView(Map* map, ObjectGuid loathebGuid, Roster const& r, uint32 now, LoathebView& view)
        {
            Creature* c = loathebGuid.IsEmpty() ? nullptr : map->GetCreature(loathebGuid);
            view.loatheb = c && c->IsAlive() ? c : nullptr;
            view.mainTank = MainTankOf(view.loatheb);
            view.mainTankIsTank = view.mainTank && PlayerbotAI::IsTank(view.mainTank);
            view.plan = r.plan;
            view.doomCount = r.doomCount;
            view.msSinceDoomResolved = r.lastResolveMs ? getMSTimeDiff(r.lastResolveMs, now) : UINT32_MAX;
            view.msToDoomResolve = MsToDoomResolve(r, now);
            view.spores = r.spores;
        }

        // Spec §4.9 item 1: this bot's top-up cast was refused during the current Doom (ReleaseTankTopUp). Under g_mutex.
        bool TopUpRefused(State const& st, uint64_t me)
        {
            auto it = st.topUpRefusedDoom.find(me);
            return it != st.topUpRefusedDoom.end() && it->second == st.roster.doomCount;
        }

        // The latched state of the bot's instance, under g_mutex (the caller holds it); null when none is latched.
        State* LatchedState(Player* bot)
        {
            auto it = g_stateByInstance.find(bot->GetMap()->GetInstanceId());
            if (it == g_stateByInstance.end() || !it->second.roster.latched)
                return nullptr;
            return &it->second;
        }

        int32 RotationIndex(Player* bot, LoathebView const& view)
        {
            if (!bot || !view.plan)
                return -1;
            uint64_t const me = bot->GetGUID().GetRawValue();
            auto const& rot = view.plan->rotation;
            for (size_t i = 0; i < rot.size(); ++i)
                if (rot[i] == me)
                    return int32(i);
            return -1;
        }

        // The real mana cost of the slot's big heal (CanCastSpell never sees mana). False for a non-bot or a slot
        // that does not know the spell.
        bool HasManaForBigHeal(Player* p)
        {
            PlayerbotAI* ai = GET_PLAYERBOT_AI(p);
            char const* const name = BigHealSpellName(p);
            if (!ai || !name)
                return false;
            uint32 const spellId = ai->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
            SpellInfo const* const info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
            if (!info)
                return false;
            int32 const cost = info->PowerType == POWER_MANA ? info->CalcPowerCost(p, info->GetSchoolMask()) : 0;
            return int32(p->GetPower(POWER_MANA)) >= cost;
        }
    }

    bool GetView(Player* bot, LoathebView& view)
    {
        view = LoathebView{};

        if (!bot || !bot->IsInWorld() || bot->GetMapId() != NAXX40_MAP_ID)
            return false;
        Map* const map = bot->GetMap();
        uint32 const instanceId = map->GetInstanceId();
        // IP's Naxx40 only: WotLK Naxx (10/25 normal) keeps upstream's Loatheb untouched.
        if (map->GetDifficulty() != RAID_DIFFICULTY_10MAN_HEROIC)
        {
            EraseState(instanceId);
            return false;
        }
        InstanceScript* const instance = bot->GetInstanceScript();
        if (!instance)
            return false;

        // Not IN_PROGRESS (before the pull, a wipe, the kill): drop the attempt so the next pull re-latches.
        if (instance->GetBossState(BOSS_STATE_ID) != IN_PROGRESS)
        {
            EraseState(instanceId);
            return false;
        }

        uint32 const now = NowMs();
        Roster r;
        bool fresh = false;
        Roster snap;  // the fresh path's view fields only (the spore list is a shared pointer, never copied)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_stateByInstance.find(instanceId);
            if (it != g_stateByInstance.end())
            {
                Roster const& cur = it->second.roster;
                uint32 const age = cur.refreshMs ? getMSTimeDiff(cur.refreshMs, now) : 0;
                if (cur.refreshMs && age >= STALE_STATE_MS)
                    it->second = State{};  // a new attempt: r stays default, so it latches from scratch
                else if (cur.refreshMs && age < REFRESH_INTERVAL_MS)
                {
                    if (!cur.latched)
                        return false;  // cached "no living Loatheb" (or not latched yet)
                    // No refresh due: take only what the view needs, never a full Roster copy.
                    fresh = true;
                    snap.loathebGuid = cur.loathebGuid;
                    snap.plan = cur.plan;
                    snap.doomUp = cur.doomUp;
                    snap.doomSeenMs = cur.doomSeenMs;
                    snap.doomLeftMs = cur.doomLeftMs;
                    snap.lastResolveMs = cur.lastResolveMs;
                    snap.doomCount = cur.doomCount;
                    snap.spores = cur.spores;
                }
                else
                    r = cur;  // a refresh is due: copy what it needs
            }
        }
        if (fresh)
        {
            FillView(map, snap.loathebGuid, snap, now, view);
            if (view.loatheb)
                return true;
            view = LoathebView{};
            // He died: refresh now (which resets the state). Needs the stored roster.
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_stateByInstance.find(instanceId);
            if (it != g_stateByInstance.end())
                r = it->second.roster;
        }

        // The scan and every computation run outside the lock (see Roster).
        Creature* const loatheb = ResolveLoatheb(map, r);
        if (!loatheb || !loatheb->IsAlive())
        {
            ResetState(instanceId, now);
            return false;
        }

        r.refreshMs = now;
        // An ungrouped bot would latch a one-member raid: it never latches or refreshes the shared clock, and reads
        // the last refreshed state, if any. Its scan is still stored, so it does not rescan on every call.
        if (Group* group = bot->GetGroup())
        {
            RaidScan const scan = ScanRaid(bot, group);
            if (!r.latched)
            {
                r.plan = std::make_shared<HealerPlan const>(AssignHealers(scan.healers));
                r.latched = true;
            }
            UpdateDoom(r, scan, now);
            UpdateSpores(map, loatheb, r, now);
        }
        bool const diagDue = r.latched && (!r.diagMs || getMSTimeDiff(r.diagMs, now) >= DIAG_INTERVAL_MS);
        if (diagDue)
            r.diagMs = now;

        DiagCounters counters;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            State& st = g_stateByInstance[instanceId];
            st.roster = r;  // keeps the rotation clock, the claims and the counters
            counters = DiagCounters{st.tankCasts,
                                    st.emergencyCasts,
                                    st.lastTankHealMs,
                                    st.pots,
                                    st.stones,
                                    st.bandages,
                                    {st.raidHeals[0], st.raidHeals[1], st.raidHeals[2]},
                                    st.topUps};
            g_stateCount.store(uint32(g_stateByInstance.size()), std::memory_order_relaxed);
        }

        if (!r.latched)
            return false;
        if (diagDue && sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
            LogDiag(map, loatheb, r, counters, now);

        FillView(map, r.loathebGuid, r, now, view);
        return view.loatheb != nullptr;
    }

    HealerJob MyHealerJob(Player* bot, LoathebView const& view)
    {
        if (!bot || !view.plan)
            return HealerJob::None;
        uint64_t const me = bot->GetGUID().GetRawValue();
        for (uint64_t g : view.plan->rotation)
            if (g == me)
                return HealerJob::Rotation;
        for (uint64_t g : view.plan->group)
            if (g == me)
                return HealerJob::Group;
        return HealerJob::None;
    }

    int32_t LockoutRemainingMs(Player* bot)
    {
        if (!bot)
            return -1;
        int32_t best = -1;
        for (uint32 spell : CM_LOCKOUT)
        {
            Aura* aura = bot->GetAura(spell);
            if (!aura)
                continue;
            // A negative duration is a permanent aura: treat it as a full lockout.
            int32 const left = aura->GetDuration() < 0 ? int32(HEAL_LOCKOUT_MS) : aura->GetDuration();
            best = std::max<int32_t>(best, left);
        }
        return best;
    }

    char const* BigHealSpellName(Player* bot)
    {
        if (!bot)
            return nullptr;
        switch (bot->getClass())
        {
            case CLASS_PRIEST:
                return "greater heal";
            case CLASS_PALADIN:
                return "holy light";
            case CLASS_DRUID:
                return "healing touch";
            case CLASS_SHAMAN:
                return "healing wave";
            default:
                return nullptr;
        }
    }

    bool RotationMayCast(Player* bot, LoathebView const& view, uint32_t& msUntilDue)
    {
        msUntilDue = UINT32_MAX;
        int32 const myIdx = RotationIndex(bot, view);
        if (myIdx < 0 || !bot->IsInWorld())
            return false;
        auto const& rot = view.plan->rotation;
        size_t const n = rot.size();
        size_t const me = size_t(myIdx);

        size_t lastCaster = SIZE_MAX;
        uint32 lastTankHealMs = 0;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            State const* st = LatchedState(bot);
            if (!st)
                return false;
            lastCaster = st->lastCaster;
            lastTankHealMs = st->lastTankHealMs;
        }
        // No heal yet: the slot after the "last caster" is slot 0, so the rotation opens at the top of the list.
        size_t const last = lastCaster < n ? lastCaster : n - 1;
        uint32 const now = NowMs();
        uint32 const since = lastTankHealMs ? getMSTimeDiff(lastTankHealMs, now) : UINT32_MAX;
        uint32 const spacing = RotationSpacingMs(n);

        Map const* map = bot->GetMap();
        std::vector<bool> ready(n, false);
        std::vector<bool> alive(n, false);
        for (size_t i = 0; i < n; ++i)
        {
            Player* p = FindPlayer(map, rot[i]);
            alive[i] = p && p->IsAlive();
            ready[i] = alive[i] && LockoutRemainingMs(p) < 0 && HasManaForBigHeal(p);
        }

        size_t const up = RotationUp(last, ready, since, spacing);
        float const tankPct = view.mainTank ? view.mainTank->GetHealthPct() : 100.0f;
        bool const mayCast = view.mainTank && up == me && ready[me] && RotationCastNow(since, spacing, tankPct);
        if (mayCast)
        {
            msUntilDue = 0;
            return true;
        }

        // Estimate: one spacing per living slot from the slot after the last caster up to this bot (a dead slot is
        // skipped once the spacing has passed), less the time since the last heal; before the first heal slot 0 is
        // due now. Never below this bot's own lockout.
        uint32 steps = 0;
        for (size_t i = 1; i <= n; ++i)
        {
            size_t const idx = (last + i) % n;
            if (alive[idx])
                ++steps;
            if (idx == me)
                break;
        }
        steps = std::max<uint32>(steps, 1);
        uint64_t base = uint64_t(steps) * spacing;
        if (!lastTankHealMs)
            base -= spacing;
        uint64_t const elapsed = lastTankHealMs ? std::min<uint64_t>(since, base) : 0;
        uint64_t due = base - elapsed;
        int32_t const lockout = LockoutRemainingMs(bot);
        if (lockout > 0)
            due = std::max<uint64_t>(due, uint64_t(lockout));
        msUntilDue = uint32_t(std::min<uint64_t>(due, UINT32_MAX - 1));
        return false;
    }

    void NoteRotationHeal(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return;
        uint64_t const me = bot->GetGUID().GetRawValue();
        uint32 const now = NowMs();
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        if (!st || !st->roster.plan)
            return;
        auto const& rot = st->roster.plan->rotation;
        for (size_t i = 0; i < rot.size(); ++i)
        {
            if (rot[i] != me)
                continue;
            st->lastCaster = i;
            st->lastTankHealMs = now;
            return;
        }
    }

    void NoteTankHealCast(Player* bot, bool emergency)
    {
        if (!bot || !bot->IsInWorld())
            return;
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        if (!st)
            return;
        ++st->tankCasts;
        if (emergency)
            ++st->emergencyCasts;
    }

    void NoteRaidHealCast(Player* bot, RaidHeal kind, bool topUp)
    {
        if (!bot || !bot->IsInWorld() || kind == RaidHeal::None)
            return;
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        if (!st)
            return;
        ++st->raidHeals[kind == RaidHeal::Prayer ? 0 : kind == RaidHeal::Chain ? 1 : 2];
        if (topUp)
            ++st->topUps;
    }

    bool TankTopUpClaimed(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        uint64_t const me = bot->GetGUID().GetRawValue();
        std::lock_guard<std::mutex> lock(g_mutex);
        State const* st = LatchedState(bot);
        if (!st)
            return false;
        if (TopUpRefused(*st, me))
            return true;
        PartyClaim const& c = st->topUpClaim;
        return c.claimant && c.claimant != me && c.doom == st->roster.doomCount;
    }

    bool ClaimTankTopUp(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        uint64_t const me = bot->GetGUID().GetRawValue();
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        if (!st)
            return false;
        if (TopUpRefused(*st, me))
            return false;
        PartyClaim& c = st->topUpClaim;
        if (c.claimant && c.claimant != me && c.doom == st->roster.doomCount)
            return false;
        c.claimant = me;
        c.doom = st->roster.doomCount;
        return true;
    }

    void ReleaseTankTopUp(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return;
        uint64_t const me = bot->GetGUID().GetRawValue();
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        if (!st)
            return;
        st->topUpRefusedDoom[me] = st->roster.doomCount;
        if (st->topUpClaim.claimant == me && st->topUpClaim.doom == st->roster.doomCount)
            st->topUpClaim = PartyClaim{};
    }

    bool PartyHealClaimed(Player* bot, uint8_t subgroup)
    {
        if (!bot || !bot->IsInWorld() || subgroup >= MAX_SUBGROUPS)
            return false;
        uint64_t const me = bot->GetGUID().GetRawValue();
        std::lock_guard<std::mutex> lock(g_mutex);
        State const* st = LatchedState(bot);
        if (!st)
            return false;
        PartyClaim const& c = st->partyClaim[subgroup];
        return c.claimant && c.claimant != me && c.doom == st->roster.doomCount;
    }

    bool ClaimPartyHeal(Player* bot, uint8_t subgroup)
    {
        if (!bot || !bot->IsInWorld() || subgroup >= MAX_SUBGROUPS)
            return false;
        uint64_t const me = bot->GetGUID().GetRawValue();
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        if (!st)
            return false;
        PartyClaim& c = st->partyClaim[subgroup];
        if (c.claimant && c.claimant != me && c.doom == st->roster.doomCount)
            return false;
        c.claimant = me;
        c.doom = st->roster.doomCount;
        return true;
    }

    Creature* EngageSpore(Player* bot, LoathebView const& view, Unit* engaged)
    {
        if (!bot || !bot->IsInWorld() || !view.loatheb)
            return nullptr;
        Unit* const victim = view.loatheb->GetVictim();
        uint32 const now = NowMs();
        // A spore's age from the refresh's first-seen time; 0 (not a stray) when the list has not seen it yet.
        auto ageOf = [&view, now](uint64_t raw) -> uint32
        {
            if (!view.spores)
                return 0;
            for (SporeSeen const& s : *view.spores)
                if (s.guid == raw)
                    return getMSTimeDiff(s.firstSeenMs, now);
            return 0;
        };
        auto engages = [victim](Creature* c, float toBot, uint32 ageMs, bool isEngaged)
        {
            return SporeEngages(toBot, DistToRangeSpot(c->GetPositionX(), c->GetPositionY()), ageMs,
                                VictimDist(c, victim), isEngaged);
        };
        // Keep the spore the bot is on while it still qualifies (the wider keep rule).
        Creature* const current = engaged ? engaged->ToCreature() : nullptr;
        if (current && current->GetEntry() == SPORE_ENTRY && current->IsInWorld() && current->IsAlive() &&
            current->GetMap() == bot->GetMap() &&
            engages(current, bot->GetExactDist2d(current), ageOf(current->GetGUID().GetRawValue()), true))
            return current;
        if (!view.spores)
            return nullptr;
        Map* const map = bot->GetMap();
        Creature* best = nullptr;
        float bestDist = std::numeric_limits<float>::max();
        for (SporeSeen const& s : *view.spores)
        {
            Creature* c = map->GetCreature(ObjectGuid(s.guid));
            if (!c || !c->IsInWorld() || !c->IsAlive())
                continue;
            float const d = bot->GetExactDist2d(c);
            if (d < bestDist && engages(c, d, getMSTimeDiff(s.firstSeenMs, now), false))
            {
                best = c;
                bestDist = d;
            }
        }
        return best;
    }

    bool ClaimManaRefill(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        return st && st->manaRefilled.insert(bot->GetGUID().GetRawValue()).second;
    }

    bool ClaimHealthstone(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        return st && st->healthstone.insert(bot->GetGUID().GetRawValue()).second;
    }

    bool HealthstoneClaimed(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        std::lock_guard<std::mutex> lock(g_mutex);
        State const* st = LatchedState(bot);
        return st && st->healthstone.count(bot->GetGUID().GetRawValue());
    }

    bool ClaimStoneNote(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        if (!st)
            return false;
        auto [it, inserted] = st->stoneNoteDoom.try_emplace(bot->GetGUID().GetRawValue(), st->roster.doomCount);
        if (inserted)
            return true;
        if (it->second == st->roster.doomCount)
            return false;
        it->second = st->roster.doomCount;
        return true;
    }

    void NoteConsumable(Player* bot, Consumable c)
    {
        if (!bot || !bot->IsInWorld())
            return;
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        if (!st)
            return;
        switch (c)
        {
            case Consumable::Potion:
                ++st->pots;
                st->lastPotionMs[bot->GetGUID().GetRawValue()] = NowMs();
                break;
            case Consumable::Healthstone:
                ++st->stones;
                break;
            case Consumable::Bandage:
                ++st->bandages;
                break;
            default:
                break;
        }
    }

    uint32_t MsSinceLastPotion(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return UINT32_MAX;
        uint32 const now = NowMs();
        std::lock_guard<std::mutex> lock(g_mutex);
        State const* st = LatchedState(bot);
        if (!st)
            return UINT32_MAX;
        auto it = st->lastPotionMs.find(bot->GetGUID().GetRawValue());
        return it == st->lastPotionMs.end() ? UINT32_MAX : getMSTimeDiff(it->second, now);
    }

    bool ClaimEmergencyHeal(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        uint32 const now = NowMs();
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        if (!st || (st->lastEmergencyMs && getMSTimeDiff(st->lastEmergencyMs, now) < ROTATION_MIN_SPACING_MS))
            return false;
        st->lastEmergencyMs = now;
        return true;
    }
}
