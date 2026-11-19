/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Princess Huhuran helper layer (overlay patch 0016). Spec:
// docs/superpowers/specs/2026-10-20-playerbot-aq40-huhuran-design.md (overlay repo), §4.1-4.6.

#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Aq40Helpers.h"
#include "Creature.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Playerbots.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "Timer.h"

namespace TempleOfAhnQirajHelpers
{

namespace
{
    using Aq40HuhuranRules::Pt;

    constexpr size_t ROLE_SLOTS = 2;  // [0] = Ranged, [1] = Healer

    struct HuhuranRankTable
    {
        bool built = false;
        std::unordered_map<ObjectGuid, uint32> rankByGuid;
    };

    // EVERYTHING per instance for Huhuran. Different instances update on parallel map threads, so the
    // map is only touched under huhuranMutex (memory: module globals need locking), held briefly, and
    // callers get copies, never references into it.
    struct HuhuranInstanceState
    {
        // Encounter clock. 0 = no attempt seen yet.
        uint32 engageMs = 0;
        uint32 lastSeenMs = 0;
        // Latches (spec §4.2 / §4.3). everParked: parked at least once this attempt (rev 2, §7.2 #1).
        bool parked = false;
        bool everParked = false;
        bool spotsLatched = false;
        // Tank station T, latched per attempt (GetHuhuranTankStation); the ring refBearing is P -> T.
        bool tankStationSet = false;
        float tankX = 0.0f;
        float tankY = 0.0f;
        // Spot ranks per role, built once after the spots latch (deaths never reshuffle; a late arrival
        // is not in the table and gets no slot).
        HuhuranRankTable ranks[ROLE_SLOTS];
        // "[Aq40Huhuran] diag" throttle. 0 = never.
        uint32 diagMs = 0;
        // Rev 3 (spec §8.2): first Berserk tick seen this attempt (0 = not yet), the healer bots already given
        // the one-time mana refill, and the MT defensives already used, keyed by (MT guid, HuhuranMtDefensive).
        uint32 berserkMs = 0;
        std::unordered_set<ObjectGuid> manaRefilled;
        std::set<std::pair<ObjectGuid, uint8>> mtDefensiveUsed;
        // Cached boss resolution. scanMs 0 = never scanned; an empty guid = "not found" by that scan.
        ObjectGuid huhuranGuid;
        uint32 scanMs = 0;
    };

    std::mutex huhuranMutex;
    std::unordered_map<uint32, HuhuranInstanceState> huhuranStateByInstance;

    uint32 NowMs()
    {
        uint32 const now = getMSTime();
        return now ? now : 1;
    }

    size_t RoleSlot(HuhuranSlotRole role) { return role == HuhuranSlotRole::Healer ? 1 : 0; }

    float RoleRadius(HuhuranSlotRole role)
    {
        return role == HuhuranSlotRole::Healer ? Aq40HuhuranRules::HEALER_RADIUS : Aq40HuhuranRules::RANGED_RADIUS;
    }

    Pt ParkPt() { return Pt{HUHURAN_PARK.GetPositionX(), HUHURAN_PARK.GetPositionY()}; }

    float ParkBearingTo(float x, float y)
    {
        return std::atan2(y - HUHURAN_PARK.GetPositionY(), x - HUHURAN_PARK.GetPositionX());
    }

    // Diag only: bearing P -> the latched T, WITHOUT latching it (0 while unlatched; the rank tables are
    // empty then, so the on-spot counts do not read it).
    float PeekRefBearing(uint32 instanceId)
    {
        std::lock_guard<std::mutex> lock(huhuranMutex);
        auto it = huhuranStateByInstance.find(instanceId);
        if (it == huhuranStateByInstance.end() || !it->second.tankStationSet)
            return 0.0f;
        return ParkBearingTo(it->second.tankX, it->second.tankY);
    }

    // Every successful resolution of her (gate or view) keeps the attempt alive. First sight, or a gap
    // longer than HUHURAN_ATTEMPT_GAP_MS since the last one, starts a fresh attempt: new engage clock,
    // latches, ranks and diag throttle; only the boss resolution cache survives. Returns ms since engage.
    uint32 TouchAttempt(uint32 instanceId, uint32 now)
    {
        std::lock_guard<std::mutex> lock(huhuranMutex);
        HuhuranInstanceState& st = huhuranStateByInstance[instanceId];
        if (!st.engageMs || getMSTimeDiff(st.lastSeenMs, now) > HUHURAN_ATTEMPT_GAP_MS)
        {
            HuhuranInstanceState fresh;
            fresh.huhuranGuid = st.huhuranGuid;
            fresh.scanMs = st.scanMs;
            st = std::move(fresh);
            st.engageMs = now;
            LOG_DEBUG("playerbots", "[Aq40Huhuran] instance {}: engage", instanceId);
        }
        st.lastSeenMs = now;
        return getMSTimeDiff(st.engageMs, now);
    }

