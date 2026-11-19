/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Naxx40 Anub'Rekhan raid spread (overlay patch 0022). Spec (overlay repo):
// docs/superpowers/specs/2026-10-30-playerbot-naxx40-anubrekhan-design.md §3-§5.

#include "Naxx40Anub.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <unordered_map>
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
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Timer.h"

namespace Naxx40Anub
{
    using namespace Naxx40AnubRules;

    namespace
    {
        // An entry not refreshed for this long is a new attempt (a wipe, or a reused instance id): re-latched.
        constexpr uint32 STALE_STATE_MS = 10000;

        // Per-instance attempt state. Bot AI of different instances runs on parallel map threads, so the container is
        // only touched under g_mutex, held briefly (memory: module globals need locking); callers get copies (the plan
        // as a shared immutable pointer). Only the refresh writes it, and one instance's bots share one map thread, so
        // a refresh (copy out, compute outside the lock, write back) never interleaves with another of the same
        // instance.
        struct State
        {
            ObjectGuid anubGuid;  // empty = not resolved yet
            bool latched = false;
            std::shared_ptr<Plan const> plan;
            float fallbackBearing = 0.0f;
            uint32 refreshMs = 0;  // 0 = never refreshed
            uint32 diagMs = 0;     // last "[Naxx40Anub] diag" line; 0 = never

            // Diag counters, sampled at each refresh.
            bool impaleCasting = false;  // 28783 seen as his current spell (only if it has a cast time)
            uint32 impaleCasts = 0;
            bool airUp = false;    // some slotted bot was falling (Impale's knock-up) at the last refresh
            uint32 airEvents = 0;  // 0 -> >0 transitions of the falling count
            uint32 airPeak = 0;    // most bots falling at once in the current / last event
            bool swarmUp = false;
            uint32 swarms = 0;
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

        // Gate failed after a scan (no living Anub'Rekhan): drops the attempt but keeps the scan time, so the map-wide
        // scan runs at most once per REFRESH_INTERVAL_MS (the "not found" result is cached too).
        void ResetState(uint32 instanceId, uint32 now)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            State& st = g_stateByInstance[instanceId];
            st = State{};
            st.refreshMs = now;
            g_stateCount.store(uint32(g_stateByInstance.size()), std::memory_order_relaxed);
        }

        // Anub'Rekhan by entry (never `find target`, which only scans the bot's threat list): the cached GUID through
        // Map::GetCreature, else ONE map-wide pass over the spawn store. The caller limits this to one per refresh.
        Creature* ResolveAnub(Map* map, State& st)
        {
            Creature* c = st.anubGuid.IsEmpty() ? nullptr : map->GetCreature(st.anubGuid);
            if (c)
                return c;
            for (auto const& entry : map->GetCreatureBySpawnIdStore())
            {
                Creature* cand = entry.second;
                if (cand && cand->IsInWorld() && cand->GetEntry() == ANUB_ENTRY)
                {
                    st.anubGuid = cand->GetGUID();
                    return cand;
                }
            }
            return nullptr;
        }

        bool CastingSpell(Creature* c, uint32 spellId)
        {
            for (CurrentSpellTypes type : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
                if (Spell* s = c->GetCurrentSpell(type))
                    if (s->m_spellInfo && s->m_spellInfo->Id == spellId)
                        return true;
            return false;
        }

        bool SwarmUp(Creature* anub)
        {
            return anub->HasAura(SPELL_LOCUST_SWARM) || CastingSpell(anub, SPELL_LOCUST_SWARM);
        }

        Player* FindPlayer(Map const* map, uint64_t raw)
        {
            Player* p = ObjectAccessor::GetPlayer(map, ObjectGuid(raw));
            return p && p->IsInWorld() && p->GetMap() == map ? p : nullptr;
        }

        // Spec §5: the attempt's slots. Bots only (never a human or a self bot: the owner is never moved and takes no
        // slot), alive on the map, not tanks (tanks keep upstream's kiting and target choice).
        void Latch(Player* bot, Group* group, Creature* anub, State& st)
        {
            std::vector<Member> members;
            float cx = 0.0f;
            float cy = 0.0f;
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* p = ref->GetSource();
                if (!p || !p->IsInWorld() || p->GetMap() != bot->GetMap() || !p->IsAlive())
                    continue;
                if (!GET_PLAYERBOT_AI(p) || IsSelfBot(p) || PlayerbotAI::IsTank(p))
                    continue;
                members.push_back(
                    Member{p->GetGUID().GetRawValue(), group->GetMemberGroup(p->GetGUID()), !PlayerbotAI::IsRanged(p)});
                cx += p->GetPositionX();
                cy += p->GetPositionY();
            }
            Point2 const boss{anub->GetPositionX(), anub->GetPositionY()};
            st.fallbackBearing =
                members.empty() ? CentreBearing(boss)
                                : FallbackBearing(boss, Point2{cx / float(members.size()), cy / float(members.size())});
            st.plan = std::make_shared<Plan const>(AssignSlots(std::move(members)));
            st.latched = true;
            LOG_DEBUG("playerbots",
                      "[Naxx40Anub] latched arc={} melee={}/{} boss=({:.1f},{:.1f}) d={:.1f} fallback={:.0f}",
                      st.plan->arcCount, st.plan->meleeLeft, st.plan->meleeRight, boss.x, boss.y,
                      Dist2d(boss, RoomCentre()), st.fallbackBearing * 180.0f / PI_F);
        }

