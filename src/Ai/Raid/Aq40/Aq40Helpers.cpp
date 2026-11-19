/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Twin Emperors helper layer (overlay patch 0016). Spec (overlay repo):
// docs/superpowers/specs/2026-10-22-playerbot-aq40-twin-emperors-v2-design.md. Pure rules: Aq40TwinsRules.h.

#include "Aq40Helpers.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Creature.h"
#include "DynamicObject.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Pet.h"
#include "Playerbots.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "ThreatManager.h"
#include "Timer.h"

namespace TempleOfAhnQirajHelpers
{

namespace
{
    using Aq40TwinsRules::Role;
    using Aq40TwinsRules::Side;

    // One raid member as the latch saw it (spec §5.2). `a` changes only on a death replacement.
    struct TwinsLatched
    {
        ObjectGuid guid;
        std::string name;
        Aq40TwinsRules::Assignment a;
        Aq40TwinsRules::Member m;
        // Healers: guid rank among the side's latched healers (TwinsMe::rankOnSide). 0 for everyone else.
        uint8 rankOnSide = 0;
        // Revision 2 §11.4: the inner ranged arc and the rank on it (TwinsMe::rangedKind / rangedRank).
        TwinsRangedKind rangedKind = TwinsRangedKind::None;
        uint8 rangedRank = 0;
    };

    // Revision 3 §12.2: one bot's unreachable-spot record for the current anchor epoch (TwinsReportSpotMove).
    struct TwinsSpotTrack
    {
        uint8 fails = 0;
        uint32 lastFailMs = 0;
        bool parked = false;
    };

    // Revision 4 §13.2: one healer's step-in clocks (TwinsHealerPass). `tank` is the side's designated tank they run
    // for and tankPos that tank's position at the last refresh (so the step-in point holds between refreshes).
    // Distance clock: distRunning / sinceMs (started beyond STEP_IN_TRIGGER), `due` the last HealerStepInDue result
    // (= stepping in, which switches the clock to its distance-only rule). Revision 11 §20.2, the separate LOS clock:
    // losRunning / losSinceMs (started within STEP_IN_TRIGGER, out of LOS), `losDue` latched by HealerLosStepInDue
    // until HealerLosStepInEnds. The record lives while either clock runs.
    struct TwinsStepIn
    {
        ObjectGuid tank;
        bool distRunning = false;
        uint32 sinceMs = 0;
        bool due = false;
        bool losRunning = false;
        uint32 losSinceMs = 0;
        bool losDue = false;
        Position tankPos;
    };

    // Revision 4 §13.2 + Revision 11 §20.2: advance one healer's two step-in clocks for tank `t` (alive or dead, a
    // present slot holder), measured from the healer's spot (hx, hy, hz). False = neither clock runs (drop the record).
    // The LOS ray (tank -> spot, WorldObject::IsWithinLOS) is cast only when the LOS rule can use it.
    bool TwinsAdvanceStepIn(TwinsStepIn& rec, Player* t, float hx, float hy, float hz, uint32 now)
    {
        bool const alive = t->IsAlive();
        float const hp = t->GetHealthPct();
        float const dist = t->GetExactDist2d(hx, hy);

        if (Aq40TwinsRules::HealerStepInClockRuns(rec.due, alive, hp, dist))
        {
            if (!rec.distRunning)
            {
                rec.distRunning = true;
                rec.sinceMs = now;
            }
            rec.due = Aq40TwinsRules::HealerStepInDue(alive, dist, getMSTimeDiff(rec.sinceMs, now));
        }
        else
        {
            rec.distRunning = false;
            rec.due = false;
        }

        bool losRuns = false;
        if (rec.losDue)
        {
            // LOS only matters within STEP_IN_END (HealerLosStepInEnds), so no ray beyond it or for a dead tank.
            bool const los = alive && dist <= Aq40TwinsRules::STEP_IN_END && t->IsWithinLOS(hx, hy, hz);
            losRuns = !Aq40TwinsRules::HealerLosStepInEnds(alive, dist, los);
        }
        else if (alive && hp < 100.0f && dist <= Aq40TwinsRules::STEP_IN_TRIGGER && !t->IsWithinLOS(hx, hy, hz))
        {
            losRuns = true;
            if (!rec.losRunning)
                rec.losSinceMs = now;
            rec.losDue = Aq40TwinsRules::HealerLosStepInDue(alive, hp, dist, false,
                                                            getMSTimeDiff(rec.losSinceMs, now));
        }
        rec.losRunning = losRuns;
        if (!losRuns)
            rec.losDue = false;

        rec.tankPos = t->GetPosition();
        return rec.distRunning || rec.losRunning;
    }

    // EVERYTHING per instance for the Twins. Different instances update on parallel map threads: two AQ40 raids
    // would race on an unlocked container and rehash under each other (the Aug 25-26 prod crashes). So the map is
    // only touched under twinsMutex, held briefly, and callers get copies, never references into it.
    struct TwinsInstanceState
    {
        bool latched = false;
        std::vector<TwinsLatched> members;
        std::unordered_map<ObjectGuid, uint32> indexByGuid;
        // Designated tanks per side (index = Side), after replacement. Empty = none.
        ObjectGuid melee[2];
        ObjectGuid warlock[2];
        Side veklorSide = Side::A;
        Side windowVeklorFrom = Side::None;
        // Ruling R13 F4: where each twin stood at the window's rising edge (= where it landed). See TwinsView.
        Position windowVeklorPos;
        Position windowVeknilashPos;
        // Latch time = engage. 0 = not latched.
        uint32 engageMs = 0;
        // Falling edge of the last landing window. 0 = none yet this attempt.
        uint32 lastWindowEndMs = 0;
        // Revision 3 §12.1: rising edge of the last landing window (the melee pre-swap clock). 0 = none yet.
        uint32 lastWindowStartMs = 0;
        bool inWindow = false;
        // Revision 3 §12.2: each side's anchor (index = Side; the authored station at the latch) and its settle
        // tracker: the twin's candidate point and since when it has stayed within ANCHOR_SETTLE_RADIUS of it
        // (0 = no candidate; reset inside the landing window, since the twins swap sides there).
        Position anchor[2];
        Position settleCand[2];
        uint32 settleSinceMs[2] = {0, 0};
        // Bumped on every anchor re-latch and every window close; each bump clears spotTrack (unparks everyone).
        uint32 anchorEpoch = 0;
        std::unordered_map<ObjectGuid, TwinsSpotTrack> spotTrack;
        // Revision 8 §17.1: each side's pickup basis (index = Side): the position of the twin on that side, re-latched
        // only once that twin is more than PICKUP_BASIS_RELATCH from it (Aq40TwinsRules::BasisShouldRelatch), outside
        // the landing window. pickupBasisValid is that rule's `valid`: cleared at the latch and at every window's
        // rising edge (both twins moved), so the first refresh after the window re-latches. pickupBasisSet is cleared
        // only at the latch: inside the window the pickups keep measuring from the basis they already had (nothing
        // moves them mid-window), and only a basis never latched this attempt falls back to the anchored spot.
        Position pickupBasis[2];
        bool pickupBasisValid[2] = {false, false};
        bool pickupBasisSet[2] = {false, false};
        // Fix round 1 (R22): the drag station, latched (the authored station farther from Vek'lor) when the
        // separation guard comes up and held until separation is back at SEPARATION_GUARD. None = guard down.
        Side dragSide = Side::None;
        // Revision 4 §13.2: the healer step-in clocks, by healer guid.
        std::unordered_map<ObjectGuid, TwinsStepIn> stepIn;
        // Revision 4 §13.4: healer bots already refilled this attempt.
        std::unordered_set<ObjectGuid> manaRefilled;
        // "[Aq40Twins] diag" throttle. 0 = never.
        uint32 diagMs = 0;
        // Revision 6 §15.1: successful side tank heals since the last diag line (TwinsCountTankHeal; the refresh that
        // writes the diag moves it into TwinsRefreshEvents::tankHeals and zeroes it).
        uint32 tankHeals = 0;
        // Last refresh. Between refreshes GetTwinsView only copies.
        uint32 refreshMs = 0;
    };

    std::mutex twinsMutex;
    std::unordered_map<uint32, TwinsInstanceState> twinsStateByInstance;
    // twinsStateByInstance.size(), written under twinsMutex. Lets the not-IN_PROGRESS path of GetTwinsView (every
    // Twins predicate, every AQ40 bot, every tick) skip the lock when no instance holds Twins state.
    std::atomic<uint32> twinsStateCount{0};

    // Final-review I2: owners whose pets the Twins pet actions changed, per instance (TwinsMarkPetsTouched). Its own
    // container, not part of TwinsInstanceState or the staging cache: the instance state is erased whenever the
    // encounter is not IN_PROGRESS (form-up included) and the staging cache at the pull, and a record must outlive
    // both, from form-up through the fight to the restore. Touched only under twinsPetMutex.
    struct TwinsPetTouched
    {
        std::unordered_set<ObjectGuid> owners;
        uint32 checkMs = 0;  // last restore pass (TWINS_PET_RESTORE_CHECK_MS throttle); 0 = none
    };
    std::mutex twinsPetMutex;
    std::unordered_map<uint32, TwinsPetTouched> twinsPetTouchedByInstance;
    // twinsPetTouchedByInstance.size(), written under twinsPetMutex: the restore pass in GetTwinsView's
    // not-IN_PROGRESS path stays lock-free while no instance has a record.
    std::atomic<uint32> twinsPetTouchedCount{0};

    // What a refresh observed, for the logs written after the lock is released.
    struct TwinsRefreshEvents
    {
        bool latchedNow = false;
        bool windowEnded = false;
        bool diagDue = false;
        // Revision 3 §12.2: "<A|B> -> (x,y) off=<d>" per anchor re-latch.
        std::vector<std::string> anchors;
        std::vector<std::string> replacements;
        // Revision 4 §13.4: "<name>" per mana refill.
        std::vector<std::string> manaRefills;
        // Revision 6 §15.1: the tank-heal count for this diag line (only meaningful with diagDue).
        uint32 tankHeals = 0;
    };

    uint32 TwinsNowMs()
    {
        uint32 const now = getMSTime();
        return now ? now : 1;  // 0 means "unset" in the state
    }

    // Final-review I2: restore the pets of every recorded owner on this map that is out of form-up and out of combat
    // (a restore's follow order would pull a pet out of a trash fight), and forget those restored. Called only while
    // the encounter is not IN_PROGRESS. The owners are resolved map-locally, so they share the caller's map thread;
    // no lock is held while restoring. An owner not on the map, or with no live pet yet, stays recorded.
    void TwinsRestoreTouchedPets(Player* bot, uint32 instanceId)
    {
        if (!twinsPetTouchedCount.load(std::memory_order_relaxed))
            return;
        uint32 const now = TwinsNowMs();
        std::vector<ObjectGuid> owners;
        {
            std::lock_guard<std::mutex> lock(twinsPetMutex);
            auto it = twinsPetTouchedByInstance.find(instanceId);
            if (it == twinsPetTouchedByInstance.end())
                return;
            if (it->second.checkMs && getMSTimeDiff(it->second.checkMs, now) < TWINS_PET_RESTORE_CHECK_MS)
                return;
            it->second.checkMs = now;
            owners.assign(it->second.owners.begin(), it->second.owners.end());
        }

        std::vector<ObjectGuid> restored;
        for (ObjectGuid const& guid : owners)
        {
            Player* const owner = ObjectAccessor::GetPlayer(*bot, guid);
            if (!owner || !owner->IsInWorld() || owner->IsInCombat() || IsTwinsFormUp(owner))
                continue;
            if (TwinsRestorePets(owner))
            {
                restored.push_back(guid);
                LOG_DEBUG("playerbots", "[Aq40Twins] instance {} pets restored: {}", instanceId, owner->GetName());
            }
        }
        if (restored.empty())
            return;

        std::lock_guard<std::mutex> lock(twinsPetMutex);
        auto it = twinsPetTouchedByInstance.find(instanceId);
        if (it == twinsPetTouchedByInstance.end())
            return;
        for (ObjectGuid const& guid : restored)
            it->second.owners.erase(guid);
        if (it->second.owners.empty())
            twinsPetTouchedByInstance.erase(it);
        twinsPetTouchedCount.store(uint32(twinsPetTouchedByInstance.size()), std::memory_order_relaxed);
    }

    size_t SideIndex(Side s) { return s == Side::B ? 1 : 0; }

    char const* SideTag(Side s) { return s == Side::A ? "A" : (s == Side::B ? "B" : "-"); }

    Position const& StationOf(Side s) { return TWINS_STATIONS[SideIndex(s)].station; }

    // healers[rank % healerCount] / ranged[rank % rangedCount] of a side (Revision 2: B seats 7 healers, not 8).
    Position const& TwinsHealerSpot(Side s, uint8 rank)
    {
        TwinsStationSpots const& st = TWINS_STATIONS[SideIndex(s)];
        return st.healers[rank % std::max<uint8>(st.healerCount, 1)];
    }

    Position const& TwinsRangedSpot(Side s, uint8 rank)
    {
        TwinsStationSpots const& st = TWINS_STATIONS[SideIndex(s)];
        return st.ranged[rank % std::max<uint8>(st.rangedCount, 1)];
    }

    bool TwinsInLandingWindow(Creature* twin) { return twin->IsInCombat() && twin->GetReactState() == REACT_PASSIVE; }