    // Living Huhuran for the bot's instance, or nullptr. Gate (map, boss state) first. A known GUID
    // resolves map-wide with no scan; a scan by entry runs at most once per HUHURAN_SCAN_INTERVAL_MS per
    // instance. A "not found" is cached only when the scanning bot stood near P - a far bot's empty
    // scan (grid searches are clamped to ~533y) would otherwise blind the whole raid for the interval.
    Creature* FindHuhuran(Player* bot)
    {
        if (!IsInAq40(bot) || !bot->IsInWorld())
            return nullptr;
        InstanceScript* instance = bot->GetInstanceScript();
        if (!instance || instance->GetBossState(AQT_DATA_HUHURAN) != IN_PROGRESS)
            return nullptr;

        Map* const map = bot->GetMap();
        uint32 const instanceId = map->GetInstanceId();
        uint32 const now = NowMs();

        ObjectGuid knownGuid;
        bool freshScan = false;
        {
            std::lock_guard<std::mutex> lock(huhuranMutex);
            auto it = huhuranStateByInstance.find(instanceId);
            if (it != huhuranStateByInstance.end())
            {
                knownGuid = it->second.huhuranGuid;
                freshScan = it->second.scanMs && getMSTimeDiff(it->second.scanMs, now) < HUHURAN_SCAN_INTERVAL_MS;
            }
        }

        if (!knownGuid.IsEmpty())
            if (Creature* known = map->GetCreature(knownGuid))
                if (known->IsAlive() && known->GetEntry() == NPC_HUHURAN)
                {
                    TouchAttempt(instanceId, now);
                    return known;
                }
        if (freshScan)
            return nullptr;  // a recent scan near P found nothing (or found her since dead/despawned)

        bool const nearPark = bot->GetExactDist2d(HUHURAN_PARK.GetPositionX(), HUHURAN_PARK.GetPositionY()) <=
                              HUHURAN_SCAN_ORIGIN_RANGE;
        // Scan outside the lock: one instance's bots share one map thread.
        Creature* found = bot->FindNearestCreature(NPC_HUHURAN, HUHURAN_SEARCH_RANGE, true);
        if (found || nearPark)
        {
            std::lock_guard<std::mutex> lock(huhuranMutex);
            HuhuranInstanceState& st = huhuranStateByInstance[instanceId];
            st.huhuranGuid = found ? found->GetGUID() : ObjectGuid::Empty;
            st.scanMs = now;
        }
        if (found)
            TouchAttempt(instanceId, now);
        return found;
    }

    // Rank of `bot` in the latched table for `role`, building the table on the first call (spec §4.3,
    // mirroring GetCthunLatchedRank): living group bots of that role, sorted by bearing from P relative
    // to the reference bearing, GUID tie-break. Returns false when the bot holds no rank.
    bool GetLatchedRank(Player* bot, HuhuranSlotRole role, float refBearing, uint32& rank, uint32& count)
    {
        uint32 const instanceId = bot->GetMap()->GetInstanceId();
        size_t const slot = RoleSlot(role);
        {
            std::lock_guard<std::mutex> lock(huhuranMutex);
            auto it = huhuranStateByInstance.find(instanceId);
            if (it != huhuranStateByInstance.end() && it->second.ranks[slot].built)
            {
                HuhuranRankTable const& table = it->second.ranks[slot];
                auto r = table.rankByGuid.find(bot->GetGUID());
                if (r == table.rankByGuid.end())
                    return false;
                rank = r->second;
                count = uint32(table.rankByGuid.size());
                return rank < count;
            }
        }

        // Only a living bot of the role may build the table, so it is never latched empty.
        if (!bot->IsAlive())
            return false;

        std::vector<std::pair<float, ObjectGuid>> order;
        auto relOf = [refBearing](Player* p)
        { return Position::NormalizeOrientation(ParkBearingTo(p->GetPositionX(), p->GetPositionY()) - refBearing); };
        auto eligible = [&](Player* p)
        {
            return p && p->IsInWorld() && p->IsAlive() && p->GetMap() == bot->GetMap() &&
                   GetHuhuranSlotRole(p, GET_PLAYERBOT_AI(p)) == role;
        };
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource())
                    if (eligible(member))
                        order.emplace_back(relOf(member), member->GetGUID());
        }
        else if (eligible(bot))
            order.emplace_back(relOf(bot), bot->GetGUID());
        std::sort(order.begin(), order.end());

