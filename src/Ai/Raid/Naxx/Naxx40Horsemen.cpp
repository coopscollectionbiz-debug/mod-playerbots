/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Four Horsemen helper layer (overlay patch 0022). Spec (overlay repo):
// docs/superpowers/specs/2026-10-27-playerbot-naxx40-four-horsemen-design.md §4, §6.

#include "Naxx40Horsemen.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Config.h"
#include "Creature.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "Naxx40HorsemenStations.h"
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

namespace Naxx40Horsemen
{
    using namespace Naxx40HorsemenRules;

    namespace
    {
        constexpr uint32 REFRESH_INTERVAL_MS = 500;  // shared-state refresh + entry scan reuse window
        // An entry not refreshed for this long is a new attempt (whoever calls GetView after a wipe, or a reused
        // instance id): it is reset and re-latched from scratch.
        constexpr uint32 STALE_STATE_MS = 10000;

        // Per-instance encounter state. Bot AI of different instances runs on parallel map threads, so the
        // container is only touched under g_mutex (memory: module globals need locking), held briefly; callers get
        // copies (the Assignment as a shared immutable pointer), never references into it. One instance's bots
        // share one map thread, so a refresh of an instance (copy out, compute outside the lock, write back) can
        // never interleave with another refresh of it.
        struct Roster
        {
            ObjectGuid horseGuid[4];  // empty = not resolved yet
            bool seen[4]{};           // resolved at least once this attempt
            bool gone[4]{};           // seen, then missing from the map (corpse removed): dead, stop rescanning
            bool deadHandled[4]{};    // a dead horseman's tanks were moved to the free pool
            bool flipped[4]{};        // this refresh swapped the pair (UpdatePairs); write-back resets its whisper edge
            bool latched = false;     // Assign ran
            // Pairs + healer groups + DPS halves; spareTanks = the free pool. Immutable once published: a refresh
            // builds a new Assignment and swaps the pointer, so views share it without copying.
            std::shared_ptr<Assignment const> assign;
            std::vector<uint64_t> benched;  // tanks off the map or dead; back to the free pool when alive again
            uint32 refreshMs = 0;     // 0 = never refreshed
            uint32 diagMs = 0;        // last "[Naxx40Horsemen] diag" line; 0 = never
        };

        // Last whisper edge sent to a human resting tank, for the active tank it was sent under (ClaimWhisperEdge).
        struct WhisperEdge
        {
            uint64_t active = 0;
            uint8_t edge = 0;
        };

        // Bot-written parts live here, beside the roster, so a refresh (which writes back only the roster) never
        // overwrites them.
        struct State
        {
            Roster roster;
            std::unordered_map<uint64_t, Horse> current;  // per-bot current horseman (SetCurrentHorse)
            std::unordered_set<uint64_t> moved;           // bots that have left their first horseman (HasMoved)
            WhisperEdge whisper[4];                       // by Horse
            uint8_t blauCircle = 0;                       // Blaumeux tank point index (SetBlauCircleIndex)
            std::unordered_set<uint64_t> manaRefilled;    // healers given their one refill (ClaimManaRefill)
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

        // Gate failed after a scan (no live Naxx40 horseman: e.g. WotLK Naxx with its own Horsemen in progress).
        // Drops the attempt like EraseState, but keeps the scan timestamp so the map-wide scan runs at most once per
        // REFRESH_INTERVAL_MS instead of on every call (the "not found" result is cached too, as in BWLHelpers).
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

        bool TankAlive(Map const* map, uint64_t raw)
        {
            Player* p = FindPlayer(map, raw);
            return p && p->IsAlive();
        }

        void PushUnique(std::vector<uint64_t>& v, uint64_t raw)
        {
            if (!raw)
                return;
            for (uint64_t g : v)
                if (g == raw)
                    return;
            v.push_back(raw);
        }