    // Every group member on the bot's map, in world (the bot alone when ungrouped, though GetTwinsView no longer
    // refreshes from an ungrouped bot). The ONE group pass per refresh:
    // every later step iterates this vector once, never nested.
    std::vector<Player*> ScanTwinsGroup(Player* bot)
    {
        std::vector<Player*> present;
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (member && member->IsInWorld() && member->GetMap() == bot->GetMap())
                    present.push_back(member);
            }
        }
        else
            present.push_back(bot);
        return present;
    }

    TwinsLatched const* FindLatched(TwinsInstanceState const& st, ObjectGuid guid)
    {
        auto it = st.indexByGuid.find(guid);
        return it == st.indexByGuid.end() ? nullptr : &st.members[it->second];
    }

    // The AssignTwins input from the living members of one group scan. Shared by the engage latch and the form-up
    // staging cache (spec §5.5), so both compute the same deterministic assignment from the same raid.
    void BuildTwinsMembers(std::vector<Player*> const& present, std::vector<Aq40TwinsRules::Member>& input,
                           std::vector<Player*>& who)
    {
        input.clear();
        who.clear();
        input.reserve(present.size());
        who.reserve(present.size());
        for (Player* p : present)
        {
            if (!p->IsAlive())
                continue;
            // A bot is `ai && !IsSelfBot` (project convention): a human, selfbot or not, is never designated.
            PlayerbotAI* const ai = GET_PLAYERBOT_AI(p);
            input.push_back(Aq40TwinsRules::Member{p->GetGUID().GetCounter(), uint8(p->getClass()),
                                                   ai && !IsSelfBot(p), PlayerbotAI::IsTank(p),
                                                   PlayerbotAI::IsHeal(p), PlayerbotAI::IsMelee(p)});
            who.push_back(p);
        }
    }

    // Each healer's guid rank among its side's healers (TwinsMe::rankOnSide), in input order; 0 for everyone else.
    std::vector<uint8> TwinsHealerRanks(std::vector<Aq40TwinsRules::Assignment> const& out,
                                        std::vector<Aq40TwinsRules::Member> const& input)
    {
        std::vector<uint8> ranks(out.size(), 0);
        for (Side s : {Side::A, Side::B})
        {
            std::vector<size_t> healers;
            for (size_t i = 0; i < out.size(); ++i)
                if (out[i].role == Role::Healer && out[i].side == s)
                    healers.push_back(i);
            std::sort(healers.begin(), healers.end(),
                      [&input](size_t x, size_t y) { return input[x].guid < input[y].guid; });
            for (size_t k = 0; k < healers.size(); ++k)
                ranks[healers[k]] = uint8(k);
        }
        return ranks;
    }

    struct TwinsRangedSlot
    {
        TwinsRangedKind kind = TwinsRangedKind::None;
        uint8 rank = 0;
    };

    // Revision 2 §11.4: each bot's inner ranged arc and its guid rank on it, in input order. Magic: not IsPhysical,
    // not a tank, not a healer; Role::Dps ranks first (spare warlocks and fill-in candidates included), then the
    // magic warlock tanks (real or fill-in) after them, so one demoted to Dps later still gets a distinct spot.
    // Hunter: cls 3 with Role::Dps, ranked among the hunters. Everyone else: None (melee keep native positioning).
    std::vector<TwinsRangedSlot> TwinsRangedRanks(std::vector<Aq40TwinsRules::Assignment> const& out,
                                                  std::vector<Aq40TwinsRules::Member> const& input)
    {
        std::vector<TwinsRangedSlot> slots(out.size());
        std::vector<size_t> magic;
        std::vector<size_t> magicTanks;
        std::vector<size_t> hunters;
        for (size_t i = 0; i < out.size(); ++i)
        {
            Aq40TwinsRules::Member const& m = input[i];
            if (!m.isBot)
                continue;
            if (out[i].role == Role::Dps && m.cls == 3)
                hunters.push_back(i);
            else if (!m.isTank && !m.isHeal && !Aq40TwinsRules::IsPhysical(m.cls, m.isMelee))
            {
                if (out[i].role == Role::Dps)
                    magic.push_back(i);
                else if (out[i].role == Role::WarlockTank)
                    magicTanks.push_back(i);
            }
        }
        auto byGuid = [&input](size_t x, size_t y) { return input[x].guid < input[y].guid; };
        std::sort(magic.begin(), magic.end(), byGuid);
        std::sort(magicTanks.begin(), magicTanks.end(), byGuid);
        std::sort(hunters.begin(), hunters.end(), byGuid);
        magic.insert(magic.end(), magicTanks.begin(), magicTanks.end());
        for (size_t k = 0; k < magic.size(); ++k)
            slots[magic[k]] = TwinsRangedSlot{TwinsRangedKind::Magic, uint8(k)};
        for (size_t k = 0; k < hunters.size(); ++k)
            slots[hunters[k]] = TwinsRangedSlot{TwinsRangedKind::Hunter, uint8(k)};
        return slots;
    }

    // Step 1: latch from the living members present at engage. A member the latch does not see (dead, elsewhere,
    // or joining later) has no assignment for the attempt.
    void LatchTwins(TwinsInstanceState& st, std::vector<Player*> const& present, uint32 now)
    {
        std::vector<Aq40TwinsRules::Member> input;
        std::vector<Player*> who;
        BuildTwinsMembers(present, input, who);

        // AssignTwins returns one Assignment per input, in input order.
        std::vector<Aq40TwinsRules::Assignment> const out = Aq40TwinsRules::AssignTwins(input);
        std::vector<uint8> const ranks = TwinsHealerRanks(out, input);
        std::vector<TwinsRangedSlot> const ranged = TwinsRangedRanks(out, input);
        st.members.clear();
        st.indexByGuid.clear();
        st.members.reserve(out.size());
        for (size_t i = 0; i < out.size(); ++i)
        {
            ObjectGuid const guid = who[i]->GetGUID();
            st.indexByGuid.emplace(guid, uint32(st.members.size()));
            st.members.push_back(
                TwinsLatched{guid, who[i]->GetName(), out[i], input[i], ranks[i], ranged[i].kind, ranged[i].rank});
            if (out[i].side == Side::None)
                continue;
            if (out[i].role == Role::MeleeTank)
                st.melee[SideIndex(out[i].side)] = guid;
            else if (out[i].role == Role::WarlockTank)
                st.warlock[SideIndex(out[i].side)] = guid;
        }

        // Healer ranks (TwinsHealerRanks, above) hold for the attempt: healers are never replaced. Ranged ranks
        // (TwinsRangedRanks) hold for it too; a death replacement only moves bots between tank slots and Dps.
        st.latched = true;
        st.engageMs = now;
        st.veklorSide = Side::A;  // he spawns on A (spec §5.5): warlock A and warrior B are active at the pull
        st.windowVeklorFrom = Side::None;
        st.windowVeklorPos = Position();
        st.windowVeknilashPos = Position();
        st.lastWindowEndMs = 0;
        st.lastWindowStartMs = 0;
        st.inWindow = false;
        st.diagMs = 0;
        for (size_t i = 0; i < 2; ++i)
        {
            st.anchor[i] = TWINS_STATIONS[i].station;
            st.settleCand[i] = Position();
            st.settleSinceMs[i] = 0;
            st.pickupBasis[i] = Position();
            st.pickupBasisValid[i] = false;
            st.pickupBasisSet[i] = false;
        }
        st.anchorEpoch = 0;
        st.spotTrack.clear();
        st.dragSide = Side::None;
        st.stepIn.clear();
        st.manaRefilled.clear();
    }

    // Revision 3 §12.2: a new anchor epoch. Every parked bot gets its anchored spot back (and its failures reset).
    void TwinsBumpAnchorEpoch(TwinsInstanceState& st)
    {
        ++st.anchorEpoch;
        st.spotTrack.clear();
    }

    // Revision 3 §12.2: follow one twin's settle point on its side, re-latching that side's anchor once it has
    // settled (Aq40TwinsRules::AnchorShouldRelatch). Only called outside the landing window.
    void TwinsTrackAnchor(TwinsInstanceState& st, Creature* twin, Side side, uint32 now, TwinsRefreshEvents& ev)
    {
        size_t const i = SideIndex(side);
        float const d = st.settleSinceMs[i] ? twin->GetExactDist2d(st.settleCand[i]) : 0.0f;
        if (!st.settleSinceMs[i] || d > Aq40TwinsRules::ANCHOR_SETTLE_RADIUS)
        {
            st.settleCand[i] = twin->GetPosition();
            st.settleSinceMs[i] = now;
            return;
        }
        // Fix round 1 (R22): the candidate is clamped to TWINS_ANCHOR_MAX_OFFSET from the AUTHORED station before the
        // relatch test, so the anchor cannot ratchet outward swap after swap (Vek'nilash settles on the pickup tank's
        // outer waiting spot, which the next anchor would push out again).
        Position const& station = TWINS_STATIONS[i].station;
        Aq40TwinsRules::Pt2 const c = Aq40TwinsRules::ClampToRadius(
            {station.GetPositionX(), station.GetPositionY()},
            {st.settleCand[i].GetPositionX(), st.settleCand[i].GetPositionY()}, TWINS_ANCHOR_MAX_OFFSET);
        Position const clamped(c.x, c.y, st.settleCand[i].GetPositionZ());
        if (!Aq40TwinsRules::AnchorShouldRelatch(d, getMSTimeDiff(st.settleSinceMs[i], now),
                                                 clamped.GetExactDist2d(st.anchor[i])))
            return;
        st.anchor[i] = clamped;
        TwinsBumpAnchorEpoch(st);
        ev.anchors.push_back(Acore::StringFormat("{} -> ({:.1f},{:.1f}) off={:.1f}", SideTag(side),
                                                 st.anchor[i].GetPositionX(), st.anchor[i].GetPositionY(),
                                                 st.anchor[i].GetExactDist2d(TWINS_STATIONS[i].station)));
    }

    // Step 2: a designated tank who is dead or no longer on the map (a released ghost is outside the instance) is
    // replaced by a living spare of the same kind, same side first (Aq40TwinsRules::PickReplacement). The promotion
    // holds for the attempt; the old holder becomes a spare (a fill-in caster becomes plain DPS), so a resurrected
    // tank does not reclaim the slot. With no candidate the slot keeps its dead holder and the diag shows it.
    void ReplaceDeadTanks(TwinsInstanceState& st, std::vector<Player*> const& present, TwinsRefreshEvents& ev)
    {
        std::unordered_set<ObjectGuid> living;
        std::vector<std::pair<Aq40TwinsRules::Assignment, Aq40TwinsRules::Member>> candidates;
        std::vector<uint32> candidateIdx;
        for (Player* p : present)
        {
            if (!p->IsAlive())
                continue;
            auto it = st.indexByGuid.find(p->GetGUID());
            if (it == st.indexByGuid.end())
                continue;
            living.insert(p->GetGUID());
            TwinsLatched const& l = st.members[it->second];
            candidates.emplace_back(l.a, l.m);
            candidateIdx.push_back(it->second);
        }

        struct Slot
        {
            ObjectGuid* holder;
            Role role;
            Side side;
        };
        Slot const slots[4] = {{&st.melee[0], Role::MeleeTank, Side::A},
                               {&st.melee[1], Role::MeleeTank, Side::B},
                               {&st.warlock[0], Role::WarlockTank, Side::A},
                               {&st.warlock[1], Role::WarlockTank, Side::B}};
        for (Slot const& slot : slots)
        {
            if (slot.holder->IsEmpty() || living.count(*slot.holder))
                continue;
            uint32 const pick = Aq40TwinsRules::PickReplacement(slot.role, slot.side, candidates);
            if (!pick)
                continue;
            size_t k = 0;
            while (k < candidates.size() && candidates[k].first.guid != pick)
                ++k;
            if (k == candidates.size())
                continue;

            TwinsLatched& promoted = st.members[candidateIdx[k]];
            auto oldIt = st.indexByGuid.find(*slot.holder);
            if (oldIt != st.indexByGuid.end())
            {
                TwinsLatched& old = st.members[oldIt->second];
                old.a.role = Role::Dps;
                if (old.a.fillIn)
                {
                    old.a.side = Side::None;
                    old.a.spare = false;
                    old.a.fillIn = false;
                }
                else
                    old.a.spare = true;  // keeps its side: a same-side spare from now on
                ev.replacements.push_back(old.name + " -> " + promoted.name);
            }
            promoted.a.role = slot.role;
            promoted.a.side = slot.side;
            promoted.a.spare = false;
            promoted.a.fillIn = false;
            candidates[k].first = promoted.a;  // not a spare any more: never picked for a second slot
            *slot.holder = promoted.guid;
        }
    }

    // Revision 4 §13.2 / §13.4, one pass over the latched healers (plus one over `present` for the four slot holders).
    // Step-in: each healer runs one clock for one of ITS side's designated tanks (melee, then warlock; active or
    // waiting), measured 2-D from the healer's spot (its anchored healer spot, or where it stands while parked), so the
    // step-in point itself (STEP_IN_DIST from the tank) does not end it. It starts once the tank is below 100 % and
    // beyond STEP_IN_TRIGGER for STEP_IN_MS; it ends when the tank is back within STEP_IN_END of the spot, dead, or no
    // longer that side's designated tank (ruling R25: not at full health). Mana: a healer bot under the
    // playerbots `raid` cheat below MANA_REFILL_BELOW_PCT is set to full once per attempt (the Nefarian / Huhuran
    // refill pattern; same map thread as the refreshing bot, so setting another member's power is safe).
    void TwinsHealerPass(TwinsInstanceState& st, std::vector<Player*> const& present, uint32 now,
                         TwinsRefreshEvents& ev)
    {
        ObjectGuid const guids[4] = {st.melee[0], st.melee[1], st.warlock[0], st.warlock[1]};
        Player* holders[4] = {nullptr, nullptr, nullptr, nullptr};
        for (Player* p : present)
            for (size_t k = 0; k < 4; ++k)
                if (!guids[k].IsEmpty() && p->GetGUID() == guids[k])
                    holders[k] = p;

        for (Player* p : present)
        {
            TwinsLatched const* l = FindLatched(st, p->GetGUID());
            if (!l || l->a.role != Role::Healer || l->a.side == Side::None)
                continue;
            ObjectGuid const g = p->GetGUID();
            if (!p->IsAlive())
            {
                st.stepIn.erase(g);
                continue;
            }

            uint32 const maxMana = p->GetMaxPower(POWER_MANA);
            if (maxMana > 0 && 100.0f * float(p->GetPower(POWER_MANA)) < Aq40TwinsRules::MANA_REFILL_BELOW_PCT *
                                                                            float(maxMana) &&
                !st.manaRefilled.count(g) && !IsSelfBot(p))
                if (PlayerbotAI* const ai = GET_PLAYERBOT_AI(p))
                    if (ai->HasCheat(BotCheatMask::raid))
                    {
                        p->SetPower(POWER_MANA, maxMana);
                        st.manaRefilled.insert(g);
                        ev.manaRefills.push_back(p->GetName());
                    }

            size_t const i = SideIndex(l->a.side);
            auto const track = st.spotTrack.find(g);
            float hx = p->GetPositionX();
            float hy = p->GetPositionY();
            float hz = p->GetPositionZ();
            if (track == st.spotTrack.end() || !track->second.parked)
            {
                Position const& authored = TwinsHealerSpot(l->a.side, l->rankOnSide);
                Position const& station = TWINS_STATIONS[i].station;
                hx = authored.GetPositionX() + st.anchor[i].GetPositionX() - station.GetPositionX();
                hy = authored.GetPositionY() + st.anchor[i].GetPositionY() - station.GetPositionY();
                // Revision 11 §20.2: the LOS ray's end (IsWithinLOS adds the collision height to both ends).
                hz = authored.GetPositionZ() + st.anchor[i].GetPositionZ() - station.GetPositionZ();
            }
            size_t const slots[2] = {i, 2 + i};  // this side's melee tank, then its warlock
            auto holderOf = [&](ObjectGuid tank) -> Player*
            {
                for (size_t k : slots)
                    if (!guids[k].IsEmpty() && guids[k] == tank)
                        return holders[k];
                return nullptr;
            };

            auto it = st.stepIn.find(g);
            if (it != st.stepIn.end())
            {
                // Ruling R25: `due` (stepping in) switches the distance clock to distance-and-life only; a running
                // LOS step-in ends only on LOS within STEP_IN_END or death. No longer designated (no holder): ends.
                Player* const t = holderOf(it->second.tank);
                if (t && TwinsAdvanceStepIn(it->second, t, hx, hy, hz, now))
                    continue;
                st.stepIn.erase(it);
            }
            for (size_t k : slots)
            {
                Player* const t = holders[k];
                if (!t)
                    continue;
                TwinsStepIn rec;
                rec.tank = guids[k];
                if (TwinsAdvanceStepIn(rec, t, hx, hy, hz, now))
                {
                    st.stepIn[g] = rec;
                    break;
                }
            }
        }
    }

    // Steps 1-6 on a private copy of the instance state; the caller writes it back under the lock.
    void RefreshTwinsState(TwinsInstanceState& st, std::vector<Player*> const& present, Creature* veklor,
                           Creature* veknilash, uint32 now, TwinsRefreshEvents& ev)
    {
        if (!st.latched)
        {
            LatchTwins(st, present, now);
            ev.latchedNow = true;
        }
        ReplaceDeadTanks(st, present, ev);

        // Step 3: Vek'lor's side from geometry with hysteresis, so a teleport no bot saw self-corrects.
        Side const prevSide = st.veklorSide;
        st.veklorSide = Aq40TwinsRules::UpdateVeklorSide(st.veklorSide, veklor->GetExactDist2d(StationOf(Side::A)),
                                                         veklor->GetExactDist2d(StationOf(Side::B)));

        // Step 4/5: the landing window and its edges. The positions have already swapped at the rising edge, so
        // the side he came FROM is the pre-update one.
        bool const window = TwinsInLandingWindow(veklor) || TwinsInLandingWindow(veknilash);
        if (window && !st.inWindow)
        {
            st.windowVeklorFrom = prevSide;
            // Ruling R15: a teleport always swaps the twins; an off-station landing (> VEKLOR_SIDE_RADIUS from both
            // stations) would otherwise keep the old side and invert every active/waiting label for the phase.
            st.veklorSide = Aq40TwinsRules::SideAfterTeleport(prevSide, st.veklorSide);
            // Ruling R13 F4: both have already teleported, and both are rooted for the window.
            st.windowVeklorPos = veklor->GetPosition();
            st.windowVeknilashPos = veknilash->GetPosition();
            st.lastWindowStartMs = now;
            // Revision 8 §17.1: both twins moved, so each side's pickup basis re-latches at the first refresh after
            // the window (the window itself keeps measuring from the current basis).
            st.pickupBasisValid[0] = false;
            st.pickupBasisValid[1] = false;
            // Revision 4 §13.5: the kite target is re-chosen after every swap (the latch below re-picks it from where
            // Vek'lor has just landed, if the twins are still inside the guard).
            st.dragSide = Side::None;
        }
        if (!window && st.inWindow)
        {
            st.lastWindowEndMs = now;
            ev.windowEnded = true;
            TwinsBumpAnchorEpoch(st);
        }
        st.inWindow = window;

        // Fix round 1 (R22): latch the drag station when the separation guard comes up, clear it when it goes down,
        // so the target does not flip while Vek'lor moves between the two stations' bisector.
        if (veklor->GetExactDist2d(veknilash) >= Aq40TwinsRules::SEPARATION_GUARD)
            st.dragSide = Side::None;
        else if (st.dragSide == Side::None)
            st.dragSide = veklor->GetExactDist2d(StationOf(Side::A)) >= veklor->GetExactDist2d(StationOf(Side::B))
                              ? Side::A
                              : Side::B;

        // Revision 3 §12.2: anchors never re-latch inside the window. The twins swap sides there, so the settle
        // trackers restart afterwards.
        if (window)
        {
            st.settleSinceMs[0] = 0;
            st.settleSinceMs[1] = 0;
        }
        else
        {
            TwinsTrackAnchor(st, veklor, st.veklorSide, now, ev);
            TwinsTrackAnchor(st, veknilash, Aq40TwinsRules::Other(st.veklorSide), now, ev);
            // Revision 8 §17.1: the pickup basis of each side follows the twin on it (Vek'lor on veklorSide, Vek'nilash
            // on the other), re-latching only past PICKUP_BASIS_RELATCH so the waiting spots hold while he is shuffled.
            for (size_t i = 0; i < 2; ++i)
            {
                Creature* const twin = SideIndex(st.veklorSide) == i ? veklor : veknilash;
                if (Aq40TwinsRules::BasisShouldRelatch(st.pickupBasisValid[i], twin->GetExactDist2d(st.pickupBasis[i])))
                {
                    // Fix round 1: a real move (not the re-latch forced by the latch or a window) moves this side's
                    // waiting spot, so its holder's park record goes too; a parked pickup would otherwise stay frozen
                    // until the next window close, i.e. through the landing it waits for. Not a global epoch bump.
                    if (st.pickupBasisValid[i])
                    {
                        st.spotTrack.erase(st.melee[i]);
                        st.spotTrack.erase(st.warlock[i]);
                    }
                    st.pickupBasis[i] = twin->GetPosition();
                    st.pickupBasisValid[i] = true;
                    st.pickupBasisSet[i] = true;
                }
            }
        }

        // Revision 4 §13.2 / §13.4 (after the anchors, which the healer spots are measured from).
        TwinsHealerPass(st, present, now, ev);

        // Step 6: diag throttle (first diag at the latch).
        if (!st.diagMs || getMSTimeDiff(st.diagMs, now) >= Aq40TwinsRules::DIAG_INTERVAL_MS)
        {
            st.diagMs = now;
            ev.diagDue = true;
            ev.tankHeals = st.tankHeals;
            st.tankHeals = 0;
        }
        st.refreshMs = now;
    }

    void FillTwinsView(TwinsInstanceState const& st, Creature* veklor, Creature* veknilash, uint32 now,
                       TwinsView& view)
    {
        view.valid = true;
        view.veklor = veklor;
        view.veknilash = veknilash;
        view.veklorSide = st.veklorSide;
        view.windowVeklorFrom = st.windowVeklorFrom;
        view.windowVeklorPos = st.windowVeklorPos;
        view.windowVeknilashPos = st.windowVeknilashPos;
        view.landingWindow = st.inWindow;
        view.msSinceWindowOrEngage =
            st.inWindow ? 0 : getMSTimeDiff(st.lastWindowEndMs ? st.lastWindowEndMs : st.engageMs, now);
        view.msSinceWindowStart =
            st.inWindow ? 0 : getMSTimeDiff(st.lastWindowStartMs ? st.lastWindowStartMs : st.engageMs, now);
        for (size_t s = 0; s < 2; ++s)
        {
            view.activeMelee[s] = st.melee[s];
            view.activeWarlock[s] = st.warlock[s];
            view.anchor[s] = st.anchor[s];
            view.pickupBasis[s] = st.pickupBasis[s];
            view.pickupBasisSet[s] = st.pickupBasisSet[s];
        }
        view.dragSide = st.dragSide;
    }

    // "mt/A", "lt*/B" (fill-in), "heal/A", "spare/B", "dps/-", "human/-", or "unlatched".
    std::string TwinsTagOf(TwinsInstanceState const& st, Player* p)
    {
        TwinsLatched const* l = FindLatched(st, p->GetGUID());
        if (!l)
            return "unlatched";
        std::string role;
        switch (l->a.role)
        {
            case Role::MeleeTank:
                role = "mt";
                break;
            case Role::WarlockTank:
                role = l->a.fillIn ? "lt*" : "lt";
                break;
            case Role::Healer:
                role = "heal";
                break;
            case Role::Dps:
                role = l->a.spare ? "spare" : "dps";
                break;
            default:
                role = l->m.isBot ? "none" : "human";
                break;
        }
        return role + "/" + SideTag(l->a.side);
    }

    std::string TwinsDescribeUnit(TwinsInstanceState const& st, Unit* u)
    {
        if (!u)
            return "none";
        if (u->IsTotem())
        {
            Player* const owner = u->GetCharmerOrOwnerPlayerOrPlayerItself();
            return "totem:" + u->GetName() + (owner ? "(" + owner->GetName() + ")" : std::string());
        }
        if (Player* const p = u->ToPlayer())
            return p->GetName() + "(" + TwinsTagOf(st, p) + ")";
        if (Player* const owner = u->GetCharmerOrOwnerPlayerOrPlayerItself())
            return "pet:" + u->GetName() + "(" + owner->GetName() + " " + TwinsTagOf(st, owner) + ")";
        return u->GetName();
    }

    // "Name", "Name*" (fill-in), "Name(dead)" (not living on the map), or "none".
    std::string TwinsSlotName(TwinsInstanceState const& st, ObjectGuid guid,
                              std::unordered_set<ObjectGuid> const& living)
    {
        TwinsLatched const* l = guid.IsEmpty() ? nullptr : FindLatched(st, guid);
        if (!l)
            return "none";
        return l->name + (l->a.fillIn ? "*" : "") + (living.count(guid) ? "" : "(dead)");
    }

    // At the latch: a LOG_WARN naming every fill-in warlock tank and any side without a melee or warlock tank
    // (spec §5.2 "log it loudly": a fill-in has no Searing Pain threat doubling), else a LOG_DEBUG summary.
    void LogTwinsLatch(uint32 instanceId, TwinsInstanceState const& st)
    {
        std::string issues;
        auto add = [&issues](std::string const& s) { issues += (issues.empty() ? "" : "; ") + s; };
        for (Side s : {Side::A, Side::B})
        {
            size_t const i = SideIndex(s);
            if (st.melee[i].IsEmpty())
                add(std::string("no melee tank on side ") + SideTag(s));
            if (st.warlock[i].IsEmpty())
                add(std::string("no warlock tank on side ") + SideTag(s));
            else if (TwinsLatched const* l = FindLatched(st, st.warlock[i]))
                if (l->a.fillIn)
                    add("fill-in warlock tank on side " + std::string(SideTag(s)) + ": " + l->name + " (class " +
                        std::to_string(l->m.cls) + ", no Searing Pain threat doubling)");
        }

        uint32 healers[2] = {0, 0};
        for (TwinsLatched const& l : st.members)
            if (l.a.role == Role::Healer && l.a.side != Side::None)
                ++healers[SideIndex(l.a.side)];

        auto name = [&st](ObjectGuid g)
        {
            TwinsLatched const* l = g.IsEmpty() ? nullptr : FindLatched(st, g);
            return l ? l->name : std::string("none");
        };
        if (!issues.empty())
            LOG_WARN("playerbots", "[Aq40Twins] instance {} latch: {}", instanceId, issues);
        LOG_DEBUG("playerbots",
                  "[Aq40Twins] instance {} latched {} members: mtA={} mtB={} ltA={} ltB={} healers A={} B={}",
                  instanceId, st.members.size(), name(st.melee[0]), name(st.melee[1]), name(st.warlock[0]),
                  name(st.warlock[1]), healers[0], healers[1]);
    }

    // One line per landing window, at its falling edge: the body each twin took, its role and side, and its
    // distance to that twin's station (now that the window is over, Vek'lor's side is where he landed). Ruling R13
    // F4: also where each twin landed (the rising-edge positions the landing zones were centred on) and that point's
    // distance from the station of the side it landed on, so an off-station landing is visible.
    void LogTwinsLanding(TwinsInstanceState const& st, Creature* veklor, Creature* veknilash, uint32 now)
    {
        Side const vlSide = st.veklorSide;
        Side const vnSide = Aq40TwinsRules::Other(vlSide);
        auto dist = [](Unit* victim, Side s)
        {
            if (!victim || s == Side::None)
                return -1.0f;
            return victim->GetExactDist2d(StationOf(s));
        };
        // Vek'lor landed on the side he did not come from; Vek'nilash on the side Vek'lor came from.
        Side const vlLandSide =
            st.windowVeklorFrom != Side::None ? Aq40TwinsRules::Other(st.windowVeklorFrom) : vlSide;
        Side const vnLandSide = Aq40TwinsRules::Other(vlLandSide);
        Unit* const vlVictim = veklor->GetVictim();
        Unit* const vnVictim = veknilash->GetVictim();
        LOG_DEBUG("playerbots",
                  "[Aq40Twins] landing t={} from={} veklorSide={} veklor->{} d={:.1f} veknilash->{} d={:.1f} "
                  "veklorLand=({:.1f},{:.1f}) off={:.1f} veknilashLand=({:.1f},{:.1f}) off={:.1f}",
                  getMSTimeDiff(st.engageMs, now), SideTag(st.windowVeklorFrom), SideTag(vlSide),
                  TwinsDescribeUnit(st, vlVictim), dist(vlVictim, vlSide), TwinsDescribeUnit(st, vnVictim),
                  dist(vnVictim, vnSide), st.windowVeklorPos.GetPositionX(), st.windowVeklorPos.GetPositionY(),
                  st.windowVeklorPos.GetExactDist2d(StationOf(vlLandSide)), st.windowVeknilashPos.GetPositionX(),
                  st.windowVeknilashPos.GetPositionY(), st.windowVeknilashPos.GetExactDist2d(StationOf(vnLandSide)));
    }

    // Revision 2 §11.2 on raw slot guids, shared by TwinsWrongVictimOf / TwinsWrongVictimFor (from the view) and the
    // diag (from the state). The active slots are labelled from the live veklorSide, as TwinsActiveTankFor does; the
    // victim is alive, so a guid match is the "designated active tank" test. A REACT_PASSIVE twin attacks nobody: Twin
    // Teleport sets it passive with its pre-teleport victim still in m_attacking until its next UpdateVictim
    // AttackStop()s it, and that stale victim (the old active tank, now labelled waiting) must not walk 120 y after it.
    // Vek'lor first: a body both twins hold keeps Vek'lor where he is.
    Creature* TwinsWrongVictimTwin(Player* p, Creature* veklor, Creature* veknilash, Side veklorSide,
                                   ObjectGuid const (&melee)[2], ObjectGuid const (&warlock)[2])
    {
        ObjectGuid const g = p->GetGUID();
        auto attacking = [p](Creature* twin)
        { return twin->GetVictim() == p && twin->GetReactState() != REACT_PASSIVE; };
        bool const anyMelee = melee[0] == g || melee[1] == g;
        bool const anyWarlock = warlock[0] == g || warlock[1] == g;
        if (Aq40TwinsRules::IsWrongVictim(attacking(veklor), warlock[SideIndex(veklorSide)] == g, anyMelee))
            return veklor;
        if (Aq40TwinsRules::IsWrongVictim(attacking(veknilash),
                                          melee[SideIndex(Aq40TwinsRules::Other(veklorSide))] == g, anyWarlock))
            return veknilash;
        return nullptr;
    }

    // Revision 3 §12.1 on raw state, shared by TwinsSpotFor / IsTwinsHoldingFire (from the view) and the diag (from the
    // state): the melee pre-swap move is due for this latched bot. Outside the window, PreSwapDue on the ms since the
    // last window started (or engage); a Dps with no ranged arc that is a spare (a spare melee tank: spare warlocks
    // hold the magic arc) or a physical melee non-hunter; never a wrong victim (the no-drag rule wins). Designated
    // tanks and healers are not Dps.
    bool TwinsPreSwapRaw(Player* p, Role role, bool spare, TwinsRangedKind rangedKind, bool inWindow,
                         uint32 msSinceWindowStart, Creature* veklor, Creature* veknilash, Side veklorSide,
                         ObjectGuid const (&melee)[2], ObjectGuid const (&warlock)[2])
    {
        if (inWindow || role != Role::Dps || rangedKind != TwinsRangedKind::None ||
            !Aq40TwinsRules::PreSwapDue(msSinceWindowStart))
            return false;
        if (!spare)
        {
            bool const isMelee = PlayerbotAI::IsMelee(p);
            uint8 const cls = uint8(p->getClass());
            if (cls == CLASS_HUNTER || !isMelee || !Aq40TwinsRules::IsPhysical(cls, isMelee))
                return false;
        }
        if (TwinsWrongVictimTwin(p, veklor, veknilash, veklorSide, melee, warlock))
            return false;
        // Revision 4 §13.1: a mutated bug within the bug-target radius (BUG_SWITCH_RADIUS, 40 y, ruling R25) is fought
        // normally (target, no hold-fire, no spot, so native melee movement reaches it); the move resumes once it is
        // dead. Last: it is the only grid search here.
        // Revision 7 §16.2: the same melee filter as the Dps target pick, so a bug beside Vek'lor never ends the move.
        Creature* const bug = FindTwinsMutatedBug(p, true);
        return !bug || p->GetExactDist2d(bug) > Aq40TwinsRules::BUG_SWITCH_RADIUS;
    }

    // Ruling R16 H4: why the active warlock is not casting, in TwinsShouldWarlockTank's terms
    // (TwinsWarlockCastsFromHere's three tests in its order, then the not-moving gate). "cast" = nothing blocks it.
    // Revision 10 §19.2: no "window" blocker any more. Inside the landing window the slot the diag names (the live
    // veklorSide's warlock) is the Vek'lor pickup, which casts; inside12 does not apply while Vek'lor is REACT_PASSIVE.
    char const* TwinsWarlockCastBlocker(Player* warlock, Creature* veklor)
    {
        if (veklor->GetReactState() != REACT_PASSIVE &&
            warlock->GetExactDist2d(veklor) < Aq40TwinsRules::WRONG_VICTIM_MIN)
            return "inside12";
        if (!warlock->IsWithinCombatRange(veklor, sPlayerbotAIConfig.SpellDistance))
            return "range";
        if (!warlock->IsWithinLOSInMap(veklor))
            return "los";
        if (warlock->isMoving())
            return "moving";
        return "cast";
    }

    // One line per instance every DIAG_INTERVAL_MS at Logger.playerbots = 5 (spec §6): who each twin is hitting
    // (role and side), whether Heal Brother can fire, where the non-healers' attacks are going, each twin's active
    // tank threat against the top non-tank threat, healer mana, the latched tanks, and the active warlock's cast
    // blocker (vlCast: cast / inside12 / range / los / moving; none = no living active warlock), and the
    // number of latched bots that are a wrong victim right now (wrongVictims, Revision 2 §11.2), in the melee pre-swap
    // move (preSwap, Revision 3 §12.1) and parked on an unreachable spot (parked, §12.2). Revision 4: living players,
    // bots and humans alike, within ARCANE_BURST_TRIGGER (11 y, 2-D) of Vek'lor (nearVeklor, §13.3) and healers whose
    // step-in is due (stepIn, §13.2). Revision 5: living healers within HEAL_REACH (36 y, 2-D) of each twin's victim
    // when that victim is its designated active tank (healersInRange=vl:N/L vn:M/L, "-" otherwise, §14.4; Revision 11
    // §20.3: L = those of the N with LOS to that tank, IsWithinLOSInMap from the healer). Revision 6:
    // successful side tank heals since the previous diag line (tankHeals, §15.1). Revision 7 §16.3: nearVeklor names
    // them, nearVeklor=N(name/role/side,...), with the victim descriptions' tags (TwinsTagOf); a bare N when none.
    // Revision 8 §17.1: pickup=A:d,B:d, each side's living waiting pickup (the melee tank on Vek'lor's side, the
    // warlock on Vek'nilash's) and its 2-D distance to the twin on that side ("-" = none), so a far pickup shows.
    // ONE pass over `present`.
    void LogTwinsDiag(Player* bot, TwinsInstanceState const& st, std::vector<Player*> const& present,
                      Creature* veklor, Creature* veknilash, uint32 now, uint32 tankHeals)
    {
        Side const vlSide = st.veklorSide;
        Side const vnSide = Aq40TwinsRules::Other(vlSide);
        ObjectGuid const vlTankGuid = st.warlock[SideIndex(vlSide)];
        ObjectGuid const vnTankGuid = st.melee[SideIndex(vnSide)];

        uint32 onVeklor = 0;
        uint32 onVeknilash = 0;
        uint32 onBug = 0;
        uint32 onOther = 0;
        uint32 idle = 0;
        uint32 dead = 0;
        float healManaSum = 0.0f;
        uint32 healManaCount = 0;
        float vlTankThreat = -1.0f;
        float vnTankThreat = -1.0f;
        float vlTop = -1.0f;
        float vnTop = -1.0f;
        std::string vlTopName = "none";
        std::string vnTopName = "none";
        char const* vlCast = "none";
        uint32 wrongVictims = 0;
        uint32 preSwap = 0;
        uint32 nearVeklor = 0;
        std::string nearVeklorNames;  // Revision 7 §16.3: "name/role/side" (TwinsTagOf), comma-separated
        // Revision 8 §17.1: each side's living waiting pickup (melee tank on Vek'lor's side, warlock on Vek'nilash's)
        // and its 2-D distance to the twin on that side (its basis twin). -1 = none.
        ObjectGuid const waitMeleeGuid = st.melee[SideIndex(vlSide)];
        ObjectGuid const waitWarlockGuid = st.warlock[SideIndex(vnSide)];
        float pickupDist[2] = {-1.0f, -1.0f};
        // Revision 5 §14.4: living healers within HEAL_REACH of each twin's victim while that victim is the twin's
        // designated active tank (else "-"). A REACT_PASSIVE twin attacks nobody (TwinsWrongVictimTwin).
        auto tankVictim = [](Creature* twin, ObjectGuid tankGuid) -> Unit*
        {
            Unit* const v = twin->GetReactState() != REACT_PASSIVE ? twin->GetVictim() : nullptr;
            return v && !tankGuid.IsEmpty() && v->GetGUID() == tankGuid ? v : nullptr;
        };
        Unit* const vlHeldBy = tankVictim(veklor, vlTankGuid);
        Unit* const vnHeldBy = tankVictim(veknilash, vnTankGuid);
        uint32 vlHealersInRange = 0;
        uint32 vnHealersInRange = 0;
        // Revision 11 §20.3: of those, the healers with LOS to that tank.
        uint32 vlHealersLos = 0;
        uint32 vnHealersLos = 0;
        uint32 const sinceStart =
            st.inWindow ? 0 : getMSTimeDiff(st.lastWindowStartMs ? st.lastWindowStartMs : st.engageMs, now);
        std::unordered_set<ObjectGuid> living;
        for (Player* p : present)
        {
            if (!p->IsAlive())
            {
                ++dead;
                continue;
            }
            living.insert(p->GetGUID());
            if (p->GetExactDist2d(veklor) <= Aq40TwinsRules::ARCANE_BURST_TRIGGER)
            {
                ++nearVeklor;
                nearVeklorNames += (nearVeklorNames.empty() ? "" : ",") + p->GetName() + "/" + TwinsTagOf(st, p);
            }
            TwinsLatched const* l = FindLatched(st, p->GetGUID());
            Role const role = l ? l->a.role : Role::None;
            if (role != Role::None && TwinsWrongVictimTwin(p, veklor, veknilash, vlSide, st.melee, st.warlock))
                ++wrongVictims;
            if (l && TwinsPreSwapRaw(p, role, l->a.spare, l->rangedKind, st.inWindow, sinceStart, veklor, veknilash,
                                     vlSide, st.melee, st.warlock))
                ++preSwap;

            if (p->GetGUID() == vlTankGuid)
            {
                vlTankThreat = veklor->GetThreatMgr().GetThreat(p);
                vlCast = TwinsWarlockCastBlocker(p, veklor);
            }
            if (p->GetGUID() == vnTankGuid)
                vnTankThreat = veknilash->GetThreatMgr().GetThreat(p);
            if (!waitMeleeGuid.IsEmpty() && p->GetGUID() == waitMeleeGuid)
                pickupDist[SideIndex(vlSide)] = p->GetExactDist2d(veklor);
            if (!waitWarlockGuid.IsEmpty() && p->GetGUID() == waitWarlockGuid)
                pickupDist[SideIndex(vnSide)] = p->GetExactDist2d(veknilash);
            if (role != Role::MeleeTank && role != Role::WarlockTank)
            {
                float const tvl = veklor->GetThreatMgr().GetThreat(p);
                if (tvl > vlTop)
                {
                    vlTop = tvl;
                    vlTopName = p->GetName();
                }
                float const tvn = veknilash->GetThreatMgr().GetThreat(p);
                if (tvn > vnTop)
                {
                    vnTop = tvn;
                    vnTopName = p->GetName();
                }
            }

            if (role == Role::Healer)
            {
                if (vlHeldBy && p->GetExactDist2d(vlHeldBy) <= Aq40TwinsRules::HEAL_REACH)
                {
                    ++vlHealersInRange;
                    vlHealersLos += p->IsWithinLOSInMap(vlHeldBy) ? 1 : 0;
                }
                if (vnHeldBy && p->GetExactDist2d(vnHeldBy) <= Aq40TwinsRules::HEAL_REACH)
                {
                    ++vnHealersInRange;
                    vnHealersLos += p->IsWithinLOSInMap(vnHeldBy) ? 1 : 0;
                }
                if (p->GetMaxPower(POWER_MANA) > 0)
                {
                    healManaSum += 100.0f * float(p->GetPower(POWER_MANA)) / float(p->GetMaxPower(POWER_MANA));
                    ++healManaCount;
                }
                continue;
            }

            Unit* victim = p->GetVictim();
            if (!victim)
                ++idle;
            else if (victim == veklor)
                ++onVeklor;
            else if (victim == veknilash)
                ++onVeknilash;
            else if (victim->GetEntry() == NPC_QIRAJI_SCARAB || victim->GetEntry() == NPC_QIRAJI_SCORPION)
                ++onBug;
            else
                ++onOther;
        }

        auto threat = [](float tank, float top, std::string const& topName)
        {
            return (tank >= 0.0f ? std::to_string(int(tank + 0.5f)) : std::string("none")) + "/" +
                   (top >= 0.0f ? std::to_string(int(top + 0.5f)) : std::string("none")) + "(" + topName + ")";
        };
        std::string const healMana =
            healManaCount ? std::to_string(int(healManaSum / float(healManaCount) + 0.5f)) : std::string("none");
        uint32 const sinceWindow =
            st.inWindow ? 0 : getMSTimeDiff(st.lastWindowEndMs ? st.lastWindowEndMs : st.engageMs, now);

        uint32 parked = 0;
        for (auto const& [guid, track] : st.spotTrack)
            parked += track.parked ? 1 : 0;
        uint32 stepIn = 0;
        for (auto const& [guid, rec] : st.stepIn)
            stepIn += rec.due || rec.losDue ? 1 : 0;

        auto inRange = [](Unit* heldBy, uint32 n, uint32 los)
        { return heldBy ? std::to_string(n) + "/" + std::to_string(los) : std::string("-"); };
        std::string const nearVeklorDesc =
            std::to_string(nearVeklor) + (nearVeklor ? "(" + nearVeklorNames + ")" : std::string());
        std::string const healersInRange =
            "vl:" + inRange(vlHeldBy, vlHealersInRange, vlHealersLos) + " vn:" +
            inRange(vnHeldBy, vnHealersInRange, vnHealersLos);
        auto pickupOf = [](float d) { return d >= 0.0f ? Acore::StringFormat("{:.1f}", d) : std::string("-"); };
        std::string const pickup = "A:" + pickupOf(pickupDist[0]) + ",B:" + pickupOf(pickupDist[1]);

        // The script's own Heal Brother test (boss_twinemperorsAI, every 3.6 s).
        bool const healBrother = veklor->IsWithinDist(veknilash, 60.0f);
        LOG_DEBUG("playerbots",
                  "[Aq40Twins] diag t={} sep={:.1f} hb={} vlSide={} window={} sinceWindow={} "
                  "veklor={:.0f}% at ({:.1f},{:.1f}) victim={} vlThreat={} vlCast={} "
                  "veknilash={:.0f}% at ({:.1f},{:.1f}) victim={} vnThreat={} "
                  "hitting: veklor={} veknilash={} bug={} other={} idle={} dead={} mutated={} healMana={} "
                  "wrongVictims={} preSwap={} parked={} nearVeklor={} stepIn={} healersInRange={} tankHeals={} "
                  "pickup={} mtA={} mtB={} ltA={} ltB={}",
                  getMSTimeDiff(st.engageMs, now), veklor->GetExactDist2d(veknilash), healBrother, SideTag(vlSide),
                  st.inWindow, sinceWindow, veklor->GetHealthPct(), veklor->GetPositionX(), veklor->GetPositionY(),
                  TwinsDescribeUnit(st, veklor->GetVictim()), threat(vlTankThreat, vlTop, vlTopName), vlCast,
                  veknilash->GetHealthPct(), veknilash->GetPositionX(), veknilash->GetPositionY(),
                  TwinsDescribeUnit(st, veknilash->GetVictim()), threat(vnTankThreat, vnTop, vnTopName), onVeklor,
                  onVeknilash, onBug, onOther, idle, dead, FindTwinsMutatedBug(bot) != nullptr, healMana, wrongVictims,
                  preSwap, parked, nearVeklorDesc, stepIn, healersInRange, tankHeals, pickup,
                  TwinsSlotName(st, st.melee[0], living), TwinsSlotName(st, st.melee[1], living),
                  TwinsSlotName(st, st.warlock[0], living), TwinsSlotName(st, st.warlock[1], living));
    }
}  // namespace