        std::lock_guard<std::mutex> lock(huhuranMutex);
        HuhuranRankTable& table = huhuranStateByInstance[instanceId].ranks[slot];
        if (!table.built)  // first writer builds the whole table
        {
            for (uint32 i = 0; i < order.size(); ++i)
                table.rankByGuid.emplace(order[i].second, i);
            table.built = true;
        }
        auto r = table.rankByGuid.find(bot->GetGUID());
        if (r == table.rankByGuid.end())
            return false;
        rank = r->second;
        count = uint32(table.rankByGuid.size());
        return rank < count;
    }

    // Snapshot of one role's latched table (copied out of the lock).
    std::vector<std::pair<ObjectGuid, uint32>> CopyRankTable(uint32 instanceId, HuhuranSlotRole role)
    {
        std::vector<std::pair<ObjectGuid, uint32>> out;
        std::lock_guard<std::mutex> lock(huhuranMutex);
        auto it = huhuranStateByInstance.find(instanceId);
        if (it == huhuranStateByInstance.end() || !it->second.ranks[RoleSlot(role)].built)
            return out;
        for (auto const& entry : it->second.ranks[RoleSlot(role)].rankByGuid)
            out.emplace_back(entry.first, entry.second);
        return out;
    }

    // Bots of `role` within SLOT_TOLERANCE of their latched spot; `latched` = the table size.
    uint32 CountOnSpot(Player* bot, HuhuranSlotRole role, float refBearing, uint32& latched)
    {
        auto const table = CopyRankTable(bot->GetMap()->GetInstanceId(), role);
        latched = uint32(table.size());
        uint32 onSpot = 0;
        for (auto const& entry : table)
        {
            Player* p = ObjectAccessor::GetPlayer(*bot, entry.first);
            if (!p || !p->IsAlive() || entry.second >= latched)
                continue;
            Pt const s = Aq40HuhuranRules::RingSpot(ParkPt(), RoleRadius(role), latched, entry.second, refBearing);
            if (p->GetExactDist2d(s.x, s.y) <= Aq40HuhuranRules::SLOT_TOLERANCE)
                ++onSpot;
        }
        return onSpot;
    }

    // True the first time `guid` asks this attempt (claims it). Each bot asks only for itself (rev 3, §8.2 #4).
    bool ClaimHuhuranManaRefill(uint32 instanceId, ObjectGuid guid)
    {
        std::lock_guard<std::mutex> lock(huhuranMutex);
        return huhuranStateByInstance[instanceId].manaRefilled.insert(guid).second;
    }

    // True at most once per DIAG_INTERVAL_MS per instance (claims the slot when it returns true).
    bool HuhuranDiagDue(uint32 instanceId, uint32 now)
    {
        std::lock_guard<std::mutex> lock(huhuranMutex);
        HuhuranInstanceState& st = huhuranStateByInstance[instanceId];
        if (st.diagMs && getMSTimeDiff(st.diagMs, now) < Aq40HuhuranRules::DIAG_INTERVAL_MS)
            return false;
        st.diagMs = now;
        return true;
    }

    // Spec §4.6: one line per instance every DIAG_INTERVAL_MS at Logger.playerbots = 5.
    void LogHuhuranDiag(Player* bot, HuhuranView const& view)
    {
        Creature* const huhuran = view.huhuran;
        float const refBearing = PeekRefBearing(bot->GetMap()->GetInstanceId());

        uint32 rangedLatched = 0;
        uint32 healerLatched = 0;
        uint32 const rangedOn = CountOnSpot(bot, HuhuranSlotRole::Ranged, refBearing, rangedLatched);
        uint32 const healersOn = CountOnSpot(bot, HuhuranSlotRole::Healer, refBearing, healerLatched);

        // Noxious Poison victims, total and healers (26053 leaves an aura), and the predicted Poison Bolt
        // set: the BOLT_TARGETS nearest living players and pets, as spell_huhuran_poison_bolt selects.
        uint32 noxVictims = 0;
        uint32 noxHealers = 0;
        // Rev 6 (§11.2): the top living off-tank's threat on her (role tanks other than the MT).
        float otThreatMax = -1.0f;
        // Rev 3 (§8.2 #6): average mana % of the living healer bots.
        float healManaSum = 0.0f;
        uint32 healManaCount = 0;
        std::vector<std::pair<float, bool>> bodies;
        auto consider = [&](Player* p)
        {
            if (!p || !p->IsInWorld() || p->GetMap() != bot->GetMap() || !p->IsAlive())
                return;
            bool const healer = PlayerbotAI::IsHeal(p);
            if (p != view.mainTank && PlayerbotAI::IsTank(p))
                otThreatMax = std::max(otThreatMax, huhuran->GetThreatMgr().GetThreat(p));
            if (p->GetMaxPower(POWER_MANA) > 0 &&
                GetHuhuranSlotRole(p, GET_PLAYERBOT_AI(p)) == HuhuranSlotRole::Healer)
            {
                healManaSum += 100.0f * float(p->GetPower(POWER_MANA)) / float(p->GetMaxPower(POWER_MANA));
                ++healManaCount;
            }
            if (p->HasAura(SPELL_HUHURAN_NOXIOUS_POISON))
            {
                ++noxVictims;
                if (healer)
                    ++noxHealers;
            }
            bodies.emplace_back(huhuran->GetExactDist(p), healer);
            if (Pet* pet = p->GetPet())
                if (pet->IsInWorld() && pet->IsAlive())
                    bodies.emplace_back(huhuran->GetExactDist(pet), false);
        };
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                consider(ref->GetSource());
        }
        else
            consider(bot);
        uint32 const boltHealers = Aq40HuhuranRules::HealersAmongNearest(bodies, Aq40HuhuranRules::BOLT_TARGETS);

        std::string const mt = view.mainTank ? view.mainTank->GetName() + "#" +
                                                   std::to_string(view.mainTank->GetGUID().GetCounter())
                                             : std::string("none");
        // Rev 2 (§7.2 #5): who she is actually hitting - the first pull's loss of her to a non-tank was only
        // visible indirectly through mt= and her distance to P.
        Unit* const victim = huhuran->GetVictim();
        std::string const victimName = victim ? victim->GetName() : std::string("none");
        std::string const healMana =
            healManaCount ? std::to_string(int(healManaSum / float(healManaCount) + 0.5f)) : std::string("none");
        // Rev 6 (§11.2): top off-tank threat as a % of the MT's (backup-threat watch value).
        float const mtThreatDiag = HuhuranMtThreat(view);
        std::string const otThreat = (otThreatMax >= 0.0f && mtThreatDiag > 0.0f)
                                         ? std::to_string(int(100.0f * otThreatMax / mtThreatDiag + 0.5f))
                                         : std::string("none");
        // The MT's Acid Spit stacks (rev 5), kept as a watch value.
        std::string const acid = view.mainTank ? std::to_string(HuhuranAcidStacks(view.mainTank)) : std::string("none");
        LOG_DEBUG("playerbots",
                  "[Aq40Huhuran] diag t={} pos=({:.1f},{:.1f}) toP={:.1f} mt={} victim={} parked={} everParked={} "
                  "latched={} berserk={} ranged={}/{} healers={}/{} noxVictims={} noxHealers={} boltHealers={} "
                  "healMana={} acid={} otThreat={}",
                  view.msSinceEngage, huhuran->GetPositionX(), huhuran->GetPositionY(),
                  huhuran->GetExactDist2d(HUHURAN_PARK.GetPositionX(), HUHURAN_PARK.GetPositionY()), mt, victimName,
                  view.parked, view.everParked, view.spotsLatched, view.berserk, rangedOn, rangedLatched, healersOn,
                  healerLatched, noxVictims, noxHealers, boltHealers, healMana, acid, otThreat);
    }
}  // namespace

