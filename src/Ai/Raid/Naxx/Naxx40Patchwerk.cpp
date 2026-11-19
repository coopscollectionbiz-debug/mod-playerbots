/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Patchwerk helper layer (overlay patch 0022). Spec (overlay repo):
// docs/superpowers/specs/2026-11-01-playerbot-naxx40-patchwerk-design.md §3, §5. Every pure decision (strike pair,
// tank priority, roles, caps, healer split, soaker pick, damage samples) is Naxx40PatchwerkRules.h.

#include "Naxx40Patchwerk.h"

#include <algorithm>
#include <atomic>
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
#include "Spell.h"
#include "StringFormat.h"
#include "ThreatManager.h"
#include "Timer.h"

namespace Naxx40Patchwerk
{
    using namespace Naxx40PatchwerkRules;

    namespace
    {
        // An entry not refreshed for this long is a new attempt (whoever calls GetView after a wipe, or a reused
        // instance id): it is reset and re-latched from scratch.
        constexpr uint32 STALE_STATE_MS = 10000;

        // What a view is filled from. The fresh (no refresh due) path copies only this.
        struct ViewFields
        {
            ObjectGuid bossGuid;   // empty = not resolved (or the cached "no living Patchwerk")
            uint32 gateOpenMs = 0;  // first refresh of the attempt that found him alive; 0 = not yet
            bool latched = false;
            Roles roles;
            StrikePair pair;
            std::vector<TankInfo> tanks;  // per latched tank, priority order
            std::shared_ptr<HealerSplit const> healers;
        };

        // Per-instance encounter state. Bot AI of different instances runs on parallel map threads, so the container
        // is only touched under g_mutex, held briefly; callers get copies (the HealerSplit as a shared immutable
        // pointer), never references into it. One instance's bots share one map thread, so a refresh of an instance
        // (copy out, compute outside the lock, write back) never interleaves with another refresh of it.
        struct Roster
        {
            ViewFields v;
            std::vector<uint64_t> priority;      // the latched tank priority (TankPriority)
            std::vector<uint64_t> stockHealers;  // bot healers at the latch outside the split (diag mana only)
            bool petInPair = false;              // a non-player unit in the strike pair at the last refresh
            float maxPetThreat = 0.0f;           // highest non-player threat on his list at the last refresh
            std::unordered_map<uint64_t, uint32> prevHp;        // per latched tank, the last refresh's health
            std::unordered_map<uint64_t, DamageSample> window;  // per latched tank, since windowStartMs
            uint32 windowStartMs = 0;
            uint32 refreshMs = 0;  // 0 = never refreshed
            uint32 diagMs = 0;     // last "[Naxx40Patchwerk] diag" line; 0 = never
        };

        // Bot-written parts live beside the roster, so a refresh (which writes back only the roster) never overwrites
        // them.
        struct State
        {
            Roster roster;
            std::unordered_map<uint64_t, uint64_t> healClaims;  // NoteHealCast: healer -> target, while casting at it
            std::unordered_set<uint64_t> manaRefilled;          // ClaimManaRefill
            uint32 boosts = 0;                                  // NoteThreatBoost, this attempt
            uint32 cancels = 0;                                 // CancelPrecastHeal, this attempt
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

        // Gate failed after a scan (no living Patchwerk). Drops the attempt like EraseState, but keeps the scan
        // timestamp so the map-wide scan runs at most once per REFRESH_INTERVAL_MS.
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
            if (!raw || !ObjectGuid(raw).IsPlayer())
                return nullptr;
            Player* p = ObjectAccessor::GetPlayer(map, ObjectGuid(raw));
            return p && p->IsInWorld() && p->GetMap() == map ? p : nullptr;
        }

        bool IsHumanOrSelf(Player* p) { return !GET_PLAYERBOT_AI(p) || IsSelfBot(p); }

        // Patchwerk by entry: the cached GUID through Map::GetCreature, else ONE map-wide pass over the spawn store (no
        // grid-search radius clamp). The caller limits this to once per REFRESH_INTERVAL_MS per instance.
        Creature* ResolveBoss(Map* map, ObjectGuid& guid)
        {
            Creature* c = guid.IsEmpty() ? nullptr : map->GetCreature(guid);
            if (c)
                return c;
            for (auto const& entry : map->GetCreatureBySpawnIdStore())
            {
                Creature* cand = entry.second;
                if (cand && cand->IsInWorld() && cand->GetEntry() == PATCHWERK_ENTRY)
                {
                    guid = cand->GetGUID();
                    return cand;
                }
            }
            return nullptr;
        }