        // Resolve the four horsemen. Cached GUIDs go through Map::GetCreature; a missing one triggers ONE map-wide
        // pass over the spawn store (all four entries at once; no grid-search radius clamp). The caller limits this
        // to once per REFRESH_INTERVAL_MS per instance.
        void ResolveHorsemen(Map* map, Roster& r, Creature* out[4])
        {
            bool needScan = false;
            for (uint8 h = 0; h < 4; ++h)
            {
                out[h] = r.horseGuid[h].IsEmpty() ? nullptr : map->GetCreature(r.horseGuid[h]);
                if (!out[h] && !r.gone[h])
                    needScan = true;
            }
            if (!needScan)
                return;

            for (auto const& entry : map->GetCreatureBySpawnIdStore())
            {
                Creature* c = entry.second;
                if (!c || !c->IsInWorld())
                    continue;
                Horse const h = HorseOfEntry(c->GetEntry());
                if (h == Horse::None || out[Idx(h)])
                    continue;
                out[Idx(h)] = c;
            }
            for (uint8 h = 0; h < 4; ++h)
            {
                if (out[h])
                {
                    r.horseGuid[h] = out[h]->GetGUID();
                    r.seen[h] = true;
                }
                else if (r.seen[h])
                    r.gone[h] = true;  // its corpse left the map: it stays dead for this attempt
            }
        }

        // Raid members on the bot's map, as Assign input. Humans are classified the same way as bots.
        std::vector<Member> BuildMembers(Player* bot)
        {
            std::vector<Member> members;
            Group* group = bot->GetGroup();
            if (!group)
                return members;
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* p = ref->GetSource();
                if (!p || !p->IsInWorld() || p->GetMap() != bot->GetMap() || !p->IsAlive())
                    continue;
                Role const role = PlayerbotAI::IsTank(p) ? Role::Tank
                                  : PlayerbotAI::IsHeal(p) ? Role::Healer
                                                           : Role::Dps;
                members.push_back(Member{p->GetGUID().GetRawValue(), role, !PlayerbotAI::IsRanged(p)});
            }
            return members;
        }

        // Works on a copy of the published Assignment (once per refresh, never per view) and publishes the result.
        void UpdatePairs(Map const* map, Roster& r, Creature* const horse[4], std::array<bool, 4> const& alive)
        {
            Assignment a = *r.assign;

            // A pair flips when its horseman is on the resting tank (the taunt landed). A flip is a new turn: the
            // write-back in GetView resets that pair's whisper edge (ClaimWhisperEdge), so a bot that rested through
            // a human's turn whispers again on its own next turn.
            for (uint8 h = 0; h < 4; ++h)
            {
                r.flipped[h] = false;
                if (!alive[h] || !a.resting[h])
                    continue;
                Unit* victim = horse[h]->GetVictim();
                // Not to a resting tank still carrying 3+ of his marks (TakesOver): it got him back on old threat the
                // marks never halved, and the active tank taunts him back.
                if (victim && victim->GetGUID().GetRawValue() == a.resting[h] &&
                    TakesOver(MarkStacks(FindPlayer(map, a.resting[h]), static_cast<Horse>(h))))
                {
                    std::swap(a.active[h], a.resting[h]);
                    r.flipped[h] = true;
                }
            }

            // A dead horseman's tanks go to the free pool (once).
            for (uint8 h = 0; h < 4; ++h)
            {
                if (alive[h] || r.deadHandled[h] || !r.seen[h])
                    continue;
                PushUnique(a.spareTanks, a.active[h]);
                PushUnique(a.spareTanks, a.resting[h]);
                a.active[h] = 0;
                a.resting[h] = 0;
                r.deadHandled[h] = true;
            }

            // Dead / absent free tanks to the bench; benched tanks that are alive again back to the free pool.
            std::vector<uint64_t> freeAlive;
            for (uint64_t g : a.spareTanks)
            {
                if (TankAlive(map, g))
                    freeAlive.push_back(g);
                else
                    PushUnique(r.benched, g);
            }
            std::vector<uint64_t> stillBenched;
            for (uint64_t g : r.benched)
            {
                bool inFree = false;
                for (uint64_t f : freeAlive)
                    inFree = inFree || f == g;
                if (inFree)
                    continue;
                if (TankAlive(map, g))
                    freeAlive.push_back(g);
                else
                    stillBenched.push_back(g);
            }
            a.spareTanks = std::move(freeAlive);
            r.benched = std::move(stillBenched);

            // RefillPairs contract: the caller clears a dead RESTING tank (bench it, never the free pool) and benches
            // a dead active one before it is overwritten by the promotion.
            std::array<bool, 4> activeAlive{};
            for (uint8 h = 0; h < 4; ++h)
            {
                if (!alive[h])
                    continue;
                if (a.resting[h] && !TankAlive(map, a.resting[h]))
                {
                    PushUnique(r.benched, a.resting[h]);
                    a.resting[h] = 0;
                }
                activeAlive[h] = a.active[h] && TankAlive(map, a.active[h]);
                if (!activeAlive[h] && a.active[h])
                    PushUnique(r.benched, a.active[h]);
            }
            // A resting tank still carrying 3+ of his horseman's marks is never promoted (RefillPairs).
            auto const restingStacks = [&]()
            {
                std::array<uint8_t, 4> s{};
                for (uint8 h = 0; h < 4; ++h)
                    s[h] = MarkStacks(FindPlayer(map, a.resting[h]), static_cast<Horse>(h));
                return s;
            };
            uint64_t* active = a.active;
            uint64_t* resting = a.resting;
            RefillPairs(active, resting, a.spareTanks, alive, activeAlive, restingStacks());
            // Both slots were empty: the first pass only filled resting. Promote it now rather than a refresh later.
            for (uint8 h = 0; h < 4; ++h)
                activeAlive[h] = a.active[h] != 0;
            RefillPairs(active, resting, a.spareTanks, alive, activeAlive, restingStacks());
            r.assign = std::make_shared<Assignment const>(std::move(a));
        }