bool IsHuhuranEncounterActive(Player* bot)
{
    return FindHuhuran(bot) != nullptr;
}

Player* GetHuhuranMainTank(Player* bot, Creature* huhuran)
{
    // Aggro decides (Nefarian rule, spec §4.2): the tank she is hitting is the MT, so a resurrected tank
    // cannot reclaim the role while another tank holds her.
    if (huhuran)
        if (Unit* const victim = huhuran->GetVictim())
            if (Player* const v = victim->ToPlayer())
                if (v->IsInWorld() && v->GetMap() == bot->GetMap() && v->IsAlive() &&
                    (v == bot || bot->IsInSameRaidWith(v)) && PlayerbotAI::IsTank(v))
                    return v;

    Player* best = nullptr;
    auto consider = [&](Player* p)
    {
        if (p && p->IsInWorld() && p->GetMap() == bot->GetMap() && p->IsAlive() && GET_PLAYERBOT_AI(p) &&
            !IsSelfBot(p) &&
            PlayerbotAI::IsTank(p) && (!best || p->GetGUID() < best->GetGUID()))
            best = p;
    };
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            consider(ref->GetSource());
    }
    else
        consider(bot);
    return best;
}

bool GetHuhuranView(Player* bot, HuhuranView& view)
{
    using namespace Aq40HuhuranRules;

    // Inert until P and T are probe-authored (M1): every trigger, action and multiplier keyed on the view
    // is a no-op, so bots run stock behaviour. The Wyvern Sting dispel rule does not read P and stays live.
    if constexpr (HUHURAN_STATIONS_PROVISIONAL)
        return false;

    Creature* const huhuran = FindHuhuran(bot);
    if (!huhuran)
        return false;

    uint32 const instanceId = bot->GetMap()->GetInstanceId();
    uint32 const now = NowMs();
    float const distP = huhuran->GetExactDist2d(HUHURAN_PARK.GetPositionX(), HUHURAN_PARK.GetPositionY());

    bool const berserk = huhuran->HasAura(SPELL_HUHURAN_BERSERK);
    bool parked = false;
    bool everParked = false;
    bool latched = false;
    uint32 msSinceEngage = 0;
    uint32 msSinceBerserk = 0;
    {
        std::lock_guard<std::mutex> lock(huhuranMutex);
        // FindHuhuran just refreshed the attempt (TouchAttempt), so engageMs is set for this attempt.
        HuhuranInstanceState& st = huhuranStateByInstance[instanceId];
        msSinceEngage = st.engageMs ? getMSTimeDiff(st.engageMs, now) : 0;
        st.parked = UpdateParked(st.parked, distP);
        st.everParked = st.everParked || st.parked;
        st.spotsLatched = SpotsLatch(st.spotsLatched);  // rev 4: latched from the first view of the attempt
        parked = st.parked;
        everParked = st.everParked;
        latched = st.spotsLatched;
        if (berserk && !st.berserkMs)
            st.berserkMs = now;
        msSinceBerserk = st.berserkMs ? getMSTimeDiff(st.berserkMs, now) : 0;
    }

    view = HuhuranView{};
    view.huhuran = huhuran;
    view.mainTank = GetHuhuranMainTank(bot, huhuran);
    view.parked = parked;
    view.everParked = everParked;
    view.spotsLatched = latched;
    view.berserk = berserk;
    view.msSinceEngage = msSinceEngage;

    // Rev 3 (spec §8.2 #4), the Nefarian landing-refill pattern (BWLHelpers.cpp): every living healer BOT starts
    // Berserk at full mana, once per attempt (claimed by GUID in the instance state, cleared with the attempt),
    // only under the owner-approved playerbots `raid` cheat, only within MANA_REFILL_WINDOW_MS of the first
    // Berserk tick, never for humans/selfbots. Each bot refills only itself.
    if (berserk && msSinceBerserk <= MANA_REFILL_WINDOW_MS && bot->IsAlive() && bot->GetMaxPower(POWER_MANA) > 0 &&
        !IsSelfBot(bot))
        if (PlayerbotAI* const botAI = GET_PLAYERBOT_AI(bot))
            if (GetHuhuranSlotRole(bot, botAI) == HuhuranSlotRole::Healer && botAI->HasCheat(BotCheatMask::raid) &&
                ClaimHuhuranManaRefill(instanceId, bot->GetGUID()))
            {
                LOG_DEBUG("playerbots", "[Aq40Huhuran] {} mana refill at berserk", bot->GetName());
                bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));
            }

    if (HuhuranDiagDue(instanceId, now))
        LogHuhuranDiag(bot, view);
    return true;
}