        // CapRoleOf's rule over raw state (the diag runs it without a view).
        CapRole RoleFor(Player* p, bool latched, Roles const& roles, std::vector<TankInfo> const& tanks)
        {
            if (!p || !latched || IsHumanOrSelf(p))
                return CapRole::None;
            uint64_t const me = p->GetGUID().GetRawValue();
            if (me == roles.mt)
                return CapRole::None;
            if (me == roles.soaker[0] || me == roles.soaker[1])
                return CapRole::Soaker;
            if (std::find(roles.spares.begin(), roles.spares.end(), me) != roles.spares.end())
                return CapRole::Spare;
            // A latched tank without a role is dead (DeriveRoles places every living one).
            if (std::any_of(tanks.begin(), tanks.end(), [me](TankInfo const& t) { return t.guid == me; }))
                return CapRole::None;
            if (PlayerbotAI::IsHeal(p))
                return CapRole::None;
            return PlayerbotAI::IsRanged(p) ? CapRole::RangedDps : CapRole::MeleeDps;
        }

        std::string NameOf(Creature* boss, uint64_t raw)
        {
            if (!raw)
                return "-";
            Unit* u = ObjectAccessor::GetUnit(*boss, ObjectGuid(raw));
            return u ? u->GetName() : std::string("?");
        }

        // "Name 85% 12345" (health %, his threat on it), "Name dead", or "-".
        std::string TankStr(Map const* map, Creature* boss, uint64_t raw)
        {
            if (!raw)
                return "-";
            Player* p = FindPlayer(map, raw);
            if (!p)
                return "?";
            if (!p->IsAlive())
                return Acore::StringFormat("{} dead", p->GetName());
            return Acore::StringFormat("{} {}% {}", p->GetName(), uint32(p->GetHealthPct()),
                                       uint32(boss->GetThreatMgr().GetThreat(p)));
        }

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

        std::string RolesStr(Map const* map, Creature* boss, Roles const& roles)
        {
            std::string spares;
            for (uint64_t g : roles.spares)
                spares += (spares.empty() ? "" : ",") + TankStr(map, boss, g);
            return Acore::StringFormat("mt={} s1={} s2={} spare={}", TankStr(map, boss, roles.mt),
                                       TankStr(map, boss, roles.soaker[0]), TankStr(map, boss, roles.soaker[1]),
                                       spares.empty() ? std::string("-") : spares);
        }

        bool SameRoles(Roles const& a, Roles const& b)
        {
            return a.mt == b.mt && a.soaker[0] == b.soaker[0] && a.soaker[1] == b.soaker[1] && a.spares == b.spares;
        }

        void LogDiag(Map const* map, Creature* boss, Group* group, Roster const& r, uint32 boosts, uint32 cancels,
                     uint32 now)
        {
            uint32 const elapsedS = r.v.gateOpenMs ? getMSTimeDiff(r.v.gateOpenMs, now) / 1000 : 0;
            uint32 const windowMs = std::max<uint32>(1, getMSTimeDiff(r.windowStartMs, now));

            std::string dmg;
            for (uint64_t g : r.priority)
            {
                auto it = r.window.find(g);
                DamageSample const s = it != r.window.end() ? it->second : DamageSample{};
                Player* p = FindPlayer(map, g);
                dmg += Acore::StringFormat(" {}:dmg={}/s hits={}", p ? p->GetName() : std::string("?"),
                                           uint64_t(s.dmg) * 1000 / windowMs, s.hits);
            }

            // The non-tank closest to (or furthest over) its cap: own threat / (limit x reference).
            std::string topCap = "-";
            float topPct = -1.0f;
            uint32 atCap = 0;  // capped non-tanks currently vetoed (ThreatVetoed: own + headroom >= limit x reference)
            if (group)
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* p = ref->GetSource();
                    if (!p || !p->IsInWorld() || p->GetMap() != map || !p->IsAlive())
                        continue;
                    CapRole const role = RoleFor(p, r.v.latched, r.v.roles, r.v.tanks);
                    if (role != CapRole::MeleeDps && role != CapRole::RangedDps)
                        continue;
                    Cap const cap = CapFor(role, p->GetGUID().GetRawValue(), r.v.roles, r.v.tanks);
                    if (!cap.capped)
                        continue;
                    float const own = boss->GetThreatMgr().GetThreat(p);
                    if (ThreatVetoed(own, cap))
                        ++atCap;
                    float const pct = cap.refThreat > 0.0f ? 100.0f * own / (cap.limit * cap.refThreat)
                                                           : (own > 0.0f ? 999.0f : 0.0f);
                    if (pct > topPct)
                    {
                        topPct = pct;
                        topCap = Acore::StringFormat("{} {:.0f}%", p->GetName(), pct);
                    }
                }
            }