        // Diag counters: Impale casts (if it has a visible cast), knock-up events (slotted bots falling), swarms.
        void Sample(Map const* map, Creature* anub, State& st)
        {
            bool const impale = CastingSpell(anub, SPELL_IMPALE);
            if (impale && !st.impaleCasting)
                ++st.impaleCasts;
            st.impaleCasting = impale;

            bool const swarm = SwarmUp(anub);
            if (swarm && !st.swarmUp)
                ++st.swarms;
            st.swarmUp = swarm;

            uint32 falling = 0;
            for (Slot const& s : st.plan->slots)
                if (Player* p = FindPlayer(map, s.guid))
                    if (p->IsAlive() && p->IsFalling())
                        ++falling;
            if (falling && !st.airUp)
            {
                ++st.airEvents;
                st.airPeak = 0;
            }
            st.airUp = falling > 0;
            st.airPeak = std::max(st.airPeak, falling);
        }

        // raidDead: every raid member on the map that is dead, tanks and humans included (spec §5 "raid deaths");
        // dead= counts slotted bots only.
        void LogDiag(Map const* map, Group* group, Creature* anub, State const& st)
        {
            uint32 raidDead = 0;
            if (group)
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                    if (Player* p = ref->GetSource())
                        if (p->IsInWorld() && p->GetMap() == map && !p->IsAlive())
                            ++raidDead;
            Point2 const boss{anub->GetPositionX(), anub->GetPositionY()};
            bool const swarm = SwarmUp(anub);
            uint32 onSpot = 0;
            uint32 moving = 0;
            uint32 off = 0;
            uint32 dead = 0;
            uint32 gone = 0;
            uint32 inSwarm = 0;
            for (Slot const& s : st.plan->slots)
            {
                Player* p = FindPlayer(map, s.guid);
                if (!p)
                {
                    ++gone;
                    continue;
                }
                if (!p->IsAlive())
                {
                    ++dead;
                    continue;
                }
                Point2 const spot = Naxx40AnubRules::SpotFor(s, *st.plan, boss, st.fallbackBearing, swarm);
                float const d = Dist2d(Point2{p->GetPositionX(), p->GetPositionY()}, spot);
                if (d <= ToleranceFor(s, swarm))
                    ++onSpot;
                else if (p->isMoving())
                    ++moving;
                else
                    ++off;
                if (anub->GetExactDist(p) <= SWARM_RADIUS)
                    ++inSwarm;
            }
            Unit* victim = anub->GetVictim();
            LOG_DEBUG("playerbots",
                      "[Naxx40Anub] diag boss={}% pos=({:.1f},{:.1f}) d={:.1f} axis={:.0f} victim={} swarm={} "
                      "swarms={} impales={} knockups={} lastPeak={} slots={} onSpot={} moving={} off={} "
                      "inSwarmRadius={} dead={} gone={} raidDead={}",
                      uint32(anub->GetHealthPct()), boss.x, boss.y, Dist2d(boss, RoomCentre()),
                      ArcAxis(boss, st.fallbackBearing) * 180.0f / PI_F, victim ? victim->GetName() : "none",
                      swarm ? "on" : "off", st.swarms, st.impaleCasts, st.airEvents, st.airPeak, st.plan->arcCount,
                      onSpot, moving, off, inSwarm, dead, gone, raidDead);
        }
    }