HuhuranSlotRole GetHuhuranSlotRole(Player* bot, PlayerbotAI* botAI)
{
    // Humans are never slotted: a bot is `ai && !IsSelfBot` (project convention).
    if (!bot || !botAI || IsSelfBot(bot))
        return HuhuranSlotRole::None;
    if (PlayerbotAI::IsHeal(bot))
        return HuhuranSlotRole::Healer;
    if (PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsTank(bot))
        return HuhuranSlotRole::Ranged;
    return HuhuranSlotRole::None;
}

bool GetHuhuranSpot(Player* bot, PlayerbotAI* botAI, HuhuranView const& view, Position& spot)
{
    if (!view.spotsLatched || !bot || !bot->IsInWorld())
        return false;
    HuhuranSlotRole const role = GetHuhuranSlotRole(bot, botAI);
    if (role == HuhuranSlotRole::None)
        return false;

    // P -> T. Latches T here from her current position if the spots latched before any Back step - with rev 4's
    // latch-at-engage this is the normal path: the first ring holder's call fixes T (so the ring bearing follows
    // her approach) before GetLatchedRank builds the table against it, and the MT's later Back step reads the
    // same latched T.
    Position const t = GetHuhuranTankStation(bot, view);
    float const refBearing = ParkBearingTo(t.GetPositionX(), t.GetPositionY());
    uint32 rank = 0;
    uint32 count = 0;
    // count > 0 is implied by rank < count; RingSpot divides by count.
    if (!GetLatchedRank(bot, role, refBearing, rank, count) || count == 0 || rank >= count)
        return false;

    Pt const s = Aq40HuhuranRules::RingSpot(ParkPt(), RoleRadius(role), count, rank, refBearing);
    float z = HUHURAN_PARK.GetPositionZ();
    bot->UpdateAllowedPositionZ(s.x, s.y, z);
    spot.Relocate(s.x, s.y, z, Position::NormalizeOrientation(std::atan2(
                                   HUHURAN_PARK.GetPositionY() - s.y, HUHURAN_PARK.GetPositionX() - s.x)));
    return true;
}

bool HuhuranKeepWyvernSting(Player* bot, Unit* target)
{
    if (!bot || bot->GetMapId() != AQ40_MAP_ID || !target)
        return false;
    Player* const victim = target->ToPlayer();
    if (!victim || PlayerbotAI::IsTank(victim))
        return false;
    return IsHuhuranEncounterActive(bot);
}

void HuhuranEraseTrackers(Player* bot)
{
    if (!bot || !bot->GetMap())
        return;

    std::lock_guard<std::mutex> lock(huhuranMutex);
    huhuranStateByInstance.erase(bot->GetMap()->GetInstanceId());
}

bool HuhuranOpenerHold(HuhuranView const& view)
{
    return Aq40HuhuranRules::HoldFireOnHuhuran(view.everParked, view.msSinceEngage);
}