        std::string NameOf(Map const* map, uint64_t raw)
        {
            if (!raw)
                return "-";
            Player* p = FindPlayer(map, raw);
            return p ? p->GetName() : "?";
        }

        void LogDiag(Map const* map, Roster const& r, Creature* const horse[4], std::array<bool, 4> const& alive)
        {
            static char const* const TAG[4] = {"K", "M", "B", "Z"};
            std::string line = Acore::StringFormat("[Naxx40Horsemen] diag instance {} alive {}{}{}{}",
                                                   map->GetInstanceId(), alive[0] ? 1 : 0, alive[1] ? 1 : 0,
                                                   alive[2] ? 1 : 0, alive[3] ? 1 : 0);
            for (uint8 h = 0; h < 4; ++h)
            {
                Horse const hh = static_cast<Horse>(h);
                Player* act = FindPlayer(map, r.assign->active[h]);
                Player* rest = FindPlayer(map, r.assign->resting[h]);
                Unit* victim = alive[h] ? horse[h]->GetVictim() : nullptr;
                // Distances to the horseman (resting tanks kept gaining his Mark, 4-6 stacks at rest).
                line += Acore::StringFormat(" | {} {}% act {}({}@{:.0f}) rest {}({}@{:.0f}) vic {}", TAG[h],
                                            alive[h] ? uint32(horse[h]->GetHealthPct()) : 0,
                                            NameOf(map, r.assign->active[h]), act ? MarkStacks(act, hh) : 0,
                                            act && alive[h] ? horse[h]->GetDistance(act) : -1.0f,
                                            NameOf(map, r.assign->resting[h]), rest ? MarkStacks(rest, hh) : 0,
                                            rest && alive[h] ? horse[h]->GetDistance(rest) : -1.0f,
                                            victim ? victim->GetName() : "none");
            }
            line += Acore::StringFormat(" | free {} benched {}", r.assign->spareTanks.size(), r.benched.size());
            // Healer mana % by group (pull 8: the front healers were seen running dry), x = dead / not in world.
            line += " | mana";
            for (uint8 g = 0; g < 5; ++g)
            {
                line += Acore::StringFormat(" g{}", g);
                for (uint64_t raw : r.assign->healers[g])
                {
                    Player* p = FindPlayer(map, raw);
                    uint32 const maxMana = p ? p->GetMaxPower(POWER_MANA) : 0;
                    if (!p || !p->IsAlive() || !maxMana)
                        line += " x";
                    else
                        line += Acore::StringFormat(" {}", 100 * p->GetPower(POWER_MANA) / maxMana);
                }
            }
            LOG_DEBUG("playerbots", "{}", line);
        }

        void FillView(Map* map, ObjectGuid const horseGuid[4], std::shared_ptr<Assignment const> assign,
                      HorsemenView& view)
        {
            for (uint8 h = 0; h < 4; ++h)
            {
                Creature* c = horseGuid[h].IsEmpty() ? nullptr : map->GetCreature(horseGuid[h]);
                view.horse[h] = c && c->IsAlive() ? c : nullptr;
                view.alive[h] = view.horse[h] != nullptr;
            }
            view.assign = std::move(assign);
        }