    bool GetView(Player* bot, AnubView& view)
    {
        view = AnubView{};

        if (!bot || !bot->IsInWorld() || bot->GetMapId() != NAXX40_MAP_ID)
            return false;
        Map* const map = bot->GetMap();
        uint32 const instanceId = map->GetInstanceId();
        // IP's Naxx40 only: WotLK Naxx (10/25 normal) keeps upstream's Anub'Rekhan untouched.
        if (map->GetDifficulty() != RAID_DIFFICULTY_10MAN_HEROIC)
        {
            EraseState(instanceId);
            return false;
        }
        InstanceScript* const instance = bot->GetInstanceScript();
        if (!instance)
            return false;
        if (instance->GetBossState(BOSS_STATE_ID) != IN_PROGRESS)
        {
            EraseState(instanceId);
            return false;
        }

        uint32 const now = NowMs();
        State st;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_stateByInstance.find(instanceId);
            if (it != g_stateByInstance.end())
            {
                State const& cur = it->second;
                uint32 const age = cur.refreshMs ? getMSTimeDiff(cur.refreshMs, now) : 0;
                if (cur.refreshMs && age >= STALE_STATE_MS)
                    it->second = State{};  // a new attempt: st stays default, so it latches from scratch
                else if (cur.refreshMs && age < REFRESH_INTERVAL_MS)
                {
                    if (!cur.latched)
                        return false;  // cached "no living Anub'Rekhan" (or not latched yet)
                    // No refresh due: the view needs only the GUID, the plan and the fallback bearing.
                    Creature* c = cur.anubGuid.IsEmpty() ? nullptr : map->GetCreature(cur.anubGuid);
                    if (c && c->IsAlive())
                    {
                        view.anub = c;
                        view.plan = cur.plan;
                        view.fallbackBearing = cur.fallbackBearing;
                        view.swarm = SwarmUp(c);
                        return true;
                    }
                    st = cur;  // he died or despawned: refresh now (which resets the state)
                }
                else
                    st = cur;  // a refresh is due
            }
        }

        // The scan and every computation run outside the lock (see State).
        Creature* const anub = ResolveAnub(map, st);
        if (!anub || !anub->IsAlive())
        {
            ResetState(instanceId, now);
            return false;
        }

        st.refreshMs = now;
        // An ungrouped bot would latch a one-member raid: it never latches, and reads the last latched plan, if any.
        if (Group* group = bot->GetGroup())
        {
            if (!st.latched)
                Latch(bot, group, anub, st);
            Sample(map, anub, st);
        }
        bool const diagDue = st.latched && (!st.diagMs || getMSTimeDiff(st.diagMs, now) >= DIAG_INTERVAL_MS);
        if (diagDue)
            st.diagMs = now;

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_stateByInstance[instanceId] = st;
            g_stateCount.store(uint32(g_stateByInstance.size()), std::memory_order_relaxed);
        }

        if (!st.latched)
            return false;
        if (diagDue && sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
            LogDiag(map, bot->GetGroup(), anub, st);

        view.anub = anub;
        view.plan = st.plan;
        view.fallbackBearing = st.fallbackBearing;
        view.swarm = SwarmUp(anub);
        return true;
    }

    Slot const* SlotOf(Player* bot, AnubView const& view)
    {
        if (!bot || !view.plan)
            return nullptr;
        return FindSlot(*view.plan, bot->GetGUID().GetRawValue());
    }

    bool SpotFor(Player* bot, PlayerbotAI* botAI, AnubView const& view, Spot& spot)
    {
        if (!bot || !botAI || !view.anub || !bot->IsAlive())
            return false;
        Slot const* const slot = SlotOf(bot, view);
        if (!slot)
            return false;

        Point2 const boss{view.anub->GetPositionX(), view.anub->GetPositionY()};
        Point2 const p = Naxx40AnubRules::SpotFor(*slot, *view.plan, boss, view.fallbackBearing, view.swarm);

        // A bot on another target (a Crypt Guard): melee fight it where it is; ranged DPS keep the arc only while that
        // target is in spell reach of the spot, else stock reach-spell would drag them off it every tick and the spread
        // would pull them back (ping-pong). Healers always hold the arc.
        Unit* target = bot->GetVictim();
        if (!target)
            target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        if (target && target != view.anub && target->IsAlive())
        {
            if (slot->melee)
                return false;
            if (!PlayerbotAI::IsHeal(bot))
            {
                float const surface = std::hypot(target->GetPositionX() - p.x, target->GetPositionY() - p.y) -
                                      target->GetCombatReach() - bot->GetCombatReach();
                if (surface > sPlayerbotAIConfig.spellDistance)
                    return false;
            }
        }

        spot.x = p.x;
        spot.y = p.y;
        spot.tolerance = ToleranceFor(*slot, view.swarm);
        return true;
    }
}