// ---- rev 2 (spec §7.2) ----

bool HuhuranThreatDisciplined(Player* bot, HuhuranView const& view)
{
    return bot && view.huhuran && bot != view.mainTank && GET_PLAYERBOT_AI(bot) && !PlayerbotAI::IsHeal(bot);
}

uint32 HuhuranAcidStacks(Unit* unit)
{
    if (!unit)
        return 0;
    Aura* const aura = unit->GetAura(SPELL_HUHURAN_ACID_SPIT);
    return aura ? aura->GetStackAmount() : 0;
}

float HuhuranMtThreat(HuhuranView const& view)
{
    if (!view.huhuran || !view.mainTank)
        return 0.0f;
    // 0 when the MT is not on her threat list: no cap (spec §7.2 #2).
    return view.huhuran->GetThreatMgr().GetThreat(view.mainTank);
}

float HuhuranOwnThreat(Player* bot, HuhuranView const& view)
{
    if (!bot || !view.huhuran)
        return 0.0f;
    return view.huhuran->GetThreatMgr().GetThreat(bot);
}

float HuhuranThreatDenominator(Player* bot, HuhuranView const& view)
{
    using Aq40HuhuranRules::ThreatRole;
    if (!bot || !view.huhuran)
        return 0.0f;
    ThreatRole const role = HuhuranThreatRole(bot, view);
    float backup = 0.0f;
    if (role == ThreatRole::Other && !view.berserk)
    {
        // The top living off-tank on her: the highest threat of a living role tank other than the MT (humans
        // included - a human off-tank holds her just as well).
        if (Group* group = bot->GetGroup())
        {
            ThreatManager& mgr = view.huhuran->GetThreatMgr();
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* const m = ref->GetSource();
                if (!m || m == view.mainTank || !m->IsInWorld() || m->GetMap() != bot->GetMap() || !m->IsAlive() ||
                    !PlayerbotAI::IsTank(m))
                    continue;
                float const t = mgr.GetThreat(m);
                if (t > backup)
                    backup = t;
            }
        }
    }
    // Rev 5 §10.3: in Berserk DPS go against the MT.
    return Aq40HuhuranRules::ThreatDenominator(role, HuhuranMtThreat(view), backup, view.berserk);
}

Aq40HuhuranRules::ThreatRole HuhuranThreatRole(Player* bot, HuhuranView const& view)
{
    using Aq40HuhuranRules::ThreatRole;
    if (bot == view.mainTank)
        return ThreatRole::MainTank;
    return PlayerbotAI::IsTank(bot) ? ThreatRole::OffTank : ThreatRole::Other;
}

bool HuhuranThreatVetoed(Player* bot, HuhuranView const& view)
{
    return Aq40HuhuranRules::ThreatVetoed(HuhuranThreatRole(bot, view), HuhuranOwnThreat(bot, view),
                                          HuhuranThreatDenominator(bot, view), view.berserk);
}

bool HuhuranThreatHardStop(Player* bot, HuhuranView const& view)
{
    return Aq40HuhuranRules::ThreatHardStop(HuhuranThreatRole(bot, view), HuhuranOwnThreat(bot, view),
                                            HuhuranThreatDenominator(bot, view), view.berserk);
}

bool GetHuhuranOffTankSpot(Player* bot, HuhuranView const& view, Position& spot)
{
    Creature* const huhuran = view.huhuran;
    Player* const mt = view.mainTank;
    if (!bot || !huhuran || !mt || bot == mt || !PlayerbotAI::IsTank(bot) || !GET_PLAYERBOT_AI(bot) ||
        IsSelfBot(bot))
        return false;

    // Rank = GUID order among the living group bot role tanks other than the MT. It shifts when the MT
    // changes; the spot then moves by one 20-degree step at most per rank, which is fine at 4.5 y.
    Group* const group = bot->GetGroup();
    if (!group)
        return false;
    uint32 rank = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* const m = ref->GetSource();
        if (!m || m == mt || m == bot || !m->IsInWorld() || m->GetMap() != bot->GetMap() || !m->IsAlive() ||
            !GET_PLAYERBOT_AI(m) || IsSelfBot(m) || !PlayerbotAI::IsTank(m))
            continue;
        if (m->GetGUID() < bot->GetGUID())
            ++rank;
    }

    Aq40HuhuranRules::Pt const s = Aq40HuhuranRules::OffTankSpot(
        Pt{huhuran->GetPositionX(), huhuran->GetPositionY()}, Pt{mt->GetPositionX(), mt->GetPositionY()},
        huhuran->GetCombatReach(), rank);
    float z = huhuran->GetPositionZ();
    bot->UpdateAllowedPositionZ(s.x, s.y, z);
    spot.Relocate(s.x, s.y, z,
                  Position::NormalizeOrientation(std::atan2(huhuran->GetPositionY() - s.y,
                                                            huhuran->GetPositionX() - s.x)));
    return true;
}