bool IsInAq40(Player* bot)
{
    return bot && bot->GetMapId() == AQ40_MAP_ID;
}

Creature* GetTwin(Player* bot, uint32 dataId)
{
    if (!IsInAq40(bot))
        return nullptr;

    InstanceScript* instance = bot->GetInstanceScript();
    return instance ? instance->GetCreature(dataId) : nullptr;
}

bool IsTwinsEncounterActive(Player* bot)
{
    if (!IsInAq40(bot))
        return false;

    InstanceScript* instance = bot->GetInstanceScript();
    return instance && instance->GetBossState(AQT_DATA_TWIN_EMPERORS) == IN_PROGRESS;
}

bool GetTwinsView(Player* bot, TwinsView& view)
{
    view = TwinsView{};

    // Inert until A, B and their spots are probe-authored: every trigger, action and multiplier keyed on the view
    // is a no-op, so bots run stock behaviour (the Huhuran pattern).
    if constexpr (TWINS_STATIONS_PROVISIONAL)
        return false;

    if (!IsInAq40(bot) || !bot->IsInWorld())
        return false;
    InstanceScript* const instance = bot->GetInstanceScript();
    if (!instance)
        return false;
    uint32 const instanceId = bot->GetMap()->GetInstanceId();

    // Not IN_PROGRESS (a wipe, or before the pull): drop the attempt so the next pull re-latches. The atomic count
    // keeps this lock-free while no instance holds state. Final-review I2: also give back the pets this strategy
    // directed (TwinsRestoreTouchedPets: lock-free while no instance has a record, then throttled).
    if (instance->GetBossState(AQT_DATA_TWIN_EMPERORS) != IN_PROGRESS)
    {
        TwinsRestoreTouchedPets(bot, instanceId);
        if (twinsStateCount.load(std::memory_order_relaxed))
        {
            std::lock_guard<std::mutex> lock(twinsMutex);
            twinsStateByInstance.erase(instanceId);
            twinsStateCount.store(uint32(twinsStateByInstance.size()), std::memory_order_relaxed);
        }
        return false;
    }

    Creature* const veklor = instance->GetCreature(AQT_DATA_VEKLOR);
    Creature* const veknilash = instance->GetCreature(AQT_DATA_VEKNILASH);
    if (!veklor || !veknilash || !veklor->IsAlive() || !veknilash->IsAlive())
        return false;

    uint32 const now = TwinsNowMs();
    // A window edge forces an early refresh, so the landing window and windowVeklorFrom are seen by the first call
    // after a teleport rather than up to a TTL late (the window is only 2 s). At most two per teleport.
    bool const windowNow = TwinsInLandingWindow(veklor) || TwinsInLandingWindow(veknilash);
    TwinsInstanceState st;
    {
        std::lock_guard<std::mutex> lock(twinsMutex);
        auto it = twinsStateByInstance.find(instanceId);
        if (it != twinsStateByInstance.end())
        {
            TwinsInstanceState const& cur = it->second;
            if (cur.latched && cur.inWindow == windowNow &&
                getMSTimeDiff(cur.refreshMs, now) < TWINS_SNAPSHOT_TTL_MS)
            {
                FillTwinsView(cur, veklor, veknilash, now, view);
                return true;
            }
            st = cur;  // refresh a private copy
        }
    }

    // An ungrouped bot would scan only itself: latch a one-member roster, or see every designated tank as dead and
    // demote them. It never refreshes; it reads the last refreshed state, if any.
    if (!bot->GetGroup())
    {
        if (!st.latched)
            return false;
        FillTwinsView(st, veklor, veknilash, now, view);
        return true;
    }

    // The group scan and every computation run outside the lock. One instance's bots share one map thread, so no
    // other refresh of this instance can interleave; the lock only guards the container across instances.
    std::vector<Player*> const present = ScanTwinsGroup(bot);
    TwinsRefreshEvents ev;
    RefreshTwinsState(st, present, veklor, veknilash, now, ev);
    {
        std::lock_guard<std::mutex> lock(twinsMutex);
        twinsStateByInstance[instanceId] = st;
        twinsStateCount.store(uint32(twinsStateByInstance.size()), std::memory_order_relaxed);
    }

    if (ev.latchedNow)
        LogTwinsLatch(instanceId, st);
    for (std::string const& r : ev.replacements)
        LOG_DEBUG("playerbots", "[Aq40Twins] instance {} replacement: {}", instanceId, r);
    if (ev.windowEnded)
        LogTwinsLanding(st, veklor, veknilash, now);
    for (std::string const& a : ev.anchors)
        LOG_DEBUG("playerbots", "[Aq40Twins] anchor {}", a);
    for (std::string const& name : ev.manaRefills)
        LOG_DEBUG("playerbots", "[Aq40Twins] mana refill {}", name);
    if (ev.diagDue)
        LogTwinsDiag(bot, st, present, veklor, veknilash, now, ev.tankHeals);

    FillTwinsView(st, veklor, veknilash, now, view);
    return true;
}