        bool AnyAlive(HorsemenView const& view)
        {
            return view.alive[0] || view.alive[1] || view.alive[2] || view.alive[3];
        }
    }

    bool GetView(Player* bot, HorsemenView& view)
    {
        view = HorsemenView{};

        // Inert until the stations are authored in game (the AQ40 TWINS_STATIONS_PROVISIONAL pattern): every
        // trigger, action and multiplier keyed on the view is a no-op, so bots run stock behaviour.
        if constexpr (Naxx40HorsemenStations::PROVISIONAL)
            return false;

        if (!bot || !bot->IsInWorld() || bot->GetMapId() != NAXX40_MAP_ID)
            return false;
        InstanceScript* const instance = bot->GetInstanceScript();
        if (!instance)
            return false;
        Map* const map = bot->GetMap();
        uint32 const instanceId = map->GetInstanceId();

        // Not IN_PROGRESS (before the pull, a wipe, the kill): drop the attempt so the next pull re-latches.
        if (instance->GetBossState(BOSS_STATE_ID) != IN_PROGRESS)
        {
            EraseState(instanceId);
            return false;
        }

        uint32 const now = NowMs();
        Roster r;
        bool fresh = false;
        ObjectGuid freshGuid[4];
        std::shared_ptr<Assignment const> freshAssign;
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
                        return false;  // cached "no live Naxx40 horseman" (or not latched yet)
                    // No refresh due: take only the GUIDs and the shared Assignment, never a Roster copy.
                    fresh = true;
                    for (uint8 h = 0; h < 4; ++h)
                        freshGuid[h] = cur.horseGuid[h];
                    freshAssign = cur.assign;
                }
                else
                    r = cur;  // a refresh is due: copy what it needs
            }
        }
        if (fresh)
        {
            FillView(map, freshGuid, std::move(freshAssign), view);
            if (AnyAlive(view))
                return true;
            view = HorsemenView{};
            // None alive any more: refresh now (which resets the state). Needs the stored roster.
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_stateByInstance.find(instanceId);
            if (it != g_stateByInstance.end())
                r = it->second.roster;
        }

        // The scan and every computation run outside the lock (see Roster).
        Creature* horse[4]{};
        ResolveHorsemen(map, r, horse);
        std::array<bool, 4> alive{};
        for (uint8 h = 0; h < 4; ++h)
            alive[h] = horse[h] && horse[h]->IsAlive();
        if (!alive[0] && !alive[1] && !alive[2] && !alive[3])
        {
            ResetState(instanceId, now);
            return false;
        }

        r.refreshMs = now;
        // An ungrouped bot would latch a one-member raid: it never latches or refreshes the pairs, and reads the
        // last refreshed state, if any. Its scan is still stored, so it does not rescan on every call.
        if (bot->GetGroup())
        {
            if (!r.latched)
            {
                r.assign = std::make_shared<Assignment const>(Assign(BuildMembers(bot)));
                r.latched = true;
            }
            UpdatePairs(map, r, horse, alive);
        }
        bool const diagDue = r.latched && (!r.diagMs || getMSTimeDiff(r.diagMs, now) >= DIAG_INTERVAL_MS);
        if (diagDue)
            r.diagMs = now;

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            State& st = g_stateByInstance[instanceId];
            for (uint8 h = 0; h < 4; ++h)
            {
                if (r.flipped[h])
                    st.whisper[h] = WhisperEdge{};
                r.flipped[h] = false;
            }
            st.roster = r;  // keeps the per-bot `current` map, the whisper edges and the Blaumeux index
            g_stateCount.store(uint32(g_stateByInstance.size()), std::memory_order_relaxed);
        }

        if (!r.latched)
            return false;
        if (diagDue && sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
            LogDiag(map, r, horse, alive);

        FillView(map, r.horseGuid, r.assign, view);
        return true;
    }

    uint8_t MarkStacks(Unit* who, Horse h)
    {
        if (!who || h == Horse::None)
            return 0;
        Aura* aura = who->GetAura(MARK_SPELL[Idx(h)]);
        return aura ? aura->GetStackAmount() : 0;
    }

    Horse CurrentHorse(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return Horse::None;
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_stateByInstance.find(instanceId);
        if (it == g_stateByInstance.end())
            return Horse::None;
        auto cur = it->second.current.find(bot->GetGUID().GetRawValue());
        return cur == it->second.current.end() ? Horse::None : cur->second;
    }

    void SetCurrentHorse(Player* bot, Horse h)
    {
        if (!bot || !bot->IsInWorld())
            return;
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_stateByInstance.find(instanceId);
        if (it == g_stateByInstance.end() || !it->second.roster.latched)
            return;  // no attempt in progress: never resurrect a dropped state
        uint64_t const me = bot->GetGUID().GetRawValue();
        auto cur = it->second.current.find(me);
        if (cur != it->second.current.end() && cur->second != Horse::None && cur->second != h)
            it->second.moved.insert(me);
        it->second.current[me] = h;
    }

    bool HasMoved(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_stateByInstance.find(instanceId);
        return it != g_stateByInstance.end() && it->second.moved.count(bot->GetGUID().GetRawValue());
    }

    uint8_t MyHealerSeat(Player* bot, HorsemenView const& view)
    {
        if (!bot || !view.assign)
            return 0xFF;
        uint64_t const me = bot->GetGUID().GetRawValue();
        for (uint8 g = 0; g < 5; ++g)
        {
            auto const& seats = view.assign->healers[g];
            for (size_t i = 0; i < seats.size(); ++i)
                if (seats[i] == me)
                    return uint8_t(i);
        }
        return 0xFF;
    }

    bool ClaimWhisperEdge(Player* bot, Horse h, uint64_t activeGuid, uint8_t edge)
    {
        if (!bot || !bot->IsInWorld() || h == Horse::None || !activeGuid)
            return false;
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_stateByInstance.find(instanceId);
        if (it == g_stateByInstance.end() || !it->second.roster.latched)
            return false;
        WhisperEdge& w = it->second.whisper[Idx(h)];
        if (w.active != activeGuid)
            w = WhisperEdge{activeGuid, 0};  // the pair flipped (or first swap): start over
        if (edge <= w.edge)
            return false;
        w.edge = edge;
        return true;
    }

    bool DoableMarks()
    {
        // IP reads the same key at config load; the module conf shares sConfigMgr. A restart applies a change.
        static bool const doable = sConfigMgr->GetOption<bool>("IndividualProgression.doableNaxx40Bosses_4H", false);
        return doable;
    }

    bool ClaimManaRefill(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_stateByInstance.find(instanceId);
        if (it == g_stateByInstance.end() || !it->second.roster.latched)
            return false;
        return it->second.manaRefilled.insert(bot->GetGUID().GetRawValue()).second;
    }

    uint8_t BlauCircleIndex(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return 0;
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_stateByInstance.find(instanceId);
        return it == g_stateByInstance.end() ? 0 : it->second.blauCircle;
    }

    void SetBlauCircleIndex(Player* bot, uint8_t idx)
    {
        if (!bot || !bot->IsInWorld())
            return;
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_stateByInstance.find(instanceId);
        if (it == g_stateByInstance.end() || !it->second.roster.latched)
            return;
        it->second.blauCircle = idx;
    }

    Player* ActiveTank(Player* bot, HorsemenView const& view, Horse h)
    {
        if (!bot || h == Horse::None)
            return nullptr;
        if (!view.assign)
            return nullptr;
        Player* p = FindPlayer(bot->GetMap(), view.assign->active[Idx(h)]);
        return p && p->IsAlive() ? p : nullptr;
    }

    Horse MyPair(Player* bot, HorsemenView const& view, bool& iAmActive)
    {
        iAmActive = false;
        if (!bot || !view.assign)
            return Horse::None;
        uint64_t const me = bot->GetGUID().GetRawValue();
        for (uint8 h = 0; h < 4; ++h)
        {
            if (view.assign->active[h] == me)
            {
                iAmActive = true;
                return static_cast<Horse>(h);
            }
            if (view.assign->resting[h] == me)
                return static_cast<Horse>(h);
        }
        return Horse::None;
    }

    bool IsSpareTank(Player* bot, HorsemenView const& view)
    {
        if (!bot || !view.assign)
            return false;
        uint64_t const me = bot->GetGUID().GetRawValue();
        for (uint64_t g : view.assign->spareTanks)
            if (g == me)
                return true;
        return false;
    }

    uint8_t MyHealerGroup(Player* bot, HorsemenView const& view)
    {
        if (!bot || !view.assign)
            return 0xFF;
        uint64_t const me = bot->GetGUID().GetRawValue();
        for (uint8 g = 0; g < 5; ++g)
            for (uint64_t guid : view.assign->healers[g])
                if (guid == me)
                    return g;
        return 0xFF;
    }

    uint8_t MyDpsSeat(Player* bot, HorsemenView const& view)
    {
        uint8_t const half = MyDpsHalf(bot, view);
        if (half == 0xFF)
            return 0xFF;
        uint64_t const me = bot->GetGUID().GetRawValue();
        bool const ranged = PlayerbotAI::IsRanged(bot);
        uint8_t n = 0;
        for (uint64_t guid : view.assign->dpsHalf[half])
        {
            if (guid == me)
                return n % 2;
            Player* const p = ObjectAccessor::GetPlayer(bot->GetMap(), ObjectGuid(guid));
            if (p && p->IsInWorld() && PlayerbotAI::IsRanged(p) == ranged)
                ++n;
        }
        return 0xFF;
    }

    uint8_t MyDpsHalf(Player* bot, HorsemenView const& view)
    {
        if (!bot || !view.assign)
            return 0xFF;
        uint64_t const me = bot->GetGUID().GetRawValue();
        for (uint8 half = 0; half < 2; ++half)
            for (uint64_t guid : view.assign->dpsHalf[half])
                if (guid == me)
                    return half;
        return 0xFF;
    }

    bool OtherHealerCovers(Player* bot, HorsemenView const& view, Horse h, Player* tank)
    {
        if (!bot || !tank || h == Horse::None || !view.assign)
            return false;
        uint64_t const me = bot->GetGUID().GetRawValue();
        std::vector<uint64_t> onHorse;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_stateByInstance.find(bot->GetMap()->GetInstanceId());
            if (it == g_stateByInstance.end())
                return false;
            for (uint8 g = 0; g < 5; ++g)
                for (uint64_t guid : view.assign->healers[g])
                {
                    if (guid == me)
                        continue;
                    auto cur = it->second.current.find(guid);
                    if (cur != it->second.current.end() && cur->second == h)
                        onHorse.push_back(guid);
                }
        }
        for (uint64_t guid : onHorse)
        {
            Player* p = FindPlayer(bot->GetMap(), guid);
            if (p && p->IsAlive() && !p->isMoving() && p->GetExactDist2d(tank) <= HEAL_REACH)
                return true;
        }
        return false;
    }

    namespace
    {
        // The Aq40 tank-heal pick (Aq40TankHealSpellName): each healer class's fast single-target heal, by name.
        char const* TankHealSpellName(Player* bot)
        {
            switch (bot->getClass())
            {
                case CLASS_PRIEST:
                    return "flash heal";
                case CLASS_PALADIN:
                    return "flash of light";
                case CLASS_SHAMAN:
                    return "lesser healing wave";
                case CLASS_DRUID:
                    return "regrowth";
                default:
                    return nullptr;
            }
        }
    }

    Player* TankHealTarget(Player* bot, PlayerbotAI* botAI, char const*& spell)
    {
        spell = nullptr;
        // Cheap tests first: alive, standing, a healer class; then the cached view; LOS and castability last.
        if (!bot || !botAI || !bot->IsAlive() || bot->isMoving())
            return nullptr;
        char const* const name = TankHealSpellName(bot);
        if (!name)
            return nullptr;
        HorsemenView view;
        if (!GetView(bot, view) || MyHealerGroup(bot, view) == 0xFF)
            return nullptr;
        Player* const tank = ActiveTank(bot, view, CurrentHorse(bot));
        if (!tank || tank == bot || !tank->IsInWorld() || tank->GetHealthPct() >= TANK_HEAL_PCT ||
            bot->GetExactDist2d(tank) > HEAL_REACH || !bot->IsWithinLOSInMap(tank))
            return nullptr;
        // CanCastSpell never sees mana, so check the real cost (the Aq40 tank heal's Aq40SpellManaCost).
        if (!botAI->CanCastSpell(name, tank))
            return nullptr;
        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
        SpellInfo const* const info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info)
            return nullptr;
        int32 const cost = info->PowerType == POWER_MANA ? info->CalcPowerCost(bot, info->GetSchoolMask()) : 0;
        if (int32(bot->GetPower(POWER_MANA)) < cost)
            return nullptr;
        spell = name;
        return tank;
    }
}