// Task 14 (Twins v2 spec §15.1): shared with Twins (TwinsTankHealTarget); was HuhuranSpellManaCost.
bool Aq40SpellManaCost(Player* bot, PlayerbotAI* botAI, char const* spellName, int32& cost)
{
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spellName)->Get();
    SpellInfo const* const info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info)
        return false;
    cost = info->PowerType == POWER_MANA ? info->CalcPowerCost(bot, info->GetSchoolMask()) : 0;
    return true;
}

// Task 14 (Twins v2 spec §15.1): shared with the Twins side tank heal; was HuhuranTankHealSpellName.
char const* Aq40TankHealSpellName(Player* bot)
{
    if (!bot)
        return nullptr;
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

bool IsHuhuranTankHealer(Player* bot, PlayerbotAI* botAI, HuhuranView const& view)
{
    Player* const mt = view.mainTank;
    if (!mt || !bot->IsAlive() || GetHuhuranSlotRole(bot, botAI) != HuhuranSlotRole::Healer)
        return false;

    // The ceil(n/2) living group healer bots nearest the MT, from live positions each tick (R12): a role
    // choice, not a station, so re-picking as bodies move is fine.
    std::vector<Aq40HuhuranRules::HealerDist> healers;
    auto consider = [&](Player* p)
    {
        if (!p || !p->IsInWorld() || p->GetMap() != bot->GetMap() || !p->IsAlive() ||
            GetHuhuranSlotRole(p, GET_PLAYERBOT_AI(p)) != HuhuranSlotRole::Healer)
            return;
        healers.push_back({p->GetExactDist2d(mt), p->GetGUID().GetRawValue()});
    };
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            consider(ref->GetSource());
    }
    else
        consider(bot);
    return Aq40HuhuranRules::IsNearestTankHealer(healers, bot->GetGUID().GetRawValue());
}

bool GetHuhuranTankHeal(Player* bot, PlayerbotAI* botAI, HuhuranView const& view, Player*& tank, char const*& spell)
{
    constexpr float TANK_HEAL_RANGE = 40.0f;  // all four fast heals; CanCastSpell passes OUT_OF_RANGE

    // No MT: stock healing (R12).
    if (!view.berserk || !view.mainTank || !bot || !botAI)
        return false;
    char const* const name = Aq40TankHealSpellName(bot);
    if (!name || !IsHuhuranTankHealer(bot, botAI, view))
        return false;

    std::vector<Player*> tanks;
    std::vector<Aq40HuhuranRules::TankHp> hp;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* const m = ref->GetSource();
            if (!m || !m->IsInWorld() || m->GetMap() != bot->GetMap() || !m->IsAlive() || !PlayerbotAI::IsTank(m))
                continue;
            tanks.push_back(m);
            hp.push_back({m == view.mainTank, m->GetHealthPct()});
        }
    }
    int const pick = Aq40HuhuranRules::PickTankHealTarget(hp);
    if (pick < 0)
        return false;
    Player* const target = tanks[static_cast<size_t>(pick)];
    // Skip (stock healing runs) when this cast cannot go off now; no fallback to another tank.
    if (bot->GetDistance(target) > TANK_HEAL_RANGE || !bot->IsWithinLOSInMap(target) ||
        !botAI->CanCastSpell(name, target))
        return false;
    // Rev 3 (§8.2 #5): CanCastSpell checks with TRIGGERED_IGNORE_POWER_AND_REAGENT_COST, so it never sees mana -
    // pull 2's dry healers kept this heal first in priority and every cast failed. Check the real cost.
    int32 cost = 0;
    if (!Aq40SpellManaCost(bot, botAI, name, cost) || int32(bot->GetPower(POWER_MANA)) < cost)
        return false;
    tank = target;
    spell = name;
    return true;
}

HuhuranMtStep GetHuhuranMtStep(Player* bot, HuhuranView const& view, Unit* currentTarget)
{
    Creature* const huhuran = view.huhuran;
    if (!bot || !huhuran || bot != view.mainTank)
        return HuhuranMtStep::None;
    bool const victim = huhuran->GetVictim() == bot;
    bool const melee = bot->IsWithinMeleeRange(huhuran);
    if (!bot->IsWithinLOSInMap(huhuran))
        return HuhuranMtStep::Close;
    // Out of melee: close in - unless she is already on the MT and nearly there (she walks to her victim;
    // running at her would undo every backing step toward T). The slack only protects Back steps, which never
    // run while she is parked (R16): parked, the MT's ReachTarget is vetoed, so it must always close itself.
    if (!melee && !(victim && !view.parked && bot->GetDistance(huhuran) <= HUHURAN_MT_CHASE_SLACK))
        return HuhuranMtStep::Close;
    if (currentTarget != huhuran)
        return HuhuranMtStep::Attack;
    if (!victim || !melee || view.parked)
        return HuhuranMtStep::None;  // not on the MT yet: earn aggro with the normal rotation first
    // Every Back precondition but arrival holds: this is where T latches in the normal case.
    Position const t = GetHuhuranTankStation(bot, view);
    if (bot->GetExactDist2d(t.GetPositionX(), t.GetPositionY()) > HUHURAN_MT_ARRIVE)
        return HuhuranMtStep::Back;
    return HuhuranMtStep::None;
}