TwinsMe GetTwinsMe(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !IsInAq40(bot))
        return TwinsMe{};

    std::lock_guard<std::mutex> lock(twinsMutex);
    auto it = twinsStateByInstance.find(bot->GetMap()->GetInstanceId());
    if (it == twinsStateByInstance.end() || !it->second.latched)
        return TwinsMe{};
    TwinsLatched const* l = FindLatched(it->second, bot->GetGUID());
    if (!l)
        return TwinsMe{};
    auto const track = it->second.spotTrack.find(bot->GetGUID());
    bool const parked = track != it->second.spotTrack.end() && track->second.parked;
    auto const step = it->second.stepIn.find(bot->GetGUID());
    bool const found = step != it->second.stepIn.end();
    bool const stepInLos = found && step->second.losDue;
    bool const stepIn = found && (step->second.due || stepInLos);
    return TwinsMe{l->a.role, l->a.side, l->a.spare, l->rankOnSide, l->rangedKind, l->rangedRank, parked,
                   stepIn, stepIn ? step->second.tankPos : Position(), stepInLos};
}

Position TwinsAnchored(Player* bot, TwinsView const& view, Aq40TwinsRules::Side s, Position const& authoredSpot)
{
    size_t const i = SideIndex(s);
    Position const& station = TWINS_STATIONS[i].station;
    Position const& anchor = view.anchor[i];
    float const dx = anchor.GetPositionX() - station.GetPositionX();
    float const dy = anchor.GetPositionY() - station.GetPositionY();
    if (std::fabs(dx) < 0.01f && std::fabs(dy) < 0.01f)
        return authoredSpot;  // the anchor is on the station: the authored, probe-verified spot itself
    float const x = authoredSpot.GetPositionX() + dx;
    float const y = authoredSpot.GetPositionY() + dy;
    float z = authoredSpot.GetPositionZ() + (anchor.GetPositionZ() - station.GetPositionZ()) + 2.0f;
    bot->UpdateAllowedPositionZ(x, y, z);
    return Position(x, y, z);
}

void TwinsReportSpotMove(Player* bot, bool moved, float dist)
{
    if (!bot || !bot->IsInWorld() || !IsInAq40(bot))
        return;
    // Only a standing bot short of its spot counts (TWINS_UNREACHABLE_*): mid-walk MoveTo is refused by its own
    // duplicate / wait-for-last-move guards.
    bool const failed = !moved && dist > TWINS_UNREACHABLE_DIST && !bot->isMoving();
    if (!moved && !failed)
        return;
    uint32 const now = TwinsNowMs();
    uint32 parkedEpoch = 0;
    bool parkedNow = false;
    {
        std::lock_guard<std::mutex> lock(twinsMutex);
        auto it = twinsStateByInstance.find(bot->GetMap()->GetInstanceId());
        if (it == twinsStateByInstance.end() || !it->second.latched)
            return;
        std::unordered_map<ObjectGuid, TwinsSpotTrack>& tracks = it->second.spotTrack;
        if (moved)
        {
            auto t = tracks.find(bot->GetGUID());
            if (t != tracks.end() && !t->second.parked)
                tracks.erase(t);  // consecutive failures only
            return;
        }
        TwinsSpotTrack& t = tracks[bot->GetGUID()];
        if (t.parked || (t.lastFailMs && getMSTimeDiff(t.lastFailMs, now) < TWINS_UNREACHABLE_DEBOUNCE_MS))
            return;
        t.lastFailMs = now;
        if (++t.fails >= TWINS_UNREACHABLE_FAILS)
        {
            t.parked = true;
            parkedNow = true;
            parkedEpoch = it->second.anchorEpoch;
        }
    }
    if (parkedNow)
        LOG_DEBUG("playerbots", "[Aq40Twins] {} parked: spot unreachable ({:.1f} y off), epoch {}", bot->GetName(),
                  dist, parkedEpoch);
}

Aq40TwinsRules::Side TwinsSideOf(TwinsView const& view, uint32 dataId)
{
    if (dataId == AQT_DATA_VEKLOR)
        return view.veklorSide;
    if (dataId == AQT_DATA_VEKNILASH)
        return Aq40TwinsRules::Other(view.veklorSide);
    return Side::None;
}

Player* TwinsActiveTankFor(TwinsView const& view, Player* bot, uint32 dataId)
{
    if (!view.valid || !bot)
        return nullptr;
    Side const side = TwinsSideOf(view, dataId);
    if (side == Side::None)
        return nullptr;
    ObjectGuid const guid =
        dataId == AQT_DATA_VEKLOR ? view.activeWarlock[SideIndex(side)] : view.activeMelee[SideIndex(side)];
    if (guid.IsEmpty())
        return nullptr;
    // Map-local lookup: null when the holder is not on the bot's map (released, disconnected).
    Player* const tank = ObjectAccessor::GetPlayer(*bot, guid);
    return tank && tank->IsAlive() ? tank : nullptr;
}

void TwinsMarkPetsTouched(Player* owner)
{
    if (!owner || !owner->IsInWorld())
        return;
    std::lock_guard<std::mutex> lock(twinsPetMutex);
    twinsPetTouchedByInstance[owner->GetMap()->GetInstanceId()].owners.insert(owner->GetGUID());
    twinsPetTouchedCount.store(uint32(twinsPetTouchedByInstance.size()), std::memory_order_relaxed);
}

void TwinsEraseTrackers(Player* bot)
{
    if (!bot || !bot->GetMap())
        return;

    std::lock_guard<std::mutex> lock(twinsMutex);
    twinsStateByInstance.erase(bot->GetMap()->GetInstanceId());
    twinsStateCount.store(uint32(twinsStateByInstance.size()), std::memory_order_relaxed);
}