            TankInfo const* lower = LowerSoaker(0, r.v.roles, r.v.tanks);
            std::string const pet = lower && lower->threat > 0.0f
                                        ? Acore::StringFormat("{:.0f}%", 100.0f * r.maxPetThreat / lower->threat)
                                        : std::string("-");
            std::vector<uint64_t> const none;
            LOG_DEBUG("playerbots",
                      "[Naxx40Patchwerk] diag boss={}% t={:02}:{:02}/7:00 {}{} pair={}/{} topCap={} atCap={} boosts={} "
                      "cancels={} pet={}{} mana soak={} mt={} stock={}",
                      uint32(boss->GetHealthPct()), elapsedS / 60, elapsedS % 60, RolesStr(map, boss, r.v.roles), dmg,
                      NameOf(boss, r.v.pair.a), NameOf(boss, r.v.pair.b), topCap, atCap, boosts, cancels, pet,
                      r.petInPair ? " PET-IN-PAIR" : "", AvgManaPct(map, r.v.healers ? r.v.healers->soaker : none),
                      AvgManaPct(map, r.v.healers ? r.v.healers->mt : none), AvgManaPct(map, r.stockHealers));
        }

        // One refresh of a grouped bot's instance (outside the lock). Reads his sorted threat list as the boss script
        // does, latches the tank priority and healer split on the first refresh where a role tank is his victim, then
        // re-derives roles, samples tank damage and logs the latch / role changes.
        void Refresh(Map* map, Creature* boss, Group* group, Roster& r, uint32 now)
        {
            ThreatManager& threat = boss->GetThreatMgr();
            std::vector<ThreatEntry> sorted;
            r.maxPetThreat = 0.0f;
            for (ThreatReference const* ref : threat.GetSortedThreatList())
            {
                Unit* u = ref ? ref->GetVictim() : nullptr;
                if (!u)
                    continue;
                sorted.push_back(
                    ThreatEntry{u->GetGUID().GetRawValue(), ref->GetThreat(), boss->IsWithinMeleeRange(u)});
                if (!u->IsPlayer())
                    r.maxPetThreat = std::max(r.maxPetThreat, ref->GetThreat());
            }
            r.v.pair = StrikeCandidates(sorted);
            r.petInPair = (r.v.pair.a && !ObjectGuid(r.v.pair.a).IsPlayer()) ||
                          (r.v.pair.b && !ObjectGuid(r.v.pair.b).IsPlayer());

            Unit* const victim = boss->GetVictim();
            uint64_t const victimRaw = victim ? victim->GetGUID().GetRawValue() : 0;

            if (!r.v.latched)
            {
                std::vector<TankInfo> candidates;
                std::vector<uint64_t> botHealers;
                bool victimIsTank = false;
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* p = ref->GetSource();
                    if (!p || !p->IsInWorld() || p->GetMap() != map || !p->IsAlive())
                        continue;
                    uint64_t const raw = p->GetGUID().GetRawValue();
                    if (PlayerbotAI::IsTank(p))
                    {
                        candidates.push_back(
                            TankInfo{raw, true, threat.GetThreat(p), p->GetMaxHealth(), boss->IsWithinMeleeRange(p)});
                        victimIsTank = victimIsTank || raw == victimRaw;
                    }
                    else if (PlayerbotAI::IsHeal(p) && !IsHumanOrSelf(p))
                        botHealers.push_back(raw);
                }
                if (!victimIsTank)
                    return;

                r.priority = TankPriority(candidates);
                auto split = std::make_shared<HealerSplit const>(SplitHealers(botHealers));
                r.stockHealers.clear();
                for (uint64_t g : botHealers)
                    if (std::find(split->soaker.begin(), split->soaker.end(), g) == split->soaker.end() &&
                        std::find(split->mt.begin(), split->mt.end(), g) == split->mt.end())
                        r.stockHealers.push_back(g);
                r.v.healers = std::move(split);
                r.v.latched = true;
                r.windowStartMs = now;
                if (sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
                {
                    std::string names;
                    for (uint64_t g : r.priority)
                    {
                        Player* p = FindPlayer(map, g);
                        names += Acore::StringFormat("{}{}({})", names.empty() ? "" : ",",
                                                     p ? p->GetName() : std::string("?"), p ? p->GetMaxHealth() : 0);
                    }
                    LOG_DEBUG("playerbots",
                              "[Naxx40Patchwerk] latch tanks={} healers soak={} mt={} stock={} victim={} after {}ms",
                              names, r.v.healers->soaker.size(), r.v.healers->mt.size(), r.stockHealers.size(),
                              NameOf(boss, victimRaw), r.v.gateOpenMs ? getMSTimeDiff(r.v.gateOpenMs, now) : 0);
                }
            }

            r.v.tanks.clear();
            for (uint64_t g : r.priority)
            {
                Player* p = FindPlayer(map, g);
                bool const alive = p && p->IsAlive();
                r.v.tanks.push_back(TankInfo{g, alive, p ? threat.GetThreat(p) : 0.0f, p ? p->GetMaxHealth() : 0,
                                             alive && boss->IsWithinMeleeRange(p)});
                // Damage sample against the previous refresh's health; a dead or absent tank restarts its baseline.
                if (!alive)
                {
                    r.prevHp.erase(g);
                    continue;
                }
                uint32 const cur = p->GetHealth();
                auto it = r.prevHp.find(g);
                if (it != r.prevHp.end())
                    AddSample(r.window[g], it->second, cur);
                r.prevHp[g] = cur;
            }

            Roles const roles = DeriveRoles(r.priority, r.v.tanks, victimRaw);
            bool const changed = !SameRoles(roles, r.v.roles);
            r.v.roles = roles;
            if (changed && sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
                LOG_DEBUG("playerbots", "[Naxx40Patchwerk] roles {}", RolesStr(map, boss, r.v.roles));
        }

        // A heal claim is held while the healer is still casting at its target.
        bool ClaimLive(Map const* map, uint64_t healer, uint64_t target)
        {
            Player* p = FindPlayer(map, healer);
            if (!p || !p->IsAlive())
                return false;
            Spell* const spell = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
            return spell && spell->m_targets.GetUnitTargetGUID().GetRawValue() == target;
        }

        void FillView(Map* map, ViewFields const& f, uint32 now, PatchwerkView& view)
        {
            Creature* c = f.bossGuid.IsEmpty() ? nullptr : map->GetCreature(f.bossGuid);
            view.boss = c && c->IsAlive() ? c : nullptr;
            view.latched = f.latched;
            view.openerHold =
                !f.latched && (!f.gateOpenMs || getMSTimeDiff(f.gateOpenMs, now) < OPENER_FAIL_OPEN_MS);
            view.roles = f.roles;
            view.pair = f.pair;
            view.tanks = f.tanks;
            view.healers = f.healers;
        }

        // The latched state of the bot's instance, under g_mutex (the caller holds it); null when none is latched.
        State* LatchedState(Player* bot)
        {
            auto it = g_stateByInstance.find(bot->GetMap()->GetInstanceId());
            if (it == g_stateByInstance.end() || !it->second.roster.v.latched)
                return nullptr;
            return &it->second;
        }

        // The view's latched tanks with this tick's threat re-read from the boss (the reference is this tick's, not the
        // last refresh's). A tank that died since the refresh is never a reference.
        std::vector<TankInfo> LiveTanks(Map const* map, PatchwerkView const& view)
        {
            std::vector<TankInfo> live = view.tanks;
            for (TankInfo& t : live)
            {
                if (!t.alive)
                    continue;
                Player* p = FindPlayer(map, t.guid);
                t.alive = p && p->IsAlive();
                if (t.alive)
                    t.threat = view.boss->GetThreatMgr().GetThreat(p);
            }
            return live;
        }
    }

    bool GetView(Player* bot, PatchwerkView& view)
    {
        view = PatchwerkView{};

        if (!bot || !bot->IsInWorld() || bot->GetMapId() != NAXX40_MAP_ID)
            return false;
        Map* const map = bot->GetMap();
        uint32 const instanceId = map->GetInstanceId();
        // IP's Naxx40 only: WotLK Naxx (10/25 normal) keeps upstream's Patchwerk untouched.
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
        std::unordered_map<uint64_t, uint64_t> claims;
        uint32 boosts = 0;
        uint32 cancels = 0;
        bool fresh = false;
        ViewFields snap;
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
                    if (cur.v.bossGuid.IsEmpty())
                        return false;  // cached "no living Patchwerk"
                    fresh = true;
                    snap = cur.v;  // no refresh due: only what the view needs
                }
                else
                {
                    r = cur;  // a refresh is due: copy what it needs
                    claims = it->second.healClaims;
                    boosts = it->second.boosts;
                    cancels = it->second.cancels;
                }
            }
        }
        if (fresh)
        {
            FillView(map, snap, now, view);
            if (view.boss)
                return true;
            view = PatchwerkView{};
            // He died: refresh now (which resets the state). Needs the stored roster.
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_stateByInstance.find(instanceId);
            if (it != g_stateByInstance.end())
                r = it->second.roster;
        }

        // The scan and every computation run outside the lock (see Roster).
        Creature* const boss = ResolveBoss(map, r.v.bossGuid);
        if (!boss || !boss->IsAlive())
        {
            ResetState(instanceId, now);
            return false;
        }

        r.refreshMs = now;
        if (!r.v.gateOpenMs)
            r.v.gateOpenMs = now;
        // An ungrouped bot would latch a one-member raid: it never latches or refreshes the roles, and reads the last
        // refreshed state, if any. Its scan is still stored, so it does not rescan on every call.
        Group* const group = bot->GetGroup();
        if (group)
            Refresh(map, boss, group, r, now);

        std::vector<uint64_t> dropped;
        for (auto const& [healer, target] : claims)
            if (!ClaimLive(map, healer, target))
                dropped.push_back(healer);

        if (r.v.latched && (!r.diagMs || getMSTimeDiff(r.diagMs, now) >= DIAG_INTERVAL_MS))
        {
            r.diagMs = now;
            if (sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
                LogDiag(map, boss, group, r, boosts, cancels, now);
            r.window.clear();
            r.windowStartMs = now;
        }

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            State& st = g_stateByInstance[instanceId];
            st.roster = r;  // keeps the claims and the refills
            for (uint64_t healer : dropped)
            {
                auto it = st.healClaims.find(healer);
                if (it != st.healClaims.end() && claims.count(healer) && claims[healer] == it->second)
                    st.healClaims.erase(it);
            }
            g_stateCount.store(uint32(g_stateByInstance.size()), std::memory_order_relaxed);
        }

        FillView(map, r.v, now, view);
        return view.boss != nullptr;
    }

    CapRole CapRoleOf(Player* bot, PatchwerkView const& view)
    {
        return RoleFor(bot, view.latched, view.roles, view.tanks);
    }

    Cap CapOf(Player* bot, PatchwerkView const& view)
    {
        CapRole const role = CapRoleOf(bot, view);
        if (role == CapRole::None || !view.boss)
            return Cap{};
        return CapFor(role, bot->GetGUID().GetRawValue(), view.roles, LiveTanks(bot->GetMap(), view));
    }

    float ThreatBoostOf(Player* bot, PatchwerkView const& view)
    {
        if (!bot || !view.latched || !view.boss)
            return 0.0f;
        return ThreatBoostFor(bot->GetGUID().GetRawValue(), view.roles, LiveTanks(bot->GetMap(), view));
    }

    void NoteThreatBoost(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return;
        std::lock_guard<std::mutex> lock(g_mutex);
        if (State* st = LatchedState(bot))
            ++st->boosts;
    }

    HealerJob HealerJobOf(Player* bot, PatchwerkView const& view)
    {
        if (!bot || !view.latched || !view.healers || IsHumanOrSelf(bot))
            return HealerJob::None;
        uint64_t const me = bot->GetGUID().GetRawValue();
        auto const& s = view.healers->soaker;
        if (std::find(s.begin(), s.end(), me) != s.end())
            return HealerJob::Soaker;
        auto const& m = view.healers->mt;
        if (std::find(m.begin(), m.end(), me) != m.end())
            return HealerJob::MainTank;
        return HealerJob::None;
    }

    Player* SoakerHealTarget(Player* bot, PatchwerkView const& view)
    {
        if (!bot || !bot->IsInWorld() || !view.boss)
            return nullptr;
        Map const* map = bot->GetMap();
        auto resolve = [map](uint64_t raw) -> Player*
        {
            // Pull 1: full-health members ARE candidates. A soaker is struck about every 2.4 s, so a heal started only
            // once it is damaged lands after the next strike; KeepPrecastHeal keeps PlayerbotAI::UpdateAI from
            // interrupting the pre-cast.
            Player* p = FindPlayer(map, raw);
            return p && p->IsAlive() ? p : nullptr;
        };
        Player* const a = resolve(view.pair.a);
        Player* const b = resolve(view.pair.b);
        if (!a && !b)
            return nullptr;
        uint64_t const aRaw = a ? a->GetGUID().GetRawValue() : 0;
        uint64_t const bRaw = b ? b->GetGUID().GetRawValue() : 0;

        uint32 aClaims = 0;
        uint32 bClaims = 0;
        {
            uint64_t const me = bot->GetGUID().GetRawValue();
            std::lock_guard<std::mutex> lock(g_mutex);
            if (State const* st = LatchedState(bot))
                for (auto const& [healer, target] : st->healClaims)
                {
                    if (healer == me)
                        continue;
                    aClaims += aRaw && target == aRaw;
                    bClaims += bRaw && target == bRaw;
                }
        }
        uint64_t const pick = PickSoakerTarget(aRaw, a ? a->GetHealthPct() : 0.0f, aClaims, bRaw,
                                               b ? b->GetHealthPct() : 0.0f, bClaims);
        return pick == aRaw ? a : b;
    }

    bool KeepPrecastHeal(Player* bot, Unit* target)
    {
        if (!bot || !target || bot->GetMapId() != NAXX40_MAP_ID || !target->IsPlayer())
            return false;
        PatchwerkView view;
        if (!GetView(bot, view) || !view.latched || HealerJobOf(bot, view) != HealerJob::Soaker)
            return false;
        uint64_t const raw = target->GetGUID().GetRawValue();
        return raw == view.pair.a || raw == view.pair.b;
    }

    bool CancelPrecastHeal(Player* bot, Spell* spell, Unit* target)
    {
        if (!bot || !spell || !target || bot->GetMapId() != NAXX40_MAP_ID)
            return false;
        PatchwerkView view;
        if (!GetView(bot, view) || !view.latched || HealerJobOf(bot, view) != HealerJob::Soaker ||
            !target->IsAlive())
            return false;
        if (!ShouldCancelPrecast(spell->GetCastTimeRemaining(), target->GetHealthPct()))
            return false;
        std::lock_guard<std::mutex> lock(g_mutex);
        if (State* st = LatchedState(bot))
            ++st->cancels;
        return true;
    }

    void NoteHealCast(Player* bot, Player* target)
    {
        if (!bot || !target || !bot->IsInWorld())
            return;
        std::lock_guard<std::mutex> lock(g_mutex);
        if (State* st = LatchedState(bot))
            st->healClaims[bot->GetGUID().GetRawValue()] = target->GetGUID().GetRawValue();
    }

    bool ClaimManaRefill(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return false;
        std::lock_guard<std::mutex> lock(g_mutex);
        State* st = LatchedState(bot);
        return st && st->manaRefilled.insert(bot->GetGUID().GetRawValue()).second;
    }

    bool ManaRefillClaimed(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return true;
        std::lock_guard<std::mutex> lock(g_mutex);
        State const* st = LatchedState(bot);
        return !st || st->manaRefilled.count(bot->GetGUID().GetRawValue());
    }
}