Position GetHuhuranTankStation(Player* bot, HuhuranView const& view)
{
    float const px = HUHURAN_PARK.GetPositionX();
    float const py = HUHURAN_PARK.GetPositionY();
    float const pz = HUHURAN_PARK.GetPositionZ();
    if (!bot || !bot->IsInWorld() || !bot->GetMap() || !view.huhuran)
        return HUHURAN_MT_STATION;

    // The candidate from her current 2-D position, computed outside the lock; only the first caller's sticks.
    float cx = HUHURAN_MT_STATION.GetPositionX();
    float cy = HUHURAN_MT_STATION.GetPositionY();
    float const dx = px - view.huhuran->GetPositionX();
    float const dy = py - view.huhuran->GetPositionY();
    float const d = std::hypot(dx, dy);
    if (d > HUHURAN_MT_STATION_DEGENERATE)
    {
        cx = px + HUHURAN_MT_STATION_OFFSET * dx / d;
        cy = py + HUHURAN_MT_STATION_OFFSET * dy / d;
    }

    float tx;
    float ty;
    bool latchedNow = false;
    uint32 const instanceId = bot->GetMap()->GetInstanceId();
    {
        std::lock_guard<std::mutex> lock(huhuranMutex);
        HuhuranInstanceState& st = huhuranStateByInstance[instanceId];
        if (!st.tankStationSet)
        {
            st.tankStationSet = true;
            st.tankX = cx;
            st.tankY = cy;
            latchedNow = true;
        }
        tx = st.tankX;
        ty = st.tankY;
    }
    if (latchedNow)
        LOG_DEBUG("playerbots", "[Aq40Huhuran] instance {}: tank station latched ({:.1f},{:.1f}), her dist to P {:.1f}",
                  instanceId, tx, ty, d);
    return Position(tx, ty, pz, Position::NormalizeOrientation(std::atan2(py - ty, px - tx)));
}

// ---- rev 3 (spec §8.2) ----

namespace
{
    char const* HuhuranMtBigDefensiveName(Player* bot)
    {
        switch (bot->getClass())
        {
            case CLASS_WARRIOR:
                return "shield wall";
            case CLASS_PALADIN:
                return "divine protection";
            case CLASS_DRUID:
                return "barkskin";
            default:
                return nullptr;
        }
    }

    bool HuhuranMtDefensiveUsed(Player* bot, HuhuranMtDefensive which)
    {
        std::lock_guard<std::mutex> lock(huhuranMutex);
        auto it = huhuranStateByInstance.find(bot->GetMap()->GetInstanceId());
        return it != huhuranStateByInstance.end() &&
               it->second.mtDefensiveUsed.count({bot->GetGUID(), uint8(which)}) != 0;
    }
}

HuhuranMtDefensive GetHuhuranMtDefensive(Player* bot, PlayerbotAI* botAI, HuhuranView const& view, char const*& spell)
{
    if (!bot || !botAI || !view.berserk || bot != view.mainTank || !bot->IsAlive())
        return HuhuranMtDefensive::None;
    if (char const* const big = HuhuranMtBigDefensiveName(bot))
        if (!HuhuranMtDefensiveUsed(bot, HuhuranMtDefensive::Big) && botAI->CanCastSpell(big, bot))
        {
            spell = big;
            return HuhuranMtDefensive::Big;
        }
    if (bot->getClass() == CLASS_WARRIOR && bot->GetHealthPct() < Aq40HuhuranRules::LAST_STAND_HP_PCT &&
        !HuhuranMtDefensiveUsed(bot, HuhuranMtDefensive::LastStand) && botAI->CanCastSpell("last stand", bot))
    {
        spell = "last stand";
        return HuhuranMtDefensive::LastStand;
    }
    return HuhuranMtDefensive::None;
}

void MarkHuhuranMtDefensiveUsed(Player* bot, HuhuranMtDefensive which)
{
    if (!bot || !bot->GetMap() || which == HuhuranMtDefensive::None)
        return;
    std::lock_guard<std::mutex> lock(huhuranMutex);
    huhuranStateByInstance[bot->GetMap()->GetInstanceId()].mtDefensiveUsed.insert({bot->GetGUID(), uint8(which)});
}

bool HuhuranMtShouldFace(Player* bot, HuhuranView const& view)
{
    return bot && view.huhuran && view.parked && bot == view.mainTank && bot->IsStopped() &&
           !bot->HasInArc(HUHURAN_FACE_ARC, view.huhuran);
}

}  // namespace TempleOfAhnQirajHelpers