bool IsTwinsPhysicalGroup(Player* member, PlayerbotAI* /*botAI*/)
{
    if (!member)
        return false;

    // By damage SCHOOL. Vek'nilash is immune to FIRE, NATURE, FROST, SHADOW and ARCANE
    // (creature_immunities SchoolMask 124) and takes physical and Holy; Vek'lor is immune to
    // physical only (SchoolMask 1). Hunters are physical despite being ranged; retribution
    // paladins are physical despite Holy also landing on Vek'lor, because their output is weapon
    // damage.
    switch (member->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_ROGUE:
        case CLASS_HUNTER:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return true;
        case CLASS_DRUID:
        case CLASS_SHAMAN:
            return PlayerbotAI::IsMelee(member);
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------------------------
// TARGETING
// ---------------------------------------------------------------------------------------------
Creature* FindTwinsMutatedBug(Player* bot, bool meleeSkipNearVeklor)
{
    if (!IsTwinsEncounterActive(bot))
        return nullptr;

    // Revision 7 §16.2: a physical melee non-hunter skips bugs near a living Vek'lor (its Arcane Burst range), so it
    // never chases one into him. Resolved once, before the loop.
    bool physicalMelee = false;
    Creature* veklor = nullptr;
    if (meleeSkipNearVeklor)
    {
        bool const isMelee = PlayerbotAI::IsMelee(bot);
        uint8 const cls = uint8(bot->getClass());
        physicalMelee = cls != CLASS_HUNTER && isMelee && Aq40TwinsRules::IsPhysical(cls, isMelee);
        if (physicalMelee)
            veklor = GetTwin(bot, AQT_DATA_VEKLOR);
    }
    bool const veklorAlive = veklor && veklor->IsAlive();

    // Searched FROM THE BOT: Cell::Visit clamps every radius to ~533y, so a search issued from a
    // seat 95y away can silently return null.
    std::list<Creature*> bugs;
    std::vector<uint32> const entries = {NPC_QIRAJI_SCARAB, NPC_QIRAJI_SCORPION};
    bot->GetCreatureListWithEntryInGrid(bugs, entries, TWINS_SEARCH_RANGE);

    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* bug : bugs)
    {
        if (!bug || !bug->IsAlive() || !bug->HasAura(SPELL_MUTATE_BUG))
            continue;
        if (veklor && Aq40TwinsRules::MeleeSkipsBug(physicalMelee, veklorAlive, bug->GetExactDist2d(veklor)))
            continue;
        float const d = bot->GetExactDist2d(bug);
        if (!best || d < bestDist)
        {
            best = bug;
            bestDist = d;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------
// DECISION PREDICATES (spec §5.3-§5.7). Each public predicate fetches the view once and the bot's own assignment
// at most once; on top of that only distance tests (plus the existing mutated-bug grid search for Dps targets).
// No group scans. The file-local *For variants take the view so TwinsShouldSuppressGenericMovement fetches once.
// ---------------------------------------------------------------------------------------------
namespace
{
    bool IsSlotHolder(ObjectGuid const& slot, Player* bot) { return !slot.IsEmpty() && slot == bot->GetGUID(); }

    // Live labels (view.veklorSide, the same as TwinsActiveTankFor): the active warlock holds Vek'lor's side, the
    // active melee tank Vek'nilash's side; the other slot of each kind is waiting for the next landing.
    bool IsActiveWarlock(TwinsView const& view, Player* bot)
    {
        return IsSlotHolder(view.activeWarlock[SideIndex(view.veklorSide)], bot);
    }

    bool IsActiveMelee(TwinsView const& view, Player* bot)
    {
        return IsSlotHolder(view.activeMelee[SideIndex(Aq40TwinsRules::Other(view.veklorSide))], bot);
    }

    bool IsWaitingTank(TwinsView const& view, Player* bot)
    {
        return IsSlotHolder(view.activeWarlock[SideIndex(Aq40TwinsRules::Other(view.veklorSide))], bot) ||
               IsSlotHolder(view.activeMelee[SideIndex(view.veklorSide)], bot);
    }

    // Revision 10 §19: the two pickups inside the landing window, labelled from windowVeklorFrom exactly as
    // TwinsLandingZoneFor names them. The live labels already call them the active tanks once the rising edge flipped
    // veklorSide, but these do not depend on that. Vek'nilash pickup = the melee tank of the side Vek'lor left.
    bool IsVeknilashPickupInWindow(TwinsView const& view, Player* bot)
    {
        if (!view.landingWindow || view.windowVeklorFrom == Side::None)
            return false;
        return Aq40TwinsRules::PickupEngagesInWindow(
            view.landingWindow, IsSlotHolder(view.activeMelee[SideIndex(view.windowVeklorFrom)], bot));
    }

    // Vek'lor pickup = the warlock of the side Vek'lor landed on (Other(windowVeklorFrom)).
    bool IsVeklorPickupInWindow(TwinsView const& view, Player* bot)
    {
        if (!view.landingWindow || view.windowVeklorFrom == Side::None)
            return false;
        return IsSlotHolder(view.activeWarlock[SideIndex(Aq40TwinsRules::Other(view.windowVeklorFrom))], bot);
    }

    bool TwinsIsPhysical(Player* bot)
    {
        return Aq40TwinsRules::IsPhysical(uint8(bot->getClass()), PlayerbotAI::IsMelee(bot));
    }

    // Ruling R13 F1: the active warlock can cast Searing Pain on Vek'lor from where it stands. Distance first (2-D,
    // like every other Twins distance rule), then range (the core's IsWithinCombatRange, at the playerbots spell
    // distance), then the vmap LOS query. Only ever asked about the one active warlock (Revision 10 §19.2: or the
    // Vek'lor pickup inside the landing window). The inside-12 y test is skipped while Vek'lor is REACT_PASSIVE (the
    // window): he casts no Arcane Burst then.
    bool TwinsWarlockCastsFromHere(Player* bot, Creature* veklor)
    {
        return (veklor->GetReactState() == REACT_PASSIVE ||
                bot->GetExactDist2d(veklor) >= Aq40TwinsRules::WRONG_VICTIM_MIN) &&
               bot->IsWithinCombatRange(veklor, sPlayerbotAIConfig.SpellDistance) && bot->IsWithinLOSInMap(veklor);
    }

    // What the active warlock does with its feet (rulings R13 F1, R15). Hold: inside the landing window. Cast: it can
    // cast from where it stands. CastSpot: it cannot, and Vek'lor is on his side's anchor, so the anchored spot works.
    // Escape: Vek'lor is off station and the bot is inside WRONG_VICTIM_MIN of him. Los (final-review I1): off
    // station, in spell range, so the blocker is line of sight; it walks to TwinsWarlockLosSpot (native reach only
    // tests distance and stood still). Reach: off station and out of range (native caster reach). Only ever computed
    // for the slot holder.
    enum class TwinsWarlockMove : uint8 { Hold, Cast, CastSpot, Escape, Los, Reach };

    TwinsWarlockMove TwinsWarlockMoveFor(Player* bot, TwinsView const& view)
    {
        if (view.landingWindow)
            return TwinsWarlockMove::Hold;
        if (TwinsWarlockCastsFromHere(bot, view.veklor))
            return TwinsWarlockMove::Cast;
        // Revision 3 §12.2: "on station" means on his side's ANCHOR, which the anchored casting spot is authored from.
        if (view.veklor->GetExactDist2d(view.anchor[SideIndex(view.veklorSide)]) <= TWINS_VEKLOR_ON_STATION)
            return TwinsWarlockMove::CastSpot;
        if (bot->GetExactDist2d(view.veklor) < Aq40TwinsRules::WRONG_VICTIM_MIN)
            return TwinsWarlockMove::Escape;
        // At or beyond WRONG_VICTIM_MIN and in range (TwinsWarlockCastsFromHere's own range test), so it failed on LOS.
        if (bot->IsWithinCombatRange(view.veklor, sPlayerbotAIConfig.SpellDistance))
            return TwinsWarlockMove::Los;
        return TwinsWarlockMove::Reach;
    }

    // Final-review I1, the active warlock's spot when only LOS stops its cast: the first point with LOS to Vek'lor on
    // the straight line from the bot to him (the MovementAction::MoveToLOS shape, without its PathGenerator call).
    // Candidates sit WarlockLosTopRadius, then every WARLOCK_LOS_STEP inward down to WARLOCK_LOS_MIN, from him on his
    // bearing to the bot: a fixed grid, so the point holds while the bot walks the line (memory
    // playerbot-escape-destination-must-be-stable), and always outside BURST_ESCAPE_TO, so the §18.2 push never moves
    // it. Each candidate is tested like IsWithinLOSInMap: the bot's eye there (z interpolated along the line) to
    // Vek'lor's hit-sphere point. None qualifies: WARLOCK_LOS_FALLBACK from him on that bearing. Pure in the bot's and
    // his positions, so both callers (the off-spot trigger and the hold spot, both withZ) pick the same x/y; withZ only
    // adds the ground query. At most ~7 rays, for the one active warlock.
    Position TwinsWarlockLosSpot(Player* bot, Creature* veklor, bool withZ)
    {
        float const vx = veklor->GetPositionX();
        float const vy = veklor->GetPositionY();
        float const vz = veklor->GetPositionZ();
        float const bz = bot->GetPositionZ();
        float const d = bot->GetExactDist2d(veklor);
        float ux = 1.0f;
        float uy = 0.0f;
        if (d > 0.01f)
        {
            ux = (bot->GetPositionX() - vx) / d;
            uy = (bot->GetPositionY() - vy) / d;
        }
        auto zAt = [&](float r) { return d > 0.01f ? vz + (bz - vz) * (r / d) : bz; };
        float const eye = bot->GetCollisionHeight();

        float r = Aq40TwinsRules::WARLOCK_LOS_FALLBACK;
        for (float c = Aq40TwinsRules::WarlockLosTopRadius(d, TWINS_SPOT_ARRIVE); c >= Aq40TwinsRules::WARLOCK_LOS_MIN;
             c -= Aq40TwinsRules::WARLOCK_LOS_STEP)
        {
            Position const from(vx + ux * c, vy + uy * c, zAt(c) + eye);
            float ox = 0.0f;
            float oy = 0.0f;
            float oz = 0.0f;
            veklor->GetHitSpherePointFor(from, ox, oy, oz);
            if (bot->GetMap()->isInLineOfSight(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(), ox, oy,
                                               oz, bot->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS,
                                               VMAP::ModelIgnoreFlags::Nothing))
            {
                r = c;
                break;
            }
        }
        float const x = vx + ux * r;
        float const y = vy + uy * r;
        float z = zAt(r);
        if (withZ)
        {
            z += 2.0f;
            bot->UpdateAllowedPositionZ(x, y, z);
        }
        return Position(x, y, z);
    }

    // The Vek'lor side GetTwinsSpot labels from: windowVeklorFrom inside the landing window (the Vek'lor pickup holds
    // its waiting spot until the roll; Revision 10 §19.1: the Vek'nilash pickup owns none), else the live veklorSide.
    Side TwinsSpotVeklorSide(TwinsView const& view)
    {
        return view.landingWindow && view.windowVeklorFrom != Side::None ? view.windowVeklorFrom : view.veklorSide;
    }

    // TwinsAnchored without the ground-z query: x/y exact, z only shifted by the anchor's z offset. For callers that
    // need a 2-D answer (the movement veto runs this for every candidate action of every bot).
    Position TwinsAnchoredXY(TwinsView const& view, Side s, Position const& authoredSpot)
    {
        size_t const i = SideIndex(s);
        Position const& station = TWINS_STATIONS[i].station;
        Position const& anchor = view.anchor[i];
        return Position(authoredSpot.GetPositionX() + anchor.GetPositionX() - station.GetPositionX(),
                        authoredSpot.GetPositionY() + anchor.GetPositionY() - station.GetPositionY(),
                        authoredSpot.GetPositionZ() + anchor.GetPositionZ() - station.GetPositionZ());
    }

    // Revision 8 §17.1, a waiting pickup spot: side `s`'s pickup basis (the twin it waits beside) + `dist` along the
    // authored bearing (authoredSpot - authored station of `s`). withZ = false: the same x/y with the basis z and no
    // ground query (the movement veto); else z = basis z + 2 through UpdateAllowedPositionZ. A basis never latched
    // this attempt falls back to the anchored spot. (Fix round 1's `burstClear` push for the waiting melee tank is
    // gone: TwinsSpotFor's Revision 9 §18.2 push to BURST_ESCAPE_TO covers it, and a passive Vek'lor casts no burst.)
    Position TwinsPickupSpot(Player* bot, TwinsView const& view, Side s, Position const& authoredSpot, float dist,
                             bool withZ)
    {
        size_t const i = SideIndex(s);
        Position const& station = TWINS_STATIONS[i].station;
        float const dx = authoredSpot.GetPositionX() - station.GetPositionX();
        float const dy = authoredSpot.GetPositionY() - station.GetPositionY();
        float const len = std::hypot(dx, dy);
        if (!view.pickupBasisSet[i] || len < 0.01f)
            return withZ ? TwinsAnchored(bot, view, s, authoredSpot) : TwinsAnchoredXY(view, s, authoredSpot);
        Position const& basis = view.pickupBasis[i];
        float const x = basis.GetPositionX() + dx / len * dist;
        float const y = basis.GetPositionY() + dy / len * dist;
        float z = basis.GetPositionZ();
        if (withZ)
        {
            z += 2.0f;
            bot->UpdateAllowedPositionZ(x, y, z);
        }
        return Position(x, y, z);
    }

    // BuildTwinsEscapeSpot's x/y (sector-snapped bearing from the centre toward the bot, clearance + tolerance out),
    // with no ground-z query. Shared with it, so the movement veto's 2-D answer and the move agree.
    void TwinsEscapeXY(Player* bot, float cx, float cy, float clearance, uint32 attempt, float& x, float& y)
    {
        float const sector = 2.0f * float(M_PI) / float(TWINS_ESCAPE_SECTORS);
        float away = std::atan2(bot->GetPositionY() - cy, bot->GetPositionX() - cx);
        if (away < 0.0f)
            away += 2.0f * float(M_PI);

        int32 const stepped = int32(away / sector) + int32(attempt) * 4;
        float const bearing = float(stepped % int32(TWINS_ESCAPE_SECTORS)) * sector;

        float const radius = clearance + TWINS_STATION_TOLERANCE;
        x = cx + std::cos(bearing) * radius;
        y = cy + std::sin(bearing) * radius;
    }

    // Revision 5 §14.3, Blizzard over a spot: while an active Blizzard's centre is within TWINS_BLIZZARD_RADIUS + 3 of
    // the chosen spot, the spot is that Blizzard's escape point (clearance TWINS_BLIZZARD_RADIUS + 3, built even when
    // the bot already stands there). The hazard dodge moved bots out and hold-spot walked them straight back in; now
    // the bot waits beside the patch and its spot comes back by itself when the patch expires. withZ = false: the
    // same x/y without the ground-z query (the movement veto). True = the spot was moved (not parkable).
    // Fix round 1: the escape point must stay outside Vek'lor's berth (BerthWaypoint returns an in-berth destination
    // unchanged, and TwinsBerthed skips the active warlock), so attempts 0..3 (each 4 sectors on) are tried and the
    // first point at least BERTH_RADIUS from a living Vek'lor wins. None qualifies: the spot stays where it is (the
    // hazard dodge still moves a bot standing in the patch). The choice is pure x/y, so veto and move agree.
    bool TwinsBlizzardOverSpot(Player* bot, Creature* veklor, Position& spot, bool withZ)
    {
        Position centre;
        if (!GetTwinsBlizzardCenter(bot, centre))
            return false;
        float const clearance = TWINS_BLIZZARD_RADIUS + TWINS_BLIZZARD_SPOT_MARGIN;
        if (spot.GetExactDist2d(centre.GetPositionX(), centre.GetPositionY()) > clearance)
            return false;
        bool const veklorLive = veklor && veklor->IsAlive();
        for (uint32 attempt = 0; attempt < TWINS_BLIZZARD_SPOT_ATTEMPTS; ++attempt)
        {
            float x = 0.0f;
            float y = 0.0f;
            TwinsEscapeXY(bot, centre.GetPositionX(), centre.GetPositionY(), clearance, attempt, x, y);
            if (veklorLive && veklor->GetExactDist2d(x, y) < Aq40TwinsRules::BERTH_RADIUS)
                continue;
            if (withZ)
                BuildTwinsEscapeSpot(bot, centre.GetPositionX(), centre.GetPositionY(), clearance, attempt, spot,
                                     false);
            else
                spot.Relocate(x, y, bot->GetPositionZ());
            return true;
        }
        return false;
    }

    // The owned spot before Revision 3's parking override (TwinsSpotFor applies it). withZ = false skips every
    // ground-z query (TwinsShouldSuppressGenericMovement only needs whether a spot exists). `parkable` = false for the
    // two unanchored, dangerous destinations (fix round 1, R22): the active warlock's Arcane Burst escape and the melee
    // pre-swap point. Parking there would hold a bot inside 12 y of Vek'lor or on Vek'nilash's landing.
    bool TwinsOwnedSpotFor(Player* bot, TwinsView const& view, TwinsMe const& me, Position& spot, bool withZ,
                           bool& parkable)
    {
        parkable = true;
        if (me.role == Role::None)
            return false;
        Side const vl = TwinsSpotVeklorSide(view);
        Side const vn = Aq40TwinsRules::Other(vl);
        // Revision 3 §12.2: every per-side spot is anchored: anchor + (authored spot - authored station). Revision 8
        // §17.1: except the two waiting pickup spots (TwinsPickupSpot).
        auto anchored = [&](Side s, Position const& authored)
        { return withZ ? TwinsAnchored(bot, view, s, authored) : TwinsAnchoredXY(view, s, authored); };
        if (IsSlotHolder(view.activeWarlock[SideIndex(vl)], bot))
        {
            // Rulings R13 F1, R15. Inside the window Vek'lor has already teleported away and is passive, so the old
            // active warlock holds where it is (the landing clear moves it if it stands in Vek'nilash's landing zone).
            // Outside it vl is the live veklorSide, which TwinsWarlockMoveFor uses.
            switch (TwinsWarlockMoveFor(bot, view))
            {
                case TwinsWarlockMove::CastSpot:
                    // Final-review M2: not parkable. A parked warlock holds where it stands, and in CastSpot mode it
                    // cannot cast from there, so parking it would stop it casting for the epoch.
                    spot = anchored(vl, TWINS_STATIONS[SideIndex(vl)].warlockCast);
                    parkable = false;
                    return true;
                case TwinsWarlockMove::Escape:
                    // Stable sector bearing; never "parks" short of 14 y (it still could not cast there).
                    BuildTwinsEscapeSpot(bot, view.veklor->GetPositionX(), view.veklor->GetPositionY(),
                                         TWINS_WARLOCK_ESCAPE_DIST - TWINS_STATION_TOLERANCE, 0, spot, false);
                    parkable = false;
                    return true;
                case TwinsWarlockMove::Los:
                    // Final-review I1: never parked either, for the same reason as the casting spot.
                    spot = TwinsWarlockLosSpot(bot, view.veklor, withZ);
                    parkable = false;
                    return true;
                default:
                    return false;  // Hold / Cast: holds where it is; Reach: native caster reach (TwinsWarlockMayReach)
            }
        }
        if (IsSlotHolder(view.activeWarlock[SideIndex(vn)], bot))
        {
            // Revision 8 §17.1: measured from Vek'nilash (side vn's pickup basis), not from the anchor.
            spot = TwinsPickupSpot(bot, view, vn, TWINS_STATIONS[SideIndex(vn)].warlockWait,
                                   Aq40TwinsRules::WARLOCK_WAIT_DIST, withZ);
            return true;
        }
        // Revision 10 §19.1: inside the landing window the Vek'nilash pickup (activeMelee[vl], vl = windowVeklorFrom)
        // owns no spot: it targets him and closes to melee with native reach, so it is the nearest unit at his roll.
        if (IsVeknilashPickupInWindow(view, bot))
            return false;
        if (IsSlotHolder(view.activeMelee[SideIndex(vl)], bot))
        {
            // Revision 8 §17.1: measured from Vek'lor (side vl's pickup basis), not from the anchor.
            spot = TwinsPickupSpot(bot, view, vl, TWINS_STATIONS[SideIndex(vl)].warriorWait,
                                   Aq40TwinsRules::WARRIOR_WAIT_DIST, withZ);
            return true;
        }
        if (IsSlotHolder(view.activeMelee[SideIndex(vn)], bot))
            return false;  // the active melee tank owns no spot: TwinsShouldHoldStation
        if (me.role == Role::Healer && me.side != Side::None)
        {
            spot = anchored(me.side, TwinsHealerSpot(me.side, me.rankOnSide));
            if (!me.stepIn)
                return true;
            // Revision 4 §13.2, the step-in (replaces stock heal reach, vetoed again): STEP_IN_DIST from the tank on
            // the segment toward the healer's own spot (where it stands while parked, as the refresh measures it), so
            // it stays on the inner side. Not parkable: a moving target that must not freeze the healer, and it ends
            // once the tank is back near the spot.
            // Revision 11 §20.2: a LOS step-in (also when the distance one is due too) uses STEP_IN_LOS_DIST, capped
            // at half the tank -> home distance (final-review M1: a nearer tank put the fixed 20 y past home).
            Position const& tank = me.stepInTank;
            Position const& home = me.parked ? bot->GetPosition() : spot;
            Aq40TwinsRules::Pt2 const t{tank.GetPositionX(), tank.GetPositionY()};
            Aq40TwinsRules::Pt2 const h{home.GetPositionX(), home.GetPositionY()};
            Aq40TwinsRules::Pt2 const p = Aq40TwinsRules::PointAlong(
                t, h, Aq40TwinsRules::HealerStepInPointDist(me.stepInLos, std::hypot(h.x - t.x, h.y - t.y)));
            float z = std::max(tank.GetPositionZ(), home.GetPositionZ()) + 2.0f;
            if (withZ)
                bot->UpdateAllowedPositionZ(p.x, p.y, z);
            spot.Relocate(p.x, p.y, z);
            parkable = false;
            return true;
        }
        // Revision 3 §12.1, the melee pre-swap move: 20 y from Vek'lor's anchor toward Vek'nilash's side (the inner
        // side), so Vek'lor's next landing (where Vek'nilash stands) finds no melee, and after the window Vek'nilash
        // stands where they wait. Outside the window only (TwinsPreSwapRaw), so vl is the live veklorSide.
        if (TwinsPreSwapRaw(bot, me.role, me.spare, me.rangedKind, view.landingWindow, view.msSinceWindowStart,
                            view.veklor, view.veknilash, view.veklorSide, view.activeMelee, view.activeWarlock))
        {
            // Authored P = PointAlong(Vek'lor's station, Vek'nilash's station, 20), then anchored on Vek'lor's side.
            Position const& vlStation = TWINS_STATIONS[SideIndex(vl)].station;
            Position const& vnStation = TWINS_STATIONS[SideIndex(vn)].station;
            Aq40TwinsRules::Pt2 const p =
                Aq40TwinsRules::PointAlong({vlStation.GetPositionX(), vlStation.GetPositionY()},
                                           {vnStation.GetPositionX(), vnStation.GetPositionY()},
                                           Aq40TwinsRules::PRE_SWAP_DIST);
            // Not a probed spot, so its own z comes from the ground too (the floor falls ~8 y from A to B).
            float z = vlStation.GetPositionZ() + 2.0f;
            if (withZ)
                bot->UpdateAllowedPositionZ(p.x, p.y, z);
            spot = anchored(vl, Position(p.x, p.y, z));
            parkable = false;
            return true;
        }
        // Revision 9 §18.1: magic Dps and hunters own no spot in the fight (the §11.4 arcs anchored them next to an
        // off-station Vek'lor, against the burst escape). Native positioning: reach to spell range on their target,
        // then fight. The arc data stays for form-up (hunters stage on B's arc). Melee Dps and spare melee tanks:
        // native too (TwinsPreSwapRaw already excludes ranged Dps from the pre-swap move above).
        return false;
    }

    // Revision 3 §12.2: a parked bot (two failed walks to its spot this anchor epoch) owns its own position instead
    // of whatever spot it would own, so the movement veto holds it still and nothing oscillates - except at the
    // escape and pre-swap points (`parkable` false), which it always walks to.
    // Revision 9 §18.2: last of all, a spot inside BURST_ESCAPE_TO (16 y, 2-D) of a living, non-passive Vek'lor is
    // pushed radially out to 16 y from him (Aq40TwinsRules::PushOutFrom). Pure x/y on the same inputs for both
    // withZ modes, so the veto and the move agree; the moved spot then gets its own ground-z query. A parked bot's
    // "hold here" is never pushed: a parked bot standing inside 16 y of him is unparked for that evaluation instead
    // (its owned spot, pushed out), so it walks out and holds again once it is 16 y clear; the burst escape handles
    // anything inside its own trigger.
    bool TwinsSpotFor(Player* bot, TwinsView const& view, TwinsMe const& me, Position& spot, bool withZ = true,
                      bool* parkableOut = nullptr)
    {
        bool parkable = true;
        if (!TwinsOwnedSpotFor(bot, view, me, spot, withZ, parkable))
            return false;
        if (TwinsBlizzardOverSpot(bot, view.veklor, spot, withZ))
            parkable = false;  // Revision 5 §14.3: always walked to, like the escape and pre-swap points
        if (parkableOut)
            *parkableOut = parkable;
        bool const burstLive =
            view.veklor && view.veklor->IsAlive() && view.veklor->GetReactState() != REACT_PASSIVE;
        if (me.parked && parkable &&
            !(burstLive && bot->GetExactDist2d(view.veklor) < Aq40TwinsRules::BURST_ESCAPE_TO))
        {
            spot.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
            return true;
        }
        if (!burstLive)
            return true;
        Aq40TwinsRules::Pt2 const p = Aq40TwinsRules::PushOutFrom(
            {view.veklor->GetPositionX(), view.veklor->GetPositionY()}, {spot.GetPositionX(), spot.GetPositionY()},
            Aq40TwinsRules::BURST_ESCAPE_TO);
        if (p.x == spot.GetPositionX() && p.y == spot.GetPositionY())
            return true;
        float z = spot.GetPositionZ();
        if (withZ)
        {
            z += 2.0f;
            bot->UpdateAllowedPositionZ(p.x, p.y, z);
        }
        spot.Relocate(p.x, p.y, z);
        return true;
    }

    // Copied from BWL's GetNefMtPathStep (fix round 3, G1), not called: the next step toward `t` along a navmesh
    // route, TWINS_MT_STEP_MAX along the path (interpolated on its segment, so it stays on the mesh), or the first
    // corner past TWINS_MT_STEP_MIN when that comes first, or the path end. False = no usable route.
    bool TwinsPathStep(Player* bot, Position const& t, Position& step)
    {
        PathGenerator path(bot);
        path.CalculatePath(t.GetPositionX(), t.GetPositionY(), t.GetPositionZ(), false);
        if (!(path.GetPathType() & (PATHFIND_NORMAL | PATHFIND_INCOMPLETE)))
            return false;
        Movement::PointsArray const& points = path.GetPath();
        if (points.size() < 2)
            return false;
        float travelled = 0.0f;
        G3D::Vector3 prev(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
        for (size_t i = 1; i < points.size(); ++i)
        {
            G3D::Vector3 const& p = points[i];
            float const seg = std::hypot(p.x - prev.x, p.y - prev.y);
            if (travelled + seg >= TWINS_MT_STEP_MAX && seg > 0.01f)
            {
                float const f = (TWINS_MT_STEP_MAX - travelled) / seg;
                step.Relocate(prev.x + (p.x - prev.x) * f, prev.y + (p.y - prev.y) * f, prev.z + (p.z - prev.z) * f);
                return true;
            }
            travelled += seg;
            if (travelled >= TWINS_MT_STEP_MIN || i + 1 == points.size())
            {
                step.Relocate(p.x, p.y, p.z);
                return true;
            }
            prev = p;
        }
        return false;
    }

    // Revision 3 §12.2 (owner amendment): where the active melee tank drags Vek'nilash while the separation guard is
    // up - the AUTHORED station farther from Vek'lor's current position, so he is kited AWAY from the other boss.
    // Both stations are probe-verified floor with a walkable crossing between them.
    // Fix round 1 (R22): the latched view.dragSide when the refresh has latched one; the live choice only for the
    // up-to-one-TTL gap between the guard coming up and the next refresh.
    Position const& TwinsDragStation(TwinsView const& view)
    {
        if (view.dragSide != Side::None)
            return StationOf(view.dragSide);
        Position const& a = StationOf(Side::A);
        Position const& b = StationOf(Side::B);
        return view.veklor->GetExactDist2d(a) >= view.veklor->GetExactDist2d(b) ? a : b;
    }

    // The cheap half of TwinsShouldHoldStation (no path): the movement veto asks this every evaluation. Revision 3
    // §12.2: only while the twins are closer than SEPARATION_GUARD; above it nobody drags him and his anchor follows.
    bool TwinsHoldStationDue(Player* bot, TwinsView const& view)
    {
        if (view.landingWindow || !IsActiveMelee(view, bot) || view.veknilash->GetVictim() != bot)
            return false;
        if (view.veklor->GetExactDist2d(view.veknilash) >= Aq40TwinsRules::SEPARATION_GUARD)
            return false;
        return view.veknilash->GetExactDist2d(TwinsDragStation(view)) > TWINS_BOSS_ON_STATION;
    }

    bool TwinsLandingZoneFor(Player* bot, TwinsView const& view, TwinsMe const& me, Position& escape)
    {
        if (me.role == Role::None || !view.landingWindow || view.windowVeklorFrom == Side::None)
            return false;
        Side const from = view.windowVeklorFrom;
        struct Zone
        {
            Position const* centre;
            float radius;
            ObjectGuid pickup;
        };
        // Vek'lor lands on Vek'nilash's old side, whose waiting warlock is his pickup; Vek'nilash lands on Vek'lor's
        // old side, whose waiting melee tank is his. Ruling R13 F4: each zone is centred where that twin actually
        // landed (its rising-edge position: both are rooted for the window and the swap is exact), not on the
        // station, because a twin dragged off station lands off station. Pickup identities are unchanged.
        Zone const zones[2] = {
            {&view.windowVeklorPos, Aq40TwinsRules::CLEAR_RADIUS_VEKNILASH_SIDE,
             view.activeWarlock[SideIndex(Aq40TwinsRules::Other(from))]},
            {&view.windowVeknilashPos, Aq40TwinsRules::CLEAR_RADIUS_VEKLOR_SIDE, view.activeMelee[SideIndex(from)]}};
        // Pets count for the nearest-target roll (spec §5.8).
        Pet* const pet = bot->GetPet();
        bool const petLive = pet && pet->IsInWorld() && pet->IsAlive();
        for (Zone const& zone : zones)
        {
            // The pickup tank itself holds still; its pet is just another body and must clear (spec §5.4 step 1).
            Position const& centre = *zone.centre;
            bool const botIn = !IsSlotHolder(zone.pickup, bot) && bot->GetExactDist2d(centre) < zone.radius;
            bool const petIn = petLive && pet->GetExactDist2d(centre) < zone.radius;
            if (!botIn && !petIn)
                continue;
            // Only the pet inside (or the bot is the pickup): the bot stays put and the action recalls the pet.
            if (!botIn ||
                !BuildTwinsEscapeSpot(bot, centre.GetPositionX(), centre.GetPositionY(), zone.radius, 0, escape))
                escape.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
            return true;
        }
        return false;
    }

    // Revision 2 §11.2: the victim hold's destination for a wrong victim (TwinsWrongVictimTwin). Not gated on the
    // landing window: the rule is about who the twin is attacking (a passive twin attacks nobody). Revision 11 §20.1:
    // `kite` (optional) is set when the destination is Vek'nilash's drag station (no path query here: the movement
    // veto calls this every evaluation; the action computes the route step).
    bool TwinsWrongVictimFor(Player* bot, TwinsView const& view, TwinsMe const& me, Position& spot,
                             bool* kite = nullptr)
    {
        if (kite)
            *kite = false;
        if (me.role == Role::None)
            return false;
        Creature* const twin = TwinsWrongVictimTwin(bot, view.veklor, view.veknilash, view.veklorSide,
                                                    view.activeMelee, view.activeWarlock);
        if (!twin)
            return false;

        if (twin == view.veklor)
        {
            // Never drag him: stay inside his 45 y chase leash and out of Arcane Burst. 12-40 y: stay put.
            Creature* const veklor = view.veklor;
            float const d = bot->GetExactDist2d(veklor);
            if (d >= Aq40TwinsRules::WRONG_VICTIM_MIN && d <= Aq40TwinsRules::WRONG_VICTIM_MAX)
            {
                spot.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
                return true;
            }
            if (d < Aq40TwinsRules::WRONG_VICTIM_MIN)
            {
                // Out of Arcane Burst to 14 y on a stable sector bearing (never "parks" short of it).
                BuildTwinsEscapeSpot(bot, veklor->GetPositionX(), veklor->GetPositionY(),
                                     TWINS_WARLOCK_ESCAPE_DIST - TWINS_STATION_TOLERANCE, 0, spot, false);
                return true;
            }
            // Beyond 40 y (should not happen: he walks to a victim past 45 y): 38 y from him on the bot's bearing,
            // radial, so the destination does not creep as the bot walks it.
            float const bearing = veklor->GetAngle(bot);
            float const x = veklor->GetPositionX() + std::cos(bearing) * TWINS_VICTIM_FAR_DIST;
            float const y = veklor->GetPositionY() + std::sin(bearing) * TWINS_VICTIM_FAR_DIST;
            float z = bot->GetPositionZ();
            bot->UpdateAllowedPositionZ(x, y, z);
            spot.Relocate(x, y, z);
            return true;
        }

        // Vek'nilash. Revision 11 §20.1: while the twins are closer than SEPARATION_GUARD, or with no living active
        // melee tank, the victim itself kites him to the drag station (the authored station farther from Vek'lor, the
        // latched dragSide, §12.2 / R22); otherwise it walks him to his side's active melee tank, which holds the
        // station (dev pull 13: with the tanks dead the old "no tank, no destination" held him mid-room in Heal Brother
        // range).
        Player* const tank = TwinsActiveTankFor(view, bot, AQT_DATA_VEKNILASH);
        if (Aq40TwinsRules::VictimKites(view.veklor->GetExactDist2d(view.veknilash), tank != nullptr))
        {
            spot = TwinsDragStation(view);
            if (kite)
                *kite = true;
            return true;
        }
        spot.Relocate(tank->GetPositionX(), tank->GetPositionY(), tank->GetPositionZ());
        return true;
    }

    // A Dps bot's target: a mutated bug within BUG_SWITCH_RADIUS (dataId 0), else the twin its damage school can
    // hurt (dataId AQT_DATA_VEKLOR / AQT_DATA_VEKNILASH). Revision 7 §16.2: a physical melee non-hunter skips a bug
    // within 14 y of a living Vek'lor (FindTwinsMutatedBug's melee filter).
    Unit* TwinsDpsTarget(Player* bot, TwinsView const& view, uint32& dataId)
    {
        if (Creature* const bug = FindTwinsMutatedBug(bot, true))
        {
            if (bot->GetExactDist2d(bug) <= Aq40TwinsRules::BUG_SWITCH_RADIUS)
            {
                dataId = 0;
                return bug;
            }
        }
        if (TwinsIsPhysical(bot))
        {
            dataId = AQT_DATA_VEKNILASH;
            return view.veknilash;
        }
        dataId = AQT_DATA_VEKLOR;
        return view.veklor;
    }

    // Own and active-tank threat on `twin` for a capped bot. False = no cap (not a twin, a healer, an active tank
    // of either twin, or unlatched).
    bool TwinsThreatPair(Player* bot, Unit* twin, float& own, float& tank)
    {
        if (!bot || !twin)
            return false;
        TwinsView view;
        if (!GetTwinsView(bot, view))
            return false;
        uint32 const dataId = twin == view.veklor ? AQT_DATA_VEKLOR
                              : twin == view.veknilash ? AQT_DATA_VEKNILASH
                                                       : 0;
        if (!dataId)
            return false;
        TwinsMe const me = GetTwinsMe(bot);
        if (me.role == Role::None || me.role == Role::Healer || IsActiveWarlock(view, bot) ||
            IsActiveMelee(view, bot))
            return false;
        own = twin->GetThreatMgr().GetThreat(bot);
        // Revision 5 §14.1: Vek'lor's denominator is whoever he is attacking, a player or pet other than this bot, so
        // Dps never pull him off his current holder (there is no hold-fire on him any more). A REACT_PASSIVE twin
        // attacks nobody (Twin Teleport leaves the stale pre-teleport victim in place until his next UpdateVictim).
        if (dataId == AQT_DATA_VEKLOR)
        {
            Unit* const victim = view.veklor->GetReactState() != REACT_PASSIVE ? view.veklor->GetVictim() : nullptr;
            if (victim && victim != bot && victim->IsAlive() && (victim->IsPlayer() || victim->IsPet()))
            {
                tank = twin->GetThreatMgr().GetThreat(victim);
                return true;
            }
        }
        // Otherwise the twin's DESIGNATED active tank (spec §5.6): the warlock is not a role tank.
        Player* const active = TwinsActiveTankFor(view, bot, dataId);
        tank = active ? twin->GetThreatMgr().GetThreat(active) : 0.0f;
        return true;
    }
}  // namespace

bool GetTwinsSpot(Player* bot, PlayerbotAI* /*botAI*/, Position& spot, bool* parkable)
{
    TwinsView view;
    if (!GetTwinsView(bot, view))
        return false;
    return TwinsSpotFor(bot, view, GetTwinsMe(bot), spot, true, parkable);
}

void TwinsStationStep(Player* bot, Position const& station, Position& step)
{
    float const d = bot->GetExactDist2d(station);
    if (d <= TWINS_MT_STEP_MIN)
    {
        step = station;  // on the station: stand, he walks to his victim
        return;
    }
    if (TwinsPathStep(bot, station, step))
        return;

    // No usable route: a straight-line step, clipped to TWINS_MT_STEP_MAX.
    float const f = std::min(1.0f, TWINS_MT_STEP_MAX / d);
    float const x = bot->GetPositionX() + (station.GetPositionX() - bot->GetPositionX()) * f;
    float const y = bot->GetPositionY() + (station.GetPositionY() - bot->GetPositionY()) * f;
    float z = station.GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);
    step.Relocate(x, y, z);
}

bool TwinsShouldHoldStation(Player* bot, PlayerbotAI* /*botAI*/, Position& step)
{
    TwinsView view;
    if (!GetTwinsView(bot, view) || !TwinsHoldStationDue(bot, view))
        return false;
    TwinsStationStep(bot, TwinsDragStation(view), step);
    return true;
}

bool TwinsInLandingZone(Player* bot, PlayerbotAI* /*botAI*/, Position& escape)
{
    TwinsView view;
    if (!GetTwinsView(bot, view))
        return false;
    return TwinsLandingZoneFor(bot, view, GetTwinsMe(bot), escape);
}

Creature* TwinsWrongVictimOf(Player* bot, PlayerbotAI* /*botAI*/)
{
    TwinsView view;
    if (!GetTwinsView(bot, view))
        return nullptr;
    Creature* const twin =
        TwinsWrongVictimTwin(bot, view.veklor, view.veknilash, view.veklorSide, view.activeMelee, view.activeWarlock);
    // Victim tests first (O(1)); the assignment lookup takes the lock, so only for a bot a twin is attacking.
    if (!twin || GetTwinsMe(bot).role == Role::None)
        return nullptr;
    return twin;
}

bool TwinsWrongVictim(Player* bot, PlayerbotAI* /*botAI*/, Position& spot, bool* kite)
{
    if (kite)
        *kite = false;
    TwinsView view;
    if (!GetTwinsView(bot, view))
        return false;
    return TwinsWrongVictimFor(bot, view, GetTwinsMe(bot), spot, kite);
}

Position TwinsBerthed(Player* bot, Position const& dest)
{
    Creature* const veklor = GetTwin(bot, AQT_DATA_VEKLOR);
    if (!veklor || !veklor->IsAlive() || !bot->IsInWorld())
        return dest;
    TwinsView view;
    if (GetTwinsView(bot, view))
    {
        if (view.landingWindow || IsActiveWarlock(view, bot))
            return dest;
    }
    else if (IsTwinsEncounterActive(bot))
        return dest;  // mid-encounter with no view: no opinion
    else
    {
        // Form-up: warlock A stages on its casting spot and is Vek'lor's active warlock from the pull.
        TwinsMe const staging = GetTwinsStagingMe(bot);
        if (staging.role == Role::WarlockTank && staging.side == Side::A)
            return dest;
    }
    Aq40TwinsRules::Pt2 const to{dest.GetPositionX(), dest.GetPositionY()};
    Aq40TwinsRules::Pt2 const w = Aq40TwinsRules::BerthWaypoint(
        {bot->GetPositionX(), bot->GetPositionY()}, to, {veklor->GetPositionX(), veklor->GetPositionY()});
    if (w.x == to.x && w.y == to.y)
        return dest;
    float z = std::max(bot->GetPositionZ(), veklor->GetPositionZ()) + 2.0f;
    bot->UpdateAllowedPositionZ(w.x, w.y, z);
    return Position(w.x, w.y, z);
}

Unit* GetTwinsAttackTarget(Player* bot, PlayerbotAI* /*botAI*/)
{
    TwinsView view;
    if (!GetTwinsView(bot, view))
        return nullptr;

    TwinsMe const me = GetTwinsMe(bot);
    if (me.role == Role::None || me.role == Role::Healer)
        return nullptr;  // no opinion / never switch

    // Designated tanks never switch to a bug (spec §5.7): the waiting ones hold, the active ones keep their twin.
    Unit* target = nullptr;
    if (IsActiveWarlock(view, bot))
        target = view.veklor;
    else if (IsActiveMelee(view, bot) || IsVeknilashPickupInWindow(view, bot))
        target = view.veknilash;  // Revision 10 §19.1: the pickup engages him inside the landing window
    else if (IsWaitingTank(view, bot))
        return nullptr;
    else
    {
        uint32 dataId = 0;
        target = TwinsDpsTarget(bot, view, dataId);
    }
    if (!target || !target->IsAlive() || !bot->IsValidAttackTarget(target))
        return nullptr;
    return target;
}

bool IsTwinsHoldingFire(Player* bot, PlayerbotAI* /*botAI*/)
{
    TwinsView view;
    if (!GetTwinsView(bot, view))
        return false;

    TwinsMe const me = GetTwinsMe(bot);
    if (me.role == Role::None || me.role == Role::Healer)
        return false;
    // Revision 10 §19.1: the Vek'nilash pickup attacks at once inside the landing window (checked before the waiting
    // test, so it holds whatever the live labels say).
    if (IsActiveWarlock(view, bot) || IsActiveMelee(view, bot) || IsVeknilashPickupInWindow(view, bot))
        return false;
    if (IsWaitingTank(view, bot))
        return true;

    uint32 dataId = 0;
    TwinsDpsTarget(bot, view, dataId);
    if (!dataId)
        return false;  // a mutated bug never holds fire
    // Revision 3 §12.1: the melee pre-swap move holds fire on Vek'nilash (attack starters and target choosers are
    // vetoed, and the threat hold stops a running swing once) until the next window closes.
    if (TwinsPreSwapRaw(bot, me.role, me.spare, me.rangedKind, view.landingWindow, view.msSinceWindowStart,
                        view.veklor, view.veknilash, view.veklorSide, view.activeMelee, view.activeWarlock))
        return true;
    // Revision 5 §14.1: Dps never hold fire on Vek'lor. He stays put while his victim is within his 45 y leash and the
    // casters' arc is 24 y out, so waiting bought nothing and left 15-28 casters idle after every swap. The threat
    // cap (denominator: whoever he is attacking, TwinsThreatPair) keeps them off him instead.
    if (dataId != AQT_DATA_VEKNILASH)
        return false;
    Player* const active = TwinsActiveTankFor(view, bot, dataId);
    Unit* const victim = view.veknilash->GetVictim();
    return Aq40TwinsRules::HoldFireOnTwin(active && victim == active, view.msSinceWindowOrEngage);
}

bool TwinsThreatVetoed(Player* bot, Unit* twin)
{
    float own = 0.0f;
    float tank = 0.0f;
    return TwinsThreatPair(bot, twin, own, tank) && Aq40TwinsRules::ThreatVetoed(own, tank);
}

bool TwinsThreatHardStop(Player* bot, Unit* twin)
{
    float own = 0.0f;
    float tank = 0.0f;
    return TwinsThreatPair(bot, twin, own, tank) && Aq40TwinsRules::ThreatHardStop(own, tank);
}

bool TwinsShouldWarlockTank(Player* bot, PlayerbotAI* /*botAI*/)
{
    TwinsView view;
    if (!GetTwinsView(bot, view))
        return false;
    // Revision 10 §19.2: inside the landing window only the Vek'lor pickup casts, from where it waits (it is the
    // nearest non-melee body when he wakes, and its damage during his passive 2 s counts as threat).
    if (view.landingWindow ? !IsVeklorPickupInWindow(view, bot) : !IsActiveWarlock(view, bot))
        return false;
    // Not moving: a cast-time spell is refused while moving (PlayerbotAI::CanCastSpell), and a walk in progress is
    // either a hazard dodge or the hold-spot walk to the casting spot or the LOS point, all of which must finish.
    return !bot->isMoving() && TwinsWarlockCastsFromHere(bot, view.veklor);
}

bool TwinsWarlockMayReach(Player* bot, PlayerbotAI* botAI)
{
    TwinsView view;
    if (!GetTwinsView(bot, view) || view.landingWindow || !IsActiveWarlock(view, bot))
        return false;
    if (TwinsWarlockMoveFor(bot, view) != TwinsWarlockMove::Reach)
        return false;
    // A hazard dodge outranks the reach: keep the veto while one is due (Revision 7 §16.1: the burst escape too).
    return !TwinsShouldClearExplode(bot, botAI) && !TwinsShouldClearBlizzard(bot, botAI) && !TwinsInBurstRange(bot);
}

bool TwinsShouldRetarget(Player* bot, PlayerbotAI* botAI)
{
    // A holding bot never retargets: GetTwinsAttackTarget still names a Dps bot's twin during the hold, and
    // "focus target" would Attack() it. (Final-review M5: no per-call log here; it fired every tick per held bot.)
    if (IsTwinsHoldingFire(bot, botAI))
        return false;
    Unit* want = GetTwinsAttackTarget(bot, botAI);
    if (!want)
        return false;
    // Over the threat cap on that twin: never re-Attack() it. The threat hold's AttackStop() clears the victim, and
    // "focus target" (name-exempt from the threat multiplier) would otherwise restart the swing the next tick,
    // undoing the 0.90 hard stop and bypassing the 0.80 MeleeAction veto. MeleeAction resumes on its own once the
    // ratio drops. TwinsThreatVetoed is false for a non-twin (a mutated bug) and for uncapped bots.
    if (TwinsThreatVetoed(bot, want))
        return false;
    return bot->GetVictim() != want;
}

// ---------------------------------------------------------------------------------------------
// HAZARDS
// ---------------------------------------------------------------------------------------------
bool BuildTwinsEscapeSpot(Player* bot, float cx, float cy, float clearance, uint32 attempt,
                          Position& spot, bool parkWhenClear)
{
    // The bearing snaps to fixed sectors so the destination is STABLE across ticks. A spot derived
    // from the bot's own live position creeps a couple of yards per tick, and MoveTo's
    // duplicate-destination guard then refuses every order, so the bot never leaves.
    float x = 0.0f;
    float y = 0.0f;
    TwinsEscapeXY(bot, cx, cy, clearance, attempt, x, y);
    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);

    if (parkWhenClear && bot->GetExactDist2d(x, y) <= TWINS_STATION_TOLERANCE)
        return false;  // already clear: park rather than shuffle

    spot.Relocate(x, y, z);
    return true;
}

Creature* FindTwinsExplodingBug(Player* bot)
{
    if (!IsTwinsEncounterActive(bot))
        return nullptr;

    std::list<Creature*> bugs;
    std::vector<uint32> const entries = {NPC_QIRAJI_SCARAB, NPC_QIRAJI_SCORPION};
    bot->GetCreatureListWithEntryInGrid(bugs, entries, TWINS_SEARCH_RANGE);

    Creature* nearest = nullptr;
    float nearestDist = 0.0f;
    for (Creature* bug : bugs)
    {
        if (!bug || !bug->IsAlive() || !bug->HasAura(SPELL_EXPLODE_BUG))
            continue;
        float const d = bot->GetExactDist2d(bug);
        if (!nearest || d < nearestDist)
        {
            nearest = bug;
            nearestDist = d;
        }
    }
    return nearest;
}

bool TwinsShouldClearExplode(Player* bot, PlayerbotAI* /*botAI*/)
{
    Creature* bug = FindTwinsExplodingBug(bot);
    // 804 is a 3s fuse, then 26059 for ~3000 fire in 15y. The bug is hostile and moving, which is
    // why the destination is stabilised by the sector snap rather than by freezing the centre.
    return bug && bot->GetExactDist2d(bug) < TWINS_EXPLODE_RADIUS + TWINS_STATION_TOLERANCE;
}

bool GetTwinsExplodeClearSpot(Player* bot, Position& spot, uint32 attempt)
{
    Creature* bug = FindTwinsExplodingBug(bot);
    if (!bug)
        return false;

    return BuildTwinsEscapeSpot(bot, bug->GetPositionX(), bug->GetPositionY(),
                                TWINS_EXPLODE_RADIUS, attempt, spot);
}

bool GetTwinsBlizzardCenter(Player* bot, Position& center)
{
    Creature* veklor = GetTwin(bot, AQT_DATA_VEKLOR);
    if (!veklor)
        return false;

    // 26607's effect 0 is a PERSISTENT_AREA_AURA, so the live patch is a DynamicObject owned by
    // its caster. It lands on a random raid member, NOT on Vek'lor, so the dynobject's own
    // position is the only source of truth.
    DynamicObject* patch = veklor->GetDynObject(SPELL_BLIZZARD);
    if (!patch)
        return false;

    center.Relocate(patch->GetPositionX(), patch->GetPositionY(), patch->GetPositionZ());
    return true;
}

bool TwinsShouldClearBlizzard(Player* bot, PlayerbotAI* /*botAI*/)
{
    if (!IsTwinsEncounterActive(bot))
        return false;

    Position center;
    if (!GetTwinsBlizzardCenter(bot, center))
        return false;

    return bot->GetExactDist2d(center.GetPositionX(), center.GetPositionY()) <
           TWINS_BLIZZARD_RADIUS + TWINS_STATION_TOLERANCE;
}

bool GetTwinsBlizzardClearSpot(Player* bot, Position& spot, uint32 attempt)
{
    Position center;
    if (!GetTwinsBlizzardCenter(bot, center))
        return false;

    // Clearance is the patch radius, so the escape can never land back inside the same patch.
    return BuildTwinsEscapeSpot(bot, center.GetPositionX(), center.GetPositionY(),
                                TWINS_BLIZZARD_RADIUS, attempt, spot);
}

bool TwinsInBurstRange(Player* bot)
{
    if (!bot->IsAlive() || !IsTwinsEncounterActive(bot))
        return false;
    Creature* const veklor = GetTwin(bot, AQT_DATA_VEKLOR);
    if (!veklor)
        return false;
    return Aq40TwinsRules::InBurstRange(bot->GetExactDist2d(veklor), veklor->IsAlive(),
                                        veklor->GetReactState() == REACT_PASSIVE);
}

bool GetTwinsBurstClearSpot(Player* bot, Position& spot, uint32 attempt)
{
    Creature* const veklor = GetTwin(bot, AQT_DATA_VEKLOR);
    if (!veklor)
        return false;
    // BuildTwinsEscapeSpot adds TWINS_STATION_TOLERANCE to the clearance (the active warlock's 14 y escape's shape).
    // Never parked: the trigger only holds inside 12.5 y, 3.5 y short of the point.
    return BuildTwinsEscapeSpot(bot, veklor->GetPositionX(), veklor->GetPositionY(),
                                Aq40TwinsRules::BURST_ESCAPE_TO - TWINS_STATION_TOLERANCE, attempt, spot, false);
}

// ---------------------------------------------------------------------------------------------
// PETS
// ---------------------------------------------------------------------------------------------
bool TwinsShouldDirectPet(Player* bot, PlayerbotAI* /*botAI*/)
{
    if (!bot->GetPet() && bot->m_Controlled.empty())
        return false;
    // Form-up (spec §5.5): pets passive and following until the pull.
    return IsTwinsEncounterActive(bot) || IsTwinsFormUp(bot);
}

// Never fails open while a bot walks to its spot: GetTwinsSpot holds for the whole walk (memory
// station-veto-must-not-fail-open-while-travelling). The hazard dodges are checked first and without the view, so
// they keep the bot even while the stations are provisional.
bool TwinsShouldSuppressGenericMovement(Player* bot, PlayerbotAI* botAI)
{
    if (!IsTwinsEncounterActive(bot))
        return false;
    // Revision 7 §16.1: the burst escape too, so generic movers never pull the bot back in mid-escape.
    if (TwinsShouldClearExplode(bot, botAI) || TwinsShouldClearBlizzard(bot, botAI) || TwinsInBurstRange(bot))
        return true;

    TwinsView view;
    if (!GetTwinsView(bot, view))
        return false;
    // Revision 10 §19.1: the Vek'nilash pickup closes to melee with native reach inside the landing window. No spot,
    // no station hold, no landing clear and no wrong-victim rule (a passive twin attacks nobody); the hazard dodges
    // above still win, and the berth is inactive in the window.
    if (IsVeknilashPickupInWindow(view, bot))
        return false;
    TwinsMe const me = GetTwinsMe(bot);
    Position unused;
    // Ruling R13 F1: the active warlock either owns its casting spot (TwinsSpotFor) or owns the position it casts
    // from, so it is held whenever it holds the slot and never fails open.
    if (me.role != Role::None && IsSlotHolder(view.activeWarlock[SideIndex(TwinsSpotVeklorSide(view))], bot))
        return true;
    return TwinsSpotFor(bot, view, me, unused, false) || TwinsHoldStationDue(bot, view) ||
           TwinsLandingZoneFor(bot, view, me, unused) || TwinsWrongVictimFor(bot, view, me, unused);
}

bool TwinsHealerFrozen(Player* bot, bool isExemptDodge)
{
    // PlayerbotAI::IsHeal first: the latch assigns Role::Healer from it, and this runs for every MovementAction of
    // every bot in AQ40, so a non-healer never fetches the view here.
    if (isExemptDodge || !PlayerbotAI::IsHeal(bot))
        return false;
    TwinsView view;
    if (!GetTwinsView(bot, view) || !view.landingWindow)
        return false;
    return Aq40TwinsRules::HealerFrozenInWindow(view.landingWindow, GetTwinsMe(bot).role == Role::Healer,
                                                isExemptDodge);
}

// ---------------------------------------------------------------------------------------------
// FORM-UP AND THE PULL (spec §5.5, rewritten 2026-10-22; rulings R11, R12). Out of combat, before the pull: every
// bot holds a staging spot, then the owner says "twins pull". The staging assignment is its own per-instance cache
// under its own mutex, apart from the engage latch (twinsStateByInstance): GetTwinsView's "erase when not
// IN_PROGRESS" and TwinsEraseTrackers never touch it, and it never touches them.
// ---------------------------------------------------------------------------------------------
namespace
{
    struct TwinsStagingEntry
    {
        TwinsMe me;
        uint8 cls = 0;
        bool isTank = false;
        bool isMelee = false;
        // Revision 3 §12.2: copied from TwinsStagingState::stageTrack when the entry is read (never stored).
        bool parked = false;
    };

    struct TwinsStagingState
    {
        std::unordered_map<ObjectGuid, TwinsStagingEntry> byGuid;
        // The lowest-guid designated melee tank (else the lowest-guid assigned bot): the one bot that announces the
        // form-up (ruling R12). Empty = none.
        ObjectGuid announcer;
        // Every assigned bot within STAGE_ARRIVE of its staging spot at the last refresh.
        bool allFormed = false;
        // The formed-up whisper went out this episode.
        bool announced = false;
        // S_A's ground z, from UpdateAllowedPositionZ at the first refresh.
        float stageAz = 0.0f;
        bool stageAzSet = false;
        // Last refresh (group scan), and the last form-up call (the episode ends after a gap).
        uint32 refreshMs = 0;
        uint32 lastSeenMs = 0;
        // Revision 3 §12.2: unreachable staging spots (TwinsReportStageMove), cleared when the episode ends.
        std::unordered_map<ObjectGuid, TwinsSpotTrack> stageTrack;
    };

    bool TwinsStageParked(TwinsStagingState const& st, ObjectGuid guid)
    {
        auto const t = st.stageTrack.find(guid);
        return t != st.stageTrack.end() && t->second.parked;
    }

    std::mutex twinsStagingMutex;
    std::unordered_map<uint32, TwinsStagingState> twinsStagingByInstance;
    // twinsStagingByInstance.size(), written under twinsStagingMutex: IsTwinsFormUp's drop path stays lock-free while
    // no instance holds a staging cache (it runs in the fight too, from the movement multiplier).
    std::atomic<uint32> twinsStagingCount{0};

    // Ruling R11: STAGE_A_DIST from station A along the probe-verified A -> B crossing line; z is A's until the first
    // refresh corrects it to the ground.
    Position TwinsStageA()
    {
        Position const& a = TWINS_STATIONS[0].station;
        Position const& b = TWINS_STATIONS[1].station;
        Aq40TwinsRules::Pt2 const p = Aq40TwinsRules::PointAlong({a.GetPositionX(), a.GetPositionY()},
                                                                 {b.GetPositionX(), b.GetPositionY()},
                                                                 Aq40TwinsRules::STAGE_A_DIST);
        return Position(p.x, p.y, a.GetPositionZ());
    }

    // The staging spot of one assignment (veklorSide = A at the pull). False for humans (Role::None).
    bool TwinsStagingSpotFor(TwinsStagingEntry const& e, Position const& stageA, Position& spot)
    {
        TwinsStationSpots const& b = TWINS_STATIONS[SideIndex(Side::B)];
        switch (e.me.role)
        {
            case Role::MeleeTank:
            case Role::WarlockTank:
            case Role::Healer:
                if (e.me.side == Side::A)
                {
                    // Ruling R16 H2: warlock A stages on A's casting spot (26 y down the crossing lane, outside the
                    // ~23 y aggro), so it pulls from where it then casts. Warrior A and the A healers keep S_A.
                    spot = e.me.role == Role::WarlockTank ? TWINS_STATIONS[SideIndex(Side::A)].warlockCast : stageA;
                    return true;
                }
                if (e.me.side != Side::B)
                    return false;
                if (e.me.role == Role::MeleeTank)
                    spot = TWINS_STAGE_B;  // R16 H3: Vek'nilash's active tank at the pull, staged on his path to A
                else if (e.me.role == Role::WarlockTank)
                    spot = b.warlockWait;
                else
                    spot = TwinsHealerSpot(Side::B, e.me.rankOnSide);
                return true;
            case Role::Dps:
                // Physical DPS and spare melee tanks fight Vek'nilash at B first; magic DPS (spare warlocks
                // included) stage with side A. Revision 2 §11.4: hunters stage on B's ranged arc, where they fight
                // him first (every B arc spot is ~60 y from his spawn, outside aggro).
                if (e.me.rangedKind == TwinsRangedKind::Hunter)
                    spot = TwinsRangedSpot(Side::B, e.me.rangedRank);
                else
                    spot = e.isTank || Aq40TwinsRules::IsPhysical(e.cls, e.isMelee) ? b.warriorWait : stageA;
                return true;
            default:
                return false;
        }
    }

    // A fresh copy of the bot's staging entry and of the instance's S_A, refreshing the instance's cache (one group
    // scan, outside the lock) at most every TWINS_STAGING_TTL_MS. The caller has checked IsTwinsFormUp. False when
    // ungrouped or when the scan did not see the bot.
    bool TwinsStagingEntryFor(Player* bot, TwinsStagingEntry& out, Position& stageA)
    {
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        uint32 const now = TwinsNowMs();
        stageA = TwinsStageA();

        TwinsStagingState st;
        {
            std::lock_guard<std::mutex> lock(twinsStagingMutex);
            auto it = twinsStagingByInstance.find(instanceId);
            if (it != twinsStagingByInstance.end())
            {
                TwinsStagingState& cur = it->second;
                // A gap in form-up calls ends the episode (the master left the room, everyone was in combat, ...).
                if (getMSTimeDiff(cur.lastSeenMs, now) > TWINS_STAGING_EPISODE_GAP_MS)
                {
                    cur.announced = false;
                    cur.allFormed = false;
                    cur.refreshMs = 0;
                    cur.stageTrack.clear();
                }
                cur.lastSeenMs = now;
                if (cur.refreshMs && getMSTimeDiff(cur.refreshMs, now) < TWINS_STAGING_TTL_MS)
                {
                    if (cur.stageAzSet)
                        stageA.m_positionZ = cur.stageAz;
                    auto e = cur.byGuid.find(bot->GetGUID());
                    if (e == cur.byGuid.end())
                        return false;
                    out = e->second;
                    out.parked = TwinsStageParked(cur, bot->GetGUID());
                    return true;
                }
                st = cur;  // refresh a private copy
            }
        }

        if (!bot->GetGroup())
            return false;

        // One group scan, then one O(n) pass: the same AssignTwins input the engage latch builds.
        std::vector<Player*> const present = ScanTwinsGroup(bot);
        std::vector<Aq40TwinsRules::Member> input;
        std::vector<Player*> who;
        BuildTwinsMembers(present, input, who);
        std::vector<Aq40TwinsRules::Assignment> const assigned = Aq40TwinsRules::AssignTwins(input);
        std::vector<uint8> const ranks = TwinsHealerRanks(assigned, input);
        std::vector<TwinsRangedSlot> const ranged = TwinsRangedRanks(assigned, input);

        if (!st.stageAzSet)
        {
            float z = stageA.GetPositionZ() + 2.0f;
            bot->UpdateAllowedPositionZ(stageA.GetPositionX(), stageA.GetPositionY(), z);
            st.stageAz = z;
            st.stageAzSet = true;
        }
        stageA.m_positionZ = st.stageAz;

        st.byGuid.clear();
        st.announcer = ObjectGuid::Empty;
        uint32 announcerCounter = 0;
        // Fallback announcer when the raid has no designated melee tank: the lowest-guid assigned bot.
        ObjectGuid fallback;
        uint32 fallbackCounter = 0;
        uint32 withSpot = 0;
        bool allFormed = true;
        for (size_t i = 0; i < assigned.size(); ++i)
        {
            TwinsStagingEntry e;
            e.me = TwinsMe{assigned[i].role, assigned[i].side, assigned[i].spare, ranks[i], ranged[i].kind,
                           ranged[i].rank, false, false, Position(), false};  // never parked / stepped in at form-up
            e.cls = input[i].cls;
            e.isTank = input[i].isTank;
            e.isMelee = input[i].isMelee;
            st.byGuid[who[i]->GetGUID()] = e;

            if (e.me.role == Role::MeleeTank && (st.announcer.IsEmpty() || input[i].guid < announcerCounter))
            {
                st.announcer = who[i]->GetGUID();
                announcerCounter = input[i].guid;
            }
            Position spot;
            if (!TwinsStagingSpotFor(e, stageA, spot))
                continue;
            if (fallback.IsEmpty() || input[i].guid < fallbackCounter)
            {
                fallback = who[i]->GetGUID();
                fallbackCounter = input[i].guid;
            }
            ++withSpot;
            // A parked bot (its spot proved unreachable) is formed up where it stands.
            if (!TwinsStageParked(st, who[i]->GetGUID()) &&
                who[i]->GetExactDist2d(spot) > Aq40TwinsRules::STAGE_ARRIVE)
                allFormed = false;
        }
        if (st.announcer.IsEmpty())
            st.announcer = fallback;
        st.allFormed = withSpot > 0 && allFormed;
        st.refreshMs = now;
        st.lastSeenMs = now;

        {
            std::lock_guard<std::mutex> lock(twinsStagingMutex);
            TwinsStagingState& cur = twinsStagingByInstance[instanceId];
            // The episode flag is the only field another bot's call may have changed since the copy (the stage tracks
            // are written on this instance's map thread only, so the copy still holds them).
            st.announced = st.announced || cur.announced;
            cur = st;
            twinsStagingCount.store(uint32(twinsStagingByInstance.size()), std::memory_order_relaxed);
        }

        auto e = st.byGuid.find(bot->GetGUID());
        if (e == st.byGuid.end())
            return false;
        out = e->second;
        out.parked = TwinsStageParked(st, bot->GetGUID());
        return true;
    }

    void TwinsDropStaging(uint32 instanceId)
    {
        if (!twinsStagingCount.load(std::memory_order_relaxed))
            return;
        std::lock_guard<std::mutex> lock(twinsStagingMutex);
        twinsStagingByInstance.erase(instanceId);
        twinsStagingCount.store(uint32(twinsStagingByInstance.size()), std::memory_order_relaxed);
    }
}  // namespace

bool IsTwinsFormUp(Player* bot)
{
    if (!IsInAq40(bot) || !bot->IsInWorld())
        return false;
    InstanceScript* const instance = bot->GetInstanceScript();
    if (!instance)
        return false;

    // The instance-wide half: the pull has not happened (or the raid wiped and the twins reset).
    Creature* const veklor = instance->GetCreature(AQT_DATA_VEKLOR);
    Creature* const veknilash = instance->GetCreature(AQT_DATA_VEKNILASH);
    if (instance->GetBossState(AQT_DATA_TWIN_EMPERORS) != NOT_STARTED || !veklor || !veknilash ||
        !veklor->IsAlive() || !veknilash->IsAlive() || veklor->IsInCombat() || veknilash->IsInCombat())
    {
        TwinsDropStaging(bot->GetMap()->GetInstanceId());
        return false;
    }

    // The per-bot half: out of combat, in the Twins room itself, with a real-player master there too. The bot's own
    // position matters: a straggler elsewhere on the map (left at the entrance, revived) would otherwise get a
    // staging spot several hundred yards off, past PathGenerator's ~296 y ceiling, with follow vetoed. It just
    // follows its master in until it qualifies.
    if (bot->IsInCombat())
        return false;
    Position const mid((TWINS_VEKLOR_SPAWN.GetPositionX() + TWINS_VEKNILASH_SPAWN.GetPositionX()) * 0.5f,
                       (TWINS_VEKLOR_SPAWN.GetPositionY() + TWINS_VEKNILASH_SPAWN.GetPositionY()) * 0.5f,
                       (TWINS_VEKLOR_SPAWN.GetPositionZ() + TWINS_VEKNILASH_SPAWN.GetPositionZ()) * 0.5f);
    if (bot->GetExactDist(mid) > Aq40TwinsRules::ROOM_RADIUS)
        return false;
    PlayerbotAI* const botAI = GET_PLAYERBOT_AI(bot);
    Player* const master = botAI ? botAI->GetMaster() : nullptr;
    if (!master || master == bot || !master->IsInWorld() || master->GetMap() != bot->GetMap())
        return false;
    if (GET_PLAYERBOT_AI(master) && !IsSelfBot(master))
        return false;  // a bot master (random-bot groups): no owner to say "twins pull"
    return master->GetExactDist(mid) <= Aq40TwinsRules::ROOM_RADIUS;
}

TwinsMe GetTwinsStagingMe(Player* bot)
{
    if (!IsTwinsFormUp(bot))
        return TwinsMe{};
    TwinsStagingEntry e;
    Position stageA;
    return TwinsStagingEntryFor(bot, e, stageA) ? e.me : TwinsMe{};
}

bool GetTwinsStagingSpot(Player* bot, Position& spot)
{
    if (!IsTwinsFormUp(bot))
        return false;
    TwinsStagingEntry e;
    Position stageA;
    if (!TwinsStagingEntryFor(bot, e, stageA) || !TwinsStagingSpotFor(e, stageA, spot))
        return false;
    if (e.parked)
        spot.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
    return true;
}

void TwinsReportStageMove(Player* bot, bool moved, float dist)
{
    if (!bot || !bot->IsInWorld() || !IsInAq40(bot))
        return;
    bool const failed = !moved && dist > TWINS_UNREACHABLE_DIST && !bot->isMoving();
    if (!moved && !failed)
        return;
    uint32 const now = TwinsNowMs();
    bool parkedNow = false;
    {
        std::lock_guard<std::mutex> lock(twinsStagingMutex);
        auto it = twinsStagingByInstance.find(bot->GetMap()->GetInstanceId());
        if (it == twinsStagingByInstance.end())
            return;
        std::unordered_map<ObjectGuid, TwinsSpotTrack>& tracks = it->second.stageTrack;
        if (moved)
        {
            auto t = tracks.find(bot->GetGUID());
            if (t != tracks.end() && !t->second.parked)
                tracks.erase(t);  // consecutive failures only
            return;
        }
        TwinsSpotTrack& t = tracks[bot->GetGUID()];
        if (t.parked || (t.lastFailMs && getMSTimeDiff(t.lastFailMs, now) < TWINS_UNREACHABLE_DEBOUNCE_MS))
            return;
        t.lastFailMs = now;
        parkedNow = ++t.fails >= TWINS_UNREACHABLE_FAILS;
        t.parked = parkedNow;
    }
    if (parkedNow)
        LOG_DEBUG("playerbots", "[Aq40Twins] {} parked at form-up: staging spot unreachable ({:.1f} y off)",
                  bot->GetName(), dist);
}

void TwinsMaybeAnnounceFormedUp(Player* bot)
{
    if (!IsTwinsFormUp(bot))
        return;
    TwinsStagingEntry e;
    Position stageA;
    if (!TwinsStagingEntryFor(bot, e, stageA))  // refreshes allFormed at most once a second
        return;
    {
        std::lock_guard<std::mutex> lock(twinsStagingMutex);
        auto it = twinsStagingByInstance.find(bot->GetMap()->GetInstanceId());
        if (it == twinsStagingByInstance.end())
            return;
        TwinsStagingState& st = it->second;
        if (st.announced || !st.allFormed || st.announcer != bot->GetGUID())
            return;
        st.announced = true;
    }
    LOG_DEBUG("playerbots", "[Aq40Twins] instance {} formed up, {} announcing", bot->GetMap()->GetInstanceId(),
              bot->GetName());
    if (PlayerbotAI* const botAI = GET_PLAYERBOT_AI(bot))
        botAI->TellMaster("Twins: formed up - say \"twins pull\" when ready.");
}

Player* TwinsTankHealTarget(Player* bot, PlayerbotAI* botAI)
{
    // Cheap tests first: a class heal (healer classes only), alive, standing (no cast-time spell while moving).
    if (!bot || !botAI || !bot->IsAlive() || bot->isMoving())
        return nullptr;
    char const* const name = Aq40TankHealSpellName(bot);
    if (!name)
        return nullptr;
    TwinsView view;
    if (!GetTwinsView(bot, view))
        return nullptr;
    TwinsMe const me = GetTwinsMe(bot);
    if (me.role != Role::Healer || me.side == Side::None)
        return nullptr;
    // A wrong victim walks first (the victim hold); TwinsWrongVictimOf's test on this view.
    if (TwinsWrongVictimTwin(bot, view.veklor, view.veknilash, view.veklorSide, view.activeMelee, view.activeWarlock))
        return nullptr;

    // The side's two designated tanks, active or waiting (the view's slots are after replacement).
    size_t const i = SideIndex(me.side);
    Player* target = nullptr;
    for (ObjectGuid const guid : {view.activeMelee[i], view.activeWarlock[i]})
    {
        if (guid.IsEmpty())
            continue;
        // Map-local lookup: null when the holder is not on the bot's map.
        Player* const tank = ObjectAccessor::GetPlayer(*bot, guid);
        if (!tank || !tank->IsAlive() || tank->GetHealthPct() >= Aq40TwinsRules::TANK_HEAL_PCT ||
            bot->GetExactDist2d(tank) > Aq40TwinsRules::HEAL_REACH || !bot->IsWithinLOSInMap(tank))
            continue;
        if (!target || tank->GetHealthPct() < target->GetHealthPct())
            target = tank;
    }
    if (!target)
        return nullptr;

    // The Huhuran tank heal's castability tests (Aq40Helpers_Huhuran.cpp GetHuhuranTankHeal): skip, and let stock
    // healing run, when this cast cannot go off now. CanCastSpell never sees mana, so check the real cost.
    if (!botAI->CanCastSpell(name, target))
        return nullptr;
    int32 cost = 0;
    if (!Aq40SpellManaCost(bot, botAI, name, cost) || int32(bot->GetPower(POWER_MANA)) < cost)
        return nullptr;
    return target;
}

void TwinsCountTankHeal(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !IsInAq40(bot))
        return;
    std::lock_guard<std::mutex> lock(twinsMutex);
    auto it = twinsStateByInstance.find(bot->GetMap()->GetInstanceId());
    if (it != twinsStateByInstance.end() && it->second.latched)
        ++it->second.tankHeals;
}

}  // namespace TempleOfAhnQirajHelpers
